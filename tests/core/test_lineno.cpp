#include "lineno_test_helper.hpp"
#include "loader/lexer.hpp"

// ---- Basic directive, defined operator, object macro, all-token-kinds: line number tracking ----

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
        // U4: a raw newline inside a directive is now whitespace, so it is
        // no longer equivalent to a '$'-newline (which still joins with
        // nothing): plain-newline substitution here would insert spaces
        // between 'c', '+' and the parameter 'x' that the '$' form does
        // not produce ("c+a + b" vs "c + a + b"). This case genuinely
        // relies on the no-space '$' join, so plain-newline equivalence no
        // longer holds and is intentionally not verified.
        false,
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
        // threshold; N = cur_before_run(1) + nl(10) - 1 = 10 (the string
        // API starts counting at line 1, like the command line's leading
        // marker does).
        "(*{#:10}*)\n;11",
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
        // threshold; N = cur_before_run(1) + nl(8) - 1 = 8.
        "(*{#:8}*)\n;a+b;9",
        {"<unknown location>:9.0: info: PP93: ''"},
        // U4: raw newlines fold to whitespace, so a plain-newline rewrite
        // of the '$\n$\n' joins here would produce "a + b" instead of the
        // no-space "a+b" that '$' still produces; equivalence no longer
        // holds (see the comment on LinenoTest.Directive above).
        false,
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


// ---- U1: diagnostics of a re-lex (directive operand, pragma body, macro
// text) are reported at the current Issue location ----
//
// Before the fix lex_operand() re-lexed with the lexer's own line counter
// starting at 1, so PP20/PP22/PP23 found while re-lexing pointed at line 1
// (plus any decoded $n newlines) instead of the directive or pragma being
// processed. They now use the same line as the other diagnostics of that
// directive/pragma (PP29, PP51, PP52, ...): the opening '{' line, the macro
// name's line for an object macro, the closing ')' line of a function-macro
// call. The fixture name matches the sandbox test presets' filter
// ^(Sandbox|RobustnessTest), so these also run in sandbox builds (none of
// the inputs uses a sandbox-restricted directive).

namespace {

std::vector<std::string> relex_diags(JieppTest& t, const std::string& src) {
    Issue::ContinueMode guard({});
    (void)pp(src);
    return t.messages();
}

const std::string kLoc = "<unknown location>:";

} // namespace

class RobustnessTestRelexLineno : public LinenoTest {};

TEST_F(RobustnessTestRelexLineno, T1_NestedPragmaInSourcePragma) {
    EXPECT_EQ((std::vector<std::string>{
                  kLoc + "4.0: error: PP22: Invalid syntax for pragma; '{'",
                  kLoc + "4.0: error: PP22: Invalid syntax for pragma; '{ (* x '",
                  kLoc + "4.0: error: PP20: Unclosed comment; '(* x '",
              }),
              relex_diags(*this, "\n\n\n{ { (* x }"));
}

TEST_F(RobustnessTestRelexLineno, T2_MultiLinePragmaReportsOpeningLine) {
    EXPECT_EQ((std::vector<std::string>{kLoc + "4.0: error: PP20: Unclosed comment; '(* x '"}),
              relex_diags(*this, "x\n\n\n{ a\n (* x }\n"));
}

TEST_F(RobustnessTestRelexLineno, T3_DecodedNewlinesAreNotSourceLines) {
    EXPECT_EQ((std::vector<std::string>{kLoc + "5.0: error: PP20: Unclosed comment; '(* x'"}),
              relex_diags(*this, "\n\n\n\n{#if 1 $n$n (* x}\n{#endif}\n"));
}

TEST_F(RobustnessTestRelexLineno, T4_ObjectMacroPragmaAtMacroNameLine) {
    EXPECT_EQ((std::vector<std::string>{kLoc + "4.0: error: PP20: Unclosed comment; '(* x '"}),
              relex_diags(*this, "{#define P ${ (* x $}}\n\n\nP\n"));
}

TEST_F(RobustnessTestRelexLineno, T5_FunctionMacroPragmaAtClosingParenLine) {
    EXPECT_EQ((std::vector<std::string>{kLoc + "4.0: error: PP20: Unclosed comment; '(* y '"}),
              relex_diags(*this, "{#define F() ${ (* y $}}\nF(\n\n)\n"));
}

