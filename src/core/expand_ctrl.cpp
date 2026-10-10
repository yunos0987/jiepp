// expand_ctrl.cpp — Conditional compilation control flow.
// Not part of Prosser's algorithm; jiepp extension for #if/#elif/#else/#endif.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../constfold/constfold.hpp"
#include "../loader/loader.hpp"
#include "../loader/directive_parser.hpp"
#include "../loader/lexer.hpp"
#include "../macro/macro.hpp"
#include "../util/text.hpp"

#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace jiepp::expand_detail {

bool ctrl_is_active(const std::vector<CtrlState>& stack) {
    for (auto& s : stack) {
        if (!s.condition.has_value() || !s.condition.value()) return false;
    }
    return true;
}

bool ctrl_parent_active(const std::vector<CtrlState>& stack) {
    for (std::size_t i = 0; i + 1 < stack.size(); ++i) {
        if (!stack[i].condition.has_value() || !stack[i].condition.value()) return false;
    }
    return true;
}

namespace {

// Only called from the non-sandbox branch of eval_cond_str() below; in a
// sandbox build it would otherwise be an unused function (__has_include is
// rejected outright there, see find_has_include_call()).
#ifndef JIEPP_SANDBOX

// Replace all __has_include("path") and __has_include(<path>) in raw_cond
// with "1" or "0" based on whether the file is found.
// This runs before macro expansion so arguments are NOT expanded.
std::string resolve_has_include(const std::string& raw_cond, Env& env) {
    static constexpr std::string_view KW = "__has_include";
    std::string result;
    result.reserve(raw_cond.size());
    std::size_t pos = 0;
    while (pos < raw_cond.size()) {
        std::size_t found = raw_cond.find(KW, pos);
        if (found == std::string::npos) {
            result.append(raw_cond, pos);
            break;
        }
        result.append(raw_cond, pos, found - pos);

        if (found > 0) {
            unsigned char prev = static_cast<unsigned char>(raw_cond[found - 1]);
            if (std::isalnum(prev) || prev == '_') {
                // KW is the tail of a longer identifier (e.g. a user macro
                // name ending in __has_include), not the operator itself.
                // Treat it as ordinary text and keep scanning after it.
                result.append(KW);
                pos = found + KW.size();
                continue;
            }
        }

        std::size_t i = found + KW.size();
        // skip whitespace
        while (i < raw_cond.size() && std::isspace(static_cast<unsigned char>(raw_cond[i]))) ++i;
        // expect '('
        if (i >= raw_cond.size() || raw_cond[i] != '(') {
            result.append(KW);
            pos = found + KW.size();
            continue;
        }
        ++i; // skip '('
        // skip whitespace
        while (i < raw_cond.size() && std::isspace(static_cast<unsigned char>(raw_cond[i]))) ++i;

        // determine quote style and extract path
        std::string path;
        Loader::LoadType load_type;
        bool valid = false;

        if (i < raw_cond.size() && (raw_cond[i] == '"' || raw_cond[i] == '\'')) {
            // "path" / 'path' form: a string literal, which the directive
            // decoding left with its IEC escapes as written (SPEC §2), so
            // find its end past "$'"-style escapes and decode it the way
            // {#include} decodes its path (strip_path()).
            std::size_t end = string_literal_end(raw_cond, i);
            if (end != std::string_view::npos) {
                path = decode_path_literal(std::string_view(raw_cond).substr(i, end - i));
                load_type = Loader::LoadType::INCLUDE;
                i = end;
                valid = true;
            }
        } else if (i < raw_cond.size() && raw_cond[i] == '<') {
            // <path> form
            ++i;
            std::size_t end = raw_cond.find('>', i);
            if (end != std::string::npos) {
                path = raw_cond.substr(i, end - i);
                load_type = Loader::LoadType::SINCLUDE;
                i = end + 1;
                valid = true;
            }
        }

        if (valid) {
            // skip whitespace after path
            while (i < raw_cond.size() && std::isspace(static_cast<unsigned char>(raw_cond[i]))) ++i;
            // expect ')'
            if (i < raw_cond.size() && raw_cond[i] == ')') {
                ++i;
                bool exists = !Loader::fullpath(path, load_type, env).empty();
                result.append(exists ? "1" : "0");
                pos = i;
                continue;
            }
        }

        // malformed — pass through as-is
        result.append(KW);
        pos = found + KW.size();
    }
    return result;
}

#endif  // !JIEPP_SANDBOX

#ifdef JIEPP_SANDBOX
// Index of the first actual __has_include operator use in s -- the keyword
// not preceded by an identifier character (so a longer name such as
// weird__has_include does not match) and followed, after optional
// whitespace, by '(' -- or npos. `defined(__has_include)` and
// `defined __has_include` are feature tests, not uses.
std::size_t find_has_include_call(const std::string& s) {
    static constexpr std::string_view KW = "__has_include";
    std::size_t pos = 0;
    while ((pos = s.find(KW, pos)) != std::string::npos) {
        std::size_t kw = pos;
        pos += KW.size();
        if (kw > 0) {
            unsigned char prev = static_cast<unsigned char>(s[kw - 1]);
            if (std::isalnum(prev) || prev == '_')
                continue;
        }
        std::size_t i = pos;
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '(')
            return kw;
    }
    return std::string::npos;
}
#endif

