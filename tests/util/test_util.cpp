#include "test_helper.hpp"

#include "util/text.hpp"

class UtilTest : public ::testing::Test {};

TEST_F(UtilTest, trim_view) {
    EXPECT_EQ("", Util::trim_view(""));
    EXPECT_EQ("", Util::trim_view("   \t\n"));
    EXPECT_EQ("abc", Util::trim_view("abc"));
    EXPECT_EQ("abc", Util::trim_view("   abc  "));
    EXPECT_EQ("a b  c", Util::trim_view("\na b  c\n"));
}

TEST_F(UtilTest, ltrim_view) {
    EXPECT_EQ("", Util::ltrim_view(""));
    EXPECT_EQ("", Util::ltrim_view("   \t\n"));
    EXPECT_EQ("abc", Util::ltrim_view("abc"));
    EXPECT_EQ("abc  ", Util::ltrim_view("   abc  "));
    EXPECT_EQ("a b  c\n", Util::ltrim_view("\na b  c\n"));
}

TEST_F(UtilTest, rtrim_view) {
    EXPECT_EQ("", Util::rtrim_view(""));
    EXPECT_EQ("", Util::rtrim_view("   \t\n"));
    EXPECT_EQ("abc", Util::rtrim_view("abc"));
    EXPECT_EQ("   abc", Util::rtrim_view("   abc  "));
    EXPECT_EQ("\na b  c", Util::rtrim_view("\na b  c\n"));
}

TEST_F(UtilTest, utf8_head_tail) {
    // ASCII is cut at exactly max_bytes.
    EXPECT_EQ("abc", Util::utf8_head("abcdef", 3));
    EXPECT_EQ("def", Util::utf8_tail("abcdef", 3));
    EXPECT_EQ("abc", Util::utf8_head("abc", 3));
    EXPECT_EQ("abc", Util::utf8_tail("abc", 5));
    // U+3042 is 3 bytes (E3 81 82); a cut inside it moves to the boundary.
    const std::string a = "\xE3\x81\x82";
    const std::string s = a + a + a;
    EXPECT_EQ(a + a, Util::utf8_head(s, 8));
    EXPECT_EQ(a + a, Util::utf8_head(s, 7));
    EXPECT_EQ(a + a, Util::utf8_head(s, 6));
    EXPECT_EQ(a, Util::utf8_head(s, 5));
    EXPECT_EQ("", Util::utf8_head(s, 2));
    EXPECT_EQ(a + a, Util::utf8_tail(s, 8));
    EXPECT_EQ(a + a, Util::utf8_tail(s, 7));
    EXPECT_EQ(a + a, Util::utf8_tail(s, 6));
    EXPECT_EQ(a, Util::utf8_tail(s, 5));
    EXPECT_EQ("", Util::utf8_tail(s, 2));
}
