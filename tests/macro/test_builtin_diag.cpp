#include "test_helper.hpp"

#include <string>
#include <vector>

// =====================================================================
// R1: the built-in function macros __error(msg...) and
// __assert(expr, msg...) (SPECIFICATION.md 12.9). The numbers E1-E33 refer
// to the behaviour table of the implementation plan.
// =====================================================================

class BuiltinDiagTest : public JieppTest {
protected:
    // Each diagnostic line without its location prefix, e.g. "PP91: 'boom'".
    std::vector<std::string> diags() {
        std::vector<std::string> out;
        for (const auto& m : messages()) {
            auto p = m.find(": PP");
            out.push_back(p == std::string::npos ? m : m.substr(p + 2));
        }
        return out;
    }
};

using Strings = std::vector<std::string>;

// ---- __error ----

// E1
TEST_F(BuiltinDiagTest, ErrorReportsPP91AndExpandsToNothing) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("a  b", pp("a __error(boom) b"));
    EXPECT_EQ(Strings({"PP91: 'boom'"}), diags());
}

TEST_F(BuiltinDiagTest, ErrorMessageIsMacroExpanded) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define M hi}__error(M there)"));
    EXPECT_EQ(Strings({"PP91: 'hi there'"}), diags());
}

// E3
TEST_F(BuiltinDiagTest, ErrorMessageKeepsCommasLikeVaArgs) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__error(a, b ,c)"));
    EXPECT_EQ(Strings({"PP91: 'a, b ,c'"}), diags());
}

// E2
TEST_F(BuiltinDiagTest, ErrorEmptyMessage) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;", pp("__error();__error( );"));
    EXPECT_EQ(Strings({"PP91: ''", "PP91: ''"}), diags());
}

// E24: the closing-parenthesis line; a raw newline in the call is a space.
TEST_F(BuiltinDiagTest, ErrorMultiLineCallReportsClosingParenLine) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("a\n\n\nb", pp("a\n__error(x\ny)\nb"));
    EXPECT_EQ(Strings({"<unknown location>:3.0: error: PP91: 'x y'"}), messages());
}

// E24: inside a macro body, the closing-parenthesis line of the outer call.
TEST_F(BuiltinDiagTest, ErrorInsideMacroBodyReportsCallLine) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("\nx\n\n\nz", pp("{#define E() __error(in body)}\nx\nE(\n)\nz"));
    EXPECT_EQ(Strings({"<unknown location>:4.0: error: PP91: 'in body'"}), messages());
}

// ---- __assert ----

// E4: true is silent and the message is not expanded at all.
TEST_F(BuiltinDiagTest, AssertTrueIsSilentAndMessageNotExpanded) {
    EXPECT_EQ(" 0", pp("__assert(1, __COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}

// E5
TEST_F(BuiltinDiagTest, AssertFalseReportsExpandedMessage) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(" 1", pp("__assert(0, c=__COUNTER__) __COUNTER__"));
    EXPECT_EQ(Strings({"PP91: 'c=0'"}), diags());
}

// E6
TEST_F(BuiltinDiagTest, AssertWithoutMessageUsesDefaultText) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;", pp("__assert(1 = 2);__assert(0, );"));
    EXPECT_EQ(Strings({"PP91: 'static assertion failed due to requirement '1 = 2''",
                       "PP91: 'static assertion failed due to requirement '0''"}),
              diags());
}

// E3 for __assert
TEST_F(BuiltinDiagTest, AssertMessageWithCommas) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__assert(0, a, b)"));
    EXPECT_EQ(Strings({"PP91: 'a, b'"}), diags());
}

// E7: the {#if} evaluator -- an object macro expands during evaluation and
// an undefined identifier is 0.
TEST_F(BuiltinDiagTest, AssertExprUsesIfEvaluator) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;;", pp("{#define LIM 4}__assert(LIM = 4, a);__assert(LIM > 4 \\or\\ TRUE, b);"
                        "__assert(NOT_A_MACRO, c);"));
    EXPECT_EQ(Strings({"PP91: 'c'"}), diags());
}

