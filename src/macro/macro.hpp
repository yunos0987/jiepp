#pragma once
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
    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override { return "__COUNTER__"; }
private:
    mutable int counter_ = 0;
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
};

// ---------------------------------------------------------------------------
// Function macro
// ---------------------------------------------------------------------------

class FunctionMacro : public Macro {
public:
    static constexpr int         NUM_OF_MAX_ARGS = (1 << 24) - 1;
    static constexpr const char* VA_ARGS = "__VA_ARGS__";
    static constexpr const char* VA_ARGC = "__VA_ARGC__";
    static constexpr const char* VA_SYM  = "...";

    // args_list: parameter names in order; last may be "..."
    FunctionMacro(std::vector<std::string> args_list, std::vector<Token> body);

    std::vector<Token> replacement(Env& env) const override;
    bool equal(const Macro& other) const override;
    std::string str() const override;
    bool is_function_macro() const override { return true; }

    // {name -> (index, is_va_arg)}
    const std::unordered_map<std::string, std::pair<int, bool>>& args() const { return args_; }
    int num_of_params_min() const { return num_params_min_; }
    int num_of_params_max() const { return num_params_max_; }
    const std::vector<Token>& body() const { return body_; }

private:
    std::unordered_map<std::string, std::pair<int, bool>> args_;
    int num_params_min_ = 0;
    int num_params_max_ = 0;
    std::vector<Token> body_;
    std::vector<std::string> args_list_; // original ordered parameter names

    static std::vector<Token> normalize(
        std::vector<Token> ts,
        const std::unordered_map<std::string, std::pair<int, bool>>& args);
};
