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
            const char* restored = std::setlocale(LC_ALL, saved.c_str());
            ASSERT_NE(nullptr, restored);
            const char* now = std::setlocale(LC_ALL, nullptr);
            EXPECT_EQ(saved, now ? now : "");
            // Back to the state computed at the top of the test.
            EXPECT_EQ(expected, Util::narrow_paths_are_utf8());
        } else {
            std::setlocale(LC_ALL, saved.c_str());
        }
    }
#else
    EXPECT_TRUE(Util::narrow_paths_are_utf8());
#endif
}

// A7: escape_line_breaks shows a NUL as $00 too (the diagnostic text itself is
// escaped by Issue::happen's escape_control_chars, tested below).
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

// The path a file was reached by: name under dir, joined as gcc/clang join
// them -- nothing is normalised, an absolute name stands alone.
TEST_F(UtilTest, reached_path) {
    EXPECT_EQ("a.iec", Util::reached_path("", "a.iec"));
    EXPECT_EQ("./a.iec", Util::reached_path("", "./a.iec"));
    EXPECT_EQ("tests/in/a.iec", Util::reached_path("tests/in", "a.iec"));
    EXPECT_EQ("tests/in/./a.iec", Util::reached_path("tests/in", "./a.iec"));
    EXPECT_EQ("././x.iec", Util::reached_path(".", "./x.iec"));
    EXPECT_EQ("./lib/./t.iec", Util::reached_path("./lib", "./t.iec"));
    EXPECT_EQ("sub/inc/../inc/b.iec", Util::reached_path("sub/inc", "../inc/b.iec"));
    EXPECT_EQ("../a.iec", Util::reached_path("", "../a.iec"));
    EXPECT_EQ("tests/in/a.iec", Util::reached_path("tests/in/", "a.iec"));
    EXPECT_EQ("/abs/a.iec", Util::reached_path("tests/in", "/abs/a.iec"));
    EXPECT_EQ("/a.iec", Util::reached_path("/", "a.iec"));
#ifdef _WIN32
    EXPECT_EQ("C:/abs/a.iec", Util::reached_path("tests/in", "C:/abs/a.iec"));
    EXPECT_EQ("C:/s.iec", Util::reached_path("C:/", "s.iec"));
    EXPECT_EQ("tests/in/x/a.iec", Util::reached_path("tests\\in", "x\\a.iec"));
    // A bare drive is not joined with '/': the file stays drive-relative.
    EXPECT_EQ("C:a.iec", Util::reached_path("C:", "a.iec"));
    EXPECT_EQ("C:", Util::reached_dir("C:main.iec"));
    EXPECT_EQ("C:a.iec", Util::reached_path(Util::reached_dir("C:main.iec"), "a.iec"));
#else
    // Elsewhere "C:" is an ordinary directory name.
    EXPECT_EQ("C:/a.iec", Util::reached_path("C:", "a.iec"));
    EXPECT_EQ("", Util::reached_dir("C:main.iec"));
    // Elsewhere a backslash is an ordinary file-name character.
    EXPECT_EQ("tests/in/x\\a.iec", Util::reached_path("tests/in", "x\\a.iec"));
#endif
}

// A run of '/' stays as written on every platform: with libstdc++
// std::filesystem::path joins it into one, so the text is handled as a string.
TEST_F(UtilTest, reached_path_keeps_runs_of_slashes) {
    EXPECT_EQ("x//a.h", Util::reached_path("", "x//a.h"));
    EXPECT_EQ("lib/x//a.h", Util::reached_path("lib", "x//a.h"));
    EXPECT_EQ("lib//a.h", Util::reached_path("lib//", "a.h"));
    EXPECT_EQ("lib/a.h", Util::reached_path("lib/", "a.h"));
    EXPECT_EQ("lib/.//b.h", Util::reached_path("lib", ".//b.h"));
    EXPECT_EQ("lib//.//b.h", Util::reached_path("lib//", ".//b.h"));
    EXPECT_EQ("a//b/../a.h", Util::reached_path("a//b", "../a.h"));
    EXPECT_EQ("//abs//a.h", Util::reached_path("lib", "//abs//a.h"));
}

TEST_F(UtilTest, reached_dir_keeps_runs_of_slashes) {
    EXPECT_EQ("lib//", Util::reached_dir("lib//a.h"));
    EXPECT_EQ("a//b", Util::reached_dir("a//b/c.h"));
    EXPECT_EQ("/", Util::reached_dir("/a.h"));
    EXPECT_EQ("//", Util::reached_dir("//a.h"));
    // Joined back, the file next to it is named the way gcc/clang name it.
    EXPECT_EQ("lib//c.h", Util::reached_path(Util::reached_dir("lib//a.h"), "c.h"));
    EXPECT_EQ("lib/c.h", Util::reached_path(Util::reached_dir("lib/a.h"), "c.h"));
}

