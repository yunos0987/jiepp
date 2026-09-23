#include "lineno_test_helper.hpp"

// ---- Basic directive, defined operator, object macro, all-token-kinds: line number tracking ----
namespace {
void normalize_diags(std::vector<std::string>& diags) {
    for (auto& diag : diags) {
        while (!diag.empty() && diag.back() == ' ') {
            diag.pop_back();
        }
    }
}
} // namespace

TEST_F(LinenoTest, Directive) {
    const auto test_case = TestCase{
        "directive-with-plain-newline-equivalence",
        R"(
{#define F(x) $
c$
+$
x$
}
{#info}__LINE__;
F(a
+
b);
{#info}__LINE__)",
        R"(





7;
c+a + b

;
11)",
        {
            "<unknown location>:7.0: info: PP93: ''",
            "<unknown location>:11.0: info: PP93: ''",
        },
        true,
    };
    SCOPED_TRACE(test_case.name);
    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}

TEST_F(LinenoTest, DefinedOperator) {
    const auto test_case = TestCase{
        "defined-operator",
        "{#if $\n$\ndefined$\n$\n($\n$\na$\n$\n)$\n$\n}{#endif};{#info}__LINE__",
        // 10 leading blank lines exceed the default 7-line compaction
        // threshold; N = cur_before_run(0) + nl(10) - 1 = 9.
        "(*{#:9}*)\n;11",
        {"<unknown location>:11.0: info: PP93: ''"},
        true,
    };
    SCOPED_TRACE(test_case.name);
    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}


TEST_F(LinenoTest, Omacro) {
    const auto test_case = TestCase{
        "object-macro",
        "{#define A $\n$\na$\n$\n+$\n$\nb$\n$\n};A;{#info}__LINE__",
        // 8 leading blank lines exceed the default 7-line compaction
        // threshold; N = cur_before_run(0) + nl(8) - 1 = 7.
        "(*{#:7}*)\n;a+b;9",
        {"<unknown location>:9.0: info: PP93: ''"},
        true,
    };

    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}


TEST_F(LinenoTest, DollarNewlineInStringLiteral) {
    // B13: a $-escaped newline inside a string literal must count toward the
    // running line number so that diagnostics after the literal report the
    // correct physical line (4), not the line before the embedded newline (3).
    const std::string input =
        "{#define X 1}\n"
        "s1 := 'line1 $\n"
        "line2';\n"
        "{#define X 2}\n";
    const std::string expected_output = "\ns1 := 'line1 $\nline2';\n\n";

    const std::string output = pp(input);
    EXPECT_EQ(expected_output, output);
    auto actual_diags = messages();
    const std::vector<std::string> expected_diags = {
        "<unknown location>:4.0: warning: PP35: Macro redefined; 'X'",
    };
    EXPECT_EQ(expected_diags, actual_diags);
}

TEST_F(LinenoTest, LinenoAfterStringLiteral) {
    // F1 regression: push_string_token() used to leave a zombie WS/newline
    // token behind when the string literal was not the first token in the
    // stream, double-counting the newline that precedes it and inflating
    // every subsequent __LINE__. "A;\n'x';\n" is 2 physical lines, so
    // __LINE__ on line 3 must read 3 (pre-fix it read 4).
    const std::string input = "A;\n'x';\nX __LINE__;\n";
    const std::string output = pp(input);
    EXPECT_EQ("A;\n'x';\nX 3;\n", output);
    EXPECT_TRUE(empty());
}

TEST_F(LinenoTest, LinenoAfterStringLiteralWithBlankLine) {
    // Drift scales with the duplicated token's own num_of_lines, not a
    // flat +1: a blank line before the string literal means the newline
    // token being duplicated carries num_of_lines = 2, so the pre-fix
    // drift was +2 (line 6 reported instead of 4).
    const std::string input = "A;\n\n'a';\nX __LINE__;\n";
    const std::string output = pp(input);
    EXPECT_EQ("A;\n\n'a';\nX 4;\n", output);
    EXPECT_TRUE(empty());
}

TEST_F(LinenoTest, LinenoAfterMergedStringLiteralUnaffected) {
    // Adjacent string-literal merging ('a' 'b') was already safe pre-fix
    // (the merge branch's own resize(scan - 1) happened to also discard
    // the zombie slot); confirm it remains correct after the fix.
    const std::string input = "A;\n'a' 'b';\nX __LINE__;\n";
    const std::string output = pp(input);
    EXPECT_EQ("A;\n'ab' ;\nX 3;\n", output);
    EXPECT_TRUE(empty());
}

TEST_F(LinenoTest, LinenoCumulativeDriftAcrossMultipleStringLiterals) {
    // Cumulative-drift repro from the review report: each string literal
    // preceded by a real newline (and not itself merging into a prior
    // string) used to add +1 of drift; three literals must not compound.
    const std::string input = "L1 __LINE__;\n'a';\nL3 __LINE__;\n'b';\nL5 __LINE__;\n";
    const std::string output = pp(input);
    EXPECT_EQ("L1 1;\n'a';\nL3 3;\n'b';\nL5 5;\n", output);
    EXPECT_TRUE(empty());
}

TEST_F(LinenoTest, LinenoAfterStringLiteralMergedAcrossNewline) {
    // Adjacent string literals still merge into one token even when the
    // whitespace between them is an actual newline (not just a space, as in
    // LinenoAfterMergedStringLiteralUnaffected): push_string_token() pops
    // the trailing WS -- here, the intervening newline -- before merging
    // 'a' and 'b' into 'ab', then re-appends that same WS token after the
    // merged token, so its line-count contribution is neither lost nor
    // double-counted and __LINE__ on the following line still reads 4.
    const std::string input = "A;\n'a'\n'b';\nX __LINE__;\n";
    const std::string output = pp(input);
    EXPECT_EQ("A;\n'ab'\n;\nX 4;\n", output);
    EXPECT_TRUE(empty());
}

TEST_F(LinenoTest, DiagnosticLineAfterStringLiteralIsAccurate) {
    // F1 regression: diagnostics (not just __LINE__ substitution) must also
    // reflect the corrected running line number after a mid-stream string
    // literal; the redefinition of M on line 4 must be reported as line 4,
    // not inflated by the zombie-token drift.
    const std::string input = "A;\n'x';\n{#define M 1}\n{#define M 2}";
    pp(input);
    const std::vector<std::string> expected_diags = {
        "<unknown location>:4.0: warning: PP35: Macro redefined; 'M'",
    };
    EXPECT_EQ(expected_diags, messages());
}

TEST_F(LinenoTest, AllLineno) {
    const auto test_case = TestCase{
        "all-token-kinds",
        R"(
{#info}__LINE__;


{#info}__LINE__;
{#nop}
{#info}__LINE__;
(*{#nop}*)
{#info}__LINE__;
/*{#nop}*/
{#info}__LINE__;
//{#nop}
{#info}__LINE__;
{nop}
{#info}__LINE__;
(*{nop}*)
{#info}__LINE__;
/*{nop}*/
{#info}__LINE__;
//{nop}
{#info}__LINE__;
(*
 *
 *)
{#info}__LINE__;
(*!
 *
 *)
{#info}__LINE__;
/*
 *
 */
{#info}__LINE__;
/*!
 *
 */
{#info}__LINE__;
//
{#info}__LINE__;
//!
{#info}__LINE__;
{#nop $
$
}
{#info}__LINE__;
{nop $
$
}
{#info}__LINE__;
)",
        R"(
2;


5;

7;

9;

11;

13;
(*{nop}*)
15;
(*{nop}*)
17;
(*{nop}*)
19;
(*{nop}*)
21;
(*
 *
 *)
25;
(*!
 *
 *)
29;
/*
 *
 */
33;
/*!
 *
 */
37;
//
39;
//!
41;



45;
(*{nop }*)


49;
)",
        {
            "<unknown location>:2.0: info: PP93: ''",
            "<unknown location>:5.0: info: PP93: ''",
            "<unknown location>:7.0: info: PP93: ''",
            "<unknown location>:9.0: info: PP93: ''",
            "<unknown location>:11.0: info: PP93: ''",
            "<unknown location>:13.0: info: PP93: ''",
            "<unknown location>:15.0: info: PP93: ''",
            "<unknown location>:17.0: info: PP93: ''",
            "<unknown location>:19.0: info: PP93: ''",
            "<unknown location>:21.0: info: PP93: ''",
            "<unknown location>:25.0: info: PP93: ''",
            "<unknown location>:29.0: info: PP93: ''",
            "<unknown location>:33.0: info: PP93: ''",
            "<unknown location>:37.0: info: PP93: ''",
            "<unknown location>:39.0: info: PP93: ''",
            "<unknown location>:41.0: info: PP93: ''",
            "<unknown location>:45.0: info: PP93: ''",
            "<unknown location>:49.0: info: PP93: ''",
        },
        true,
    };

    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}

