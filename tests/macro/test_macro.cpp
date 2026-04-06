#include "test_helper.hpp"
#include <regex>

class MacroTest : public JieppTest {};

TEST_F(MacroTest, Counter) {
    // Case-sensitive: only uppercase __COUNTER__ expands; counter resets per Env
    EXPECT_EQ("0;1;__counter__", pp("__COUNTER__;__COUNTER__;__counter__"));
    // Inside {st}...{end} block: expanded
    EXPECT_EQ("(*{st}*)0;(*{end}*)", pp("{st}__COUNTER__;{end}"));
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
