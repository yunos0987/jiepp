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

// A6: a base so large that "base * 10 + digit" used to overflow a signed int
// (undefined behavior, observed as a wrapped small base) is an invalid base
// regardless of how many digits it has.
TEST_F(ConstfoldTest, BaseNHugeBaseIsInvalidNotWrapped) {
    Issue::Blocking guard({});
    for (const char* expr : {"4294967298#101 = 5", "4294967300#1",
                             "18446744073709551618#11 = 4"}) {
        eval_const_expr(expr);
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code()) << expr;
    }
    eval_const_expr("4294967298#101 = 5");
    EXPECT_NE(std::string::npos, message().find("invalid base: 4294967298#101"));
}

TEST_F(ConstfoldTest, BaseNLeadingZerosStillValid_Guard) {
    EXPECT_NE(0LL, eval_const_expr("0036#z = 35"));
    EXPECT_NE(0LL, eval_const_expr("002#101 = 5"));
    EXPECT_TRUE(empty());
}

// A6: the float -> integer range is the half-open [-2^63, 2^63). 2^63 itself
// used to pass the check (static_cast<double>(INT64_MAX) is 2^63) and then
// overflowed the conversion (undefined behavior).
TEST_F(ConstfoldTest, FloatCastRangeIsHalfOpen) {
    Issue::Blocking guard({});
    eval_const_expr("LWORD#9223372036854775808.0");
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());

    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#9223372036854774784.0 = LWORD#16#7FFFFFFFFFFFFC00"));
    EXPECT_TRUE(empty());
    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#-9223372036854775808.0 = LWORD#16#8000000000000000"));
    EXPECT_TRUE(empty());

    eval_const_expr("LWORD#-9223372036854777856.0");
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());

    // The out-of-range value becomes 0, so the comparison is true; before the
    // fix this was true with no diagnostic at all.
    EXPECT_NE(0LL, eval_const_expr("BYTE#9223372036854775808.0 = BYTE#0"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

// The range error depends on the value, so it stays silent in an unevaluated
// operand (SPEC 6.4). The right-hand side compares with LWORD#0: LWORD against
// Int would be a value-independent type error (PP50) that is reported even
// there.
TEST_F(ConstfoldTest, FloatCastRangeUnevaluatedSilent_Guard) {
    Issue::Blocking guard({});
    auto r = eval_const_value(
        "FALSE \\and\\ (LWORD#9223372036854775808.0 = LWORD#0)");
    EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
    EXPECT_TRUE(empty());
}

// A7: a NUL used to end the expression early (the scanner read a C string),
// so `1\0garbage(((` was just `1`. It is PP52 now, and the diagnostic shows
// the NUL as $00.
TEST_F(ConstfoldTest, NulInExpressionIsPP52) {
    Issue::Blocking guard({});
    for (const std::string& expr : {std::string("1\0garbage(((", 12),
                                    std::string("1\0", 2)}) {
        EXPECT_EQ(0LL, eval_const_expr(expr));
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    }
    eval_const_expr(std::string("1\0x", 3));
    EXPECT_NE(std::string::npos, message().find("null character in expression: 1$00x"));
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

// ---- H: the 2^63 magnitude (9223372036854775808, __LINT_MIN__'s absolute
// value) in {#if}. gcc/clang read this as unsigned with a warning, so
// "-9223372036854775808 < 0" is false there; jiepp has no unsigned Int, so
// (coordinator decision, option A) it accepts the magnitude as INT64_MIN
// only when a unary minus is its *direct* operand (CF_MINUS
// CF_INT_MIN_MAG in the grammar) or inside a typed literal, and keeps
// today's PP52 "integer literal overflow" everywhere else. Edge-case table
// from plan.md item h.

TEST_F(ConstfoldTest, LintMinMagDirectUnaryMinus) {
    EXPECT_NE(0LL, eval_const_expr("-9223372036854775808 < 0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LintMinMagWhitespaceBeforeLiteralStillDirect) {
    // Whitespace between the '-' and the literal does not change tokenization.
    EXPECT_NE(0LL, eval_const_expr("- 9223372036854775808 < 0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LintMinMagParenthesizedDirectMinus) {
    EXPECT_NE(0LL, eval_const_expr("(-9223372036854775808) < 0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LintMinMagInsideParenthesesNotDirectOverflows) {
    // Only a minus directly before the literal counts: here the '-' is
    // outside the parentheses, so the literal itself is bare inside them
    // and reports the same PP52 as any other occurrence.
    EXPECT_THROW(eval_const_expr("-(9223372036854775808)"), Issue::Exception);
    // message() (not code(), which would drain the one-message buffer
    // first) also confirms exactly one diagnostic and its code (PP52).
    EXPECT_NE(std::string::npos,
              message().find("PP52: Invalid expression; 'integer literal overflow: "
                              "9223372036854775808'"));
}

TEST_F(ConstfoldTest, LintMinMagDoubleMinusWraps) {
    // jiepp lexes "--" as two CF_MINUS tokens (no "--" rule in constfold.l):
    // the inner "-9223372036854775808" is INT64_MIN via CF_MINUS
    // CF_INT_MIN_MAG, and the outer '-' negates it, which wraps back to
    // INT64_MIN (two's-complement) with no diagnostic.
    EXPECT_NE(0LL, eval_const_expr("--9223372036854775808 = -9223372036854775807 - 1"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LintMinMagAfterUnaryPlusOrNotOverflows) {
    // The literal is not the *direct* operand of '-' in either case (it is
    // the operand of '+'/'not', which are not the special grammar rule), so
    // both still report PP52.
    EXPECT_THROW(eval_const_expr("-+9223372036854775808"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(eval_const_expr("-not 9223372036854775808"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

TEST_F(ConstfoldTest, LintMinMagAfterBinaryMinusOverflows) {
    // A binary '-' (here, add_expr's CF_MINUS) does not count either.
    EXPECT_THROW(eval_const_expr("1-9223372036854775808"), Issue::Exception);
    EXPECT_NE(std::string::npos,
              message().find("PP52: Invalid expression; 'integer literal overflow: "
                              "9223372036854775808'"));
}

TEST_F(ConstfoldTest, LintMinMagAfterUnaryMinusThenBinaryMinusWraps) {
    // "1 - -9223372036854775808": the second '-' is the direct (unary)
    // operand of the literal, giving INT64_MIN; "1 - INT64_MIN" then wraps
    // (two's-complement) with no diagnostic.
    EXPECT_NE(0LL, eval_const_expr("1 - -9223372036854775808 = -9223372036854775807"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LintMinMagAfterTypeKeywordMinusOverflows) {
    // Bare "BOOL" (no '#') is an ordinary primary worth 0 (`primary:
    // CF_TYPE_KW` yields CfValue::int_value(0), i.e. ValueKind::Int, not
    // ValueKind::Bool); the '-' between it and the literal is the binary
    // add_expr minus, not a unary operand of the literal.
    EXPECT_THROW(eval_const_expr("BOOL - 9223372036854775808"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

TEST_F(ConstfoldTest, LintMinMagBareOrWithUnaryPlusOverflows) {
    EXPECT_THROW(eval_const_expr("9223372036854775808"), Issue::Exception);
    EXPECT_NE(std::string::npos,
              message().find("PP52: Invalid expression; 'integer literal overflow: "
                              "9223372036854775808'"));
    EXPECT_THROW(eval_const_expr("+9223372036854775808"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

TEST_F(ConstfoldTest, ValuesAboveMinMagStillOverflow) {
    // One more than the magnitude, and __ULINT_MAX__'s value: neither
    // matches the exact "9223372036854775808" digit string the lexer
    // special-cases, so both still go through std::stoll and overflow, same
    // as before this change.
    EXPECT_THROW(eval_const_expr("9223372036854775809"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(eval_const_expr("18446744073709551615"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

TEST_F(ConstfoldTest, LintMinMagSeparatorAndLeadingZeroSpellingsValid) {
    // The lexer strips '_' separators and leading zeros before comparing
    // against the magnitude string, so both spellings are recognized too.
    EXPECT_NE(0LL, eval_const_expr("-9_223_372_036_854_775_808 < 0"));
    EXPECT_TRUE(empty());
    EXPECT_NE(0LL, eval_const_expr("-09223372036854775808 < 0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, LintMinMagSeparatorSpellingOverflowMessageKeepsOriginalText) {
    // REQUIRED (design review): exercises the bare-literal PP52 diagnostic
    // path with cflval.text set from a non-canonical spelling, so a
    // regression that forgets to construct CF_INT_MIN_MAG's semantic value
    // (see the yylex() wrapper's switch) shows up as a crash or garbage
    // text here, not just as a wrong *value*.
    EXPECT_THROW(eval_const_expr("9_223_372_036_854_775_808"), Issue::Exception);
    EXPECT_NE(std::string::npos, message().find(
        "PP52: Invalid expression; 'integer literal overflow: "
        "9_223_372_036_854_775_808'"));
}

TEST_F(ConstfoldTest, LintMinTypedLiteral) {
    // H: a typed literal accepts the magnitude directly (cast_value's
    // CF_INT_MIN_MAG rule): the bit pattern fits LWORD exactly.
    EXPECT_NE(0LL, eval_const_expr(
        "LWORD#9223372036854775808 = LWORD#16#8000000000000000"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, BasedLiteralMinMagUnaffected) {
    // The based-literal path (constfold.l's '#' rule) is untouched: it
    // already accumulates in uint64_t and wraps to INT64_MIN with no
    // diagnostic.
    EXPECT_NE(0LL, eval_const_expr(
        "16#8000000000000000 = -9223372036854775807 - 1"));
    EXPECT_TRUE(empty());
}
// ---- R1 (H1): eval_const_value() returns the typed value ----

TEST_F(ConstfoldTest, EvalConstValueReturnsKindAndValue) {
    {
        auto r = eval_const_value("10");
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
        EXPECT_EQ(ValueKind::Int, r.value.kind);
        EXPECT_EQ(10, r.value.i);
    }
    {
        auto r = eval_const_value("-1");
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
        EXPECT_EQ(ValueKind::Int, r.value.kind);
        EXPECT_EQ(-1, r.value.i);
    }
    {
        auto r = eval_const_value("2147483648");
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
        EXPECT_EQ(ValueKind::Int, r.value.kind);
        EXPECT_EQ(2147483648LL, r.value.i);
    }
    {
        auto r = eval_const_value("2.5");
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
        EXPECT_EQ(ValueKind::Float, r.value.kind);
        EXPECT_DOUBLE_EQ(2.5, r.value.f);
    }
    {
        auto r = eval_const_value("BYTE#16#FF");
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
        EXPECT_EQ(ValueKind::Bitstring, r.value.kind);
        EXPECT_EQ(BitKind::BYTE, r.value.bit_kind);
        EXPECT_EQ(255u, r.value.bits);
    }
    {
        auto r = eval_const_value("TRUE");
        EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
        EXPECT_EQ(ValueKind::Bool, r.value.kind);
        EXPECT_TRUE(r.value.b);
    }
    // eval_const_expr() still returns only the 0/1 truth value.
    EXPECT_EQ(1LL, eval_const_expr("10"));
    EXPECT_EQ(1LL, eval_const_expr("-1"));
    EXPECT_EQ(1LL, eval_const_expr("2147483648"));
    EXPECT_EQ(1LL, eval_const_expr("2.5"));
    EXPECT_EQ(1LL, eval_const_expr("BYTE#16#FF"));
    EXPECT_EQ(1LL, eval_const_expr("TRUE"));
    EXPECT_EQ(0LL, eval_const_expr("0"));
    EXPECT_TRUE(empty());
}

// ---- R1 (H1/M1): failure codes, including literal errors raised inside
// scanner/grammar actions; division by zero is not a failure ----

TEST_F(ConstfoldTest, EvalConstValueReportsFailureCode) {
    Issue::ContinueMode guard({});
    auto failure_of = [&](const std::string& expr) {
        auto r = eval_const_value(expr);
        return r.failure;
    };
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, failure_of(""));
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, failure_of("1 +"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, failure_of("1 x"));
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, failure_of("TRUE + 1"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, failure_of("99999999999999999999"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, failure_of("2#102"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, failure_of("1.0e999"));
    auto cs = codes();
    ASSERT_EQ(7u, cs.size());

    auto r = eval_const_value("1 / 0 + 5");
    EXPECT_EQ(Issue::Code::SUCCESS, r.failure);
    EXPECT_EQ(ValueKind::Int, r.value.kind);
    EXPECT_EQ(5, r.value.i);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code()); // "division by zero"

    // A failure does not leak into the next evaluation.
    EXPECT_EQ(Issue::Code::SUCCESS, failure_of("1"));

    // {#if} keeps its result for the literal-error cases (F7).
    EXPECT_EQ(1LL, eval_const_expr("99999999999999999999 > 0"));
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
}

// ---- Float literal range: subnormals are accepted, zero/inf results are PP52 ----
// std::stod throws on ERANGE, and glibc sets ERANGE for subnormal results while
// the UCRT does not, so the outcome used to differ per platform. The lexer now
// uses strtod and reports only a true overflow (+-HUGE_VAL) or an underflow
// to exactly zero from a nonzero literal.

TEST_F(ConstfoldTest, FloatLiteralSubnormalIsAccepted) {
    EXPECT_NE(0LL, eval_const_expr("1.0e-310 > 0.0"));
    EXPECT_TRUE(empty());
    EXPECT_NE(0LL, eval_const_expr("2.0e-308 > 0.0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, FloatLiteralUnderflowToZeroIsPP52) {
    EXPECT_THROW(eval_const_expr("1.0e-400 = 0.0"), Issue::Exception);
    EXPECT_NE(std::string::npos,
              message().find("PP52: Invalid expression; 'float literal underflow: 1.0e-400'"));
}

TEST_F(ConstfoldTest, FloatLiteralZeroIsNotUnderflow) {
    EXPECT_NE(0LL, eval_const_expr("0.0e-400 = 0.0"));
    EXPECT_TRUE(empty());
}

TEST_F(ConstfoldTest, FloatLiteralOverflowIsPP52) {
    EXPECT_THROW(eval_const_expr("1.0e400 = 0.0"), Issue::Exception);
    EXPECT_NE(std::string::npos,
              message().find("PP52: Invalid expression; 'float literal overflow: 1.0e400'"));
}
