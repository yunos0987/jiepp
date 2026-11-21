#include "test_helper.hpp"
#include <chrono>
#include <ctime>
#include <regex>

class MacroTest : public JieppTest {};

TEST_F(MacroTest, Counter) {
    // Case-sensitive: only uppercase __COUNTER__ expands; counter resets per Env
    EXPECT_EQ("0;1;__counter__", pp("__COUNTER__;__COUNTER__;__counter__"));
    // Between two pragmas: expanded
    EXPECT_EQ("(*{my_pragma}*)0;(*{my_pragma}*)", pp("{my_pragma}__COUNTER__;{my_pragma}"));
    // Inside string literal: not expanded
    EXPECT_EQ("'__COUNTER__';", pp("'__COUNTER__';"));
    // Inside (* *) comment: not expanded
    EXPECT_EQ("(* __COUNTER__ *)", pp("(* __COUNTER__ *)"));
    // Inside /* */ comment: not expanded
    EXPECT_EQ("/* __COUNTER__ */", pp("/* __COUNTER__ */"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroTest, FileRegular) {
    Env env = setup();
    env.push_file("a.iec");
    EXPECT_EQ("'<unknown location>';'<unknown location>';__file__;", pp("__FILE__;__FILE__;__file__;", env));
    env.pop_file();

    Issue::push({1, "b.iec"});
    EXPECT_EQ("'b.iec';'b.iec';__file__;", pp("__FILE__;__FILE__;__file__;"));
    Issue::pop();

    EXPECT_TRUE(empty());
}

TEST_F(MacroTest, FileEscape) {
    Issue::push({1, "'$'"});
    EXPECT_EQ(";'$27$$$27';", pp(";__FILE__;"));
    Issue::pop();

    EXPECT_TRUE(empty());
}

TEST_F(MacroTest, Date) {
    auto result = pp("__DATE__");
    std::regex pattern(
        R"('(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) ([012][0-9]|30|31) (2[0-9]{3})')");
    EXPECT_TRUE(std::regex_match(result, pattern)) << "unexpected date format: " << result;
    EXPECT_TRUE(empty());
}

TEST_F(MacroTest, Time) {
    auto result = pp("__TIME__");
    std::regex pattern(R"('(0[0-9]|1[0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9])')");
    EXPECT_TRUE(std::regex_match(result, pattern)) << "unexpected time format: " << result;
    EXPECT_TRUE(empty());
}

namespace {
// The local time `year-mon-mday hour:min:sec` as a time_t.
std::time_t local_time_t(int year, int mon, int mday, int hour, int min, int sec) {
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    t.tm_isdst = -1;
    return std::mktime(&t);
}
} // namespace

// __DATE__ and __TIME__ come from the Env's start time, not from the clock at
// each expansion: one run has one value, and the day is padded with a space.
TEST_F(MacroTest, DateAndTimeComeFromTheStartTime) {
    Env env = setup();
    env.set_start_time(local_time_t(2020, 3, 9, 1, 2, 3));
    EXPECT_EQ("'Mar  9 2020' '01:02:03'", pp("__DATE__ __TIME__", env));
    EXPECT_EQ("'Mar  9 2020' '01:02:03'", pp("__DATE__ __TIME__", env));

    env.set_start_time(local_time_t(2021, 12, 25, 23, 59, 58));
    EXPECT_EQ("'Dec 25 2021' '23:59:58'", pp("__DATE__ __TIME__", env));
    EXPECT_TRUE(empty());
}

// The clock is read once, when the Env is made: an Env whose start time is in
// the past keeps giving that time however often it is preprocessed, never the
// current time (SPECIFICATION 12.2). A clock read at each expansion would give
// today's date here.
TEST_F(MacroTest, TimeIsTheSameThroughoutARun) {
    Env env = setup();
    env.set_start_time(local_time_t(2001, 2, 3, 4, 5, 6));
    const std::string expected = "'Feb  3 2001' '04:05:06'\n";
    std::string input;
    for (int i = 0; i < 2000; ++i)
        input += "__DATE__ __TIME__\n";
    for (int round = 0; round < 2; ++round) {
        const std::string out = pp(input, env);
        ASSERT_EQ(expected.size() * 2000, out.size()) << round;
        for (std::size_t at = 0; at < out.size(); at += expected.size())
            ASSERT_EQ(expected, out.substr(at, expected.size())) << round << ' ' << at;
    }
    // The start time is not the clock's time now.
    EXPECT_NE(env.start_time(), std::time(nullptr));
}

#ifndef JIEPP_SANDBOX
// __TIMESTAMP__ is the file's modification time with the day padded with a
// space ("Mon Mar  9 01:02:03 2020"), like gcc/clang.
TEST_F(MacroTest, TimeStampPadsTheDay) {
    const fs::path file = fs::temp_directory_path() / "jiepp_timestamp_test.iec";
    write_file(file, "x\n");
    const std::pair<std::time_t, const char*> cases[] = {
        {local_time_t(2020, 3, 9, 1, 2, 3), "'Mon Mar  9 01:02:03 2020'"},
        {local_time_t(2020, 3, 19, 10, 20, 30), "'Thu Mar 19 10:20:30 2020'"},
    };
    for (const auto& [t, expected] : cases) {
        // Half a second past t, so that the "now"-based clock conversion of
        // the fallback below and in TimeStampMacro (no std::chrono::clock_cast
        // in libstdc++ before 13) cannot truncate to the second before.
        const auto sys_tp =
            std::chrono::system_clock::from_time_t(t) + std::chrono::milliseconds(500);
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        const auto file_tp = std::chrono::clock_cast<fs::file_time_type::clock>(sys_tp);
#else
        const auto file_tp = fs::file_time_type::clock::now() +
                             std::chrono::duration_cast<fs::file_time_type::duration>(
                                 sys_tp - std::chrono::system_clock::now());
#endif
        std::error_code ec;
        fs::last_write_time(file, file_tp, ec);
        ASSERT_FALSE(ec) << ec.message();
        Env env = setup();
        env.push_file(file.generic_string());
        EXPECT_EQ(expected, pp("__TIMESTAMP__", env));
        env.pop_file();
    }
    std::error_code ec;
    fs::remove(file, ec);
    EXPECT_TRUE(empty());
}
#endif

TEST_F(MacroTest, RedefineIdenticalModuloWhitespaceAmount) {
    // C17 6.10.3p2: redefinitions are identical if "all white-space
    // separations are considered identical" -- presence/absence, not amount.
    EXPECT_EQ(";;", pp("{#define X 1 + 2};{#define X 1  +  2};"));
    EXPECT_TRUE(empty());

    // Differing presence/absence of a separation must still be flagged.
    EXPECT_EQ(";;", pp("{#define Y 1+2};{#define Y 1 + 2};"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());

    // Function-macro bodies: same rule applies.
    EXPECT_EQ(";;", pp("{#define F(a,b) a + b};{#define F(a,b) a  +  b};"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";;", pp("{#define G(a,b) a+b};{#define G(a,b) a + b};"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
}

TEST_F(MacroTest, Line) {
    const std::string input =
        "__LINE__;\n"
        "__LINE__;\n"
        "(*\n"
        "\n"
        "*)\n"
        "__LINE__;\n"
        "//\n"
        "__LINE__;\n"
        "__line__;\n";
    const std::string expected =
        "1;\n"
        "2;\n"
        "(*\n"
        "\n"
        "*)\n"
        "6;\n"
        "//\n"
        "8;\n"
        "__line__;\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_TRUE(empty());
}
