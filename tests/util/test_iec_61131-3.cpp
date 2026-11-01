#include "test_helper.hpp"

#include "util/iec_61131-3.hpp"

class UtilTest : public ::testing::Test {};

TEST_F(UtilTest, encode_iec_string) {
    EXPECT_EQ("''", Util::encode_iec_string(""));
    EXPECT_EQ("'a'", Util::encode_iec_string("a"));
    EXPECT_EQ("'abc'", Util::encode_iec_string("abc"));
    EXPECT_EQ("\"abc\"", Util::encode_iec_string("abc", '\"'));
    EXPECT_EQ("'😁☹️🌚'", Util::encode_iec_string("😁☹️🌚"));
    EXPECT_EQ("'a$$b'", Util::encode_iec_string("a$b"));
    EXPECT_EQ("'$27$22'", Util::encode_iec_string("\'\""));
    EXPECT_EQ(R"("$0027$0022ab")", Util::encode_iec_string(R"('"ab)", '"'));
    EXPECT_EQ("'a$nb$n'", Util::encode_iec_string("a\nb\n"));
}

TEST_F(UtilTest, encode_line_marker_path) {
    EXPECT_EQ("''", Util::encode_line_marker_path(""));
    EXPECT_EQ("'a.iec'", Util::encode_line_marker_path("a.iec"));
    EXPECT_EQ("'dir/a b.iec'", Util::encode_line_marker_path("dir/a b.iec"));
    EXPECT_EQ("'a$24b'", Util::encode_line_marker_path("a$b"));
    EXPECT_EQ("'a$7Bb$7Dc'", Util::encode_line_marker_path("a{b}c"));
    EXPECT_EQ("'$27$22'", Util::encode_line_marker_path("\'\""));
    EXPECT_EQ("'a$nb$rc$td$p'", Util::encode_line_marker_path("a\nb\rc\td\f"));
    EXPECT_EQ("'a$24b$7Bc$7Dd$27e.iec'", Util::encode_line_marker_path("a$b{c}d'e.iec"));
    EXPECT_EQ("'😁'", Util::encode_line_marker_path("😁"));
}
