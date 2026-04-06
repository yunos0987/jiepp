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

