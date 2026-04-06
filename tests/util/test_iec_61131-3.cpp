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
    EXPECT_EQ("'a$nb$n'", Util::encode_iec_string("a\nb\n"));
}

TEST_F(UtilTest, decode_iec_string) {
    EXPECT_EQ("", Util::decode_iec_string("''"));
    EXPECT_EQ("a", Util::decode_iec_string("'a'"));
    EXPECT_EQ("abc", Util::decode_iec_string("'abc'"));
    EXPECT_EQ("abc", Util::decode_iec_string("\"abc\""));
    EXPECT_EQ("😁☹️🌚", Util::decode_iec_string("'😁☹️🌚'"));
    EXPECT_EQ("a$b", Util::decode_iec_string("'a$$b'"));
    EXPECT_EQ("\'\"", Util::decode_iec_string("'$27$22'"));
    EXPECT_EQ("a\nb\n", Util::decode_iec_string("'a$nb$n'"));
}
