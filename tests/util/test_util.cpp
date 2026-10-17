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

// B-3: the sandbox diagnostic strip (SPEC 14) is called only from sandbox
// builds, but both functions are compiled in every build; test them directly
// here so a normal build exercises them too. Literals are split ("\xC2\x9B"
// "abc") so a following hex digit is not read into the escape.
TEST_F(UtilTest, strip_control_chars) {
    using Util::strip_control_chars;
    EXPECT_EQ("", strip_control_chars(""));
    EXPECT_EQ("abc", strip_control_chars("abc"));
    // C0 controls go, except tab, LF and CR.
    EXPECT_EQ("ab\tc\nd\re", strip_control_chars(std::string("a\x01" "b\tc\nd\re\x1F", 11)));
    EXPECT_EQ("ab", strip_control_chars(std::string("a\0b", 3)));
    EXPECT_EQ("[31mX", strip_control_chars("\x1B[31mX"));
    // DEL goes.
    EXPECT_EQ("ab", strip_control_chars("a\x7F" "b"));
    // UTF-8 C1 controls (C2 80 - C2 9F) go as a pair.
    EXPECT_EQ("xyz", strip_control_chars("x\xC2\x80" "y\xC2\x9F" "z"));
    EXPECT_EQ("[31m", strip_control_chars("\xC2\x9B" "[31m"));
    // A pair re-formed by removing a byte in between does not survive.
    EXPECT_EQ("", strip_control_chars("\xC2\x01\x9B"));
    EXPECT_EQ("", strip_control_chars("\xC2\x7F\x9B"));
    EXPECT_EQ("", strip_control_chars("\xC2\xC2\x80\x9B"));
    EXPECT_EQ("ab", strip_control_chars("a\xC2\x01\x9B" "b"));
    // Other UTF-8 characters stay: U+00A0 (C2 A0), U+0100 (C4 80), U+3042.
    EXPECT_EQ("\xC2\xA0", strip_control_chars("\xC2\xA0"));
    EXPECT_EQ("\xC4\x80", strip_control_chars("\xC4\x80"));
    EXPECT_EQ("\xE3\x81\x82", strip_control_chars("\xE3\x81\x82"));
    // Invalid UTF-8 is passed through: a lone 0x9B and a trailing C2 stay.
    EXPECT_EQ("a\x9B" "b", strip_control_chars("a\x9B" "b"));
    EXPECT_EQ("a\xC2", strip_control_chars("a\xC2"));
}

// C1Pairs::Keep (the file path rule on Windows, where the path is in the
// ANSI code page): C0 controls and DEL still go, C2 80 - C2 9F stays.
TEST_F(UtilTest, strip_control_chars_keep_c1_pairs) {
    using Util::strip_control_chars;
    using Util::C1Pairs;
    // CP932 U+FF82 U+3042 = C2 82 A0.
    EXPECT_EQ("\xC2\x82\xA0.iec", strip_control_chars("\xC2\x82\xA0.iec", C1Pairs::Keep));
    EXPECT_EQ("\xA0.iec", strip_control_chars("\xC2\x82\xA0.iec", C1Pairs::Remove));
    EXPECT_EQ("\xA0.iec", strip_control_chars("\xC2\x82\xA0.iec"));
    EXPECT_EQ("\xC2\x9B", strip_control_chars("\xC2\x9B", C1Pairs::Keep));
    EXPECT_EQ("\xC2\x82\xA0", strip_control_chars("\xC2\x01\x82\x7F\xA0", C1Pairs::Keep));
    EXPECT_EQ("a\tb\nc\rd", strip_control_chars("a\x1B\tb\nc\rd\x7F", C1Pairs::Keep));
}

// A7: a NUL is shown as $00 too, so a diagnostic never carries a raw 0x00.
TEST_F(UtilTest, EscapeLineBreaksAlsoEscapesNul) {
    using Util::escape_line_breaks;
    EXPECT_EQ("", escape_line_breaks(""));
    EXPECT_EQ("a$00b", escape_line_breaks(std::string_view("a\0b", 3)));
    EXPECT_EQ("$00", escape_line_breaks(std::string_view("\0", 1)));
    EXPECT_EQ("a$nb$rc$00", escape_line_breaks(std::string_view("a\nb\rc\0", 6)));
    EXPECT_EQ("plain text", escape_line_breaks("plain text"));
}

TEST_F(UtilTest, line_breaks_to_spaces) {
    using Util::line_breaks_to_spaces;
    EXPECT_EQ("", line_breaks_to_spaces(""));
    EXPECT_EQ("a b c", line_breaks_to_spaces("a\nb\rc"));
    // CR LF becomes two spaces.
    EXPECT_EQ("a  b", line_breaks_to_spaces("a\r\nb"));
    // Tab and other bytes are unchanged.
    EXPECT_EQ("a\tb\xC2\x9B", line_breaks_to_spaces("a\tb\xC2\x9B"));
    // As Issue::happen() combines them: strip first, then replace.
    EXPECT_EQ("a  b", line_breaks_to_spaces(Util::strip_control_chars("a\x01\r\n\x7F" "b")));
}
