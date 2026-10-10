#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../env/issue.hpp"
#include "../loader/token.hpp"

class Env;

namespace jiepp::expand_detail {
struct ArgWs;
}

// Per-token __VA_OPT__ layout of a normalized macro body (S-01). Present
// (size == body size) only when the body has a __VA_OPT__ token. Offsets are
// relative to the entry's own token, so the content subrange of an enclosing
// __VA_OPT__ can use the matching subrange of the table: inside a closed
// content every nested '(' also closes inside it, and an offset that leaves
// the subrange is clamped by subst() to "no further token".
struct VaOptSpan {
    std::uint32_t next = 0;   // offset to the next non-MASK_WS token (size - i if none)
    std::uint32_t close = 0;  // __VA_OPT__ only: offset to the ')' matching the '(' at
                              // `next`; 0 = malformed (no '(' there, or unclosed)
    std::uint32_t nested = 0; // __VA_OPT__ only: __VA_OPT__ tokens after that '(' up to
                              // `close` (to the body end if unclosed; 0 if no '(') --
                              // one PP37 each, raised by subst() per occurrence
};

// Computes the VaOptSpan table of `body` in O(body) (one backward pass, one
// '('/')' stack pass, one prefix count). Returns an empty table when `body`
// has no __VA_OPT__ token.
std::vector<VaOptSpan> compute_va_opt_spans(std::span<const Token> body);

// ---------------------------------------------------------------------------
// Base class
// ---------------------------------------------------------------------------

class Macro {
public:
    virtual ~Macro() = default;
    virtual std::vector<Token> replacement(Env& env) const = 0;
    virtual bool equal(const Macro& other) const = 0;
    virtual std::string str() const = 0;
    virtual bool is_function_macro() const { return false; }
};

// ---------------------------------------------------------------------------
// Object macros
// ---------------------------------------------------------------------------

class ObjectMacro : public Macro {};

class UserDefinedObjectMacro : public ObjectMacro {
public:
    explicit UserDefinedObjectMacro(std::vector<Token> ts);
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override;
    const std::vector<Token>& tokens() const { return ts_; }
    // See VaOptSpan; derived from tokens(), so not part of equal().
    const std::vector<VaOptSpan>& va_opt_spans() const { return va_opt_spans_; }
private:
    std::vector<Token> ts_;
    std::vector<VaOptSpan> va_opt_spans_;
    static std::vector<Token> normalize(std::vector<Token> ts);
};

class CounterMacro : public ObjectMacro {
public:
    // `start` is a test seam: production code always uses the default (0).
    // It lets a unit test instantiate a CounterMacro already close to
    // INT32_MAX without actually calling replacement() ~2^31 times.
    explicit CounterMacro(std::uint32_t start = 0) : counter_(start) {}
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__COUNTER__"; }
private:
    mutable std::uint32_t counter_;
};

class LineMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__LINE__"; }
};

class FileMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__FILE__"; }
};

class DateMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__DATE__"; }
};

class TimeMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__TIME__"; }
};

class TimeStampMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__TIMESTAMP__"; }
};

class IncludeLevelMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__INCLUDE_LEVEL__"; }
};

class BaseFileMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__BASE_FILE__"; }
};

class FileNameMacro : public ObjectMacro {
public:
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__FILE_NAME__"; }
};

class DefinedOperator : public Macro {
public:
    std::vector<Token> replacement(Env& /*env*/) const override { return {}; }
    bool equal(const Macro& other) const override {
        return dynamic_cast<const DefinedOperator*>(&other) != nullptr;
    }
    std::string str() const override { return "defined"; }

    // C3: set by expand()'s 'defined' branch the first time an operand is
    // missing/non-identifier or an opened '(' has no matching ')' within one
    // #if/#elif condition, so only the first such error in that condition is
    // reported (like clang) and eval_cond() can short-circuit the rest of
    // the expression without evaluating it. Not part of macro identity, so
    // equal() (which only checks the dynamic type) intentionally ignores it.
    bool operand_error = false;
    // The code of that first error (INVALID_DEFINED_OPERAND, or
    // INVALID_EXPRESSION for a missing ')'), set together with
    // operand_error; read by eval_cond_value_tokens() as its failure code.
    Issue::Code operand_error_code = Issue::Code::SUCCESS;
    // While true, expand() treats the word 'defined' as an ordinary
    // identifier instead of this operator. Set by __error/__assert while
    // they expand a message inside a condition (a message is not an
    // expression); cleared again for a nested expression evaluation.
    bool suspended = false;
};

