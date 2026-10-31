// builtin_function_macros.cpp -- invoke() of the built-in function-like
// macros __error(msg...) and __assert(expr, msg...) (SPEC 12.9) and the
// named counters __counter(name), __counter_peek(name) and
// __counter_set(name, value) (SPEC 12.10) and __nosplit(args...)
// (SPEC 12.11) and __xmlattr(args...) / __xmltext(args...) (SPEC 12.12)
// and __arg_lhs / __arg_rhs / __arg_dir(args...) (SPEC 12.13) (macro.hpp).
// They are called from expand()'s function-macro call branch (expand.cpp)
// with the collected, flattened, NOT pre-expanded arguments.

#include "expand_helpers.hpp"
#include "preprocessor.hpp"
#include "preprocessor_internal.hpp"

#include "../loader/lexer.hpp"
#include "../macro/macro.hpp"
#include "../util/text.hpp"

#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace jiepp::expand_detail;

namespace {

// While a message is expanded inside an {#if}/{#elif} condition, the
// transient 'defined' operator is suspended, so the word 'defined' in a
// message is plain text instead of an operator without an operand (PP40,
// which would make the whole condition false). Only applies when the
// operator is installed; a nested expression evaluation (an __assert in the
// message) re-enables it for its own expression (DefinedScope,
// expand_ctrl.cpp) and restores the suspension afterwards.
class SuspendDefined {
public:
    explicit SuspendDefined(Env& env)
        : op_(dynamic_cast<DefinedOperator*>(env.lookup("defined"))) {
        if (op_) {
            saved_ = op_->suspended;
            op_->suspended = true;
        }
    }
    ~SuspendDefined() {
        if (op_)
            op_->suspended = saved_;
    }
    SuspendDefined(const SuspendDefined&) = delete;
    SuspendDefined& operator=(const SuspendDefined&) = delete;

private:
    DefinedOperator* op_;
    bool saved_ = false;
};

// The message of __error/__assert: the variable-argument part from
// args[first] on, joined like __VA_ARGS__ (commas kept, one space on a side
// only where the call had whitespace there), then macro-expanded (hide sets
// kept) and concatenated as text, like the operand of {#error}.
// *only_ws is set to whether the expanded text is empty or whitespace only.
std::string expand_message(std::size_t first,
                           const std::vector<std::vector<Token>>& args,
                           const std::vector<ArgWs>& ws,
                           Env& env,
                           bool* only_ws) {
    std::vector<Token> msg = select_arg(static_cast<int>(first), args, /*is_va=*/true,
                                        &ws, VaJoin::plain);
    std::vector<Token> ots;
    {
        SuspendDefined suspend(env);
        expand(msg, ots, env);
    }
    std::string text;
    for (const auto& t : ots)
        text += t.text;
    if (only_ws) {
        *only_ws = true;
        for (char c : text) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                *only_ws = false;
                break;
            }
        }
    }
    return text;
}

// PP91 through the shared message path ({#error} uses it too).
void report_error_message(std::string msg) {
    jiepp::preprocessor_detail::report_message(std::move(msg), Issue::Code::ERROR_MESSAGE);
}

constexpr std::uint32_t kCounterMax =
    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());

// An operand as written (whitespace collapsed like stringizing, SPEC 4.1),
// with line breaks escaped so it fits a one-line diagnostic.
std::string operand_text(const std::vector<Token>& arg) {
    return Util::escape_line_breaks(stringize_text(arg));
}

// The counter name of a named-counter call: exactly one identifier token
// once whitespace and comments are removed (the test of {#ifdef}). It is not
// macro-expanded: like the operand of defined, it names something in its
// own namespace. Otherwise reports PP39 and returns nullopt.
std::optional<std::string> counter_name(const char* builtin, const std::vector<Token>& arg) {
    const Token* name = nullptr;
    std::size_t count = 0;
    for (const auto& t : arg) {
        if (t.type & Token::MASK_WS)
            continue;
        name = &t;
        ++count;
    }
    if (count == 0) {
        ISSUE(INVALID_BUILTIN_OPERAND, std::string(builtin) + ": counter name missing");
        return std::nullopt;
    }
    if (count != 1 || name->type != Token::ANY || !iec3_is_identifier(name->text)) {
        ISSUE(INVALID_BUILTIN_OPERAND, std::string(builtin) +
                                           ": counter name must be an identifier: " +
                                           operand_text(arg));
        return std::nullopt;
    }
    return name->text;
}

Token counter_token(std::uint32_t v) {
    return Token::create(Token::ANY, std::to_string(v));
}

} // namespace

std::vector<Token> ErrorBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                        const std::vector<ArgWs>& ws,
                                        Env& env) const {
    report_error_message(expand_message(0, args, ws, env, nullptr));
    return {};
}

