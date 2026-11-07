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

#ifdef JIEPP_SANDBOX
thread_local bool g_sandbox_has_include_reported = false;
#endif

// Installs the transient 'defined' operator for one condition evaluation and
// removes it again on scope exit (also when the evaluation throws). When it
// is already installed -- a built-in such as __assert evaluated inside an
// {#if}/{#elif} condition -- the scope reuses it for this nested evaluation:
// the outer condition's operand-error state is saved and reset, the operator
// is re-enabled if a message expansion had suspended it, and everything is
// restored on scope exit, so the nested evaluation can neither clear nor
// inherit the outer condition's PP40 short-circuit. In a sandbox build the
// "PP62 reported for __has_include" state is kept the same way.
class DefinedScope {
public:
    explicit DefinedScope(Env& env) : env_(env) {
#ifdef JIEPP_SANDBOX
        saved_has_include_reported_ = g_sandbox_has_include_reported;
        g_sandbox_has_include_reported = false;
#endif
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
#ifdef JIEPP_SANDBOX
        g_sandbox_has_include_reported = saved_has_include_reported_;
#endif
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
#ifdef JIEPP_SANDBOX
    bool saved_has_include_reported_ = false;
#endif
};

// -nC: the comments of a condition become blanks as the lexer makes them
// (lex_text()), except inside a header name written directly after
// "__has_include (", where gcc/clang read comment openers as path text
// (read_header_name(), which needs the comment's text). One pass over the
// tokens: the keyword, blanks, '(', blanks and '<' open a header name, and
// the first token with a '>' in its text closes it.
void blank_comments_outside_header_names(std::vector<Token>& ts) {
    enum { normal, after_keyword, after_paren, in_header } state = normal;
    for (auto& t : ts) {
        if (state == in_header) {
            if (t.text.find('>') != std::string::npos)
                state = normal;
            continue;
        }
        if (t.type == Token::C) {
            t = t.num_of_lines > 0 ? Token::newline(t.num_of_lines) : Token::create(Token::WS, " ");
            continue;
        }
        if (t.type & Token::MASK_WS)
            continue;
        if (state == after_keyword && t.type == Token::LP)
            state = after_paren;
        else if (state == after_paren && t.type == Token::ANY && t.text == "<")
            state = in_header;
        else
            state = t.type == Token::ANY && t.text == "__has_include" ? after_keyword : normal;
    }
}

// The tokens of an {#if}/{#elif} condition, macro-expanded, as
// expand_operand_tokens() makes them for an operand; under -nC the comments
// are lexed and then made blanks by blank_comments_outside_header_names().
std::vector<Token> expand_condition(const std::string& raw_cond, Env& env) {
    namespace pd = jiepp::preprocessor_detail;
    if (!env.get_remove_comments())
        return pd::expand_operand_tokens(raw_cond, env);
    env.charge_expansion_steps(raw_cond.size());
    auto ts = pd::lex_operand(raw_cond, /*remove_comments=*/false);
    blank_comments_outside_header_names(ts);
    ts_mark_output_only(ts);
    std::vector<Token> ots;
    expand(ts, ots, env);
    return ots;
}

} // namespace

#ifdef JIEPP_SANDBOX
bool& sandbox_has_include_reported() {
    return g_sandbox_has_include_reported;
}
#endif

