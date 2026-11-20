#pragma once
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
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

// ---- text and file helpers ----

// `s` without its space characters (' ' only; newlines and tabs stay).
inline std::string strip(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

// The bytes of file `p` (binary mode: no newline translation).
inline std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// Creates or overwrites file `p` with `content` (binary mode).
inline void write_file(const fs::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary);
    f << content;
}

// ---- expected-message builders ----
//
// The exact message text in PlainTextMessage::message()'s
// "<unknown location>:L.0: ..." form used by pp()/pp_file() when no real
// file has been pushed.

// The PP29 warning at line `line` for context `ctx`.
inline std::string w29(int line, const std::string& ctx) {
    return "<unknown location>:" + std::to_string(line) +
           ".0: warning: PP29: Missing terminating quote character; '" + ctx + "'";
}

// The PP49 warning at line `line` for context `ctx`.
inline std::string w49(int line, const std::string& ctx) {
    return "<unknown location>:" + std::to_string(line) +
           ".0: warning: PP49: Extra tokens at end of directive; '" + ctx + "'";
}

// The PP80 warning for a __VA_OPT__ token in the definition of macro `name`.
inline std::string vow(const std::string& name) {
    return "<unknown location>:1.0: warning: PP80: __VA_OPT__ outside a variadic macro; '" +
           name + "'";
}

// `n` PP80 warnings for macro `name`.
inline std::vector<std::string> vows(const std::string& name, int n) {
    return std::vector<std::string>(static_cast<std::size_t>(n), vow(name));
}

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

    // Each diagnostic line without its location prefix, e.g. "PP91: 'boom'".
    std::vector<std::string> diags() {
        std::vector<std::string> out;
        for (const auto& m : messages()) {
            auto p = m.find(": PP");
            out.push_back(p == std::string::npos ? m : m.substr(p + 2));
        }
        return out;
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

// Resets the process-wide Issue state (default-off codes enabled by
// Issue::enable(), -Werror, ...) when a test scope ends, even on an early
// return from a failed ASSERT. Output goes back to std::cerr.
struct IssueStateReset {
    IssueStateReset() = default;
    IssueStateReset(const IssueStateReset&) = delete;
    IssueStateReset& operator=(const IssueStateReset&) = delete;
    ~IssueStateReset() { Issue::initialize(std::cerr); }
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