// E7/M6: a parameter of the enclosing macro is substituted, in the message too.
TEST_F(BuiltinDiagTest, AssertOuterParameterSubstituted) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;", pp("{#define CHK(n) __assert(n <= 4, CHK: n too large)}CHK(4);CHK(5);"));
    EXPECT_EQ(Strings({"PP91: 'CHK: 5 too large'"}), diags());
}

// E7: __VA_ARGC__ is already a number when __assert runs.
TEST_F(BuiltinDiagTest, AssertWithVaArgc) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(" [1, 2]; [1,2,3,4,5]; [];",
              pp("{#define SERIES(...) __assert(__VA_ARGC__ >= 1 \\and\\ __VA_ARGC__ <= 4, "
                 "SERIES arity) [__VA_ARGS__]}SERIES(1, 2);SERIES(1,2,3,4,5);SERIES();"));
    EXPECT_EQ(Strings({"PP91: 'SERIES arity'", "PP91: 'SERIES arity'"}), diags());
}

// E8
TEST_F(BuiltinDiagTest, AssertDefinedOperatorWrittenDirectly) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define X}__assert(defined(X), m1)__assert(defined X, m2)"
                     "__assert(defined(Y), m3)__assert(\\not\\ defined(Y), m4)"));
    EXPECT_EQ(Strings({"PP91: 'm3'"}), diags());
}

// E8/F2: like {#if D}, a defined produced during evaluation is recognised.
TEST_F(BuiltinDiagTest, AssertDefinedFromExpansionLikeIf) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define D defined(X)}{#define X}__assert(D, m1){#undef X}__assert(D, m2)"));
    EXPECT_EQ(Strings({"PP91: 'm2'"}), diags());
}

// E8: an operand arriving through an outer parameter was pre-expanded
// (defined(1)): PP40, no PP91.
TEST_F(BuiltinDiagTest, AssertDefinedThroughParameterIsPreExpanded) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define REQ(n) __assert(defined(n), m)}{#define X 1}REQ(X)"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[0]);
}

// E9/M3: only the evaluator diagnostic, exactly one, no PP91.
TEST_F(BuiltinDiagTest, AssertInvalidExpressionReportsOnlyEvaluatorDiag) {
    Issue::ContinueMode guard({});
    auto only = [&](const std::string& src, Issue::Code expected) {
        EXPECT_EQ("", pp(src)) << src;
        auto cs = codes();
        ASSERT_EQ(1u, cs.size()) << src;
        EXPECT_EQ(expected, cs[0]) << src;
    };
    only("__assert(1 +, m)", Issue::Code::MISSING_EXPRESSION);
    only("__assert( , m)", Issue::Code::MISSING_EXPRESSION);
    only("__assert(1 x, m)", Issue::Code::INVALID_EXPRESSION);
    only("__assert(TRUE + 1, m)", Issue::Code::EXPR_TYPE_ERROR);
    only("__assert(defined, m)", Issue::Code::INVALID_DEFINED_OPERAND);
    only("__assert(defined(X Y), m)", Issue::Code::INVALID_EXPRESSION);
}

// E10: division by zero is PP52 and evaluation continues with 0.
TEST_F(BuiltinDiagTest, AssertDivisionByZeroDecidedByValue) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";", pp("__assert(1 / 0 = 0, m1);"));
    EXPECT_EQ(Strings({"PP52: Invalid expression; 'division by zero'"}), diags());
    EXPECT_EQ(";", pp("__assert(1 / 0, m2);"));
    EXPECT_EQ(Strings({"PP52: Invalid expression; 'division by zero'", "PP91: 'm2'"}), diags());
}

