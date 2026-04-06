#include "constfold_test_helper.hpp"

// ---- Comparison operators ----

TEST_F(ConstfoldTest, ExprLt) {
    EXPECT_THROW(eval_const_expr("false<false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("false<true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true<false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true<true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_EQ(0LL, eval_const_expr("5<3"));
    EXPECT_EQ(0LL, eval_const_expr("0<0"));
    EXPECT_NE(0LL, eval_const_expr("0<1"));
    EXPECT_EQ(0LL, eval_const_expr("1<0"));
    EXPECT_NE(0LL, eval_const_expr("-5<3"));
    EXPECT_EQ(0LL, eval_const_expr("5.0<3.0"));
    EXPECT_NE(0LL, eval_const_expr("-5.0<3.0"));
    EXPECT_EQ(0LL, eval_const_expr("a<a"));
    EXPECT_EQ(0LL, eval_const_expr("a<b"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprLe) {
    EXPECT_THROW(eval_const_expr("false<=false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("false<=true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true<=false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true<=true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_EQ(0LL, eval_const_expr("5<=3"));
    EXPECT_NE(0LL, eval_const_expr("-5<=3"));
    EXPECT_NE(0LL, eval_const_expr("0<=0"));
    EXPECT_EQ(0LL, eval_const_expr("1<=0"));
    EXPECT_NE(0LL, eval_const_expr("0<=1"));
    EXPECT_NE(0LL, eval_const_expr("5<=5"));
    EXPECT_EQ(0LL, eval_const_expr("5.0<=3.0"));
    EXPECT_NE(0LL, eval_const_expr("-5.0<=3.0"));
    EXPECT_NE(0LL, eval_const_expr("a<=a"));
    EXPECT_NE(0LL, eval_const_expr("a<=b"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprGe) {
    EXPECT_THROW(eval_const_expr("false>=false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("false>=true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true>=false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true>=true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_NE(0LL, eval_const_expr("0>=0"));
    EXPECT_EQ(0LL, eval_const_expr("0>=1"));
    EXPECT_NE(0LL, eval_const_expr("5>=5"));
    EXPECT_NE(0LL, eval_const_expr("5>=3"));
    EXPECT_EQ(0LL, eval_const_expr("-5>=3"));
    EXPECT_NE(0LL, eval_const_expr("5.0>=3.0"));
    EXPECT_EQ(0LL, eval_const_expr("-5.0>=3.0"));
    EXPECT_NE(0LL, eval_const_expr("a>=a"));
    EXPECT_NE(0LL, eval_const_expr("a>=b"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprGt) {
    EXPECT_THROW(eval_const_expr("false>false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("false>true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true>false"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(eval_const_expr("true>true"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_EQ(0LL, eval_const_expr("0>0"));
    EXPECT_EQ(0LL, eval_const_expr("0>1"));
    EXPECT_NE(0LL, eval_const_expr("1>0"));
    EXPECT_EQ(0LL, eval_const_expr("3>5"));
    EXPECT_NE(0LL, eval_const_expr("5>3"));
    EXPECT_EQ(0LL, eval_const_expr("-5>3"));
    EXPECT_NE(0LL, eval_const_expr("5.0>3.0"));
    EXPECT_EQ(0LL, eval_const_expr("-5.0>3.0"));
    EXPECT_EQ(0LL, eval_const_expr("a>a"));
    EXPECT_EQ(0LL, eval_const_expr("a>b"));
    EXPECT_TRUE(empty());
}

