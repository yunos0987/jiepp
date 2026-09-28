#include "option.hpp"
#include "jiepp.hpp"
#include "../env/issue.hpp"
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/resource.h>
#endif

namespace {

constexpr size_t FRAME_SIZE_ESTIMATE = 8192;  // ~8KB per recursion frame
constexpr int    MAX_RECURSION_LIMIT = 65536; // upper bound (~512MB stack)

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
};

// D3: mirror main()'s own exception handling here. jiepp_command() runs on
// this worker thread; a C++ exception that escapes a WinAPI thread callback
// cannot safely unwind past ABI boundary, so without this catch, an
// exception jiepp_command() does not itself catch (see the Issue::Exception
// remark above) would previously crash the process (observed as rc=127)
// instead of reporting "jiepp: error: PP01: ..." and exiting 1.
DWORD WINAPI jiepp_thread_func(LPVOID arg) {
    auto* a = static_cast<JieppThreadArgs*>(arg);
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
        // Windows: use CreateThread to manage stack size via thread parameters
        size_t stack_size = opts.recursion_limit
            ? (static_cast<size_t>(*opts.recursion_limit) * FRAME_SIZE_ESTIMATE)
            : 0;  // 0 = use default stack size

        JieppThreadArgs args{&opts, 1};
        HANDLE thread = CreateThread(NULL, stack_size, jiepp_thread_func, &args, 0, NULL);
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
            size_t stack_bytes = static_cast<size_t>(*opts.recursion_limit) * FRAME_SIZE_ESTIMATE;
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
        }
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
