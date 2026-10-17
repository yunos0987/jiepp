#include "test_helper.hpp"

#include "util/text.hpp"
#include "util/path.hpp"

#include <clocale>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <locale.h>  // ___lc_codepage_func()
#endif

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

// C1Pairs::Keep (the file path rule when narrow paths are not UTF-8, see
// Util::narrow_paths_are_utf8()): C0 controls and DEL still go, C2 80 - C2 9F stays.
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

// narrow_paths_are_utf8(): true when a narrow path string is read as UTF-8.
// Always true off Windows; on Windows it follows the process ANSI code page
// or the CRT locale code page, evaluated at every call.
TEST_F(UtilTest, narrow_paths_are_utf8_follows_process_codepage) {
#ifdef _WIN32
    const bool expected = GetACP() == CP_UTF8 || ___lc_codepage_func() == CP_UTF8;
    EXPECT_EQ(expected, Util::narrow_paths_are_utf8());
    if (GetACP() != CP_UTF8) {
        // The predicate is not cached: a UTF-8 CRT locale switches it on and
        // restoring the previous locale switches it off again.
        const char* prev = std::setlocale(LC_ALL, nullptr);
        const std::string saved = prev ? prev : "C";
        if (std::setlocale(LC_ALL, ".UTF8") != nullptr) {
            EXPECT_TRUE(Util::narrow_paths_are_utf8());
            std::setlocale(LC_ALL, saved.c_str());
            // The ACP is not UTF-8 here, so the result is the CRT locale alone.
            // If the saved locale was not UTF-8 the predicate must be false again.
            if (___lc_codepage_func() != CP_UTF8)
                EXPECT_FALSE(Util::narrow_paths_are_utf8());
            else
                EXPECT_TRUE(Util::narrow_paths_are_utf8());
        } else {
            std::setlocale(LC_ALL, saved.c_str());
        }
    }
#else
    EXPECT_TRUE(Util::narrow_paths_are_utf8());
#endif
}

// A7: a NUL is shown as $00 too, so a diagnostic of a normal build (P2-3) does
// not carry a raw 0x00 (checked for `{#error a$00b}`, which prints 'a$00b').
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

// P2-3: escape_control_chars() is what normal-build diagnostics use instead
// of strip_control_chars(): the same bytes are targeted but shown as $XX.
TEST_F(UtilTest, EscapeControlCharsC0AndDel) {
    using Util::escape_control_chars;
    using Util::C1Pairs;
    EXPECT_EQ("", escape_control_chars("", C1Pairs::Remove, false));
    EXPECT_EQ("abc", escape_control_chars("abc", C1Pairs::Remove, false));
    // Every C0 value except TAB, LF, CR becomes $XX (uppercase hex).
    for (int c = 0; c < 0x20; ++c) {
        if (c == '\t' || c == '\n' || c == '\r')
            continue;
        SCOPED_TRACE(c);
        char hex[8];
        std::snprintf(hex, sizeof hex, "$%02X", c);
        const std::string in = std::string("a") + static_cast<char>(c) + "b";
        EXPECT_EQ(std::string("a") + hex + "b", escape_control_chars(in, C1Pairs::Remove, false));
        EXPECT_EQ(std::string("a") + hex + "b", escape_control_chars(in, C1Pairs::Remove, true));
    }
    EXPECT_EQ("a$00b", escape_control_chars(std::string_view("a\0b", 3), C1Pairs::Remove, false));
    EXPECT_EQ("$1B[31mX", escape_control_chars("\x1B[31mX", C1Pairs::Remove, false));
    EXPECT_EQ("a$7Fb", escape_control_chars("a\x7F" "b", C1Pairs::Remove, false));
    // TAB is kept.
    EXPECT_EQ("a\tb", escape_control_chars("a\tb", C1Pairs::Remove, false));
}