// E11
TEST_F(BuiltinDiagTest, BuiltinsInsideIfExpression) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("y", pp("{#if __error(x) 1}y{#endif}"));
    EXPECT_EQ(Strings({"PP91: 'x'"}), diags());
    EXPECT_EQ("w", pp("{#if __assert(0, z) 1}w{#endif}"));
    EXPECT_EQ(Strings({"PP91: 'z'"}), diags());
}

// E11/R1-C4: a nested evaluation keeps the outer condition's defined error.
TEST_F(BuiltinDiagTest, NestedAssertInIfKeepsOuterDefinedError) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("b", pp("{#if defined(1) \\or\\ __assert(1 = 1, m) 1}a{#else}b{#endif}"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[0]);
}

// E12: each operand that is macro-expanded fires once.
TEST_F(BuiltinDiagTest, BuiltinsFireInDirectiveOperands) {
    Issue::ContinueMode guard({});
    pp("{#token __error(t) T}|{#string __error(s) S}|{#warning w __error(m)}|{foo __error(p) bar}");
    EXPECT_EQ(Strings({"PP91: 't'", "PP91: 's'", "PP91: 'm'", "PP92: 'w '", "PP91: 'p'"}),
              diags());
}

// E12 (L4): {#include}/{#line}/{#wstring} operands; the diagnostic line is
// the line of the opening brace of a multi-line directive.
TEST_F(BuiltinDiagTest, BuiltinsInMoreOperandsReportDirectiveLine) {
    Issue::ContinueMode guard({});
    pp("\n{#wstring\n__error(w)\nabc}\n{#line __error(ln) 10}\nq\n{#include\n__error(inc) 'no_such_zz.iec'}");
    auto ms = messages();
    ASSERT_EQ(4u, ms.size());
    EXPECT_EQ("<unknown location>:2.0: error: PP91: 'w'", ms[0]);
    EXPECT_EQ("<unknown location>:5.0: error: PP91: 'ln'", ms[1]);
    EXPECT_EQ("<unknown location>:11.0: error: PP91: 'inc'", ms[2]);
    EXPECT_NE(std::string::npos, ms[3].find("PP11")) << ms[3];
}

// E13: a -D value fires at use only.
TEST_F(BuiltinDiagTest, DOptionValueFiresAtUseOnly) {
    Issue::ContinueMode guard({});
    Env env = setup({{"CHECK", "__error(fromD)"}});
    EXPECT_TRUE(empty());
    EXPECT_EQ("a  b", pp("a CHECK b", env));
    EXPECT_EQ(Strings({"PP91: 'fromD'"}), diags());
}

// E14
TEST_F(BuiltinDiagTest, NoEffectInDisabledGroup) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("ok", pp("{#if 0}__error(x)__assert(0, y){#elif 0}__error(z){#else}ok{#endif}"));
    EXPECT_EQ("a", pp("{#if 1}a{#elif __error(e)}b{#endif}"));
    EXPECT_TRUE(empty());
}