std::vector<Token> AssertBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                         const std::vector<ArgWs>& ws,
                                         Env& env) const {
    // args has at least one entry (min_args() == 1; an empty call is one
    // empty argument).
    CondValue r = eval_cond_value_tokens(args[0], env);
    if (r.failure != Issue::Code::SUCCESS) {
        // The evaluator diagnostic was reported: like clang static_assert,
        // nothing is stacked on top. If {#ignore} suppressed it, the
        // assertion must still not pass silently, so PP91 is reported.
        if (!Issue::is_ignored(r.failure))
            return {};
    } else if (r.value.truthy()) {
        // True: silent, and the message is not expanded at all.
        return {};
    }

    std::string msg;
    bool only_ws = true;
    if (args.size() >= 2)
        msg = expand_message(1, args, ws, env, &only_ws);
    if (only_ws) {
        // clang's static_assert wording; EXPR is the expression as written,
        // whitespace collapsed like stringizing (SPEC 4.1).
        msg = "static assertion failed due to requirement '" + stringize_text(args[0]) + "'";
    }
    report_error_message(std::move(msg));
    return {};
}

std::vector<Token> CounterBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                          const std::vector<ArgWs>& /*ws*/,
                                          Env& env) const {
    // args has exactly one entry (min_args() == max_args() == 1; an empty
    // call is one empty argument, reported as a missing name).
    const std::string builtin = str();
    auto name = counter_name(builtin.c_str(), args[0]);
    if (!name)
        return {};
    if (kind_ == Kind::peek) {
        // No insert: peeking a never-used name gives 0 without creating it.
        std::uint32_t v = env.counter_value(*name);
        if (v > kCounterMax) {
            ISSUE(COUNTER_OVERFLOW, builtin + "(" + *name + ")");
            v = kCounterMax;
        }
        return {counter_token(v)};
    }
    // Mirrors CounterMacro::replacement(): once the value to emit would
    // exceed INT32_MAX, report PP53 and saturate (it never recovers until
    // __counter_set).
    std::uint32_t& slot = env.counter_slot(*name);
    if (slot > kCounterMax) {
        ISSUE(COUNTER_OVERFLOW, builtin + "(" + *name + ")");
        slot = kCounterMax;
    }
    return {counter_token(slot++)};
}

std::vector<Token> CounterSetBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                             const std::vector<ArgWs>& /*ws*/,
                                             Env& env) const {
    // args has exactly two entries (min_args() == max_args() == 2).
    // Order: name check, then value evaluation, then store, so a value that
    // reads the same counter (__counter_set(a, __counter(a) + 10)) is
    // evaluated first and the stored value wins.
    auto name = counter_name("__counter_set", args[0]);
    if (!name)
        return {};

    CondValue r = eval_cond_value_tokens(args[1], env);
    if (r.failure != Issue::Code::SUCCESS) {
        // The evaluator diagnostic was reported and the counter keeps its
        // value. If {#ignore} suppressed that diagnostic, the failure must
        // still not pass silently, so PP39 is reported.
        if (Issue::is_ignored(r.failure))
            ISSUE(INVALID_BUILTIN_OPERAND,
                  "__counter_set: value could not be evaluated: " + operand_text(args[1]));
        return {};
    }

    // The typed value, never the 0/1 truth value of {#if}.
    std::uint64_t v = 0;
    bool in_range = true;
    std::string shown;
    switch (r.value.kind) {
    case ValueKind::Float: {
        // No implicit float-to-integer conversion (SPEC 6.3 never converts
        // between Int and Float). A suppressed PP50 still must not pass
        // silently, so PP39 is reported instead.
        const std::string ctx = "__counter_set: value must be an integer: " + operand_text(args[1]);
        if (Issue::is_ignored(Issue::Code::EXPR_TYPE_ERROR))
            ISSUE(INVALID_BUILTIN_OPERAND, ctx);
        else
            ISSUE(EXPR_TYPE_ERROR, ctx);
        return {};
    }
    case ValueKind::Int:
        in_range = r.value.i >= 0 && r.value.i <= static_cast<std::int64_t>(kCounterMax);
        v = in_range ? static_cast<std::uint64_t>(r.value.i) : 0;
        shown = std::to_string(r.value.i);
        break;
    case ValueKind::Bool:
        v = r.value.b ? 1 : 0;
        break;
    case ValueKind::Bitstring:
        // The unsigned bit pattern (BYTE#16#FF = 255).
        in_range = r.value.bits <= kCounterMax;
        v = r.value.bits;
        shown = std::to_string(r.value.bits);
        break;
    }
    if (!in_range) {
        ISSUE(INVALID_BUILTIN_OPERAND, "__counter_set: value out of range 0..2147483647: " + shown);
        return {};
    }
    env.counter_slot(*name) = static_cast<std::uint32_t>(v);
    return {};
}

