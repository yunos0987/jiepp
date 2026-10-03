#include "test_helper.hpp"

// task_slug arg-expand-once: a function-macro actual argument must be
// macro-expanded at most once per invocation, and the single expansion
// result (including any diagnostics/side effects from it) must be shared
// by every occurrence of the corresponding formal parameter -- C17
// 6.10.3.1p1, like clang/gcc. Before the fix, src/core/expand_subst.cpp's
// subst() called expand(actual, result, env) once PER OCCURRENCE, so
// __COUNTER__ incremented once per occurrence and PP34 (reported while
// expanding a malformed actual) fired once per occurrence too. See
// design-arg-once.md section 6 for the T1-T26 table this file implements.

class ArgExpandOnceTest : public JieppTest {};

// ---- T1-T8: plain multi-use parameters (no variadic) ----

TEST_F(ArgExpandOnceTest, T1_SimpleDoubleUse) {
    EXPECT_EQ("0 0", pp("{#define F(a) a a}F(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T2_ActualIsObjectMacro) {
    EXPECT_EQ("0 0", pp("{#define F(a) a a}{#define G __COUNTER__}F(G)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T3_NestedFunctionMacroSharesOuterExpansion) {
    EXPECT_EQ("0 0 0 0",
              pp("{#define F(a) a a}{#define G2(b) F(b) F(b)}G2(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T4_DeeplyNestedDoublingChain) {
    EXPECT_EQ("0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0",
              pp("{#define D(a) a a}D(D(D(D(__COUNTER__))))"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T5_StringizeOperandPlusTwoPlainUses) {
    // @a stringizes the RAW (unexpanded) actual; the two plain uses of a
    // share one expansion.
    EXPECT_EQ("'__COUNTER__' 0 0", pp("{#define S(a) @a a a}S(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T6_GlueOperandsPlusTwoPlainUses) {
    // a@@x pastes the RAW actual; the two plain uses of a share one
    // expansion.
    EXPECT_EQ("__COUNTER__x 0 0", pp("{#define Q(a) a@@x a a}Q(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T7_GlueBothSidesPlusOnePlainUse_Guard) {
    // Guard: both @@ operands use the raw actual (never expanded); only
    // the last, non-glue-adjacent "a" expands -- already correct pre-fix.
    EXPECT_EQ("__COUNTER____COUNTER__ 0", pp("{#define T(a) a@@a a}T(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T8_TwoIndependentDoubleUseParameters) {
    EXPECT_EQ("0 1 0 1",
              pp("{#define H(a, b) a b a b}H(__COUNTER__, __COUNTER__)"));
    EXPECT_TRUE(empty());
}

// ---- T9-T14: variadic parameters, __VA_OPT__, __VA_ARGC__ ----

TEST_F(ArgExpandOnceTest, T9_VariadicDoubleUse) {
    EXPECT_EQ("0, 1 | 0, 1",
              pp("{#define V(...) __VA_ARGS__ | __VA_ARGS__}V(__COUNTER__, __COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T10_VaOptContentSharesPlainUse) {
    EXPECT_EQ("0 | 0",
              pp("{#define W(...) __VA_OPT__(__VA_ARGS__) | __VA_ARGS__}W(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T11_StringizedVaOptContentSharesPlainUse) {
    EXPECT_EQ("'0' | 0",
              pp("{#define X(...) @__VA_OPT__(__VA_ARGS__) | __VA_ARGS__}X(__COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T12_SharedVariadicSpellingKeepsR6Spacing_Guard) {
    // Guard: the stringized __VA_OPT__ content (R6: a newline counts as
    // whitespace) and the plain use (clang LeadingSpace: a newline directly
    // before the token is not a space) must still differ in whitespace even
    // though they share one underlying expansion (D3: a single
    // va_sep-tagged spelling, filtered per consumer).
    EXPECT_EQ("'a , b' | a, b\n",
              pp("{#define X(...) @__VA_OPT__(__VA_ARGS__) | __VA_ARGS__}X(a\n, b)"));
    // Same-whitespace input: both consumers agree.
    EXPECT_EQ("'a , b' | a , b",
              pp("{#define X(...) @__VA_OPT__(__VA_ARGS__) | __VA_ARGS__}X(a , b)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T13_NamedVariadicDoubleUsePlusArgc) {
    EXPECT_EQ("0, 1 | 2 | 0, 1",
              pp("{#define NV(args...) args | __VA_ARGC__ | args}"
                 "NV(__COUNTER__, __COUNTER__)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T14_RegularParamUsedInVaOptAndPlain) {
    EXPECT_EQ("0 | 0",
              pp("{#define N(a, ...) __VA_OPT__(a) | a}N(__COUNTER__, 1)"));
    EXPECT_TRUE(empty());
}

// ---- T15-T16: unused / stringize-only / VA_OPT-false parameters never expand ----

TEST_F(ArgExpandOnceTest, T15_VaOptFalseNeverExpandsItsArgument_Guard) {
    // O(a, ...) with __VA_OPT__(a) and no variadic actuals never
    // substitutes a at all, so the malformed P(2,3) argument is never
    // expanded and never diagnosed (clang: lazy).
    EXPECT_EQ("", pp("{#define P(x) x+1}{#define O(a, ...) __VA_OPT__(a)}O(P(2,3))"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T16_UnusedAndStringizeOnlyParametersNeverExpand_Guard) {
    EXPECT_EQ("1", pp("{#define P(x) x+1}{#define U(a) 1}U(P(2,3))"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'P(2,3)'", pp("{#define P(x) x+1}{#define Z(a) @a}Z(P(2,3))"));
    EXPECT_TRUE(empty());
}

// ---- T17-T18: a diagnostic raised while expanding an actual fires once ----

TEST_F(ArgExpandOnceTest, T17_DiagnosticFiresOnceForDoubleUseParameter) {
    Issue::ContinueMode guard({});
    pp("{#define P(x) x+1}{#define F(a) a a}F(P(2,3))");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

TEST_F(ArgExpandOnceTest, T18_DiagnosticFiresOnceAcrossVaOptAndPlainUse) {
    Issue::ContinueMode guard({});
    pp("{#define P(x) x+1}"
       "{#define X(...) @__VA_OPT__(__VA_ARGS__) | __VA_ARGS__}X(P(2,3))");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

// ---- T19-T22: hide sets, directives-in-arguments, bracketed arguments ----

TEST_F(ArgExpandOnceTest, T19_SelfReferentialHideSet_Guard) {
    EXPECT_EQ("B B", pp("{#define A(x) x x}{#define B A(B)}B"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T20_ReinvocationAcrossMacroBoundary_Guard) {
    EXPECT_EQ("2*9*g",
              pp("{#define f(a) a*g}{#define g(a) f(a)}f(2)(9)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("g 1*g",
              pp("{#define F(a) a a}{#define f(a) a*g}{#define g(a) f(a)}F(g)(1)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("1*g 1*g",
              pp("{#define F(a) a a}{#define f(a) a*g}{#define g(a) f(a)}F(f(1))"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T21_DirectiveInArgumentExecutedOnce_Guard) {
    EXPECT_EQ("1 1", pp("{#define F(a) a a}F({#define XX 1}XX)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgExpandOnceTest, T22_BracketedArgumentNotSplitOnComma_Guard) {
    EXPECT_EQ("a[x,y] a[x,y]", pp("{#define F(a) a a}F(a[x,y])"));
    EXPECT_TRUE(empty());
}

// ---- T23: line-number fidelity for a double-use parameter ----

TEST_F(ArgExpandOnceTest, T23_LineFidelityForDoubleUseParameter_Guard) {
    // A function-macro call spanning two physical lines, with a
    // double-use parameter: both copies of the replacement must reflect
    // the closing ')' line (gcc/clang rule, unaffected by memoisation),
    // and the call's own newline is still re-emitted exactly once.
    EXPECT_EQ(";c + d c + d\n;2",
              pp("{#define F(a) a a};F(c + d\n);{#info}__LINE__"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_NE(std::string::npos, msgs[0].find("2.0"));
}

// ---- T24: placemarker path (empty actual adjacent to @@) is untouched ----

TEST_F(ArgExpandOnceTest, T24_PlacemarkerPathUntouched_Guard) {
    // The trailing space comes from the body's "x@@a a" WS token between
    // the glue and the second (empty) "a"; unrelated to this fix (D1/D2
    // never touch the placemarker path) and unchanged from 5382fab.
    EXPECT_EQ("x ", pp("{#define E(a) x@@a a}E()"));
    EXPECT_TRUE(empty());
}

// ---- T25: expansion-depth guard semantics unaffected ----

TEST_F(ArgExpandOnceTest, T25_ExpansionDepthGuardStillThrowsAndRestores_Guard) {
    // J(a) a a -- a double-use parameter -- wrapping a chain of I(a) (a)
    // calls three deep: must still exceed a max_expansion_depth of 3 and
    // restore expansion_depth() to 0 after the throw (nesting depth is
    // unaffected by memoisation, only the number of expand() calls is).
    Env env = setup();
    env.set_max_expansion_depth(3);
    EXPECT_THROW(
        pp("{#define I(a) (a)}{#define J(a) a a}J(I(I(I(0))))", env),
        Issue::Exception);
    EXPECT_EQ(0, env.expansion_depth());
}

// ---- T26: identical redefinition of a double-use-parameter macro stays silent ----
// (watchpoint; see also FuncMacroTest redefinition tests in
// tests/macro/test_functionmacro.cpp, which this references by name.)

TEST_F(ArgExpandOnceTest, T26_IdenticalRedefinitionStaysSilent_Guard) {
    EXPECT_EQ("", pp("{#define F(a) a a}{#define F(a)  a  a}"));
    EXPECT_TRUE(empty());
}