TEST_F(RobustnessTestRelexLineno, T6_PragmaInMacroArgumentAtClosingParenLine) {
    EXPECT_EQ((std::vector<std::string>{
                  kLoc + "5.0: error: PP20: Unclosed comment; '(* arg '",
                  kLoc + "5.0: error: PP20: Unclosed comment; '(* arg '",
              }),
              relex_diags(*this, "{#define F(x) x}\n\nF(\n{ (* arg }\n)\n"));
}

TEST_F(RobustnessTestRelexLineno, T7_LineDirectiveApplies) {
    EXPECT_EQ((std::vector<std::string>{kLoc + "100.0: error: PP20: Unclosed comment; '(* q '"}),
              relex_diags(*this, "a\n{#line 100}\n{ (* q }\n"));
}

#ifndef JIEPP_SANDBOX
// Not in the sandbox-safe fixture: a sandbox build does not apply the file
// name of {#line N 'file'}.
TEST_F(LinenoTest, RelexT8_LineDirectiveFileNameApplies) {
    EXPECT_EQ((std::vector<std::string>{"other.iec:200.0: error: PP20: Unclosed comment; '(* q '"}),
              relex_diags(*this, "a\n{#line 200 'other.iec'}\n{ (* q }\n"));
}
#endif

TEST_F(RobustnessTestRelexLineno, T9_T11_DirectiveOperands) {
    EXPECT_EQ((std::vector<std::string>{kLoc + "3.0: error: PP20: Unclosed comment; '(* z'"}),
              relex_diags(*this, "\n\n{#define Z (* z}\n"));
    EXPECT_EQ((std::vector<std::string>{kLoc + "3.0: error: PP20: Unclosed comment; '(* z'"}),
              relex_diags(*this, "\n\n{#ifdef X (* z}\n{#endif}\n"));
    EXPECT_EQ((std::vector<std::string>{kLoc + "4.0: error: PP20: Unclosed comment; '(* z'"}),
              relex_diags(*this, "\n\n{#if 0}\n{#else (* z}\n{#endif}\n"));
}

TEST_F(RobustnessTestRelexLineno, T12_NestedDirectiveInPragmas) {
    EXPECT_EQ((std::vector<std::string>{
                  kLoc + "3.0: error: PP22: Invalid syntax for pragma; '{'",
                  kLoc + "3.0: error: PP22: Invalid syntax for pragma; '{'",
                  kLoc + "3.0: error: PP22: Invalid syntax for pragma; '{'",
                  kLoc + "3.0: error: PP22: Invalid syntax for pragma; '{ {#define Q 1 '",
                  kLoc + "3.0: error: PP23: Invalid preprocessor syntax; '{#define Q 1 '",
              }),
              relex_diags(*this, "\n\n{ { {#define Q 1 }\n"));
}

// The relex lexer pushes no location of its own: the diagnostic goes to the
// current Issue location. iec3_tokens_from_string() still counts the text's
// own lines (source lexing is unchanged).
TEST_F(RobustnessTestRelexLineno, T14_RelexUsesCurrentLocation) {
    Issue::ContinueMode guard({});
    {
        Issue::LineGuard g(7, "f.iec");
        (void)iec3_relex("\n\n(* x", false);
    }
    EXPECT_EQ((std::vector<std::string>{"f.iec:7.0: error: PP20: Unclosed comment; '(* x'"}),
              messages());
    {
        Issue::LineGuard g(7, "f.iec");
        (void)iec3_tokens_from_string("\n\n(* x", false);
    }
    EXPECT_EQ((std::vector<std::string>{"f.iec:3.0: error: PP20: Unclosed comment; '(* x'"}),
              messages());
}

// PP20 inside a multi-line pragma now agrees with PP29 from the same kind of
// re-lex, which was already reported at the opening '{' line (SPEC: PP29).
TEST_F(RobustnessTestRelexLineno, T15_AgreesWithPP29) {
    EXPECT_EQ((std::vector<std::string>{
                  kLoc + "3.0: error: PP20: Unclosed comment; '(* x '",
                  kLoc + "6.0: warning: PP29: Missing terminating quote character; ''x'",
              }),
              relex_diags(*this, "\n\n{ a\n\n (* x }\n{attribute\n\n 'x}\n"));
    // Same shape as T6, with PP29: twice at the closing ')' line.
    EXPECT_EQ((std::vector<std::string>{
                  kLoc + "5.0: warning: PP29: Missing terminating quote character; ''arg '",
                  kLoc + "5.0: warning: PP29: Missing terminating quote character; ''arg '",
              }),
              relex_diags(*this, "{#define F(x) x}\n\nF(\n{ 'arg }\n)\n"));
}
