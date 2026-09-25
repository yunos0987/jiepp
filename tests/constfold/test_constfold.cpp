#include "constfold_test_helper.hpp"

// ---- Equality operators and exact integration cases ----

TEST_F(ConstfoldTest, ExprEq) {
    EXPECT_NE(0LL, eval_const_expr("false=false"));
    EXPECT_EQ(0LL, eval_const_expr("false=true"));
    EXPECT_EQ(0LL, eval_const_expr("true=false"));
    EXPECT_NE(0LL, eval_const_expr("true=true"));
    EXPECT_NE(0LL, eval_const_expr("0=0"));
    EXPECT_EQ(0LL, eval_const_expr("0=1"));
    EXPECT_EQ(0LL, eval_const_expr("1=0"));
    EXPECT_NE(0LL, eval_const_expr("1=1"));
    EXPECT_NE(0LL, eval_const_expr("0.0=0.0"));
    EXPECT_EQ(0LL, eval_const_expr("0.0=1.0"));
    EXPECT_EQ(0LL, eval_const_expr("1.0=0.0"));
    EXPECT_NE(0LL, eval_const_expr("1.0=1.0"));
    EXPECT_NE(0LL, eval_const_expr("a=a"));
    EXPECT_NE(0LL, eval_const_expr("a=b"));
    EXPECT_THROW(eval_const_expr("byte#16#0 = word#16#0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprNe) {
    EXPECT_EQ(0LL, eval_const_expr("false<>false"));
    EXPECT_NE(0LL, eval_const_expr("false<>true"));
    EXPECT_NE(0LL, eval_const_expr("true<>false"));
    EXPECT_EQ(0LL, eval_const_expr("true<>true"));
    EXPECT_EQ(0LL, eval_const_expr("0<>0"));
    EXPECT_NE(0LL, eval_const_expr("0<>1"));
    EXPECT_NE(0LL, eval_const_expr("1<>0"));
    EXPECT_EQ(0LL, eval_const_expr("1<>1"));
    EXPECT_EQ(0LL, eval_const_expr("0.0<>0.0"));
    EXPECT_NE(0LL, eval_const_expr("0.0<>1.0"));
    EXPECT_NE(0LL, eval_const_expr("1.0<>0.0"));
    EXPECT_EQ(0LL, eval_const_expr("1.0<>1.0"));
    EXPECT_EQ(0LL, eval_const_expr("a<>a"));
    EXPECT_EQ(0LL, eval_const_expr("a<>b"));
    EXPECT_THROW(eval_const_expr("byte#16#0 <> word#16#0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprExactSuccessCases) {
    EXPECT_NE(0LL, eval_const_expr("not not (- + -1<>+ - +1)"));
    EXPECT_NE(0LL, eval_const_expr("not not (- + -1=+ - +1)=not not (- - -1=+ + +1)"));
    EXPECT_EQ(0LL, eval_const_expr("not not (- + -1=+ - +1)<>not not (- - -1=+ + +1)"));
    EXPECT_NE(0LL, eval_const_expr("not byte#16#00 and byte#16#ff"));
    EXPECT_NE(0LL, eval_const_expr("\\not\\ false"));
    EXPECT_EQ(0LL, eval_const_expr("false \\or\\ false"));
    EXPECT_EQ(0LL, eval_const_expr("false \\and\\ false"));
    EXPECT_EQ(0LL, eval_const_expr("false \\xor\\ false"));
    EXPECT_NE(0LL, eval_const_expr("not byte#16#00"));
    EXPECT_NE(0LL, eval_const_expr("not word#16#0"));
    EXPECT_EQ(0LL, eval_const_expr("not word#16#ffff"));
    EXPECT_EQ(0LL, eval_const_expr("not dword#16#ffff_ffff"));
    EXPECT_NE(0LL, eval_const_expr("not lword#16#0"));
    EXPECT_EQ(0LL, eval_const_expr("not lword#16#ffff_ffff_ffff_ffff"));
    EXPECT_NE(0LL, eval_const_expr("16#1234 or 16#ffff"));
    EXPECT_NE(0LL, eval_const_expr("16#1234 xor 16#ffff"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ExprExactErrorCases) {
    struct Case { const char* expr; Issue::Code code; };
    static const Case cases[] = {
        {"not byte#16#0 and word#16#0",          Issue::Code::EXPR_TYPE_ERROR},
        {"+byte#16#0",                           Issue::Code::EXPR_TYPE_ERROR},
        {"-byte#16#0",                           Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0+byte#16#0",                  Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0-0",                          Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0-byte#16#0",                  Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0*0",                          Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0*byte#16#0",                  Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0<byte#16#0",                  Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0<=byte#16#0",                 Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0>=byte#16#0",                 Issue::Code::EXPR_TYPE_ERROR},
        {"byte#16#0>byte#16#0",                  Issue::Code::EXPR_TYPE_ERROR},
        {"not 0.0",                              Issue::Code::EXPR_TYPE_ERROR},
        {"word#16#1234 or 16#ffff",              Issue::Code::EXPR_TYPE_ERROR},
    };
    for (const auto& c : cases) {
        EXPECT_THROW(eval_const_expr(c.expr), Issue::Exception);
        EXPECT_EQ(c.code, code()) << c.expr;
    }
    EXPECT_TRUE(empty());
}

// ---- Base-N literal validation ----

TEST_F(ConstfoldTest, BaseNInvalidBase) {
    EXPECT_THROW(eval_const_expr("0#0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(eval_const_expr("1#0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(eval_const_expr("37#0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNInvalidDigit) {
    EXPECT_THROW(eval_const_expr("2#9"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(eval_const_expr("16#G"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNValidCases) {
    EXPECT_NE(0LL, eval_const_expr("2#1010 = 10"));
    EXPECT_NE(0LL, eval_const_expr("8#17 = 15"));
    EXPECT_NE(0LL, eval_const_expr("16#ff = 255"));
    EXPECT_NE(0LL, eval_const_expr("36#z = 35"));
    EXPECT_TRUE(empty());
}

// ---- A1: '_' may only separate two digits in a based literal ----

TEST_F(ConstfoldTest, BaseNInternalUnderscoreValid) {
    EXPECT_NE(0LL, eval_const_expr("16#ff_ff = 65535"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNTrailingUnderscoreInvalid) {
    // "16#ff_" only matches "16#ff" as a based literal; the leftover "_" is
    // re-lexed as an identifier, so the parser reports a syntax error
    // (INVALID_EXPRESSION / PP52) rather than accepting the trailing '_'.
    EXPECT_THROW(eval_const_expr("16#ff_"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNDoubleUnderscoreInvalid) {
    // "2#1__0" only matches "2#1" as a based literal (a lone '_' cannot
    // start the "(_?[0-9A-Za-z])*" group); the leftover "__0" is re-lexed as
    // an identifier, producing the same syntax error as above.
    EXPECT_THROW(eval_const_expr("2#1__0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

// ---- B: Ed.3 allows one '_' directly after '#' (or the sign), before the
// first digit ----

TEST_F(ConstfoldTest, BaseNLeadingUnderscoreValid) {
    EXPECT_NE(0LL, eval_const_expr("16#_ff = 255"));
    EXPECT_NE(0LL, eval_const_expr("2#_1010 = 10"));
    EXPECT_NE(0LL, eval_const_expr("8#_7_7 = 63"));
    EXPECT_NE(0LL, eval_const_expr("16#_f_f = 255"));
    EXPECT_NE(0LL, eval_const_expr("BYTE#16#_ff = BYTE#16#ff"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNDoubleLeadingUnderscoreInvalid) {
    // "16#__ff": the leading '_' must be followed directly by a digit, so
    // two in a row still fails to match, same as BaseNDoubleUnderscoreInvalid.
    EXPECT_THROW(eval_const_expr("16#__ff"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNLoneLeadingUnderscoreInvalid) {
    // "16#_" has nothing after the '_', so it cannot match either the
    // leading-underscore form or the plain form.
    EXPECT_THROW(eval_const_expr("16#_"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNSignedLeadingUnderscoreValid) {
    // The leading '_' comes after the sign, not before it (review follow-up
    // to item b): "16#-_ff" is base 16, sign '-', digits "_ff" -> -255.
    EXPECT_NE(0LL, eval_const_expr("16#-_ff = -255"));
    EXPECT_NE(0LL, eval_const_expr("16#+_ff = 255"));
    EXPECT_TRUE(empty());
}

// ---- A2: based literals accumulate in uint64_t; report overflow past 2^64-1 ----

TEST_F(ConstfoldTest, BaseNMaxUint64Valid) {
    // 2^64-1 (all-Fs, base 16) is exactly representable; it round-trips
    // through int64_t as -1 (two's-complement bit pattern).
    EXPECT_NE(0LL, eval_const_expr("16#ffff_ffff_ffff_ffff = -1"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BaseNOverflowPastUint64Max) {
    // 16^16 == 2^64 exceeds the 2^64-1 limit and must be reported instead of
    // silently wrapping (the previous int64_t accumulation had no overflow
    // check at all).
    EXPECT_THROW(eval_const_expr("16#1_0000_0000_0000_0000"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

// ---- Division and modulo by zero ----

TEST_F(ConstfoldTest, DivisionByZero) {
    EXPECT_THROW(eval_const_expr("5/0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, ModuloByZero) {
    EXPECT_THROW(eval_const_expr("5 mod 0"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

// ---- Integer literal overflow ----

TEST_F(ConstfoldTest, IntegerLiteralOverflow) {
    EXPECT_THROW(eval_const_expr("99999999999999999999999"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}