std::string eval_cond_str(const std::string& raw_cond, Env& env, bool* operand_error) {
    // C3/U5: the transient 'defined' operator is installed by an RAII
    // scope, so it is removed on scope exit even when expand_condition()
    // below throws. __has_include(...) is an operator of that expansion, as
    // in gcc/clang (expand()'s __has_include branch; PP62 in a sandbox
    // build).
    DefinedScope defined_scope(env);
    std::string result;
    for (const auto& t : expand_condition(raw_cond, env))
        result += t.text;
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
    // scanner knows no comment syntax. A __has_include(<a//b>) call was
    // resolved during the expansion, so its path is no longer in the text.
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

    // __has_include(...) is resolved by expand() like in eval_cond_str(). A
    // nested built-in call evaluates its own expression; a message is not an
    // expression (the operator is suspended there).
    std::string text;
    {
        DefinedScope defined_scope(env);
        std::vector<Token> ots;
        expand(ts, ots, env);
#ifdef JIEPP_SANDBOX
        // Sandbox: the operator is a filesystem probe (PP62, reported once
        // by expand()); the call text stays in the expression, so the
        // evaluator below reports it as well (same pair as {#if}).
        if (g_sandbox_has_include_reported)
            note(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE);
#endif
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
bool eval_ifdef(const std::string& raw_arg, bool is_ifndef, const char* directive, Env& env) {
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
              std::string("{#") + directive + " " +
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
// itself is restricted (PP62): there it reads as undefined. The
// __has_builtin(NAME) operator counts in both builds (it needs no filesystem
// access) and likewise survives {#undef __has_builtin}.
bool macro_name_is_defined(std::string_view name, Env& env) {
#ifndef JIEPP_SANDBOX
    if (name == "__has_include")
        return true;
#endif
    if (name == "__has_builtin")
        return true;
    Macro* m = env.lookup(name);
    return m && !dynamic_cast<DefinedOperator*>(m);
}

// The value of __has_builtin(NAME), SPEC 5.2: 1 for the two operators and for
// a symbol-table entry that is still a built-in kind (is_builtin_macro()); 0
// for value macros, user macros, undefined/redefined names and unknown names.
bool has_builtin(std::string_view name, Env& env) {
    if (name == "__has_builtin" || name == "defined")
        return true;
#ifndef JIEPP_SANDBOX
    if (name == "__has_include")
        return true;
#endif
    return is_builtin_macro(env.lookup(name));
}

// The __has_builtin operator branch of expand(): `work` has the operator token
// already popped. Errors follow the 'defined' branch (first error per
// condition only, state kept in the DefinedOperator, remaining text not
// evaluated by the caller); the result token is "1"/"0".
[[gnu::noinline]] void expand_has_builtin(std::vector<Token>& work,
                                          std::vector<Token>& ots,
                                          Env& env,
                                          DefinedOperator& defop) {
    auto skip_ws = [&work]() {
        while (!work.empty() && (work.back().type & Token::MASK_WS))
            work.pop_back();
    };
    auto fail = [&](Issue::Code code, const std::string& what) {
        if (!defop.operand_error) {
            defop.operand_error = true;
            defop.operand_error_code = code;
            if (code == Issue::Code::INVALID_BUILTIN_OPERAND)
                ISSUE(INVALID_BUILTIN_OPERAND, what);
            else
                ISSUE(INVALID_EXPRESSION, what);
        }
        ots.push_back(Token::create(Token::ANY, "0"));
    };

    skip_ws();
    if (work.empty() || work.back().type != Token::LP) {
        fail(Issue::Code::INVALID_EXPRESSION, "missing '(' after '__has_builtin'");
        return;
    }
    work.pop_back();
    skip_ws();

    if (work.empty() || work.back().type == Token::RP) {
        fail(Issue::Code::INVALID_BUILTIN_OPERAND, "__has_builtin: name missing");
        return;
    }
    if (work.back().type != Token::ANY || !iec3_is_identifier(work.back().text)) {
        fail(Issue::Code::INVALID_BUILTIN_OPERAND,
             "__has_builtin: name must be an identifier: " +
                 Util::escape_line_breaks(work.back().text));
        return;
    }
    std::string operand = std::move(work.back().text);
    work.pop_back();
    skip_ws();
    if (work.empty() || work.back().type != Token::RP) {
        fail(Issue::Code::INVALID_EXPRESSION, "missing ')' after '__has_builtin'");
        return;
    }
    work.pop_back();
    ots.push_back(Token::create(Token::ANY, has_builtin(operand, env) ? "1" : "0"));
}

} // namespace jiepp::expand_detail
