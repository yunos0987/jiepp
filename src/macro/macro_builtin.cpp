// macro_builtin.cpp — Built-in object macros.
// CounterMacro, LineMacro, FileMacro, DateMacro, TimeMacro,
// TimeStampMacro, IncludeLevelMacro, BaseFileMacro, FileNameMacro.

#include "macro.hpp"
#include "../env/env.hpp"
#include "../env/issue.hpp"
#include "../util/iec_61131-3.hpp"
#include "../core/preprocessor_internal.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#if defined(__cpp_lib_format)
#include <format>
#endif
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// CounterMacro
// ---------------------------------------------------------------------------

std::vector<Token> CounterMacro::replacement(Env& /*env*/) const {
    return {Token::create(Token::ANY, std::to_string(counter_++))};
}

bool CounterMacro::equal(const Macro& other) const {
    return dynamic_cast<const CounterMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// LineMacro
// ---------------------------------------------------------------------------

std::vector<Token> LineMacro::replacement(Env& env) const {
    return {Token::create(Token::ANY, std::to_string(Issue::lineno()))};
}

bool LineMacro::equal(const Macro& other) const {
    return dynamic_cast<const LineMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// FileMacro
// ---------------------------------------------------------------------------

std::vector<Token> FileMacro::replacement(Env& env) const {
    std::string fp = Issue::filepath();
    return {Token::create(Token::STRING, Util::encode_iec_string(fp))};
}

bool FileMacro::equal(const Macro& other) const {
    return dynamic_cast<const FileMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// DateMacro
// ---------------------------------------------------------------------------

std::vector<Token> DateMacro::replacement(Env& /*env*/) const {
    auto now  = std::chrono::system_clock::now();
    auto tt   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%b %d %Y", &tm_buf);
#if defined(__cpp_lib_format)
    return {Token::create(Token::STRING, std::format("'{}'", buf))};
#else
    return {Token::create(Token::STRING, std::string("'") + buf + "'")};
#endif
}

bool DateMacro::equal(const Macro& other) const {
    return dynamic_cast<const DateMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// TimeMacro
// ---------------------------------------------------------------------------

std::vector<Token> TimeMacro::replacement(Env& /*env*/) const {
    auto now  = std::chrono::system_clock::now();
    auto tt   = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buf);
#if defined(__cpp_lib_format)
    return {Token::create(Token::STRING, std::format("'{}'", buf))};
#else
    return {Token::create(Token::STRING, std::string("'") + buf + "'")};
#endif
}

bool TimeMacro::equal(const Macro& other) const {
    return dynamic_cast<const TimeMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// TimeStampMacro
// ---------------------------------------------------------------------------

std::vector<Token> TimeStampMacro::replacement(Env& env) const {
#ifdef JIEPP_SANDBOX
    // Sandbox: no filesystem access
    return {Token::create(Token::STRING, "''")};
#else
    namespace fs = std::filesystem;
    fs::path fp(env.current_file());
    std::error_code ec;
    auto lwt = fs::last_write_time(fp, ec);
    if (ec) {
        return {Token::create(Token::STRING, "''")};
    }
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(lwt);
#else
    // Portable fallback: synchronize file_clock and system_clock via "now"
    auto file_now = fs::file_time_type::clock::now();
    auto sys_now  = std::chrono::system_clock::now();
    auto sctp = sys_now + std::chrono::duration_cast<std::chrono::system_clock::duration>(lwt - file_now);
#endif
    auto tt   = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm_buf);
#if defined(__cpp_lib_format)
    return {Token::create(Token::STRING, std::format("'{}'", buf))};
#else
    return {Token::create(Token::STRING, std::string("'") + buf + "'")};
#endif
#endif  // !JIEPP_SANDBOX
}

bool TimeStampMacro::equal(const Macro& other) const {
    return dynamic_cast<const TimeStampMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// IncludeLevelMacro
// ---------------------------------------------------------------------------

std::vector<Token> IncludeLevelMacro::replacement(Env& env) const {
    return {Token::create(Token::ANY, std::to_string(env.include_level()))};
}

bool IncludeLevelMacro::equal(const Macro& other) const {
    return dynamic_cast<const IncludeLevelMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// BaseFileMacro
// ---------------------------------------------------------------------------

std::vector<Token> BaseFileMacro::replacement(Env& env) const {
#ifdef JIEPP_SANDBOX
    return {Token::create(Token::STRING, "''")};
#else
    std::string fp = Issue::base_filepath();
    return {Token::create(Token::STRING, Util::encode_iec_string(fp))};
#endif
}

bool BaseFileMacro::equal(const Macro& other) const {
    return dynamic_cast<const BaseFileMacro*>(&other) != nullptr;
}

// ---------------------------------------------------------------------------
// FileNameMacro
// ---------------------------------------------------------------------------

std::vector<Token> FileNameMacro::replacement(Env& env) const {
#ifdef JIEPP_SANDBOX
    return {Token::create(Token::STRING, "''")};
#else
    namespace fs = std::filesystem;
    std::string fp = Issue::filepath();
    std::string name = fs::path(fp).filename().generic_string();
    return {Token::create(Token::STRING, Util::encode_iec_string(name))};
#endif
}

bool FileNameMacro::equal(const Macro& other) const {
    return dynamic_cast<const FileNameMacro*>(&other) != nullptr;
}
