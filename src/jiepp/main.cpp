#include "option.hpp"
#include "jiepp.hpp"
#include "../env/issue.hpp"
#include "../util/stack_guard.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>

#ifdef _WIN32
// NOMINMAX: windows.h's own max()/min() macros would otherwise shadow
// std::max()/std::min() used below (requested_stack_bytes(), the setrlimit
// clamps) and fail to compile against the standard library overloads.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/resource.h>
#endif

namespace {

// --recursion-limit N requests N x 8 KiB of stack. This is a sizing unit, not
// a frame-size estimate: one level of nested function-macro expansion was
// measured at ~2 KiB (Release) and ~6.4 KiB (Debug).
constexpr std::size_t STACK_BYTES_PER_RECURSION_UNIT = 8192;
constexpr int         MAX_RECURSION_LIMIT = 65536;                 // 512 MiB
#ifdef _WIN32
// Floor for --recursion-limit on Windows, so a small N does not newly shrink
// the stack: a CreateThread() commit size below the executable's 1 MiB
// reserve had no effect, so Windows never ran on less than 1 MiB before this
// floor existed. POSIX has no such floor -- an explicit --recursion-limit N
// there keeps meaning exactly N x 8 KiB, like before.
constexpr std::size_t MIN_STACK_BYTES     = std::size_t{1} << 20;  // 1 MiB
#endif
// Stack used when --recursion-limit is absent, like clang's DesiredStackSize
// (clang/include/clang/Basic/Stack.h).
constexpr std::size_t DEFAULT_STACK_BYTES = std::size_t{8} << 20;  // 8 MiB

std::size_t requested_stack_bytes(const JieppOptions& opts) {
    if (!opts.recursion_limit)
        return DEFAULT_STACK_BYTES;
    const std::size_t bytes = static_cast<std::size_t>(*opts.recursion_limit) * STACK_BYTES_PER_RECURSION_UNIT;
#ifdef _WIN32
    return std::max(bytes, MIN_STACK_BYTES);
#else
    return bytes;
#endif
}

#ifndef _WIN32
// No --recursion-limit: like clang's ensureSufficientStack() (cc1_main.cpp)
// and gcc's stack_limit_increase(), raise only the soft RLIMIT_STACK to
// DEFAULT_STACK_BYTES -- never lower it, never touch the hard limit, clamp
// to the hard limit, and carry on silently if that is impossible.
void raise_soft_stack_limit_to_default() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) != 0)
        return;
    const rlim_t want = static_cast<rlim_t>(DEFAULT_STACK_BYTES);
    if (rl.rlim_cur == RLIM_INFINITY || rl.rlim_cur >= want)
        return;
    rl.rlim_cur = (rl.rlim_max == RLIM_INFINITY || rl.rlim_max >= want) ? want : rl.rlim_max;
    (void)setrlimit(RLIMIT_STACK, &rl);
}

// Stack budget for the PP63 guard: the soft RLIMIT_STACK now in force (read
// back after any change above), DEFAULT_STACK_BYTES when unlimited, or 0
// (guard disabled) when it cannot be read.
std::size_t current_stack_budget() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) != 0)
        return 0;
    if (rl.rlim_cur == RLIM_INFINITY)
        return DEFAULT_STACK_BYTES;
    return static_cast<std::size_t>(rl.rlim_cur);
}
#endif

// Last-resort diagnostic for exceptions that escape jiepp_command() itself
// (jiepp_command() already turns every Issue::Exception into a printed
// diagnostic + return 1, so only a genuinely unexpected std::exception
// reaches here). Not tied to any source file, so it uses Issue::CLI_LOCATION
// ("jiepp") rather than a line/column location.
void report_uncaught_exception([[maybe_unused]] const std::exception& e) {
#ifdef JIEPP_SANDBOX
    std::cerr << Issue::CLI_LOCATION << ": error: PP01: Unknown error\n";
#else
    std::cerr << Issue::CLI_LOCATION << ": error: PP01: Unknown error; " << e.what() << "\n";
#endif
}

// Unit-D-review hardening: a `catch (...)` fallback for a thrown object that
// is not even a std::exception (so no .what() exists to report), on top of
// the std::exception overload above. Belt-and-suspenders alongside it: no
// such throw site is known to exist today, but a truly last-resort handler
// should not assume one never will.
void report_uncaught_exception() {
    std::cerr << Issue::CLI_LOCATION << ": error: PP01: Unknown error\n";
}

#ifdef _WIN32

struct JieppThreadArgs {
    const JieppOptions* opts;
    int result;
    std::size_t stack_bytes;
};