// E15
TEST_F(BuiltinDiagTest, NoEffectInsideRejectedCallArguments) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("F2", pp("{#define F2(a, b) a b}F2(__error(x))"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

// E16/F1: at most once per call; never for an unused parameter or a @ operand.
TEST_F(BuiltinDiagTest, ErrorInMacroArgumentExpandedAtMostOncePerCall) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(" ", pp("{#define TWICE(a) a a}TWICE(__error(x))"));
    EXPECT_EQ(Strings({"PP91: 'x'"}), diags());
    EXPECT_EQ("z", pp("{#define UNUSED(a) z}UNUSED(__error(y))"));
    EXPECT_EQ("'__error(w)'", pp("{#define STR(a) @a}STR(__error(w))"));
    EXPECT_TRUE(empty());
}

// E17
TEST_F(BuiltinDiagTest, NameWithoutParenIsPlainText) {
    EXPECT_EQ("__error; x := __assert;", pp("__error; x := __assert;"));
    EXPECT_TRUE(empty());
}

// E18
TEST_F(BuiltinDiagTest, UnterminatedOrEmptyCalls) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("a ", pp("a __error(x"));
    EXPECT_EQ(Strings({"PP34: Argument count mismatch; 'missing closing parenthesis'"}), diags());
    EXPECT_EQ(";", pp("__assert();"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, cs[0]);
}

// E19/E20: {#ifdef}/defined see the builtins; {#undef} removes them (with
// the PP35 built-in warning, M5) and the call is then plain text.
TEST_F(BuiltinDiagTest, IfdefSeesBuiltinsAndUndefRemovesThem) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("12", pp("{#ifdef __error}1{#endif}{#if defined(__assert)}2{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("4__error(x)", pp("{#undef __error}{#ifdef __error}3{#else}4{#endif}__error(x)"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__error: undefining built-in macro'"}), diags());
}

// E19
TEST_F(BuiltinDiagTest, RedefineBuiltinWarnsPP35) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("[1]", pp("{#define __error(x) [x]}__error(1)"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__error: redefining built-in macro'"}), diags());
}

#ifndef JIEPP_SANDBOX
// E22: {#ignore PP91} covers {#error}, __error and __assert; the message is
// still expanded (its side effects do not depend on the ignore state).
TEST_F(BuiltinDiagTest, IgnorePP91SuppressesBuiltinDiagnostics) {
    EXPECT_EQ("1", pp("{#ignore PP91}__error(a)__assert(0, b __COUNTER__)__COUNTER__{#error c}"));
    EXPECT_TRUE(empty());
}
#endif

// E23: library mode throws like {#error}.
TEST_F(BuiltinDiagTest, LibraryModeThrowsOnBuiltinError) {
    try {
        pp("__error(x) after");
        FAIL() << "expected Issue::Exception";
    } catch (const Issue::Exception& e) {
        EXPECT_EQ(Issue::Code::ERROR_MESSAGE, e.code);
    }
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
    EXPECT_THROW(pp("__assert(0, y)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

// E26: the inner __error of a failing message reports first.
TEST_F(BuiltinDiagTest, AssertNestedErrorInMessageOrder) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__assert(0, outer __error(inner))"));
    EXPECT_EQ(Strings({"PP91: 'inner'", "PP91: 'outer '"}), diags());
    EXPECT_EQ("", pp("__assert(1, outer __error(inner))"));
    EXPECT_TRUE(empty());
}

// E27: __error in the expression fires during evaluation.
TEST_F(BuiltinDiagTest, AssertErrorInsideExpr) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;", pp("__assert(__error(e1) 1, m1);__assert(__error(e2) 0, m2);"));
    EXPECT_EQ(Strings({"PP91: 'e1'", "PP91: 'e2'", "PP91: 'm2'"}), diags());
}

// E28: a state directive in the arguments runs once at collection; a
// control directive there is PP02.
TEST_F(BuiltinDiagTest, DirectivesInsideBuiltinArguments) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(" 1", pp("__error(a {#define Q 1} Q) Q"));
    EXPECT_EQ(Strings({"PP91: 'a  1'"}), diags());
    EXPECT_THROW(pp("__error({#if 1} x)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

// E29: the hide set survives (one PP91, no PP60).
TEST_F(BuiltinDiagTest, AssertExprKeepsHideSet) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define N __assert(N, self)}N"));
    EXPECT_EQ(Strings({"PP91: 'self'"}), diags());
}

// E30: the expression costs one step per byte (like an {#if} operand).
TEST_F(BuiltinDiagTest, AssertExpressionChargesExpansionSteps) {
    const std::string spaces(4000, ' ');
    {
        Env env = setup();
        env.set_max_expansion_steps(2000);
        EXPECT_EQ("", pp("__assert(1 = 1, m)", env));
        EXPECT_TRUE(empty());
    }
    {
        Env env = setup();
        env.set_max_expansion_steps(2000);
        EXPECT_THROW(pp("__assert(1" + spaces + "= 1, m)", env), Issue::Exception);
        EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
    }
}

// Watchpoint: commas inside [...] do not split arguments.
TEST_F(BuiltinDiagTest, WatchpointBracketArgumentNotSplit) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define F(x) __error(x)}F(a[x,y])"));
    EXPECT_EQ(Strings({"PP91: 'a[x,y]'"}), diags());
    EXPECT_EQ("", pp("{#define G(p, q) __error(p | q)}G(a[x,y], b[1,2])"));
    EXPECT_EQ(Strings({"PP91: 'a[x,y] | b[1,2]'"}), diags());
    EXPECT_EQ("", pp("__error(a[x,y])__assert(0, a[1,2])"));
    EXPECT_EQ(Strings({"PP91: 'a[x,y]'", "PP91: 'a[1,2]'"}), diags());
}

// Watchpoint: an identical redefinition (different whitespace amount) of a
// macro whose body uses __assert stays silent.
TEST_F(BuiltinDiagTest, WatchpointIdenticalRedefinitionSilent) {
    EXPECT_EQ(";", pp("{#define C(n) __assert(n > 0, bad n)}"
                      "{#define C(n)  __assert(n > 0,   bad n)}C(1);"));
    EXPECT_TRUE(empty());
    Env env = setup({{"K", "__assert(1, k)"}, {"K", "__assert(1, k)"}});
    EXPECT_TRUE(empty());
}

// Watchpoint (L4): nested brackets.
TEST_F(BuiltinDiagTest, WatchpointNestedBracketArgument) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define F(x) __error(x)}F(a[b[1,2], c[3,4]])"));
    EXPECT_EQ(Strings({"PP91: 'a[b[1,2], c[3,4]]'"}), diags());
}

// M1: literal errors raised inside constfold actions are failures.
TEST_F(BuiltinDiagTest, AssertLiteralErrorsAreFailures) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__assert(99999999999999999999 > 0, m)"));
    EXPECT_EQ(Strings({"PP52: Invalid expression; 'integer literal overflow: 99999999999999999999'"}),
              diags());
    EXPECT_EQ("", pp("__assert(2#102 = 2, m)"));
    EXPECT_EQ(Strings({"PP52: Invalid expression; 'invalid digit for base: 2#102'"}), diags());
}