TEST_F(UtilTest, reached_dir) {
    EXPECT_EQ("", Util::reached_dir(""));
    EXPECT_EQ("", Util::reached_dir("main.iec"));
    EXPECT_EQ("", Util::reached_dir("<stdin>"));
    EXPECT_EQ(".", Util::reached_dir("./main.iec"));
    EXPECT_EQ("tests/in", Util::reached_dir("tests/in/main.iec"));
    EXPECT_EQ("sub/inc/../inc", Util::reached_dir("sub/inc/../inc/b.iec"));
}

// __DATE__, __TIME__ and __TIMESTAMP__: gcc/clang write the day padded with a
// space to two characters ("Mar  9 2020"), and the names do not follow the
// locale.
namespace {
std::tm make_tm(int year, int mon, int mday, int hour, int min, int sec, int wday) {
    std::tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = mday;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    t.tm_wday = wday;
    return t;
}
} // namespace

TEST_F(UtilTest, format_date) {
    EXPECT_EQ("Mar  9 2020", Util::format_date(make_tm(2020, 3, 9, 1, 2, 3, 1)));
    EXPECT_EQ("Mar 19 2020", Util::format_date(make_tm(2020, 3, 19, 1, 2, 3, 4)));
    EXPECT_EQ("Jan  1 1999", Util::format_date(make_tm(1999, 1, 1, 0, 0, 0, 5)));
    EXPECT_EQ("Dec 31 2026", Util::format_date(make_tm(2026, 12, 31, 23, 59, 59, 4)));
    // Always 11 characters.
    EXPECT_EQ(11u, Util::format_date(make_tm(2020, 2, 3, 0, 0, 0, 1)).size());
}

TEST_F(UtilTest, format_time) {
    EXPECT_EQ("01:02:03", Util::format_time(make_tm(2020, 3, 9, 1, 2, 3, 1)));
    EXPECT_EQ("00:00:00", Util::format_time(make_tm(2020, 3, 9, 0, 0, 0, 1)));
    EXPECT_EQ("23:59:59", Util::format_time(make_tm(2020, 3, 9, 23, 59, 59, 1)));
}

TEST_F(UtilTest, format_timestamp) {
    EXPECT_EQ("Mon Mar  9 01:02:03 2020", Util::format_timestamp(make_tm(2020, 3, 9, 1, 2, 3, 1)));
    EXPECT_EQ("Thu Mar 19 10:20:30 2020", Util::format_timestamp(make_tm(2020, 3, 19, 10, 20, 30, 4)));
    EXPECT_EQ("Sun Jan  1 00:00:00 2023", Util::format_timestamp(make_tm(2023, 1, 1, 0, 0, 0, 0)));
}

// What a diagnostic quotes of a construct that runs to the end of the input.
TEST_F(UtilTest, diag_excerpt) {
    EXPECT_EQ("", Util::diag_excerpt(""));
    EXPECT_EQ("(* x", Util::diag_excerpt("(* x"));
    // Only line breaks after the first line: nothing was left out.
    EXPECT_EQ("(* x", Util::diag_excerpt("(* x\n"));
    EXPECT_EQ("(* x", Util::diag_excerpt("(* x\r\n"));
    // Later lines are left out.
    EXPECT_EQ("(* x...", Util::diag_excerpt("(* x\ny\n"));
    EXPECT_EQ("{...", Util::diag_excerpt("{\n    "));
    // A long first line is cut at the limit.
    EXPECT_EQ(std::string(64, 'a') + "...", Util::diag_excerpt(std::string(100, 'a')));
    EXPECT_EQ(std::string(64, 'a'), Util::diag_excerpt(std::string(64, 'a')));
    EXPECT_EQ("abc...", Util::diag_excerpt("abcdef", 3));
    // Never in the middle of a UTF-8 character.
    EXPECT_EQ("a\xE3\x81\x82...", Util::diag_excerpt("a\xE3\x81\x82\xE3\x81\x84", 5));
    EXPECT_EQ("a...", Util::diag_excerpt("a\xE3\x81\x82\xE3\x81\x84", 3));
}

// A prerequisite of a dependency rule: the reached-by path without its
// leading "./" runs (gcc/clang); everything after them is kept.
TEST_F(UtilTest, dependency_path) {
    EXPECT_EQ("x.iec", Util::dependency_path("x.iec"));
    EXPECT_EQ("x.iec", Util::dependency_path("./x.iec"));
    EXPECT_EQ("x.iec", Util::dependency_path("././x.iec"));
    EXPECT_EQ("x.iec", Util::dependency_path(".//x.iec"));
    EXPECT_EQ("lib/./t.iec", Util::dependency_path("./lib/./t.iec"));
    EXPECT_EQ("../x.iec", Util::dependency_path("../x.iec"));
    EXPECT_EQ("../x.iec", Util::dependency_path("./../x.iec"));
    EXPECT_EQ(".x.iec", Util::dependency_path(".x.iec"));
    EXPECT_EQ("/abs/./x.iec", Util::dependency_path("/abs/./x.iec"));
    EXPECT_EQ("", Util::dependency_path(""));
    EXPECT_EQ(".", Util::dependency_path("."));
    EXPECT_EQ("", Util::dependency_path("./"));
    EXPECT_EQ("..", Util::dependency_path(".."));
}
