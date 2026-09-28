// expand_ctrl.cpp — Conditional compilation control flow.
// Not part of Prosser's algorithm; jiepp extension for #if/#elif/#else/#endif.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"

#include "../constfold/constfold.hpp"
#include "../loader/loader.hpp"
#include "../loader/lexer.hpp"
#include "../macro/macro.hpp"
#include "../util/text.hpp"

#include <cctype>
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

        if (i < raw_cond.size() && raw_cond[i] == '"') {
            // "path" form
            ++i;
            std::size_t end = raw_cond.find('"', i);
            if (end != std::string::npos) {
                path = raw_cond.substr(i, end - i);
                load_type = Loader::LoadType::INCLUDE;
                i = end + 1;
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
        } else if (i < raw_cond.size() && raw_cond[i] == '\'') {
            // 'path' form (IEC string style)
            ++i;
            std::size_t end = raw_cond.find('\'', i);
            if (end != std::string::npos) {
                path = raw_cond.substr(i, end - i);
                load_type = Loader::LoadType::INCLUDE;
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
    bool had_defined = env.exist("defined");
    // C3/U5: install the temporary 'defined' operator via an RAII guard so
    // it is removed on scope exit even when preprocess_text() below throws
    // (previously the matching env.undef("defined") was skipped on that
    // path, leaking the operator into `env` for the rest of the run).
    struct DefinedGuard {
        Env& env;
        bool owns;
        ~DefinedGuard() { if (owns) env.undef("defined"); }
    } guard{env, !had_defined};
    DefinedOperator* defop;
    if (!had_defined) {
        auto d = std::make_unique<DefinedOperator>();
        defop = d.get();
        env.define("defined", std::move(d));
    } else {
        // Defensive only: 'defined' is installed transiently and
        // non-reentrantly by this function, so a nested eval_cond_str call
        // observing an already-installed 'defined' is not expected to
        // happen in practice. Reset it anyway so a hypothetical nested call
        // does not inherit a stale operand_error from an unrelated caller.
        defop = dynamic_cast<DefinedOperator*>(env.lookup("defined"));
        if (defop)
            defop->operand_error = false;
    }
    std::string result = preprocess_text(cond, env);
    if (operand_error)
        *operand_error = defop && defop->operand_error;
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
    int64_t val = eval_const_expr(expanded);
    return val != 0;
}

// C2: {#ifdef NAME}/{#ifndef NAME}. See expand_helpers.hpp for the contract.
bool eval_ifdef(const std::string& raw_arg, bool is_ifndef, Env& env) {
    auto ts = ts_trim(iec3_tokens_from_string(raw_arg, /*remove_comments=*/true));
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
