#include "constfold_test_helper.hpp"

// ---- Arithmetic operators ----

TEST_F(ConstfoldTest, ExprPos) {
    EXPECT_THROW(eval_const_expr("+false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("+true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_NE(0LL, eval_const_expr("+0=0"));
    EXPECT_EQ(0LL, eval_const_expr("+0=1"));
    EXPECT_EQ(0LL, eval_const_expr("+1=0"));
    EXPECT_NE(0LL, eval_const_expr("+1=1"));
    EXPECT_NE(0LL, eval_const_expr("+0.0=0.0"));
    EXPECT_EQ(0LL, eval_const_expr("+0.0=1.0"));
    EXPECT_EQ(0LL, eval_const_expr("+1.0=0.0"));
    EXPECT_NE(0LL, eval_const_expr("+1.0=1.0"));
    EXPECT_EQ(0LL, eval_const_expr("+a"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprNeg) {
    EXPECT_THROW(eval_const_expr("-false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("-true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_NE(0LL, eval_const_expr("-0=0"));
    EXPECT_EQ(0LL, eval_const_expr("-0=-1"));
    EXPECT_EQ(0LL, eval_const_expr("-1=0"));
    EXPECT_NE(0LL, eval_const_expr("-1=-1"));
    EXPECT_NE(0LL, eval_const_expr("-0.0=0.0"));
    EXPECT_EQ(0LL, eval_const_expr("-0.0=-1.0"));
    EXPECT_EQ(0LL, eval_const_expr("-1.0=0.0"));
    EXPECT_NE(0LL, eval_const_expr("-1.0=-1.0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprAdd) {
    EXPECT_NE(0LL, eval_const_expr("5+3=8"));
    EXPECT_NE(0LL, eval_const_expr("-5+3=-2"));
    EXPECT_EQ(0LL, eval_const_expr("-5+3=2"));
    EXPECT_THROW(eval_const_expr("byte#16#0+0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprSub) {
    EXPECT_NE(0LL, eval_const_expr("5-3=2"));
    EXPECT_NE(0LL, eval_const_expr("-5-3=-8"));
    EXPECT_EQ(0LL, eval_const_expr("-5-3=8"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprMul) {
    EXPECT_NE(0LL, eval_const_expr("5*2=10"));
    EXPECT_NE(0LL, eval_const_expr("5*3=15"));
    EXPECT_NE(0LL, eval_const_expr("-5*3=-15"));
    EXPECT_EQ(0LL, eval_const_expr("-5*3=15"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprDiv) {
    EXPECT_NE(0LL, eval_const_expr("10/2=5"));
    EXPECT_NE(0LL, eval_const_expr("7/3=2"));
    EXPECT_NE(0LL, eval_const_expr("-15/3=-5"));
    EXPECT_NE(0LL, eval_const_expr("1/1=1"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprMod) {
    EXPECT_NE(0LL, eval_const_expr("10 mod 3=1"));
    EXPECT_NE(0LL, eval_const_expr("7 mod 2=1"));
    EXPECT_NE(0LL, eval_const_expr("8 mod 4=0"));
    EXPECT_TRUE(empty());
}

// ---- A3: wraparound arithmetic (2's complement, no crash / UB) ----

TEST_F(ConstfoldTest, WraparoundAddOverflowsToIntMin) {
    // INT64_MAX + 1 wraps to INT64_MIN. INT64_MIN itself cannot be spelled
    // as a literal (its magnitude exceeds INT64_MAX), so it is produced via
    // "-9223372036854775807 - 1" (safe: no operand overflows) on both sides.
    EXPECT_NE(0LL, eval_const_expr(
        "9223372036854775807 + 1 = (-9223372036854775807 - 1)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, WraparoundSubOverflowsToIntMax) {
    EXPECT_NE(0LL, eval_const_expr(
        "(-9223372036854775807 - 1) - 1 = 9223372036854775807"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, WraparoundMulIntMinTimesNeg1) {
    EXPECT_NE(0LL, eval_const_expr(
        "(-9223372036854775807 - 1) * -1 = (-9223372036854775807 - 1)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, WraparoundUnaryNegIntMin) {
    EXPECT_NE(0LL, eval_const_expr(
        "-(-9223372036854775807 - 1) = (-9223372036854775807 - 1)"));
    EXPECT_TRUE(empty());
}

// ---- A3 / N1: INT64_MIN / -1 and INT64_MIN mod -1 must not crash ----

TEST_F(ConstfoldTest, IntMinDivNeg1WrapsInsteadOfTrapping) {
    // Naive `INT64_MIN / -1` overflows the quotient and traps in hardware
    // (observed as an abnormal exit, rc=127, before this fix). Two's
    // complement wraparound defines the result as INT64_MIN itself.
    EXPECT_NE(0LL, eval_const_expr(
        "(-9223372036854775807 - 1) / -1 = (-9223372036854775807 - 1)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, IntMinModNeg1IsZero) {
    // Same hardware trap as division; the remainder is mathematically 0.
    EXPECT_NE(0LL, eval_const_expr("(-9223372036854775807 - 1) mod -1 = 0"));
    EXPECT_TRUE(empty());
}

// ---- A4: type_error() must not crash eval_const_expr() when EXPR_TYPE_ERROR
// (PP50) is suppressed via {#ignore} ----

TEST_F(ConstfoldTest, TypeErrorIgnoredReturnsFalseInsteadOfCrashing) {
    Issue::add_ignoring(Issue::Code::EXPR_TYPE_ERROR);
    EXPECT_EQ(0LL, eval_const_expr("1.0+1"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, TypeErrorNotIgnoredStillThrows) {
    // Without {#ignore}, EXPR_TYPE_ERROR must still throw Issue::Exception
    // as before (Issue::happen() itself throws; type_error()'s own throw is
    // reached only when the diagnostic was suppressed).
    EXPECT_THROW(eval_const_expr("1.0+1"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, DigitSeparatorsInDecimalAndFloat) {
    // I2: IEC 61131-3 digit separators are valid in plain decimal/float
    // literals too, not only based ones -- constfold.l previously only
    // stripped '_' for the N#... rule, so "1_000" was a syntax error.
    EXPECT_NE(0LL, eval_const_expr("1_000=1000"));
    EXPECT_NE(0LL, eval_const_expr("1_2_3=123"));
    EXPECT_NE(0LL, eval_const_expr("1_000.5=1000.5"));
    EXPECT_NE(0LL, eval_const_expr("1_0.0e1=100.0"));
    // Based literals must still work unaffected by this change.
    EXPECT_NE(0LL, eval_const_expr("16#1_00=256"));
    EXPECT_TRUE(empty());
}

