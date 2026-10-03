#pragma once
#include "lineno.hpp"

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
    // item a: the line number is LineNo (64-bit storage, 32-bit unsigned
    // wrap, see lineno.hpp), matching FileContext's counter.
    using LocationEntry = std::pair<LineNo, std::string>;

    // Sentinel `file` value for diagnostics raised by CLI-level code
    // (jiepp_command()/main()/parse_args(), guarded by CliMode below) before
    // any source file has been pushed -- CLI option errors, -o/-MF open
    // failures, INVALID_COMMAND, the top-level input's FILE_NOT_FOUND, and
    // (C5/U2) a lexer diagnostic raised while tokenizing a -D/-U operand
    // (e.g. UNCLOSED_COMMENT), even though that reaches happen() through a
    // with_fallback_line()-pushed LineGuard copy of the dummy entry below,
    // not through loc_stack_ still being exactly the dummy. PlainTextMessage
    // ::message() renders this exact value without a trailing ":line.column",
    // i.e. "jiepp: error: PPxx: message". This is distinct from the
    // "<unknown location>" bottom-of-stack dummy pushed by initialize(): the
    // string-input preprocess()/preprocess_text() API keeps using that
    // dummy's "<unknown location>:N.0" form unchanged.
    static constexpr const char* CLI_LOCATION = "jiepp";

    // ---- Initialization ----
    static void initialize(std::ostream& stream);
    static void set_output(std::ostream& stream);

    // B1: library entry point (setup(), see preprocessor.cpp) calls this
    // before doing anything that can raise a diagnostic, so a caller that
    // links jiepp_lib directly and never calls initialize() still gets a
    // non-empty loc_stack_ (initialize()'s own dummy bottom entry). A no-op
    // whenever loc_stack_ already has an entry -- in particular, right after
    // main()'s own initialize() call, and on every setup() call after the
    // first -- so it never resets ignorings_/blockings_/werror_/etc. the way
    // initialize() does, and never discards a LineGuard the caller may
    // already have pushed.
    static void ensure_location_stack();

    // ---- Location stack ----
    static void push(LocationEntry loc);
    static LocationEntry pop();
    static LocationEntry top();
    // C4: set the line number of the top-of-stack entry in place, keeping
    // its file path. Used by preprocess() so that each call starts
    // diagnostics at the Env's own current line instead of wherever a
    // previous preprocess()/preprocess_text() call on the same loc_stack_
    // last left it (loc_stack_ otherwise only moves via advance_lineno()).
    static void set_top_lineno(LineNo ln);
    static std::string base_filepath();
    static std::string filepath() { return top().second; }
    static LineNo lineno() { return top().first; }

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
    static bool cli_mode_;
    // ---- E0: continue-after-error mode (set only by jiepp_command, via
    // ContinueMode below; the library's own default -- every ERROR/SEVERE
    // throws -- is unaffected when this is false) ----
    static bool continue_mode_;
    static int error_count_;
    static std::set<Code> continue_abort_codes_;
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
        LineGuard(LineNo ln, std::optional<std::string> fp = std::nullopt);
        ~LineGuard();
        // item f review follow-up: a copy would push once but pop twice
        // (double-pop) when both the original and the copy go out of
        // scope. Non-copyable, like FileContext::FileScope.
        LineGuard(const LineGuard&) = delete;
        LineGuard& operator=(const LineGuard&) = delete;
    };

    // Execute f with fallback line context for error reporting.
    template <typename F>
    static void with_lineno(LineNo lineno, F&& f) {
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

    // RAII guard marking CLI-level code (jiepp_command()/main()/parse_args())
    // for the duration of its scope: diagnostics raised while cli_mode_ is
    // set and no source file has been pushed yet (loc_stack_ still holds
    // only the initialize()-time dummy) render as CLI_LOCATION, not the
    // dummy's "<unknown location>:N.0" form. Guards nest safely: an inner
    // guard just restores the (already-true) outer value on scope exit.
    struct CliMode {
        CliMode();
        ~CliMode();
    private:
        bool original_;
    };

    // E0: RAII guard enabling continue-after-error mode for the duration of
    // its scope, with `abort_codes` as the set of codes that still stop
    // processing (SEVERE codes are always added, mirroring Blocking). While
    // active, happen() throws only for codes in this set; every other
    // ERROR-severity code (including a -Werror-promoted WARNING, judged by
    // its own original code, not by the fact that it was promoted) is
    // instead counted via error_count_ and swallowed so the caller can keep
    // going. continue_mode_ == false (the default, unaffected by this guard
    // ever having run before) reproduces today's library behavior exactly:
    // every ERROR/SEVERE throws, and a -Werror-promoted WARNING always
    // throws too (see happen()) -- this is what keeps the existing
    // EXPECT_THROW tests and WerrorPromotion passing unchanged.
    // error_count_ itself is deliberately NOT touched by this guard: it is
    // reset only by initialize(), so a caller can read it after the guarded
    // scope has already ended (jiepp_command does exactly this to decide
    // between the E3 destination table and the ordinary success path).
    struct ContinueMode {
        ContinueMode(std::set<Code> abort_codes);
        ~ContinueMode();
    private:
        bool original_continue_;
        std::set<Code> original_abort_;
    };
};

// ---- ISSUE macro ----
// Shorthand for Issue::happen with auto Issue::Code:: prefix.
// C++ source location is captured automatically via std::source_location default arg.
#define ISSUE(code, ...) \
    Issue::happen(Issue::Code::code __VA_OPT__(,) __VA_ARGS__)

#define FATAL(...) \
    Issue::fatal(__VA_ARGS__)
