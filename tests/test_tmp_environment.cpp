// Gives every test process its own temporary directory.
//
// ctest runs each test case as a separate process, and `ctest -j` runs many of
// them at the same time. Many tests (and their fixtures) create files and
// directories under fs::temp_directory_path() with fixed names such as
// "jiepp_smoke_test" and remove them again in TearDown(), so concurrent
// processes deleted each other's files. Pointing the process's temporary
// directory at a per-process subdirectory (named after the process id) makes
// every fixed name private to its process without touching the tests.
//
// The environment is registered at static-initialization time, so it applies
// to every test in the jiepp_test binary and runs before the first test.

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

long current_process_id() {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(::getpid());
#endif
}

void set_temp_env(const std::string& dir) {
#ifdef _WIN32
    // fs::temp_directory_path() reads TMP, then TEMP, on Windows.
    _putenv_s("TMP", dir.c_str());
    _putenv_s("TEMP", dir.c_str());
#else
    // fs::temp_directory_path() reads TMPDIR (and a few others) on POSIX.
    ::setenv("TMPDIR", dir.c_str(), 1);
#endif
}

class PerProcessTempDirEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        std::error_code ec;
        const fs::path base = fs::temp_directory_path(ec);
        if (ec)
            return; // keep the default temporary directory
        dir_ = base / ("jiepp_test_" + std::to_string(current_process_id()));
        // A directory left by an earlier process that had the same id would
        // otherwise leak its files into this run.
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_, ec);
        if (ec) {
            dir_.clear();
            return;
        }
        set_temp_env(dir_.string());
    }

    void TearDown() override {
        if (dir_.empty())
            return;
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }

private:
    fs::path dir_;
};

// gtest takes ownership of the registered environment.
[[maybe_unused]] ::testing::Environment* const g_per_process_temp_dir_env =
    ::testing::AddGlobalTestEnvironment(new PerProcessTempDirEnvironment);

} // namespace