std::vector<Token> NosplitBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                          const std::vector<ArgWs>& ws,
                                          Env& env) const {
    // Joined like __VA_ARGS__ (empty for an empty call) and expanded once.
    // 'defined' is NOT suspended (unlike expand_message()): the result is
    // part of an {#if} expression, like a function-macro argument.
    std::vector<Token> joined = select_arg(0, args, /*is_va=*/true, &ws, VaJoin::plain);
    std::vector<Token> out;
    expand(joined, out, env);
    // Same depth rule as the argument collector in expand.cpp: only a comma
    // outside (...)/[...] would separate arguments, so only those change.
    int depth = 0;
    for (auto& t : out) {
        if (t.type == Token::LP || t.type == Token::LB)
            ++depth;
        else if ((t.type == Token::RP || t.type == Token::RB) && depth > 0)
            --depth;
        else if (t.type == Token::SEP && depth == 0)
            t.type = Token::ANY;
    }
    return out;
}

namespace {

// Raw LF, CR and TAB become one space each; & < > become entity references,
// and so do " and ' when `quotes` is set (an attribute value). Everything
// else (including '$' and bytes >= 0x80) is copied unchanged.
std::string xml_escape(const std::string& text, bool quotes) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += quotes ? "&quot;" : "\""; break;
        case '\'': out += quotes ? "&apos;" : "'"; break;
        case '\n':
        case '\r':
        case '\t': out += ' '; break;
        default: out += c; break;
        }
    }
    return out;
}

} // namespace

std::vector<Token> XmlBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                      const std::vector<ArgWs>& ws,
                                      Env& env) const {
    // Joined like @__VA_ARGS__ and expanded once (like __nosplit), then the
    // text is built like `@` and escaped. __xmlattr gives one WSTRING token
    // holding the double-quoted text; __xmltext gives the text itself.
    std::vector<Token> joined = select_arg(0, args, /*is_va=*/true, &ws, VaJoin::stringize);
    std::vector<Token> expanded;
    expand(joined, expanded, env);
    const bool attr = kind_ == Kind::attr;
    std::string text = xml_escape(stringize_text(expanded), attr);
    std::vector<Token> out;
    if (attr)
        out.push_back(Token::create(Token::WSTRING, "\"" + text + "\""));
    else if (!text.empty())
        out.push_back(Token::create(Token::STRING, std::move(text)));
    return out;
}

namespace {

struct ArgSeparator {
    std::size_t begin; // index of the first separator token
    std::size_t end;   // index just past the separator
    bool arrow;        // '=>' rather than ':='
};

// The first ':=' or '=>' outside (...)/[...]. The lexer gives ':' '=' and
// '=' '>' as separate tokens, so two adjacent tokens (no whitespace between
// them) match as well as one token of that spelling (a glued ':=').
// String, comment and pragma tokens never match: they are other token types.
std::optional<ArgSeparator> find_arg_separator(const std::vector<Token>& ts) {
    int depth = 0;
    for (std::size_t i = 0; i < ts.size(); ++i) {
        const Token& t = ts[i];
        if (t.type == Token::LP || t.type == Token::LB) {
            ++depth;
        } else if ((t.type == Token::RP || t.type == Token::RB) && depth > 0) {
            --depth;
        } else if (depth == 0 && t.type == Token::ANY) {
            if (t.text == ":=")
                return ArgSeparator{i, i + 1, false};
            if (t.text == "=>")
                return ArgSeparator{i, i + 1, true};
            if (i + 1 < ts.size() && ts[i + 1].type == Token::ANY) {
                if (t.text == ":" && ts[i + 1].text == "=")
                    return ArgSeparator{i, i + 2, false};
                if (t.text == "=" && ts[i + 1].text == ">")
                    return ArgSeparator{i, i + 2, true};
            }
        }
    }
    return std::nullopt;
}

// ts[from, to) without the whitespace and comment tokens at both ends.
std::vector<Token> trim_ws(const std::vector<Token>& ts, std::size_t from, std::size_t to) {
    while (from < to && (ts[from].type & Token::MASK_WS))
        ++from;
    while (to > from && (ts[to - 1].type & Token::MASK_WS))
        --to;
    return std::vector<Token>(ts.begin() + static_cast<std::ptrdiff_t>(from),
                              ts.begin() + static_cast<std::ptrdiff_t>(to));
}

} // namespace

std::vector<Token> ArgSplitBuiltin::invoke(std::vector<std::vector<Token>>& args,
                                           const std::vector<ArgWs>& ws,
                                           Env& env) const {
    // Joined like __VA_ARGS__ and expanded once, like __nosplit; the
    // result is rescanned like any built-in result.
    std::vector<Token> joined = select_arg(0, args, /*is_va=*/true, &ws, VaJoin::plain);
    std::vector<Token> expanded;
    expand(joined, expanded, env);
    auto sep = find_arg_separator(expanded);
    if (!sep) {
        ISSUE(ARG_SEPARATOR_MISSING, str() + ": no ':=' or '=>' at the top level");
        return {};
    }
    switch (kind_) {
    case Kind::lhs: return trim_ws(expanded, 0, sep->begin);
    case Kind::rhs: return trim_ws(expanded, sep->end, expanded.size());
    case Kind::dir: break;
    }
    return {Token::create(Token::ANY, sep->arrow ? "1" : "0")};
}