// ---------------------------------------------------------------------------
// Function macro
// ---------------------------------------------------------------------------

class FunctionMacro : public Macro {
public:
    static constexpr int         NUM_OF_MAX_ARGS = (1 << 24) - 1;
    static constexpr const char* VA_ARGS = "__VA_ARGS__";
    static constexpr const char* VA_ARGC = "__VA_ARGC__";
    static constexpr const char* VA_OPT  = "__VA_OPT__";
    static constexpr const char* VA_SYM  = "...";

    // args_list: parameter names in order. The last may be "..." (C99
    // variadic: the variable arguments are __VA_ARGS__). With
    // named_variadic, the last name is instead a GNU named variadic
    // parameter (`args...`): it takes the variable arguments, and
    // __VA_ARGS__ is an ordinary identifier in the body (gcc/clang).
    FunctionMacro(std::vector<std::string> args_list, std::vector<Token> body,
                  bool named_variadic = false);

    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override;
    bool is_function_macro() const override { return true; }

    // {name -> (index, is_va_arg)}. A variadic macro (either form) also has
    // the key VA_SYM ("...") -> (index of the first variable argument, true);
    // no body token can spell it (the lexer emits each '.' separately), so
    // subst() uses it to ask "is this macro variadic" (__VA_OPT__).
    const std::unordered_map<std::string, std::pair<int, bool>>& args() const { return args_; }
    int num_of_params_min() const { return num_params_min_; }
    int num_of_params_max() const { return num_params_max_; }
    const std::vector<Token>& body() const { return body_; }

    // task_slug arg-expand-once (D2): per formal index (by pidx, as used in
    // args()), the dense slot for a parameter spelled >= 2 times in the
    // body, or -1 if spelled 0 or 1 times. "Spelled" over-counts (counts
    // every ANY body token resolving through args() to pidx, including
    // @/@@ operands and __VA_OPT__ content, excluding __VA_ARGC__), so it
    // never under-counts a real expansion need. Computed once in the
    // constructor; not part of equal().
    const std::vector<int>& arg_slots() const { return arg_slots_; }
    // Per slot (indexed by arg_slots()[pidx]): the precomputed spelling
    // count, i.e. the number of times expand_subst.cpp's subst() must be
    // asked for that parameter's expansion within one invocation.
    const std::vector<int>& arg_slot_uses() const { return arg_slot_uses_; }
    int num_arg_slots() const { return static_cast<int>(arg_slot_uses_.size()); }

    // See VaOptSpan; derived from body(), so not part of equal().
    const std::vector<VaOptSpan>& va_opt_spans() const { return va_opt_spans_; }

private:
    std::unordered_map<std::string, std::pair<int, bool>> args_;
    int num_params_min_ = 0;
    int num_params_max_ = 0;
    std::vector<Token> body_;
    std::vector<std::string> args_list_; // original ordered parameter names
    bool named_variadic_ = false;
    std::vector<int> arg_slots_;     // size == number of formals; -1 or slot index
    std::vector<int> arg_slot_uses_; // size == number of slots; spelling count
    std::vector<VaOptSpan> va_opt_spans_;

    // task_slug arg-expand-once (D2). num_formals is the number of distinct
    // formal-parameter slots in args() (regular params plus, for a
    // variadic macro, one more for the variable-arguments index) -- i.e.
    // one past the highest pidx that occurs in args()'s values.
    static void compute_arg_slots(
        const std::vector<Token>& body,
        const std::unordered_map<std::string, std::pair<int, bool>>& args,
        int num_formals,
        std::vector<int>& out_slots,
        std::vector<int>& out_slot_uses);

    static std::vector<Token> normalize(
        std::vector<Token> ts,
        const std::unordered_map<std::string, std::pair<int, bool>>& args);
};

// ---------------------------------------------------------------------------
// Built-in function-like macros (__error, __assert; named counters below)
// ---------------------------------------------------------------------------

