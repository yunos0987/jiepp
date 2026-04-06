#include "constfold_test_helper.hpp"

// ---- Logical and bitwise operators ----

TEST_F(ConstfoldTest, ExprNot) {
    EXPECT_NE(0LL, eval_const_expr("not false"));
    EXPECT_EQ(0LL, eval_const_expr("not true"));
    EXPECT_EQ(0LL, eval_const_expr("not -1"));
    EXPECT_NE(0LL, eval_const_expr("not 0"));
    EXPECT_NE(0LL, eval_const_expr("not 2"));
    EXPECT_NE(0LL, eval_const_expr("not a"));
    EXPECT_EQ(0LL, eval_const_expr("not byte#16#ff"));
    EXPECT_NE(0LL, eval_const_expr("not dword#16#0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprLogicalNot) {
    EXPECT_NE(0LL, eval_const_expr(R"(\not\ false)"));
    EXPECT_EQ(0LL, eval_const_expr(R"(\not\ true)"));
    EXPECT_NE(0LL, eval_const_expr(R"(\not\ 0)"));
    EXPECT_EQ(0LL, eval_const_expr(R"(\not\ -1)"));
    EXPECT_EQ(0LL, eval_const_expr(R"(\not\ 1)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprAnd) {
    EXPECT_EQ(0LL, eval_const_expr("false and false"));
    EXPECT_EQ(0LL, eval_const_expr("false and true"));
    EXPECT_EQ(0LL, eval_const_expr("true and false"));
    EXPECT_NE(0LL, eval_const_expr("true and true"));
    EXPECT_NE(0LL, eval_const_expr("byte#16#cd and byte#16#f0"));
    EXPECT_NE(0LL, eval_const_expr("word#16#babe and word#16#f0f0"));
    EXPECT_NE(0LL, eval_const_expr("dword#16#babe_cafe and dword#16#f0f0_f0f0"));
    EXPECT_NE(0LL, eval_const_expr("lword#16#0123_4567_89ab_cdef and lword#16#f0f0_f0f0_f0f0_f0f0"));
    EXPECT_EQ(0LL, eval_const_expr("byte#16#cd and byte#16#00"));
    EXPECT_EQ(0LL, eval_const_expr("word#16#babe and word#16#0000"));
    EXPECT_EQ(0LL, eval_const_expr("dword#16#babe_cafe and dword#16#0000_0000"));
    EXPECT_EQ(0LL, eval_const_expr("lword#16#0123_4567_89ab_cdef and lword#16#0000_0000_0000_0000"));
    EXPECT_EQ(0LL, eval_const_expr("a and a"));
    EXPECT_EQ(0LL, eval_const_expr("a and b"));
    EXPECT_NE(0LL, eval_const_expr("16#1234 and 16#ffff"));
    EXPECT_THROW(eval_const_expr("word#16#1234 and 16#ffff"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprAmp) {
    EXPECT_EQ(0LL, eval_const_expr("false & false"));
    EXPECT_EQ(0LL, eval_const_expr("false & true"));
    EXPECT_EQ(0LL, eval_const_expr("true & false"));
    EXPECT_NE(0LL, eval_const_expr("true & true"));
    EXPECT_NE(0LL, eval_const_expr("byte#16#cd & byte#16#f0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprOr) {
    EXPECT_EQ(0LL, eval_const_expr("false or false"));
    EXPECT_NE(0LL, eval_const_expr("false or true"));
    EXPECT_NE(0LL, eval_const_expr("true or false"));
    EXPECT_NE(0LL, eval_const_expr("true or true"));
    EXPECT_NE(0LL, eval_const_expr("byte#16#cd or byte#16#f0"));
    EXPECT_NE(0LL, eval_const_expr("word#16#babe or word#16#f0f0"));
    EXPECT_NE(0LL, eval_const_expr("dword#16#babe_cafe or dword#16#f0f0_f0f0"));
    EXPECT_NE(0LL, eval_const_expr("lword#16#0123_4567_89ab_cdef or lword#16#f0f0_f0f0_f0f0_f0f0"));
    EXPECT_NE(0LL, eval_const_expr("byte#16#00 or byte#16#f0"));
    EXPECT_NE(0LL, eval_const_expr("word#16#0000 or word#16#f0f0"));
    EXPECT_NE(0LL, eval_const_expr("dword#16#0000_0000 or dword#16#f0f0_f0f0"));
    EXPECT_NE(0LL, eval_const_expr("lword#16#0000_0000_0000_0000 or lword#16#f0f0_f0f0_f0f0_f0f0"));
    EXPECT_EQ(0LL, eval_const_expr("byte#16#00 or byte#16#00"));
    EXPECT_EQ(0LL, eval_const_expr("word#16#0000 or word#16#0000"));
    EXPECT_EQ(0LL, eval_const_expr("dword#16#0000_0000 or dword#16#0000_0000"));
    EXPECT_EQ(0LL, eval_const_expr("lword#16#0000_0000_0000_0000 or lword#16#0000_0000_0000_0000"));
    EXPECT_EQ(0LL, eval_const_expr("a or a"));
    EXPECT_EQ(0LL, eval_const_expr("a or b"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprXor) {
    EXPECT_EQ(0LL, eval_const_expr("false xor false"));
    EXPECT_NE(0LL, eval_const_expr("false xor true"));
    EXPECT_NE(0LL, eval_const_expr("true xor false"));
    EXPECT_EQ(0LL, eval_const_expr("true xor true"));
    EXPECT_EQ(0LL, eval_const_expr("byte#16#f0 xor byte#16#f0"));
    EXPECT_EQ(0LL, eval_const_expr("word#16#f0f0 xor word#16#f0f0"));
    EXPECT_EQ(0LL, eval_const_expr("dword#16#f0f0_f0f0 xor dword#16#f0f0_f0f0"));
    EXPECT_EQ(0LL, eval_const_expr("lword#16#f0f0_f0f0_f0f0_f0f0 xor lword#16#f0f0_f0f0_f0f0_f0f0"));
    EXPECT_EQ(0LL, eval_const_expr("byte#16#00 xor byte#16#00"));
    EXPECT_EQ(0LL, eval_const_expr("word#16#0000 xor word#16#0000"));
    EXPECT_EQ(0LL, eval_const_expr("dword#16#0000_0000 xor dword#16#0000_0000"));
    EXPECT_EQ(0LL, eval_const_expr("lword#16#0000_0000_0000_0000 xor lword#16#0000_0000_0000_0000"));
    EXPECT_NE(0LL, eval_const_expr("byte#16#cd xor byte#16#f0"));
    EXPECT_NE(0LL, eval_const_expr("word#16#babe xor word#16#f0f0"));
    EXPECT_NE(0LL, eval_const_expr("dword#16#babe_cafe xor dword#16#f0f0_f0f0"));
    EXPECT_NE(0LL, eval_const_expr("lword#16#0123_4567_89ab_cdef xor lword#16#f0f0_f0f0_f0f0_f0f0"));
    EXPECT_EQ(0LL, eval_const_expr("a xor a"));
    EXPECT_EQ(0LL, eval_const_expr("a xor b"));
    EXPECT_THROW(eval_const_expr("word#16#1234 xor 16#ffff"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprLogicalOr) {
    EXPECT_EQ(0LL, eval_const_expr(R"(false \or\ false)"));
    EXPECT_NE(0LL, eval_const_expr(R"(false \or\ true)"));
    EXPECT_NE(0LL, eval_const_expr(R"(true \or\ false)"));
    EXPECT_NE(0LL, eval_const_expr(R"(true \or\ true)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprLogicalAnd) {
    EXPECT_EQ(0LL, eval_const_expr(R"(false \and\ false)"));
    EXPECT_EQ(0LL, eval_const_expr(R"(false \and\ true)"));
    EXPECT_NE(0LL, eval_const_expr(R"(true \and\ true)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprLogicalXor) {
    EXPECT_EQ(0LL, eval_const_expr(R"(false \xor\ false)"));
    EXPECT_NE(0LL, eval_const_expr(R"(false \xor\ true)"));
    EXPECT_NE(0LL, eval_const_expr(R"(true \xor\ false)"));
    EXPECT_EQ(0LL, eval_const_expr(R"(true \xor\ true)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprComplex) {
    EXPECT_EQ(0LL, eval_const_expr("- +1=1"));
    EXPECT_NE(0LL, eval_const_expr("- +1=-1"));
    EXPECT_EQ(0LL, eval_const_expr("not not (- + -1=+ - +1)"));
    EXPECT_NE(0LL, eval_const_expr("(1+2)*3=9"));
    EXPECT_NE(0LL, eval_const_expr("1+2*3=7"));
    EXPECT_TRUE(empty());
}

