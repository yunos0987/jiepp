#include "constfold_test_helper.hpp"

// ---- Shift operators (<< >>) ----

class ShiftTest : public ConstfoldTest {};

// basic left shift
TEST_F(ShiftTest, ShlInt) {
    EXPECT_NE(0LL, eval_const_expr("1 << 0 = 1"));
    EXPECT_NE(0LL, eval_const_expr("1 << 1 = 2"));
    EXPECT_NE(0LL, eval_const_expr("1 << 3 = 8"));
    EXPECT_NE(0LL, eval_const_expr("1 << 10 = 1024"));
    EXPECT_NE(0LL, eval_const_expr("0 << 5 = 0"));
    EXPECT_NE(0LL, eval_const_expr("5 << 2 = 20"));
    EXPECT_TRUE(empty());
}

// basic right shift
TEST_F(ShiftTest, ShrInt) {
    EXPECT_NE(0LL, eval_const_expr("1 >> 0 = 1"));
    EXPECT_NE(0LL, eval_const_expr("1 >> 1 = 0"));
    EXPECT_NE(0LL, eval_const_expr("16 >> 2 = 4"));
    EXPECT_NE(0LL, eval_const_expr("8 >> 3 = 1"));
    EXPECT_NE(0LL, eval_const_expr("0 >> 5 = 0"));
    EXPECT_NE(0LL, eval_const_expr("10 >> 2 = 2"));
    EXPECT_TRUE(empty());
}

// overflow / edge cases
TEST_F(ShiftTest, OverShift) {
    EXPECT_NE(0LL, eval_const_expr("1 << 64 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 << 100 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 >> 64 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 >> 100 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 << -1 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 >> -1 = 0"));
    EXPECT_TRUE(empty());
}

// bitstring left shift
TEST_F(ShiftTest, ShlBitstring) {
    // BYTE#16#01 << 4 = BYTE#16#10
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#01 << 4 = BYTE#16#10"));
    // WORD#16#0001 << 8 = WORD#16#0100
    EXPECT_NE(0LL, eval_const_expr("WORD#16#0001 << 8 = WORD#16#0100"));
    // DWORD#16#00000001 << 16 = DWORD#16#00010000
    EXPECT_NE(0LL, eval_const_expr("DWORD#16#00000001 << 16 = DWORD#16#00010000"));
    EXPECT_TRUE(empty());
}

// bitstring right shift
TEST_F(ShiftTest, ShrBitstring) {
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#80 >> 4 = BYTE#16#08"));
    EXPECT_NE(0LL, eval_const_expr("WORD#16#FF00 >> 8 = WORD#16#00FF"));
    EXPECT_TRUE(empty());
}

// bitstring mask (shifted value should be masked to the bit-width)
TEST_F(ShiftTest, BitstringMask) {
    // BYTE#16#FF << 4 should overflow within BYTE range → BYTE#16#F0
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#FF << 4 = BYTE#16#F0"));
    EXPECT_TRUE(empty());
}

// precedence: shift is lower than add, higher than cmp
// C precedence: add binds tighter than shift. So 1 + 2 << 3 = (1+2) << 3 = 24.
TEST_F(ShiftTest, Precedence) {
    // add binds tighter: 1 + 2 << 3 = (1+2) << 3 = 3 << 3 = 24
    EXPECT_NE(0LL, eval_const_expr("1 + 2 << 3 = 24"));
    // cmp binds looser: 8 << 1 = 16 is parsed as (8 << 1) = 16
    EXPECT_NE(0LL, eval_const_expr("8 << 1 = 16"));
    // chaining: 1 << 2 << 3 = (1 << 2) << 3 = 4 << 3 = 32
    EXPECT_NE(0LL, eval_const_expr("1 << 2 << 3 = 32"));
    EXPECT_TRUE(empty());
}

// ---- D: Int out-of-range shifts follow clang's PPExpressionEvaluator
// (rev2/rev3 decision, SPEC §6.3): '<<' out of range -> 0; '>>' out of
// range behaves like a shift by 63 (-1 for a negative left operand, 0
// otherwise). In-range results are unchanged from ShlInt/ShrInt above.
// INT64_MIN is written as (-9223372036854775807-1): __LINT_MIN__ (item h)
// is not usable in {#if} yet.

TEST_F(ShiftTest, ClangOutOfRangeInt) {
    EXPECT_NE(0LL, eval_const_expr("-8 >> 1 = -4"));
    EXPECT_NE(0LL, eval_const_expr("-1 >> 63 = -1"));
    EXPECT_NE(0LL, eval_const_expr("-1 >> 64 = -1"));
    EXPECT_NE(0LL, eval_const_expr("-1 >> 100 = -1"));
    EXPECT_NE(0LL, eval_const_expr("8 >> 64 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 << 63 = (-9223372036854775807-1)"));
    EXPECT_NE(0LL, eval_const_expr("1 << 64 = 0"));
    EXPECT_NE(0LL, eval_const_expr("-1 << 64 = 0"));
    EXPECT_NE(0LL, eval_const_expr("-1 << 1 = -2"));
    EXPECT_NE(0LL, eval_const_expr("16 >> -2 = 0"));
    EXPECT_NE(0LL, eval_const_expr("1 << -1 = 0"));
    EXPECT_NE(0LL, eval_const_expr("-16 << -2 = 0"));
    EXPECT_NE(0LL, eval_const_expr("-1 >> -1 = -1"));
    EXPECT_NE(0LL, eval_const_expr("-1 << -1 = 0"));
    EXPECT_NE(0LL, eval_const_expr("(-9223372036854775807-1) >> 70 = -1"));
    EXPECT_NE(0LL, eval_const_expr("3 << 62 < 0"));
    EXPECT_NE(0LL, eval_const_expr("-1 << (-9223372036854775807-1) = 0"));
    EXPECT_NE(0LL, eval_const_expr(
        "(-9223372036854775807-1) >> (-9223372036854775807-1) = -1"));
    EXPECT_TRUE(empty());
}

// ---- D: Bitstring out-of-range shifts pin the unchanged, width-consistent
// rule (rev3 decision): both '<<' and '>>' give 0 for every width, whatever
// the original bit pattern -- deliberately different from clang's unsigned
// clamp-to-63 for '>>' (e.g. 0x8000000000000000u >> 64 = 1 in C), because
// bitstrings are not a C type.

TEST_F(ShiftTest, BitstringOutOfRangeGivesZero) {
    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#16#8000000000000000 >> 64 = LWORD#16#0"));
    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#16#8000000000000000 >> -1 = LWORD#16#0"));
    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#16#8000000000000000 << -1 = LWORD#16#0"));
    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#16#8000000000000000 >> 63 = LWORD#16#1"));
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#80 >> 64 = BYTE#16#00"));
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#02 << -1 = BYTE#16#00"));
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#ff >> -1 = BYTE#16#00"));
    EXPECT_TRUE(empty());
}

// type error: float shift
TEST_F(ShiftTest, TypeError) {
    EXPECT_THROW(eval_const_expr("1.0 << 2"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("1.0 >> 2"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
}