// Called like a function macro (needs an opening parenthesis, PP34 on an
// unterminated or wrong-count call, arguments collected by the expand()
// call loop with [...] comma protection and the same handling of directives
// in the argument list), but its arguments are NOT pre-expanded and its
// result is computed by invoke(). Deliberately NOT derived from
// FunctionMacro, so define_directive_text()/-dM skip it (like __COUNTER__).
class BuiltinFunctionMacro : public Macro {
public:
    std::vector<Token> replacement(Env& /*env*/) const override { return {}; } // unused
    bool is_function_macro() const override { return true; }
    virtual int min_args() const = 0;
    // FunctionMacro::NUM_OF_MAX_ARGS means a variadic tail.
    virtual int max_args() const = 0;
    // args: one token vector per actual, ts_flatten()ed like FunctionMacro
    // actuals (hide sets kept). ws: per-actual whitespace flags, built as for
    // a variadic FunctionMacro call (empty when max_args() is not variadic).
    // Implementations live in src/core/builtin_function_macros.cpp.
    virtual std::vector<Token> invoke(std::vector<std::vector<Token>>& args,
                                      const std::vector<jiepp::expand_detail::ArgWs>& ws,
                                      Env& env) const = 0;
};

// __error(msg...): reports PP91 with the macro-expanded message; expands to
// nothing.
class ErrorBuiltin : public BuiltinFunctionMacro {
public:
    bool equal(const Macro& other) const override {
        return dynamic_cast<const ErrorBuiltin*>(&other) != nullptr;
    }
    std::string str() const override { return "__error"; }
    int min_args() const override { return 0; }
    int max_args() const override { return FunctionMacro::NUM_OF_MAX_ARGS; }
    std::vector<Token> invoke(std::vector<std::vector<Token>>& args,
                              const std::vector<jiepp::expand_detail::ArgWs>& ws,
                              Env& env) const override;
};

// __assert(expr, msg...): evaluates expr with the {#if} rules; when it is
// false, reports PP91 with the macro-expanded message (or the clang default
// text); expands to nothing.
class AssertBuiltin : public BuiltinFunctionMacro {
public:
    bool equal(const Macro& other) const override {
        return dynamic_cast<const AssertBuiltin*>(&other) != nullptr;
    }
    std::string str() const override { return "__assert"; }
    int min_args() const override { return 1; }
    int max_args() const override { return FunctionMacro::NUM_OF_MAX_ARGS; }
    std::vector<Token> invoke(std::vector<std::vector<Token>>& args,
                              const std::vector<jiepp::expand_detail::ArgWs>& ws,
                              Env& env) const override;
};

// ---------------------------------------------------------------------------
// Named counters (__counter, __counter_peek, __counter_set; SPEC 12.10)
// ---------------------------------------------------------------------------

// __counter(name): the current value of the named counter, then increments
// it (first value 0). __counter_peek(name): the current value without
// incrementing (a never-used name gives 0 and is not created). The name is
// one identifier and is NOT macro-expanded. The values live in the Env
// (Param::counter_value()/counter_slot()), not in this object, so undefining
// the built-in does not clear them.
class CounterBuiltin : public BuiltinFunctionMacro {
public:
    enum class Kind { next, peek };
    explicit CounterBuiltin(Kind kind) : kind_(kind) {}
    bool equal(const Macro& other) const override {
        auto* o = dynamic_cast<const CounterBuiltin*>(&other);
        return o != nullptr && o->kind_ == kind_;
    }
    std::string str() const override {
        return kind_ == Kind::next ? "__counter" : "__counter_peek";
    }
    int min_args() const override { return 1; }
    int max_args() const override { return 1; }
    std::vector<Token> invoke(std::vector<std::vector<Token>>& args,
                              const std::vector<jiepp::expand_detail::ArgWs>& ws,
                              Env& env) const override;

private:
    Kind kind_;
};

// __counter_set(name, value): evaluates value with the {#if} rules and, when
// it is an integer in 0..2147483647, stores it; expands to nothing.
class CounterSetBuiltin : public BuiltinFunctionMacro {
public:
    bool equal(const Macro& other) const override {
        return dynamic_cast<const CounterSetBuiltin*>(&other) != nullptr;
    }
    std::string str() const override { return "__counter_set"; }
    int min_args() const override { return 2; }
    int max_args() const override { return 2; }
    std::vector<Token> invoke(std::vector<std::vector<Token>>& args,
                              const std::vector<jiepp::expand_detail::ArgWs>& ws,
                              Env& env) const override;
};
