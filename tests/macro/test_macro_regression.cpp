#include "test_helper.hpp"

// Regression guards for two behaviors the user checks after every macro
// change: (A) an identical redefinition of a macro stays silent (no PP35)
// while a genuinely different redefinition still warns, and (B) commas
// inside `[...]` (IEC multi-dimensional array subscripts) do not split a
// function-macro argument.

class MacroRegressionTest : public JieppTest {};

// ---- A: identical redefinition is silent; different definition warns ----

TEST_F(MacroRegressionTest, IdenticalRedefinitionSilent) {
    EXPECT_EQ(";;1;", pp("{#define A 1};{#define A 1};A;"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, IdenticalRedefinitionSilentVariadicParamWhitespace) {
    EXPECT_EQ(";;1 2;",
              pp("{#define G(x, ...) x __VA_ARGS__};{#define G(x,...) x __VA_ARGS__};G(1,2);"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, IdenticalRedefinitionSilentBodyWithComment) {
    EXPECT_EQ(";;", pp("{#define C 1 (* c *)};{#define C 1 (* c *)};"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, IdenticalRedefinitionSilentEmptyBody) {
    EXPECT_EQ(";;", pp("{#define E};{#define E};"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, IdenticalRedefinitionSilentDashDTwice) {
    Env env = setup({{"A", "1"}, {"A", "1"}});
    EXPECT_TRUE(empty());
    EXPECT_EQ("[1];", pp("[A];", env));
}

TEST_F(MacroRegressionTest, IdenticalRedefinitionSilentDashDThenDefine) {
    Env env = setup({{"X", "1"}});
    EXPECT_EQ("1;", pp("{#define X 1}X;", env));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, BuiltinRedefinedWithSameValueSilentDifferentValueWarns) {
    EXPECT_EQ("32767;", pp("{#define __INT_MAX__ 32767}__INT_MAX__;"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("", pp("{#define __INT_MAX__ 1}"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
}

// C17 6.10.3p2: a renamed parameter makes the replacement lists non-identical
// (the substituted identifiers differ token-for-token), so this is a
// different definition, not an identical one. gcc warns here; jiepp follows
// gcc (see RedefineIdenticalModuloWhitespaceAmount in test_macro.cpp for the
// companion whitespace-only rule).
TEST_F(MacroRegressionTest, RenamedParameterIsDifferentDefinition) {
    pp("{#define F(x) [x]};{#define F(y) [y]};");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, cs[0]);
}

// ---- B: IEC multi-dimensional array arguments pass intact ----

TEST_F(MacroRegressionTest, ArrayArgumentCommasDoNotSplitVariadicArgs) {
    EXPECT_EQ(";<2:a[1,2], b>;",
              pp("{#define V(...) <__VA_ARGC__:__VA_ARGS__>};V(a[1,2], b);"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, ArrayArgumentWithNestedMacroCall) {
    EXPECT_EQ(";<a[<x>, y]>;", pp("{#define F(v) <v>};F(a[F(x), y]);"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, ArrayArgumentWithNestedParentheses) {
    EXPECT_EQ(";<a[(x,y)]>;", pp("{#define F(v) <v>};F(a[(x,y)]);"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, ArrayArgumentCommasDoNotSplitTwoParamArgs) {
    EXPECT_EQ(";<m[i,j] + n[k,l]|f(u, v)>;",
              pp("{#define G(p, q) <p|q>};G(m[i,j] + n[k,l], f(u, v));"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroRegressionTest, ArrayArgumentKeepsInnerSpaces) {
    EXPECT_EQ(";<a[x, y, z]>;", pp("{#define F(v) <v>};F(a[x, y, z]);"));
    EXPECT_TRUE(empty());
}
