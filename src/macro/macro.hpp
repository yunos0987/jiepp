#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../loader/token.hpp"

class Env;

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
private:
    std::vector<Token> ts_;
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

    // True iff the body stringizes its variable arguments (@__VA_ARGS__/@args,
    // or __VA_ARGS__/args used inside @__VA_OPT__(...)): the only case where
    // expand.cpp needs to compute per-actual-argument whitespace flags (R6).
    // Computed once in the constructor from body_/args_, so it cannot affect
    // equal() and is not compared there.
    bool stringizes_va() const { return stringizes_va_; }

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

private:
    std::unordered_map<std::string, std::pair<int, bool>> args_;
    int num_params_min_ = 0;
    int num_params_max_ = 0;
    std::vector<Token> body_;
    std::vector<std::string> args_list_; // original ordered parameter names
    bool named_variadic_ = false;
    bool stringizes_va_ = false;
    std::vector<int> arg_slots_;     // size == number of formals; -1 or slot index
    std::vector<int> arg_slot_uses_; // size == number of slots; spelling count

    static bool compute_stringizes_va(
        const std::vector<Token>& body,
        const std::unordered_map<std::string, std::pair<int, bool>>& args);

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
