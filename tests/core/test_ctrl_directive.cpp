#include "test_helper.hpp"
#include <algorithm>
#include <vector>

static std::string strip(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

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