#ifndef JIEPP_SANDBOX
// M2: a suppressed evaluator diagnostic does not make the assertion pass.
TEST_F(BuiltinDiagTest, AssertWithSuppressedEvaluatorDiagStillFails) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#ignore PP52}__assert(1 x, m)"));
    EXPECT_EQ(Strings({"PP91: 'm'"}), diags());
    // The first failure (a literal error, PP52) suppressed, a later one
    // (PP50) shown: PP91 is reported as well; nothing passes silently.
    EXPECT_EQ("", pp("__assert(99999999999999999999 + TRUE, m2)"));
    EXPECT_EQ(Strings({"PP50: Type error in expression", "PP91: 'm2'"}), diags());
    EXPECT_EQ("", pp("{#ignore PP51}__assert(1 +, m)"));
    EXPECT_EQ(Strings({"PP91: 'm'"}), diags());
    EXPECT_EQ("", pp("{#ignore PP50}__assert(TRUE + 1, m)"));
    EXPECT_EQ(Strings({"PP91: 'm'"}), diags());
    EXPECT_EQ("", pp("{#ignore PP40}__assert(defined 1, m)"));
    EXPECT_EQ(Strings({"PP91: 'm'"}), diags());
    // Without a message: the E6 default text.
    EXPECT_EQ("", pp("__assert(1 x)"));
    EXPECT_EQ(Strings({"PP91: 'static assertion failed due to requirement '1 x''"}), diags());
}
#endif