// D3: mirror main()'s own exception handling here. jiepp_command() runs on
// this worker thread; a C++ exception that escapes a WinAPI thread callback
// cannot safely unwind past ABI boundary, so without this catch, an
// exception jiepp_command() does not itself catch (see the Issue::Exception
// remark above) would previously crash the process (observed as rc=127)
// instead of reporting "jiepp: error: PP01: ..." and exiting 1.
DWORD WINAPI jiepp_thread_func(LPVOID arg) {
    auto* a = static_cast<JieppThreadArgs*>(arg);
    // Record this thread's stack base for the PP63 guard (expand.cpp) before
    // anything runs on it.
    Util::note_stack_base(a->stack_bytes);
    try {
        a->result = jiepp_command(*a->opts);
    } catch (const Issue::Exception&) {
        a->result = 1;
    } catch (const std::exception& e) {
        report_uncaught_exception(e);
        a->result = 1;
    } catch (...) {
        report_uncaught_exception();
        a->result = 1;
    }
    return 0;
}

#endif

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // B15: the Windows CRT defaults stdout to text mode, silently translating
    // '\n' to "\r\n" on write. -o FILE is already opened in binary mode, so
    // without this, redirected stdout and -o output would differ byte-for-byte
    // for identical content. stderr is left in text mode: it only carries
    // human-facing diagnostics, never diffed against file output.
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    Issue::initialize(std::cerr);

    // Diagnostics raised directly by main() (below), parse_args(), or
    // jiepp_command() before any source file has been pushed are not tied
    // to a source file; see Issue::CLI_LOCATION.
    Issue::CliMode cli_mode_guard;

    try {
        JieppOptions opts = parse_args(argc, argv);

        // Validate recursion_limit
        if (opts.recursion_limit && *opts.recursion_limit > MAX_RECURSION_LIMIT) {
            try {
                ISSUE(RECURSION_LIMIT_RANGE, "must be <= " + std::to_string(MAX_RECURSION_LIMIT));
            } catch (const std::exception&) {
            }
            return 1;
        }

#ifdef _WIN32
        // Windows: run jiepp_command() on a worker thread whose stack is
        // *reserved* at stack_bytes (STACK_SIZE_PARAM_IS_A_RESERVATION);
        // pages are committed on demand. Without the flag the size is a
        // commit size: it charges memory up front, and a value below the
        // executable's 1 MiB default reserve has no effect (why Windows
        // additionally floors requested_stack_bytes() at 1 MiB).
        const std::size_t stack_bytes = requested_stack_bytes(opts);
        JieppThreadArgs args{&opts, 1, stack_bytes};
        HANDLE thread = CreateThread(NULL, stack_bytes, jiepp_thread_func, &args,
                                     STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
        if (!thread) {
            try {
                ISSUE(THREAD_CREATE_FAILED, "");
            } catch (const std::exception&) {
            }
            return 1;
        }
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        return args.result;
#else
        // POSIX: set stack limit via setrlimit when specified.
        // U5/F1: only the soft limit (rlim_cur) is raised, like gcc/clang --
        // the hard limit (rlim_max) is never touched. The previous code set
        // rlim_max = stack_bytes too, which *lowers* the hard limit whenever
        // stack_bytes is below the process's existing hard limit, making it
        // impossible for anything later in the process (or a child) to ever
        // raise the soft limit back above stack_bytes again.
        if (opts.recursion_limit) {
            // Explicit --recursion-limit: set the soft limit to exactly
            // requested_stack_bytes() (it may lower it), like before --
            // POSIX has no 1 MiB floor (unlike Windows above).
            const std::size_t stack_bytes = requested_stack_bytes(opts);
            struct rlimit rl;
            if (getrlimit(RLIMIT_STACK, &rl) != 0) {
                try {
                    ISSUE(STACK_LIMIT_FAILED, std::to_string(stack_bytes) + " bytes");
                } catch (const std::exception&) {
                }
                return 1;
            }
            // If the hard limit is below the requested size, clamp the soft
            // limit to it instead of failing outright (setrlimit() would
            // reject rlim_cur > rlim_max for an unprivileged process).
            rl.rlim_cur = (rl.rlim_max == RLIM_INFINITY)
                              ? static_cast<rlim_t>(stack_bytes)
                              : std::min(static_cast<rlim_t>(stack_bytes), rl.rlim_max);
            if (setrlimit(RLIMIT_STACK, &rl) != 0) {
                try {
                    ISSUE(STACK_LIMIT_FAILED, std::to_string(stack_bytes) + " bytes");
                } catch (const std::exception&) {
                }
                return 1;
            }
        } else {
            // No --recursion-limit: raise the default (8 MiB) like clang/gcc,
            // instead of leaving the OS/shell default in effect.
            raise_soft_stack_limit_to_default();
        }
        // Record this thread's stack base for the PP63 guard (expand.cpp),
        // reading back the soft limit now in force (just changed above).
        Util::note_stack_base(current_stack_budget());
        return jiepp_command(opts);
#endif
    } catch (const Issue::Exception&) {
        // Issue::happen() already printed the specific, correctly-coded
        // diagnostic before throwing (e.g. during parse_args(), which runs
        // outside jiepp_command()'s own try/catch and so would otherwise
        // reach the generic handler below). Just propagate the failure
        // exit code without appending a second, uninformative "PP01:
        // Unknown error" line on top of the diagnostic already shown.
        return 1;
    } catch (const std::exception& e) {
        report_uncaught_exception(e);
        return 1;
    } catch (...) {
        report_uncaught_exception();
        return 1;
    }
}
