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

// type error: float shift
TEST_F(ShiftTest, TypeError) {
    EXPECT_THROW(eval_const_expr("1.0 << 2"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("1.0 >> 2"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
}