// Installs the transient 'defined' operator for one condition evaluation and
// removes it again on scope exit (also when the evaluation throws). When it
// is already installed -- a built-in such as __assert evaluated inside an
// {#if}/{#elif} condition -- the scope reuses it for this nested evaluation:
// the outer condition's operand-error state is saved and reset, the operator
// is re-enabled if a message expansion had suspended it, and everything is
// restored on scope exit, so the nested evaluation can neither clear nor
// inherit the outer condition's PP40 short-circuit.
class DefinedScope {
public:
    explicit DefinedScope(Env& env) : env_(env) {
        if (auto* d = dynamic_cast<DefinedOperator*>(env.lookup("defined"))) {
            op_ = d;
            owns_ = false;
            saved_error_ = d->operand_error;
            saved_code_ = d->operand_error_code;
            saved_suspended_ = d->suspended;
            d->operand_error = false;
            d->operand_error_code = Issue::Code::SUCCESS;
            d->suspended = false;
        } else {
            auto owned = std::make_unique<DefinedOperator>();
            op_ = owned.get();
            env.define("defined", std::move(owned));
            owns_ = true;
        }
    }
    ~DefinedScope() {
        if (owns_) {
            env_.undef("defined");
        } else {
            op_->operand_error = saved_error_;
            op_->operand_error_code = saved_code_;
            op_->suspended = saved_suspended_;
        }
    }
    DefinedScope(const DefinedScope&) = delete;
    DefinedScope& operator=(const DefinedScope&) = delete;
    // Valid for the lifetime of the scope (a superseded or undefined macro
    // is retired, never destroyed: symtab.hpp lifetime invariant).
    DefinedOperator* op() const { return op_; }

private:
    Env& env_;
    DefinedOperator* op_ = nullptr;
    bool owns_ = false;
    bool saved_error_ = false;
    Issue::Code saved_code_ = Issue::Code::SUCCESS;
    bool saved_suspended_ = false;
};

} // namespace

std::string eval_cond_str(const std::string& raw_cond, Env& env, bool* operand_error) {
#ifdef JIEPP_SANDBOX
    // In sandbox mode the __has_include operator is not allowed (filesystem
    // probe); only an actual use is PP62, see find_has_include_call().
    if (find_has_include_call(raw_cond) != std::string::npos)
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "__has_include");
    std::string cond = raw_cond;
#else
    std::string cond = resolve_has_include(raw_cond, env);
#endif
    // C3/U5: the transient 'defined' operator is installed by an RAII
    // scope, so it is removed on scope exit even when expand_operand_text()
    // below throws.
    DefinedScope defined_scope(env);
    std::string result = jiepp::preprocessor_detail::expand_operand_text(cond, env);
    if (operand_error)
        *operand_error = defined_scope.op()->operand_error;
    return result;
}

bool eval_cond(const std::string& raw_cond, Env& env) {
    bool operand_error = false;
    std::string expanded = eval_cond_str(raw_cond, env, &operand_error);
    // C3: a 'defined' operand/paren error already reported the diagnostic;
    // the whole condition is false and the rest of it is not evaluated,
    // like clang.
    if (operand_error)
        return false;
    // Comments are whitespace in the expression, as in C: those written in
    // the operand and those a macro body brought in (bodies keep comments
    // unless -nC) both survive expand_operand_text() as text, and constfold's
    // scanner knows no comment syntax. Blanked only after
    // resolve_has_include() ran on the raw text, so a
    // __has_include(<a//b>) path is not mistaken for a comment.
    int64_t val = eval_const_expr(iec3_blank_out_comments(expanded));
    return val != 0;
}

