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