// M4/E31: the word defined in a message is plain text inside {#if}.
TEST_F(BuiltinDiagTest, DefinedInMessageIsPlainTextInsideIf) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("a", pp("{#if __error(X must be defined) 1}a{#endif}"));
    EXPECT_EQ(Strings({"PP91: 'X must be defined'"}), diags());
    EXPECT_EQ("b", pp("{#if __assert(0, need defined X) 1}b{#endif}"));
    EXPECT_EQ(Strings({"PP91: 'need defined X'"}), diags());
    // Outside a condition, defined in a message was never an operator.
    EXPECT_EQ("", pp("__error(is defined)"));
    EXPECT_EQ(Strings({"PP91: 'is defined'"}), diags());
}

// Round-2 note: an __assert nested in a message inside {#if} gets the
// defined operator back for its own expression, and the suspension and the
// outer condition's state are restored afterwards.
TEST_F(BuiltinDiagTest, NestedAssertInMessageReenablesDefined) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("c", pp("{#define Z}{#if __error(outer __assert(defined(Y), inner) "
                      "__assert(defined(Z), never) defined) 1}c{#endif}"));
    EXPECT_EQ(Strings({"PP91: 'inner'", "PP91: 'outer   defined'"}), diags());
}

// M5: undefining or redefining a built-in macro warns PP35; predefined value
// macros do not get the built-in warning.
TEST_F(BuiltinDiagTest, UndefOrRedefineBuiltinWarns) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("__assert(0, m)", pp("{#undef __assert}__assert(0, m)"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__assert: undefining built-in macro'"}), diags());
    EXPECT_EQ("1", pp("{#define __error 1}__error"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__error: redefining built-in macro'"}), diags());
    pp("{#undef __COUNTER__}{#undef __LINE__}{#define __FILE__ 1}");
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__COUNTER__: undefining built-in macro'",
                       "PP35: Macro redefined; '__LINE__: undefining built-in macro'",
                       "PP35: Macro redefined; '__FILE__: redefining built-in macro'"}),
              diags());
    pp("{#undef __INT_MAX__}{#undef _JIEPP}{#undef NEVER_DEFINED}{#define __DINT_MAX__ 2147483647}");
    EXPECT_TRUE(empty());
}

// M7/E32: quotes start IEC string literals inside the arguments.
TEST_F(BuiltinDiagTest, QuotesInArgumentsFollowLiteralRules) {
    Issue::ContinueMode guard({});
    // Nothing is output after the call; only its line count is kept.
    EXPECT_EQ("\n\n\n", pp("__error(can't happen)\nmore\nlines\n"));
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[1]);
    EXPECT_EQ("", pp("__error('x')"));
    EXPECT_EQ(Strings({"PP91: ''x''"}), diags());
}

// L2: the clang default text, also when the message expands to nothing;
// EXPR is the expression as written with whitespace collapsed.
TEST_F(BuiltinDiagTest, AssertDefaultTextMatchesClang) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__assert(1 = 2)"));
    EXPECT_EQ(Strings({"PP91: 'static assertion failed due to requirement '1 = 2''"}), diags());
    EXPECT_EQ("", pp("{#define EMPTY}__assert(0, EMPTY)"));
    EXPECT_EQ(Strings({"PP91: 'static assertion failed due to requirement '0''"}), diags());
    EXPECT_EQ("\n", pp("__assert( 1   =\n 2 )"));
    EXPECT_EQ(Strings({"PP91: 'static assertion failed due to requirement '1 = 2''"}), diags());
}

// L4: an outer-parameter message was pre-expanded once at substitution,
// even when the assertion holds and the message is not used.
TEST_F(BuiltinDiagTest, OuterParameterMessageIsPreExpandedOnce) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(" 1", pp("{#define C(a, m) __assert(a, m)}C(0, __COUNTER__) __COUNTER__"));
    EXPECT_EQ(Strings({"PP91: '0'"}), diags());
    EXPECT_EQ(" 1", pp("{#define C(a, m) __assert(a, m)}C(1, __COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}
