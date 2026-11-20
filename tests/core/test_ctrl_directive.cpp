#include "test_helper.hpp"
#include <algorithm>
#include <vector>

// ---- #if ----

// NOTE: boolean literals are case-sensitive in this implementation.
// Only lowercase "true"/"false" are recognized.
class CtrlDirectiveTest : public JieppTest {};

TEST_F(CtrlDirectiveTest, IfBoolean) {
    EXPECT_EQ("e",  strip(pp("{#if false}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#endif}e")));
    // Mixed-case "falsE"/"truE" are treated as undefined identifiers → false
    EXPECT_EQ("e",  strip(pp("{#if falsE}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if truE}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, IfAmpAmpAndBarBar) {
    EXPECT_EQ("1e", strip(pp("{#define A 1}{#if defined(A) && A = 1}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if defined(B) && B = 1}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if defined(B) || 1}1{#endif}e")));
    // A macro may supply the operator.
    EXPECT_EQ("1e", strip(pp("{#define BOTH(x, y) x && y}{#if BOTH(1, 1)}1{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if 0}1{#elif 0 || 1}2{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, SkippedGroupDoesNotReportDirectiveOperands) {
    // An invalid escape (PP21) in a directive matters only where it is reached.
    EXPECT_EQ("e", strip(pp("{#if 0}{#define A $q}{#if $q}{#endif}{#unknown $q}{#endif}e")));
    EXPECT_EQ("e", strip(pp("{#if 0}{#if 1}{#else $q}{#endif $q}{#elif 0}{#endif}e")));
    EXPECT_TRUE(empty());
    // The same operand is reported once its enclosing group is active.
    EXPECT_THROW(pp("{#if 0}{#else $q}{#endif}"), Issue::Exception);
    EXPECT_THROW(pp("{#if 1}{#else}{#endif $q}"), Issue::Exception);
}

TEST_F(CtrlDirectiveTest, IfInteger) {
    EXPECT_EQ("1e", strip(pp("{#if -1}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if -0}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if 0}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if +0}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if +1}1{#endif}e")));
    EXPECT_TRUE(empty());
}

// NOTE: Float/string/date/time literals in #if do not raise INVALID_EXPRESSION.
// Float values are evaluated numerically (non-zero is truthy).
TEST_F(CtrlDirectiveTest, IfFloatTruthy) {
    EXPECT_EQ("1e", strip(pp("{#if -1.0}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if 0.0}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, IfStringError) {
    // C1: the context is the (re-escaped) expression text, like every other
    // eval_const_expr() diagnostic -- not bison's own "syntax error" text,
    // which cf::CfParser::error() no longer raises itself.
    EXPECT_THROW(pp("{#if 'xyz'}1{#endif}e"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP51: Missing expression; ''xyz''", message());

    EXPECT_THROW(pp("{#if ''}1{#endif}e"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP51: Missing expression; ''''", message());
}

TEST_F(CtrlDirectiveTest, IfIdentifier) {
    // undefined identifier → false
    EXPECT_EQ("e",  strip(pp("{#if a}1{#endif}e")));
    // defined identifier
    EXPECT_EQ("1e", strip(pp("{#define a true}{#if a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a false}{#if a}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, IfEmptyIsError) {
    EXPECT_THROW(pp("{#if}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#if  }1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#define a}{#if a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, IfSyntaxError) {
    // {#if;} has non-empty raw condition but becomes invalid syntax after parsing.
    EXPECT_THROW(pp("{#if;}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

// The message of a closed {#error}/{#warning} is quoted whole, however long
// (clang and gcc do not cut an #error message); only a directive with no
// closing '}' is cut (the test below).
TEST_F(CtrlDirectiveTest, LongMessageOfClosedDirectiveIsNotCut) {
    Issue::ContinueMode guard({});
    const std::string a(2000, 'a');
    {
        SCOPED_TRACE("no $n");
        pp("{#error " + a + "}");
        const auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_NE(std::string::npos, msgs[0].find(a)) << msgs[0].size();
    }
    {
        SCOPED_TRACE("with $n");
        pp("{#warning " + std::string(1000, 'a') + "$n" + std::string(1000, 'b') + "}");
        const auto msgs = messages();
        ASSERT_EQ(2u, msgs.size());
        EXPECT_NE(std::string::npos, msgs[0].find(std::string(1000, 'a'))) << msgs[0].size();
        EXPECT_NE(std::string::npos, msgs[1].find(std::string(1000, 'b'))) << msgs[1].size();
    }
}

// A directive with no closing '}' still runs to the end of the input (as a C
// directive without a newline does), but no diagnostic quotes the rest of the
// file: each one stays on one short line, however long the input is.
TEST_F(CtrlDirectiveTest, UnclosedDirectiveDiagnosticsAreShort) {
    std::string body;
    for (int i = 0; i < 3000; ++i)
        body += "x" + std::to_string(i) + " := " + std::to_string(i) + ";\n";
    const char* const heads[] = {"{#if 1\n", "{#error abc\n", "{#define G(a\n", "{#include 'zz\n"};
    for (const char* head : heads) {
        SCOPED_TRACE(head);
        Issue::ContinueMode guard({});
        pp(std::string(head) + body);
        const auto msgs = messages();
        ASSERT_GE(msgs.size(), 2u);
        EXPECT_EQ(Issue::Code::INVALID_PP_SYNTAX, PlainTextMessage::parse_code(msgs[0])) << msgs[0];
        for (const auto& m : msgs)
            EXPECT_LT(m.size(), 1000u) << m.substr(0, 200);
    }
}

// C1: a malformed {#if}/{#elif} expression must raise exactly one
// diagnostic, not the bison grammar's own "syntax error" (from
// cf::CfParser::error(), constfold.y) followed by eval_const_expr()'s own
// message carrying the actual expression text (constfold.cpp). Use
// ContinueMode so a lingering double report would show as a second message
// instead of being masked by the first exception thrown.
//
// INVALID_EXPRESSION (PP52) when a complete expression was already reduced
// before the extra tokens ("a complete expression was followed by more");
// MISSING_EXPRESSION (PP51) otherwise.
TEST_F(CtrlDirectiveTest, IfMalformedExpressionReportsOnce) {
    Issue::ContinueMode guard({});

    {
        SCOPED_TRACE("{#if 1 x}");
        pp("{#if 1 x}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("1 x")) << msgs[0];
    }
    {
        // "(1" reduces the inner parenthesized expression to a complete
        // expr before failing on the missing ")", so cf_expr_complete is
        // already true -> INVALID_EXPRESSION, like the other "complete
        // expression followed by more" cases above.
        SCOPED_TRACE("{#if (1}");
        pp("{#if (1}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("(1")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#if 1 2}");
        pp("{#if 1 2}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("1 2")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#if defined X Y}");
        pp("{#if defined X Y}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
    }
    {
        SCOPED_TRACE("{#if 1 +}");
        pp("{#if 1 +}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("1 +")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#if )}");
        pp("{#if )}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
    }
    {
        SCOPED_TRACE("{#if and}");
        pp("{#if and}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
    }
    {
        SCOPED_TRACE("{#if 0}{#elif 1+}");
        pp("{#if 0}{#elif 1+}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("1+")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#if 'xyz'}");
        pp("{#if 'xyz'}1{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
    }
}

// ---- #else ----

TEST_F(CtrlDirectiveTest, IfElse) {
    EXPECT_EQ("2e", strip(pp("{#if false}1{#else}2{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#else}2{#endif}e")));
    EXPECT_TRUE(empty());
}

// ---- #elif ----

TEST_F(CtrlDirectiveTest, Elif1) {
    EXPECT_EQ("3e", strip(pp("{#if false}1{#elif false}2{#else}3{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if false}1{#elif true}2{#else}3{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#elif false}2{#else}3{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#elif true}2{#else}3{#endif}e")));
    EXPECT_EQ("3e", strip(pp("{#if false}1{#elif a}2{#else}3{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#define a true}{#if false}1{#elif a}2{#else}3{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, ElifEmptyError) {
    EXPECT_THROW(pp("{#if false}1{#elif}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#if false}1{#elif }2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#define a}{#if false}1{#elif a}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, Elif2) {
    EXPECT_EQ("4e", strip(pp("{#if 0}1{#elif 0}2{#elif 0}3{#else}4{#endif}e")));
    EXPECT_EQ("3e", strip(pp("{#if 0}1{#elif 0}2{#elif 1}3{#else}4{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if 0}1{#elif 1}2{#elif 0}3{#else}4{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if 1}1{#elif 0}2{#elif 0}3{#else}4{#endif}e")));
    EXPECT_TRUE(empty());
}

// ---- #elifdef / #elifndef (C23/C++23) ----

TEST_F(CtrlDirectiveTest, Elifdef) {
    EXPECT_EQ("3e", strip(pp("{#if 0}1{#elifdef A}2{#else}3{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#define A}{#if 0}1{#elifdef A}2{#else}3{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if 0}1{#elifndef A}2{#else}3{#endif}e")));
    EXPECT_EQ("3e", strip(pp("{#define A}{#if 0}1{#elifndef A}2{#else}3{#endif}e")));
    // NAME is not macro-expanded, like {#ifdef}.
    EXPECT_EQ("2e", strip(pp("{#define A B}{#define B}{#undef B}{#if 0}1{#elifdef A}2{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, ElifdefNotEvaluatedAfterTakenBranch) {
    // A true branch was taken: {#elifdef} is not evaluated.
    EXPECT_EQ("1e", strip(pp("{#if 1}1{#elifdef A}2{#endif}e")));
    // A taken {#elifdef} closes the chain for a later {#elif}.
    EXPECT_EQ("2e", strip(pp("{#define A}{#if 0}1{#elifdef A}2{#elif 1}3{#endif}e")));
    // Mixed with {#elif} in one chain.
    EXPECT_EQ("3e", strip(pp("{#if 0}1{#elif 0}2{#elifndef A}3{#else}4{#endif}e")));
    // Inside an inactive group it is skipped like {#elif}.
    EXPECT_EQ("k", strip(pp("{#if 0}{#if 1}{#elifdef A}{#else}{#endif}{#endif}k")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, ElifdefOperandErrors) {
    Issue::ContinueMode guard({});
    // Extra tokens: PP49 naming the directive, only NAME is tested.
    EXPECT_EQ("e", strip(pp("{#if 0}1{#elifdef A B}2{#endif}e")));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find("'{#elifdef A B}'")) << msgs[0];
    // Missing / non-identifier NAME: PP40 and the branch is false.
    EXPECT_EQ("3e", strip(pp("{#if 0}1{#elifdef}2{#else}3{#endif}e")));
    EXPECT_EQ(std::vector<Issue::Code>{Issue::Code::INVALID_DEFINED_OPERAND}, codes());
    EXPECT_EQ("3e", strip(pp("{#if 0}1{#elifndef 1}2{#else}3{#endif}e")));
    EXPECT_EQ(std::vector<Issue::Code>{Issue::Code::INVALID_DEFINED_OPERAND}, codes());
}

TEST_F(CtrlDirectiveTest, ElifdefPlacementErrors) {
    EXPECT_THROW(pp("{#if 0}{#else}{#elifdef A}{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ELIF_ERROR, code());
    EXPECT_THROW(pp("{#elifdef A}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ELIF_ERROR, code());
    EXPECT_THROW(pp("{#elifndef A}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ELIF_ERROR, code());
    // A similar but unknown name is still PP45.
    EXPECT_THROW(pp("{#elifndefx A}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, code());
}

TEST_F(CtrlDirectiveTest, ElifdefOperandScannedForUnterminatedQuote) {
    Issue::ContinueMode guard({});
    // Not evaluated (an earlier branch was taken), but the enclosing group
    // is active: the operand is still scanned, like {#elif}.
    EXPECT_EQ("1", strip(pp("{#if 1}1{#elifdef A 'x}2{#endif}")));
    EXPECT_EQ(std::vector<Issue::Code>{Issue::Code::UNTERMINATED_STRING_LITERAL}, codes());
    // Enclosing group inactive: not scanned.
    EXPECT_EQ("", strip(pp("{#if 0}{#if 1}{#elifdef A 'x}{#endif}{#endif}")));
    EXPECT_TRUE(codes().empty());
    // Evaluated: PP29 from the scan, then PP49 for the extra token.
    EXPECT_EQ("2", strip(pp("{#define A}{#if 0}{#elifdef A 'x}2{#endif}")));
    EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::UNTERMINATED_STRING_LITERAL,
                                        Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}),
              codes());
}

// ---- #ifdef / #ifndef ----

TEST_F(CtrlDirectiveTest, Ifdef) {
    EXPECT_EQ("1e", strip(pp("{#define a}{#ifdef a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#ifdef x}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#ifdef A}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a false}{#ifdef a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a 0}{#ifdef a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#undef a}{#ifdef a}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, Ifndef) {
    EXPECT_EQ("e",  strip(pp("{#define a}{#ifndef a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#ifndef A}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#ifndef x}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a false}{#ifndef a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#undef a}{#ifndef a}1{#endif}e")));

    EXPECT_TRUE(empty());
}

// ---- #if with macro replacement ----

TEST_F(CtrlDirectiveTest, IfWithReplace) {
    EXPECT_THROW(pp("{#define a}{#if a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    EXPECT_EQ("e",  strip(pp("{#define a false}{#if a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a true}{#if a}1{#endif}e")));

    EXPECT_TRUE(empty());
}

// ---- #if expression operators (macro-based cases only; others in test_constfold.cpp) ----

TEST_F(CtrlDirectiveTest, ExprPos) {
    EXPECT_THROW(pp("{#define a}{#if +a}1{#else}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, ExprNeg) {
    EXPECT_THROW(pp("{#define a}{#if -a}1{#else}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, ExprNot) {
    EXPECT_EQ("1e", strip(pp("{#define a false}{#if not a}1{#else}2{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#define a true}{#if not a}1{#else}2{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, ExprExactErrorCases) {
    static const std::vector<std::pair<const char*, Issue::Code>> cases = {
        {"{#define a}{#if +a}1{#else}2{#endif}e",       Issue::Code::MISSING_EXPRESSION},
        {"{#define a}{#if -a}1{#else}2{#endif}e",       Issue::Code::MISSING_EXPRESSION},
        {"{#define a}{#if not a}1{#else}2{#endif}e",    Issue::Code::MISSING_EXPRESSION},
    };
    for (const auto& [i, c] : cases) {
        EXPECT_THROW(pp(i), Issue::Exception) << i;
        EXPECT_EQ(c, code()) << i;
    }
}

// ---- H: __LINT_MIN__ in {#if}; the 2^63 magnitude via macro expansion
// (constfold's token stream comes from the already-expanded text, so the
// grammar's "CF_MINUS CF_INT_MIN_MAG" rule applies the same way whether the
// '-' and the literal were typed directly or produced by macro expansion) ----

TEST_F(CtrlDirectiveTest, LintMinUsableInIf) {
    EXPECT_EQ("1e", strip(pp("{#if __LINT_MIN__ < 0}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if __LINT_MIN__ = -9223372036854775807 - 1}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, LintMinPlainTextOutputUnchanged) {
    // builtin_macros.def is untouched: outside {#if}, __LINT_MIN__ is still
    // plain object-macro replacement, unrelated to constfold.
    EXPECT_EQ("x := -9223372036854775808;", pp("x := __LINT_MIN__;"));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, MacroDefinedNegativeMinMagUsableInIf) {
    // The macro's replacement text "-9223372036854775808" is expanded into
    // the {#if} condition before constfold sees it, so this is the same
    // token sequence as a literal "-9223372036854775808" in the source.
    EXPECT_EQ("1e", strip(pp("{#define M -9223372036854775808}{#if M < 0}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, MacroDefinedMinMagAfterUnaryMinusUsableInIf) {
    EXPECT_EQ("1e", strip(pp("{#define N 9223372036854775808}{#if -N < 0}1{#endif}e")));
    EXPECT_TRUE(empty());
}

// A7: a NUL in the expression (decoded from $00 in a directive operand, or a
// raw byte) used to cut the expression at that point (the scanner read it as
// a C string), so `1$00garbage(((` was evaluated as `1`. It is now PP52 and
// the whole expression is false. The diagnostic shows the NUL as $00.
TEST_F(CtrlDirectiveTest, ConditionalNulExpression) {
    Issue::ContinueMode guard({});
    auto check = [&](const std::string& src, const char* expected_out) {
        EXPECT_EQ(expected_out, strip(pp(src))) << src;
        auto cs = codes();
        ASSERT_EQ(1u, cs.size()) << src;
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, cs[0]) << src;
    };
    check("{#if 1$00garbage(((}T{#else}F{#endif}", "F");
    check("{#if 0$00 or 1}T{#else}F{#endif}", "F");
    check("{#if $00}T{#else}F{#endif}", "F");
    // Raw NUL bytes (a $00 in plain text is not decoded, so the escape form
    // above does not cover them).
    check(std::string("{#if 1\0garbage(((}T{#else}F{#endif}", 35), "F");
}

TEST_F(CtrlDirectiveTest, ConditionalNulExpressionMessageShowsEscape) {
    Issue::ContinueMode guard({});
    pp(std::string("{#if 1\0x}T{#endif}", 18));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_NE(std::string::npos, msgs[0].find("1$00x")) << msgs[0];
    EXPECT_EQ(std::string::npos, msgs[0].find('\0'));
}

// SPEC 6.4: division by zero in an unevaluated operand is not reported.
TEST_F(CtrlDirectiveTest, IfDeadDivisionSilent) {
    EXPECT_EQ("n", strip(pp("{#if FALSE \\and\\ 1/0 = 1}y{#else}n{#endif}")));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", strip(pp("{#if TRUE \\or\\ 1 mod 0 = 1}y{#else}n{#endif}")));
    EXPECT_TRUE(empty());
}

// SPEC 6.3: a division or modulo by zero in an evaluated operand makes the
// whole condition false, like clang (gcc only zeroes that operation), not
// just that operation. Inputs measured with clang and gcc (`#if 1/0 + 5` is
// false in clang, true in gcc; `#if 1/0 == 0` is false in both).
TEST_F(CtrlDirectiveTest, IfDivisionByZeroMakesConditionFalse) {
    Issue::ContinueMode guard({});
    const std::string div0 = "<unknown location>:1.0: error: PP52: Invalid expression; 'division by zero'";
    const std::string mod0 = "<unknown location>:1.0: error: PP52: Invalid expression; 'modulo by zero'";
    EXPECT_EQ("n", strip(pp("{#if 1/0 + 5}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({div0}), messages());
    EXPECT_EQ("n", strip(pp("{#if 1/0 = 0}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({div0}), messages());
    EXPECT_EQ("n", strip(pp("{#if 1 mod 0 + 5}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({mod0}), messages());
    EXPECT_EQ("n", strip(pp("{#if (2/0) * 0 + 1}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({div0}), messages());
    // In an {#elif}: the group is not taken, a later {#else} is.
    EXPECT_EQ("n", strip(pp("{#if 0}a{#elif 1/0 + 5}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({div0}), messages());
    // Suppressed, still false.
    EXPECT_EQ("n", strip(pp("{#ignore PP52}{#if 1/0 + 5}y{#else}n{#endif}")));
    EXPECT_TRUE(empty());
}

// clang: one PP24 for each group still open at the end of the file,
// innermost first, each at the line of the {#if} that opened it -- also
// after an {#elif}/{#else} of the group, and for a group opened in a
// skipped group (`#if 1 / x / #if 0 / y` gives two errors, at lines 3 and
// 1; gcc gives the same two).
TEST_F(CtrlDirectiveTest, UnclosedGroupsAreReportedAtTheirIf) {
    Issue::ContinueMode guard({});
    auto pp24 = [](int line) {
        return "<unknown location>:" + std::to_string(line) +
               ".0: error: PP24: Unterminated conditional directive; 'unclosed if'";
    };
    pp("{#if 1}\nx\n{#if 0}\ny\n");
    EXPECT_EQ(std::vector<std::string>({pp24(3), pp24(1)}), messages());
    pp("a\n{#if 1}\nx\n");
    EXPECT_EQ(std::vector<std::string>({pp24(2)}), messages());
    pp("{#if 0}\nq\n{#else}\nz\n");
    EXPECT_EQ(std::vector<std::string>({pp24(1)}), messages());
    pp("{#if 0}\n{#elif 1}\nq\n{#elif 0}\nr\n");
    EXPECT_EQ(std::vector<std::string>({pp24(1)}), messages());
    pp("{#ifdef A}\n{#ifndef B}\n{#if 1}\nx\n{#endif}\n");
    EXPECT_EQ(std::vector<std::string>({pp24(2), pp24(1)}), messages());
    pp("{#if 0}\n{#if 1}\n{#else}\n");
    EXPECT_EQ(std::vector<std::string>({pp24(2), pp24(1)}), messages());
    pp("{#if 1}\n{#else}\n{#if 0}\n{#elif 1}\n{#else}\n");
    EXPECT_EQ(std::vector<std::string>({pp24(3), pp24(1)}), messages());
}

// The PP24 of a group opened before a {#line N 'file'} is at the file name
// the {#if} was read under (clang and gcc: `#line 100` / `#if 1` /
// `#line 200 "bar.c"` / `#if 1` gives bar.c:200 and main.c:100).
TEST_F(CtrlDirectiveTest, UnclosedGroupKeepsItsFileName) {
    Issue::ContinueMode guard({});
    pp("{#line 100}\n{#if 1}\n{#line 200 'bar.iec'}\n{#if 1}\nx\n");
    EXPECT_EQ(std::vector<std::string>(
                  {"bar.iec:200.0: error: PP24: Unterminated conditional directive; 'unclosed if'",
                   "<unknown location>:100.0: error: PP24: Unterminated conditional directive; "
                   "'unclosed if'"}),
              messages());
}

// After a division by zero nothing that depends on values is reported in
// the same expression (clang: one error for `1/0 + 1/0` and `1/0 || 1/0`;
// gcc reports each division).
TEST_F(CtrlDirectiveTest, OneDivisionByZeroPerExpression) {
    Issue::ContinueMode guard({});
    const std::string div0 = "<unknown location>:1.0: error: PP52: Invalid expression; 'division by zero'";
    EXPECT_EQ("n", strip(pp("{#if 1/0 + 1/0}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({div0}), messages());
    EXPECT_EQ("n", strip(pp("{#if 1/0 \\or\\ 1/0}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>({div0}), messages());
    EXPECT_EQ("n", strip(pp("{#if 1 mod 0 + 2/0}y{#else}n{#endif}")));
    EXPECT_EQ(std::vector<std::string>(
                  {"<unknown location>:1.0: error: PP52: Invalid expression; 'modulo by zero'"}),
              messages());
}
