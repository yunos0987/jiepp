#include "test_helper.hpp"

class StringDirectiveTest : public JieppTest {};

TEST_F(StringDirectiveTest, Basic) {
    // Fact: {#string} / {##} stringize their argument and emit a quoted string
    EXPECT_EQ("'xyz'",       pp("{#string xyz}"));
    EXPECT_EQ("'$27xyz$27'", pp("{#string 'xyz'}"));
    EXPECT_EQ("' x$$y z '",  pp("{#string $ x$$y z }"));
    EXPECT_EQ("'xyz'",       pp("{#string: xyz}"));
    EXPECT_EQ("'$27xyz$27'", pp("{#string: 'xyz'}"));
    EXPECT_EQ("' x$$y z '",  pp("{#string:$ x$$y z }"));
    EXPECT_EQ("'xyz'",       pp("{## xyz}"));
    EXPECT_EQ("'$27xyz$27'", pp("{## 'xyz'}"));
    EXPECT_TRUE(empty());
}

TEST_F(StringDirectiveTest, Wstring) {
    // {#wstring} emits double-quoted (wide) strings
    EXPECT_EQ("\"xyz\"",       pp("{#wstring xyz}"));
    EXPECT_EQ("\"xyz\"",       pp("{#wstring: xyz}"));
    EXPECT_TRUE(empty());
}

TEST_F(StringDirectiveTest, WstringWithMacro) {
    EXPECT_EQ("\"hello\"", pp("{#define M hello}{#wstring M}"));
    EXPECT_TRUE(empty());
}