TEST_F(UtilTest, EscapeControlCharsLineBreaks) {
    using Util::escape_control_chars;
    using Util::C1Pairs;
    EXPECT_EQ("a$nb$rc$r$nd", escape_control_chars("a\nb\rc\r\nd", C1Pairs::Remove, false));
    EXPECT_EQ("a\nb\rc\r\nd", escape_control_chars("a\nb\rc\r\nd", C1Pairs::Remove, true));
    // keep_line_breaks keeps LF/CR only; the other controls are still escaped.
    EXPECT_EQ("a$1Bb\nc", escape_control_chars("a\x1B" "b\nc", C1Pairs::Remove, true));
}

TEST_F(UtilTest, EscapeControlCharsC1Pairs) {
    using Util::escape_control_chars;
    using Util::C1Pairs;
    EXPECT_EQ("$C2$9B[31m", escape_control_chars("\xC2\x9B[31m", C1Pairs::Remove, false));
    EXPECT_EQ("x$C2$80y$C2$9Fz",
              escape_control_chars("x\xC2\x80" "y\xC2\x9F" "z", C1Pairs::Remove, false));
    // Other UTF-8 characters stay: U+00A0 (C2 A0), U+0100 (C4 80), U+3042.
    EXPECT_EQ("\xC2\xA0", escape_control_chars("\xC2\xA0", C1Pairs::Remove, false));
    EXPECT_EQ("\xC4\x80", escape_control_chars("\xC4\x80", C1Pairs::Remove, false));
    EXPECT_EQ("\xE3\x81\x82", escape_control_chars("\xE3\x81\x82", C1Pairs::Remove, false));
    // A leading C2 that is not itself part of a pair stays raw.
    EXPECT_EQ("\xC2$C2$9B", escape_control_chars("\xC2\xC2\x9B", C1Pairs::Remove, false));
    // A C0 control between C2 and 0x9B separates them: no pair is formed.
    EXPECT_EQ("\xC2$01\x9B", escape_control_chars("\xC2\x01\x9B", C1Pairs::Remove, false));
    // Invalid UTF-8: a lone 0x9B and a trailing C2 stay.
    EXPECT_EQ("a\x9B" "b", escape_control_chars("a\x9B" "b", C1Pairs::Remove, false));
    EXPECT_EQ("a\xC2", escape_control_chars("a\xC2", C1Pairs::Remove, false));
    // C1Pairs::Keep skips the pair rule (a path in an ANSI code page) but
    // still escapes C0 and DEL.
    EXPECT_EQ("\xC2\x82\xA0.iec", escape_control_chars("\xC2\x82\xA0.iec", C1Pairs::Keep, false));
    EXPECT_EQ("\xC2\x9B$01$7F", escape_control_chars("\xC2\x9B\x01\x7F", C1Pairs::Keep, false));
}

// Property: for every sequence of 1-3 bytes from a set that contains the
// bytes of every rule, the output has no C0 control (except TAB, and LF/CR
// when kept), no DEL and no C2 80 - C2 9F pair.
TEST_F(UtilTest, EscapeControlCharsPropertyNoForbiddenBytes) {
    using Util::escape_control_chars;
    using Util::C1Pairs;
    const std::string alphabet("a\x00\x09\x0A\x0D\x1B\x7F\xC2\x80\x9B\xA0", 11);
    auto check = [&](const std::string& in) {
        for (bool keep : {false, true}) {
            const std::string out = escape_control_chars(in, C1Pairs::Remove, keep);
            for (std::size_t i = 0; i < out.size(); ++i) {
                const auto c = static_cast<unsigned char>(out[i]);
                if (c < 0x20 && c != '\t' && !(keep && (c == '\n' || c == '\r')))
                    ADD_FAILURE() << "C0 byte in output for input of " << in.size() << " bytes";
                if (c == 0x7F)
                    ADD_FAILURE() << "DEL in output";
                if (c == 0xC2 && i + 1 < out.size()) {
                    const auto n = static_cast<unsigned char>(out[i + 1]);
                    if (n >= 0x80 && n <= 0x9F)
                        ADD_FAILURE() << "C1 pair in output";
                }
            }
        }
    };
    for (char a : alphabet) {
        check(std::string(1, a));
        for (char b : alphabet) {
            check(std::string{a, b});
            for (char c : alphabet)
                check(std::string{a, b, c});
        }
    }
}
