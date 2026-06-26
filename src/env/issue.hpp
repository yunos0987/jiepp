#pragma once
#include <exception>
#include <functional>
#include <optional>
#include <ostream>
#include <set>
#if __has_include(<source_location>)
#  include <source_location>
#endif
#ifndef __cpp_lib_source_location
// Minimal source_location substitute using compiler builtins
// (Clang < 15 with old libstdc++, or GCC < 10)
#  include <cstdint>
namespace std {
struct source_location {
    [[nodiscard]] static constexpr source_location current(
        const char* f  = __builtin_FILE(),
        uint_least32_t l = static_cast<uint_least32_t>(__builtin_LINE())) noexcept {
        source_location sl;
        sl.file_ = f;
        sl.line_ = l;
        return sl;
    }
    [[nodiscard]] constexpr const char* file_name() const noexcept { return file_; }
    [[nodiscard]] constexpr uint_least32_t line() const noexcept { return line_; }
private:
    const char* file_  = "";
    uint_least32_t line_ = 0;
};
} // namespace std
#endif
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class IssueMessage;

class Issue {
public:
    // ---- Severity ----
    enum class Severity { INFO, WARNING, ERROR, SEVERE };

    // ---- Issue codes ----
    enum class Code : unsigned int {
#define JIEPP_ISSUE_CODE(name, id, severity, message) name = (id),
#include "issue_codes.def"
#undef JIEPP_ISSUE_CODE
    };

    // ---- Code severity helpers ----
    static Severity severity_of(Code c);
    static bool is_severe (Code c) { return severity_of(c) == Severity::SEVERE; }
    static bool is_error  (Code c) { return severity_of(c) == Severity::ERROR; }
    static bool is_warning(Code c) { return severity_of(c) == Severity::WARNING; }
    static bool is_info   (Code c) { return severity_of(c) == Severity::INFO; }
    static bool is_fatal  (Code c) { return is_severe(c); }

    // ---- Code name lookup ----
    static std::string_view codename(Code code);

    // ---- Location type ----
    using LocationEntry = std::pair<int, std::string>;

    // ---- Initialization ----
    static void initialize(std::ostream& stream);
    static void set_output(std::ostream& stream);

    // ---- Location stack ----
    static void push(LocationEntry loc);
    static LocationEntry pop();
    static LocationEntry top();
    static std::string base_filepath();
    static std::string filepath() { return top().second; }
    static int lineno() { return top().first; }

    // ---- Ignore / block lists ----
    static void add_ignoring(Code code);
    static bool is_ignored(Code code);
    static void add_blocking(Code code);
    static void remove_blocking(Code code);
    static bool is_blocked(Code code);

    // ---- Issue a diagnostic ----
    static void happen(Code code, std::string context = "", std::source_location loc = std::source_location::current());
    [[noreturn]] static void fatal(std::string context = "", std::source_location loc = std::source_location::current());

    static std::ostream* stream_;
    static std::vector<Issue::LocationEntry> loc_stack_;
    static std::set<Code> ignorings_;
    static std::set<Code> blockings_;
    static bool silent_;
    static bool suppress_warnings_;
    static bool werror_;
    static IssueMessage& message_;

    // ---- Exception ----
    class Exception : public std::exception {
    public:
        Code code;
        Exception(Code code): code(code) {}
        const char* what() const noexcept override { return Issue::codename(code).data(); }
    };

    // ---- RAII guards ----
    struct LineGuard {
        LineGuard(int ln, std::optional<std::string> fp = std::nullopt);
        ~LineGuard();
    };

    // Execute f with fallback line context for error reporting.
    template <typename F>
    static void with_lineno(int lineno, F&& f) {
        Issue::LineGuard guard(lineno);
        f();
    }

    struct Blocking {
        Blocking(std::set<Code> codes);
        ~Blocking();
    private:
        std::set<Code> original_;
    };

    struct Ignoring {
        Ignoring(std::set<Code> codes);
        ~Ignoring();
    private:
        std::set<Code> original_;
    };
};

// ---- ISSUE macro ----
// Shorthand for Issue::happen with auto Issue::Code:: prefix.
// C++ source location is captured automatically via std::source_location default arg.
#define ISSUE(code, ...) \
    Issue::happen(Issue::Code::code __VA_OPT__(,) __VA_ARGS__)

#define FATAL(...) \
    Issue::fatal(__VA_ARGS__)
