#include "option.hpp"
#include "jiepp.hpp"
#include "../env/issue.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace {

constexpr size_t FRAME_SIZE_ESTIMATE = 8192;  // ~8KB per recursion frame
constexpr int    MAX_RECURSION_LIMIT = 65536; // upper bound (~512MB stack)

#ifdef _WIN32

struct JieppThreadArgs {
    const JieppOptions* opts;
    int result;
};

DWORD WINAPI jiepp_thread_func(LPVOID arg) {
    auto* a = static_cast<JieppThreadArgs*>(arg);
    a->result = jiepp_command(*a->opts);
    return 0;
}

#endif

} // namespace

int main(int argc, char* argv[]) {
    Issue::initialize(std::cerr);

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
        // POSIX: set stack limit via setrlimit when specified
        if (opts.recursion_limit) {
            size_t stack_bytes = static_cast<size_t>(*opts.recursion_limit) * FRAME_SIZE_ESTIMATE;
            struct rlimit rl;
            rl.rlim_cur = stack_bytes;
            rl.rlim_max = stack_bytes;
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
    } catch (const std::exception& e) {
#ifdef JIEPP_SANDBOX
        (void)e;
        std::cerr << "<unknown location>: error: PP01: Unknown error\n";
#else
        std::cerr << "<unknown location>: error: PP01: Unknown error; " << e.what() << "\n";
#endif
        return 1;
    }
}
