#pragma once
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <ranges>
#include <fstream>
#include <optional>
#include <system_error>
#include <utility>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "env/issue.hpp"
#include "macro/macro.hpp"
#include "env/env.hpp"
#include "env/issue_message.hpp"
#include "core/preprocessor.hpp"

namespace fs = std::filesystem;

inline const fs::path& jiepp_root_dir() {
    static const fs::path p = fs::path(JIEPP_ROOT_DIR);
    return p;
}

inline const fs::path& jiepp_test_dir() {
    static const fs::path p = jiepp_root_dir() / "tests";
    return p;
}

// ---- RAII test guards (item e) ----
//
// Plain save-a-variable / restore-at-the-end-of-the-test CWD and std::cout
// swaps leave that global state (CWD, std::cout's streambuf) permanently
// changed for every later test in the same binary if an assertion failure
// or an unexpected exception skips the manual restore line. These two
// guards make the restore unconditional instead.

// Switches the process CWD to `dir` on construction and restores whatever
// CWD was in effect before construction, in a non-throwing destructor (the
// error_code overload of current_path() -- a destructor must not throw).
// Non-copyable.
class CwdGuard {
public:
    explicit CwdGuard(const fs::path& dir) : saved_(fs::current_path()) {
        fs::current_path(dir);
    }
    ~CwdGuard() {
        std::error_code ec;
        fs::current_path(saved_, ec);
    }
    CwdGuard(const CwdGuard&) = delete;
    CwdGuard& operator=(const CwdGuard&) = delete;
private:
    fs::path saved_;
};

// Redirects std::cout to `target`'s streambuf on construction and restores
// std::cout's original streambuf on destruction. Non-copyable.
class CoutRedirect {
public:
    explicit CoutRedirect(std::ostream& target) : saved_(std::cout.rdbuf(target.rdbuf())) {}
    ~CoutRedirect() { std::cout.rdbuf(saved_); }
    CoutRedirect(const CoutRedirect&) = delete;
    CoutRedirect& operator=(const CoutRedirect&) = delete;
private:
    std::streambuf* saved_;
};

// ---- preprocess helpers ----

inline std::string pp(std::istream& input, Env& env) {
    std::ostringstream o;
    preprocess(input, o, env);
    return o.str();
}

inline std::string pp(std::istream& input) {
    auto env = setup();
    auto r = pp(input, env);
    return r;
}

inline std::string pp_file(const fs::path& path, Env& env) {
    env.push_file(path.generic_string());
    Issue::push({1, path.generic_string()});
    std::ifstream i(path);
    auto r = pp(i, env);
    Issue::pop();
    env.pop_file();
    return r;
}

inline std::string pp_file(const fs::path& path) {
    Env env = setup();
    auto r = pp_file(path, env);
    return r;
}

inline std::string pp(const std::string& input, Env& env) {
    std::istringstream i(input);
    auto r = pp(i, env);
    return r;
}

inline std::string pp(const std::string& input) {
    auto env = setup();
    auto r = pp(input, env);
    return r;
}

// ---- Error capture infrastructure ----

class DiagBox {
public:
    DiagBox() { }
    ~DiagBox() { }

    void initialize() {
        release();
    }

    std::ostream& output() {
        return o_;
    }

    bool empty() const {
        return o_.str().empty();
    }

    std::vector<std::string> messages() {
        std::vector<std::string> ms;
        std::istringstream i(release());
        std::string line;
        while (std::getline(i, line))
            ms.push_back(PlainTextMessage::strip_source_location(line));
        return ms;
    }

    std::vector<Issue::Code> codes() {
        std::vector<Issue::Code> cs;
        for (const auto& line : messages())
            cs.push_back(PlainTextMessage::parse_code(line));
        return cs;
    }

    std::vector<std::string> codenames() {
        std::vector<std::string> cns;
        for (const auto& line : messages())
            cns.push_back(std::string(Issue::codename(PlainTextMessage::parse_code(line))));
        return cns;
    }

    std::string message()  {
        auto all = messages();
        if (all.size() != 1)
            return "**FAIL** num of errors: " + std::to_string(all.size()); // maybe assertion failure.
        return all.back();
    }
    Issue::Code code() {
        auto all = codes();
        if (all.size() != 1)
            return (Issue::Code) 0; // maybe assertion failure.
        return all.back();
    }
    std::string codename() {
        auto all = codenames();
        if (all.size() != 1)
            return "**FAIL** num of errors: " + std::to_string(all.size()); // maybe assertion failure.
        return all.back();
    }

private:
    std::string release() {
        std::string all = o_.str();
        o_.str("");
        o_.clear();
        return all;
    }

    std::ostringstream o_;
};

// SetUp/TearDown helper for test fixtures
struct JieppTest : public ::testing::Test, public DiagBox {

    void SetUp() override {
        ::testing::Test::SetUp();
        Issue::initialize(output());
        DiagBox::initialize();
    }

    void TearDown() override {
        ::testing::Test::TearDown();
    }
};
