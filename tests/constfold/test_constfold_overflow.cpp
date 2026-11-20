// test_constfold_overflow.cpp -- PP55 INTEGER_OVERFLOW (SPEC 6.3): an Int
// operation whose mathematical result is not an Int. Each C expression below
// was measured with clang 22.1 ("integer overflow in preprocessor
// expression", one warning per operation, the value wrapped) and gcc 13.3.
#include "constfold_test_helper.hpp"

namespace {

constexpr const char* kOverflow = "Integer overflow in preprocessor expression";

std::string w55(const std::string& expr) {
    return std::string("<unknown location>:1.0: warning: PP55: ") + kOverflow + "; '" + expr + "'";
}

} // namespace

// The WP5 table: the value is the wrapped one (the same truth value as
// clang), and an over-wide literal is PP52 as before, not PP55.
TEST_F(ConstfoldTest, OverflowWarnsAndKeepsTheWrappedValue) {
    EXPECT_EQ(0LL, eval_const_expr("9223372036854775807 + 1 > 0"));
    EXPECT_EQ(w55("9223372036854775807 + 1 > 0"), message());
    EXPECT_NE(0LL, eval_const_expr("4611686018427387904 * 4 = 0"));
    EXPECT_EQ(w55("4611686018427387904 * 4 = 0"), message());
    EXPECT_THROW(eval_const_expr("99999999999999999999 > 0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

// The bounds of Int: one past them is PP55, the bounds themselves are not.
TEST_F(ConstfoldTest, OverflowAtTheBoundsOfInt) {
    for (const char* expr : {"9223372036854775806 + 1 = 9223372036854775807",
                             "(-9223372036854775807 - 1) + 0 < 0",
                             "-9223372036854775807 - 1 < 0",
                             "-9223372036854775807 + -1 < 0",
                             "4611686018427387904 * -2 = (-9223372036854775807 - 1)",
                             "-4611686018427387904 * 2 = (-9223372036854775807 - 1)",
                             "(-9223372036854775807 - 1) / 1 < 0",
                             "-(-9223372036854775807) = 9223372036854775807",
                             "(-9223372036854775807 - 1) mod -1 = 0"}) {
        EXPECT_NE(0LL, eval_const_expr(expr)) << expr;
        EXPECT_TRUE(empty()) << expr;
    }
    for (const char* expr : {"9223372036854775807 + 1 < 0",
                             "(-9223372036854775807 - 1) - 1 > 0",
                             "-9223372036854775807 - 2 > 0",
                             "0 - 9223372036854775807 - 2 > 0",
                             "4611686018427387904 * 2 < 0",
                             "4611686018427387905 * -2 > 0",
                             "3037000500 * 3037000500 < 0",
                             "(-9223372036854775807 - 1) * -1 < 0",
                             "-(-9223372036854775807 - 1) < 0",
                             "(-9223372036854775807 - 1) / -1 < 0"}) {
        EXPECT_NE(0LL, eval_const_expr(expr)) << expr;
        EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code()) << expr;
    }
}

// One warning per operation that overflows (clang: 3 for this expression,
// the third being INT64_MIN + INT64_MIN), in evaluation order.
TEST_F(ConstfoldTest, OverflowOncePerOperation) {
    EXPECT_NE(0LL, eval_const_expr(
        "(9223372036854775807 + 1) + (9223372036854775807 + 1) = 0"));
    EXPECT_EQ((std::vector<Issue::Code>(3, Issue::Code::INTEGER_OVERFLOW)), codes());
    // MAX + 1 wraps to MIN; MIN + MAX and -1 + 1 fit: one warning.
    EXPECT_NE(0LL, eval_const_expr(
        "9223372036854775807 + 1 + 9223372036854775807 + 1 = 0"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
}

// Not in an operand that is not evaluated (SPEC 6.4), like clang.
TEST_F(ConstfoldTest, OverflowNotInUnevaluatedOperand) {
    EXPECT_EQ(0LL, eval_const_expr("0 && (9223372036854775807 + 1 > 0)"));
    EXPECT_NE(0LL, eval_const_expr("1 || (9223372036854775807 + 1 > 0)"));
    EXPECT_EQ(0LL, eval_const_expr("FALSE \\and\\ (1 << 64 = 0)"));
    EXPECT_TRUE(empty());
    EXPECT_NE(0LL, eval_const_expr("1 && (9223372036854775807 + 1 < 0)"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
    // Only && / || (\and\ / \or\) short-circuit: the unescaped AND evaluates
    // both sides (SPEC 6.4), so its right side reports PP55.
    EXPECT_EQ(0LL, eval_const_expr("FALSE AND (1 << 64 = 0)"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
    // A count out of range is PP55 whatever the left operand, 0 included
    // (clang; gcc does not warn for 0 << 64).
    EXPECT_NE(0LL, eval_const_expr("(0 << 64) = 0"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
}

// After a division by zero nothing that depends on values is reported
// (clang stops there); before it, PP55 is.
TEST_F(ConstfoldTest, OverflowAndDivisionByZero) {
    Issue::Blocking guard({});
    EXPECT_EQ(0LL, eval_const_expr("1 / 0 + (9223372036854775807 + 1) > 0"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_EQ(0LL, eval_const_expr("(9223372036854775807 + 1) + 1 / 0 > 0"));
    EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::INTEGER_OVERFLOW,
                                        Issue::Code::INVALID_EXPRESSION}),
              codes());
}

// Bit strings are not checked: a bit string shift drops the bits shifted
// out (IEC SHL/SHR), and a typed literal or a cast keeps the low bits of
// its value. (clang warns for a C unsigned '<<' that shifts out bits or by
// 64 or more; it does not for unsigned + - *.)
TEST_F(ConstfoldTest, NoOverflowForBitStrings) {
    for (const char* expr : {"BYTE#16#FF << 1 = BYTE#16#FE",
                             "LWORD#16#8000000000000000 << 1 = LWORD#0",
                             "LWORD#16#FFFFFFFFFFFFFFFF << 64 = LWORD#0",
                             "WORD#16#8000 << -1 = WORD#0",
                             "BYTE#300 = BYTE#44",
                             "LWORD#-1 = LWORD#16#FFFFFFFFFFFFFFFF",
                             "LWORD#-9223372036854775808 = LWORD#16#8000000000000000",
                             "not 0 = DWORD#16#FFFFFFFF"}) {
        EXPECT_NE(0LL, eval_const_expr(expr)) << expr;
        EXPECT_TRUE(empty()) << expr;
    }
}

// -Werror makes it an error; {#ignore} hides it. The value is the same.
TEST_F(ConstfoldTest, OverflowWerrorAndIgnore) {
    Issue::add_ignoring(Issue::Code::INTEGER_OVERFLOW);
    EXPECT_EQ(0LL, eval_const_expr("9223372036854775807 + 1 > 0"));
    EXPECT_TRUE(empty());
    Issue::ignorings_.clear();

    Issue::werror_ = true;
    EXPECT_THROW(eval_const_expr("9223372036854775807 + 1 > 0"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP55: " + std::string(kOverflow) +
                  "; '9223372036854775807 + 1 > 0'",
              message());
}

// The same evaluator behind {#if}/{#elif}, __assert and __counter_set.
TEST_F(ConstfoldTest, OverflowInEveryCondition) {
    EXPECT_EQ("no", pp("{#if 9223372036854775807 + 1 > 0}yes{#else}no{#endif}"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
    EXPECT_EQ("b", pp("{#if 0}a{#elif (1 << 63) < 0}b{#endif}"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
    // Not evaluated: a skipped group, an {#elif} after a taken branch.
    EXPECT_EQ("a", pp("{#if 1}a{#elif 9223372036854775807 + 1}b{#endif}"
                      "{#if 0}{#if 9223372036854775807 + 1}{#endif}{#endif}"));
    EXPECT_TRUE(empty());
    // __assert: the condition is true (INT64_MIN < 0); only PP55.
    EXPECT_EQ("x", pp("__assert(9223372036854775807 + 1 < 0, m)x"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
    // __counter_set: PP55 is not a failure, the wrapped value is set.
    EXPECT_EQ("0", pp("__counter_set(c, 9223372036854775807 + 1 + 9223372036854775807 + 1)"
                      "__counter_peek(c)"));
    EXPECT_EQ(Issue::Code::INTEGER_OVERFLOW, code());
}
