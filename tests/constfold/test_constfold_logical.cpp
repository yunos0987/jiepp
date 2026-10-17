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

// An exception thrown out of the parser inside a dead operand never runs the
// action that lowers cf_unevaluated_depth; evaluate() resets it, so the next
// evaluation is not wrongly treated as unevaluated.
TEST_F(ConstfoldTest, DepthResetAfterAbort) {
    IssueStateReset reset;
    Issue::initialize(output());
    Issue::enable(Issue::Code::UNDEFINED_IDENTIFIER);
    Issue::ContinueMode guard({});

    // PP50 inside the dead right operand of \or\ (a Float operand is a type
    // error even when it is not evaluated: SPEC 6.4).
    EXPECT_EQ(0LL, eval_const_expr("TRUE \\or\\ (1.5 + 1)"));  // type error: 0
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());

    EXPECT_EQ(0LL, eval_const_expr("FOO"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], ::testing::HasSubstr("PP54: 'FOO' is not defined, evaluates to 0"));
}

// ---- Value-dependent diagnostics in unevaluated operands (SPEC 6.4) ----
// Division/modulo by zero and a float value out of the integer range are not
// reported in the right operand of \and\ / \or\ that is not evaluated, and
// the float range is then no evaluation failure either.

TEST_F(ConstfoldTest, DivisionByZeroInDeadAndOperand) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(0LL, eval_const_expr("FALSE \\and\\ 1/0 = 1"));
    EXPECT_EQ(0LL, eval_const_expr("0 \\and\\ 1/0 = 1"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ModuloByZeroInDeadOrOperand) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(1LL, eval_const_expr("TRUE \\or\\ 1 mod 0 = 0"));
    EXPECT_EQ(1LL, eval_const_expr("1 \\or\\ 1 mod 0 = 0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, FloatRangeInDeadOperand) {
    Issue::ContinueMode guard({});
    // (A Float shift count is a type error, PP50, reported even here; it is
    // covered by ShiftTest.ShiftCountTypeErrorInUnevaluatedOperand.)
    for (const char* expr : {"FALSE \\and\\ BYTE#1.0e30 = BYTE#0"}) {
        auto r = eval_const_value(expr);
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure) << expr;
        EXPECT_TRUE(empty()) << expr;
    }
}

TEST_F(ConstfoldTest, FloatRangeInLiveOperandStillReports) {
    Issue::ContinueMode guard({});
    auto r = eval_const_value("TRUE \\and\\ BYTE#1.0e30 = BYTE#0");
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, r.failure);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

TEST_F(ConstfoldTest, NestedDeadOperand) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(0LL, eval_const_expr("FALSE \\and\\ (TRUE \\or\\ 1/0 = 0)"));
    EXPECT_EQ(1LL, eval_const_expr("TRUE \\or\\ (1/0 = 0 \\and\\ TRUE)"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LiveOperandsStillReport) {
    Issue::ContinueMode guard({});
    for (const char* expr : {"TRUE \\and\\ 1/0 = 0",
                             "FALSE \\or\\ 5 mod 0 = 0",
                             "TRUE \\and\\ (FALSE \\or\\ 1/0 = 0)"}) {
        eval_const_expr(expr);
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code()) << expr;
    }
}

TEST_F(ConstfoldTest, StaticErrorsInDeadOperand) {
    Issue::ContinueMode guard({});
    for (const char* expr : {"FALSE \\and\\ 99999999999999999999 > 0",
                             "FALSE \\and\\ 2#102 = 2",
                             "FALSE \\and\\ 9223372036854775808 > 0"}) {
        eval_const_expr(expr);
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code()) << expr;
    }
    eval_const_expr("FALSE \\and\\ (TRUE + 1)");
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    eval_const_expr("FALSE \\and\\ (1 +");
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

// At depth 0 the order and the set of diagnostics are unchanged.
TEST_F(ConstfoldTest, DepthZeroOrderUnchanged) {
    Issue::ContinueMode guard({});
    eval_const_expr("1/0 = 0 \\or\\ 2#102 = 2");
    auto msgs = messages();
    ASSERT_EQ(2u, msgs.size());
    EXPECT_THAT(msgs[0], ::testing::HasSubstr("PP52: Invalid expression; 'division by zero'"));
    EXPECT_THAT(msgs[1], ::testing::HasSubstr("PP52"));
    EXPECT_THAT(msgs[1], ::testing::Not(::testing::HasSubstr("division by zero")));
}