// See expand_helpers.hpp for the contract. Mirrors eval_cond_str() +
// eval_cond() on tokens instead of re-lexed text.
CondValue eval_cond_value_tokens(std::vector<Token> ts, Env& env) {
    CondValue r;
    auto note = [&r](Issue::Code code) {
        if (r.failure == Issue::Code::SUCCESS)
            r.failure = code;
    };

    // Work budget (PP64): one step per byte of the operand, like the
    // operand charge of expand_operand_tokens() for an {#if} condition.
    std::size_t bytes = 0;
    for (const auto& t : ts)
        bytes += t.text.size();
    env.charge_expansion_steps(bytes);

    // __has_include(...) written in the operand: the same rule as
    // find_has_include_call() / resolve_has_include() on an {#if} text --
    // the keyword followed (after whitespace or comments) by '(' -- before
    // macro expansion, so its argument is not expanded.
    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (ts[i].type != Token::ANY || ts[i].text != "__has_include")
            continue;
        std::size_t j = i + 1;
        while (j < ts.size() && (ts[j].type & Token::MASK_WS))
            ++j;
        if (j >= ts.size() || ts[j].type != Token::LP)
            continue;
#ifdef JIEPP_SANDBOX
        // Sandbox: the operator is a filesystem probe (PP62), reported once
        // like eval_cond_str(); the call text stays in the expression, so
        // the evaluator below reports it as well (same pair as {#if}).
        note(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE);
        ISSUE(SANDBOX_RESTRICTED_DIRECTIVE, "__has_include");
        break;
#else
        std::size_t k = j;
        int depth = 0;
        for (; k < ts.size(); ++k) {
            if (ts[k].type == Token::LP)
                ++depth;
            else if (ts[k].type == Token::RP && --depth == 0)
                break;
        }
        if (k >= ts.size())
            continue; // unclosed: left as is, the evaluator reports it
        // Comments are whitespace here, as in the expression itself.
        std::string text;
        for (std::size_t m = i; m <= k; ++m)
            text += (ts[m].type == Token::C) ? std::string(" ") : ts[m].text;
        std::string res = resolve_has_include(text, env);
        if (res == "1" || res == "0") {
            ts.erase(ts.begin() + static_cast<std::ptrdiff_t>(i + 1),
                     ts.begin() + static_cast<std::ptrdiff_t>(k + 1));
            ts[i] = Token::create(Token::ANY, res);
        }
#endif
    }

    std::string text;
    {
        DefinedScope defined_scope(env);
        std::vector<Token> ots;
        expand(ts, ots, env);
        // C3: a 'defined' operand/paren error already reported its
        // diagnostic; the rest of the expression is not evaluated, like
        // eval_cond().
        if (defined_scope.op()->operand_error) {
            note(defined_scope.op()->operand_error_code);
            return r;
        }
        for (const auto& t : ots)
            text += t.text;
    }
    // Comments are whitespace in the expression (see eval_cond()).
    cf::EvalResult ev = eval_const_value(iec3_blank_out_comments(text));
    if (ev.failure != Issue::Code::SUCCESS)
        note(ev.failure);
    if (r.failure == Issue::Code::SUCCESS)
        r.value = ev.value;
    return r;
}

// C2: {#ifdef NAME}/{#ifndef NAME}. See expand_helpers.hpp for the contract.
bool eval_ifdef(const std::string& raw_arg, bool is_ifndef, Env& env) {
    auto ts = ts_trim(jiepp::preprocessor_detail::lex_operand(raw_arg, /*remove_comments=*/true));
    if (ts.empty()) {
        ISSUE(INVALID_DEFINED_OPERAND, "macro name missing");
        return false;
    }
    if (ts[0].type != Token::ANY || !iec3_is_identifier(ts[0].text)) {
        ISSUE(INVALID_DEFINED_OPERAND,
              "macro name must be an identifier: " + Util::escape_line_breaks(raw_arg));
        return false;
    }
    if (ts.size() > 1) {
        // Like gcc/clang ("extra tokens at end of #ifdef directive"), a
        // warning; only NAME is tested. The extra tokens are never evaluated
        // as an expression (formerly "{#ifdef X) \or\ (1}" was spliced into a
        // defined(...) string and evaluated). Not reached in an inactive
        // group (the caller only evaluates active {#ifdef}s).
        ISSUE(EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
              std::string(is_ifndef ? "{#ifndef " : "{#ifdef ") +
                  Util::escape_line_breaks(Util::trim_view(raw_arg)) + "}");
    }
    bool is_def = macro_name_is_defined(ts[0].text, env);
    return is_ifndef ? !is_def : is_def;
}

// Whether NAME counts as defined for {#ifdef}/{#ifndef}/`defined`.
// A macro in env counts (the transient 'defined' operator itself does not).
// Like gcc/clang, `__has_include` also counts in a normal build so code can
// feature-test it; jiepp's __has_include(...) operator cannot be disabled
// by {#undef}/{#define}, so it stays defined even after
// {#undef __has_include}. Not in a JIEPP_SANDBOX build, where the operator
// itself is restricted (PP62): there it reads as undefined.
bool macro_name_is_defined(std::string_view name, Env& env) {
#ifndef JIEPP_SANDBOX
    if (name == "__has_include")
        return true;
#endif
    Macro* m = env.lookup(name);
    return m && !dynamic_cast<DefinedOperator*>(m);
}

} // namespace jiepp::expand_detail
