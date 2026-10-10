#include "test_helper.hpp"

#include <string>
#include <vector>

// =====================================================================
// R2: the named counters __counter(name), __counter_peek(name) and
// __counter_set(name, value) (SPECIFICATION.md 12.10). The numbers N1-N20
// refer to the behaviour table of the implementation plan.
// =====================================================================

class NamedCounterTest : public JieppTest {
protected:
    // Each diagnostic line without its location prefix, e.g. "PP39: ...".
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
using Codes = std::vector<Issue::Code>;

namespace {
std::string pp39(const std::string& ctx) {
    return "PP39: Invalid operand for built-in macro; '" + ctx + "'";
}
} // namespace

// N1
TEST_F(NamedCounterTest, CounterStartsAtZeroAndIncrements) {
    EXPECT_EQ("0 1 2", pp("__counter(a) __counter(a) __counter(a)"));
    EXPECT_TRUE(empty());
}

// N1/N6: independent per name, case-sensitive.
TEST_F(NamedCounterTest, CountersAreIndependentPerName) {
    EXPECT_EQ("0 0 1 1 2 0",
              pp("__counter(a) __counter(b) __counter(a) __counter(b) __counter(a) __counter(A)"));
    EXPECT_TRUE(empty());
}

// N2
TEST_F(NamedCounterTest, PeekDoesNotIncrement) {
    EXPECT_EQ("0 0 0 1 1",
              pp("__counter_peek(a) __counter_peek(a) __counter(a) __counter_peek(a) __counter_peek(a)"));
    EXPECT_EQ("0", pp("__counter_peek(never_used)"));
    EXPECT_TRUE(empty());
}

// N3: expands to nothing; name check, then value evaluation, then store.
TEST_F(NamedCounterTest, SetChangesValueAndExpandsToNothing) {
    EXPECT_EQ("[]10 11", pp("[__counter_set(a, 10)]__counter(a) __counter(a)"));
    EXPECT_EQ("10", pp("__counter_set(a, __counter(a) + 10)__counter_peek(a)"));
    EXPECT_TRUE(empty());
}

// N7: the value uses the {#if} evaluator.
TEST_F(NamedCounterTest, SetValueUsesIfEvaluator) {
    EXPECT_EQ("10", pp("{#define BASE 5}__counter_set(a, BASE * 2)__counter_peek(a)"));
    EXPECT_EQ("1", pp("__counter_set(a, TRUE)__counter_peek(a)"));
    EXPECT_EQ("015", pp("__counter(b)__counter(b)__counter_set(a, __counter(b) + 3)__counter_peek(a)"));
    EXPECT_EQ("4", pp("__counter_set(a, UNDEFINED_X + 4)__counter_peek(a)"));
    EXPECT_EQ("1", pp("{#define X}__counter_set(a, defined(X))__counter_peek(a)"));
    EXPECT_TRUE(empty());
}

// N7: out of range is PP39 and leaves the counter unchanged.
TEST_F(NamedCounterTest, SetValueOutOfRangeIsPP39) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("3", pp("__counter_set(a, 3)__counter_set(a, -1)__counter_set(a, 2147483648)"
                      "__counter_peek(a)"));
    EXPECT_EQ(Strings({pp39("__counter_set: value out of range 0..2147483647: -1"),
                       pp39("__counter_set: value out of range 0..2147483647: 2147483648")}),
              diags());
}

// N7: an unevaluable value reports only the evaluator diagnostic.
TEST_F(NamedCounterTest, SetInvalidExpressionLeavesCounterUnchanged) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("3", pp("__counter_set(a, 3)__counter_set(a, 1 +)__counter_set(a, )"
                      "__counter_set(a, TRUE + 1)__counter_set(a, 1 x)__counter_peek(a)"));
    EXPECT_EQ(Codes({Issue::Code::MISSING_EXPRESSION, Issue::Code::MISSING_EXPRESSION,
                     Issue::Code::EXPR_TYPE_ERROR, Issue::Code::INVALID_EXPRESSION}),
              codes());
}

// N4: the name must be exactly one identifier; nothing is output, and the
// value of __counter_set is then not evaluated.
TEST_F(NamedCounterTest, NameMustBeIdentifier) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__counter()__counter(1)__counter(a b)__counter('s')__counter(a[x,y])"
                     "__counter((*! d *) a)__counter_peek( )"));
    EXPECT_EQ(Strings({pp39("__counter: counter name missing"),
                       pp39("__counter: counter name must be an identifier: 1"),
                       pp39("__counter: counter name must be an identifier: a b"),
                       pp39("__counter: counter name must be an identifier: 's'"),
                       pp39("__counter: counter name must be an identifier: a[x,y]"),
                       pp39("__counter: counter name must be an identifier: (*! d *) a"),
                       pp39("__counter_peek: counter name missing")}),
              diags());
    EXPECT_EQ("0", pp("__counter_set(1, __counter(z))__counter_peek(z)"));
    EXPECT_EQ(Strings({pp39("__counter_set: counter name must be an identifier: 1")}), diags());
}

// N4: ordinary comments around the name are whitespace.
TEST_F(NamedCounterTest, NameMayHaveComments) {
    EXPECT_EQ("0 1", pp("__counter((* c *) a /* d */) __counter_peek( (* e *) a )"));
    EXPECT_TRUE(empty());
}

// N5: the name is not macro-expanded.
TEST_F(NamedCounterTest, NameIsNotMacroExpanded) {
    EXPECT_EQ("0 1 0 2", pp("{#define a b}__counter(a) __counter(a) __counter_peek(b) __counter_peek(a)"));
    EXPECT_TRUE(empty());
}

// N5 caveat: through an outer parameter the actual was pre-expanded.
TEST_F(NamedCounterTest, NameThroughParameterIsPreExpanded) {
    EXPECT_EQ("0 1 0", pp("{#define N(x) __counter(x)}{#define k z}N(k) __counter_peek(z) __counter_peek(k)"));
    EXPECT_TRUE(empty());
}

// N6: counter names are their own namespace.
TEST_F(NamedCounterTest, CounterNamespaceIsSeparateFromMacros) {
    EXPECT_EQ("0 9 1 n 0n",
              pp("{#define c 9}__counter(c) c {#undef c}__counter(c) {#ifdef c}y{#else}n{#endif} "
                 "__counter(d){#ifdef d}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

// N9: arity PP34 with the usual recovery (name output, arguments dropped
// unexpanded).
TEST_F(NamedCounterTest, WrongArgumentCountIsPP34) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("__counter __counter_peek __counter_set __counter_set 0",
              pp("__counter(a, b) __counter_peek(a, b) __counter_set(a) __counter_set(a, 1, 2) "
                 "__counter_peek(a)"));
    EXPECT_EQ(Codes(4, Issue::Code::ARGUMENT_COUNT_MISMATCH), codes());
}

// N8: overflow saturates at 2147483647 with PP53, like __COUNTER__.
TEST_F(NamedCounterTest, OverflowSaturatesWithPP53) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("2147483647 2147483647 2147483647 2147483647",
              pp("__counter_set(a, 2147483647)__counter(a) __counter(a) __counter(a) __counter_peek(a)", env));
    EXPECT_EQ(Strings({"PP53: Counter value exceeds 2147483647; '__counter(a)'",
                       "PP53: Counter value exceeds 2147483647; '__counter(a)'",
                       "PP53: Counter value exceeds 2147483647; '__counter_peek(a)'"}),
              diags());
    // Only __counter_set recovers.
    EXPECT_EQ("5", pp("__counter_set(a, 5)__counter(a)", env));
    EXPECT_TRUE(empty());
}

// N10: disabled group and skipped {#elif}.
TEST_F(NamedCounterTest, NoEffectInDisabledGroup) {
    EXPECT_EQ("y1", pp("{#if 0}__counter(a)__counter_set(a, 9){#elif __counter(a)}x{#else}y{#endif}"
                       "__counter_peek(a)"));
    EXPECT_EQ("t0", pp("{#if 1}t{#elif __counter(b)}u{#else}__counter(b){#endif}__counter_peek(b)"));
    EXPECT_TRUE(empty());
}

// N10: the arguments of a PP34-rejected call are dropped unexpanded.
TEST_F(NamedCounterTest, NoIncrementInsideRejectedCallArguments) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("F2 0", pp("{#define F2(a, b) a b}F2(__counter(a)) __counter(a)"));
    EXPECT_EQ(Codes({Issue::Code::ARGUMENT_COUNT_MISMATCH}), codes());
}

// N11/F1: an actual argument is expanded at most once per call; never for an
// unused parameter or a @ operand; a body that spells the call twice counts
// twice.
TEST_F(NamedCounterTest, ArgumentExpandedAtMostOncePerCall) {
    EXPECT_EQ("0 0 z '__counter(c)' 1 2 3",
              pp("{#define T(a) a a}{#define U(a) z}{#define S(a) @a}"
                 "{#define TWO() __counter(c) __counter(c)}"
                 "T(__counter(c)) U(__counter(c)) S(__counter(c)) TWO() __counter_peek(c)"));
    EXPECT_TRUE(empty());
}

// N12/N20: a @@ operand is the raw call text; an intermediate macro relays it.
TEST_F(NamedCounterTest, GluedOperandNeedsRelay) {
    EXPECT_EQ("edge_0 edge_1 slot___counter(s) 0",
              pp("{#define CAT(a,b) a@@b}{#define CAT_X(a,b) CAT(a,b)}"
                 "CAT_X(edge_, __counter(e)) CAT_X(edge_, __counter(e)) "
                 "CAT(slot_, __counter(s)) __counter_peek(s)"));
    EXPECT_TRUE(empty());
}

// N13: once per evaluated condition.
TEST_F(NamedCounterTest, UsableInIfExpression) {
    EXPECT_EQ("AB2y", pp("{#if __counter(a) = 0}A{#else}B{#endif}{#if __counter(a) = 0}A{#else}B{#endif}"
                         "__counter_peek(a){#if __counter_peek(a) = 2}y{#endif}"));
    EXPECT_TRUE(empty());
}

// N14: the state lives as long as the Env.
TEST_F(NamedCounterTest, StateIsPerEnv) {
    Env env = setup();
    EXPECT_EQ("0", pp("__counter(a)", env));
    EXPECT_EQ("1", pp("__counter(a)", env));
    EXPECT_EQ(2u, env.counter_value("a"));
    Env fresh = setup();
    EXPECT_EQ("0", pp("__counter(a)", fresh));
    EXPECT_TRUE(empty());
}

// N16: {#ifdef}/defined see the built-ins; {#undef} removes them.
TEST_F(NamedCounterTest, IfdefSeesCounterBuiltins) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("123", pp("{#ifdef __counter}1{#endif}{#if defined(__counter_peek)}2{#endif}"
                        "{#ifndef __counter_set}n{#else}3{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("n__counter(a)", pp("{#undef __counter}{#ifdef __counter}y{#else}n{#endif}__counter(a)"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__counter: undefining built-in macro'"}), diags());
}

// N17
TEST_F(NamedCounterTest, CounterMacroUnaffected) {
    EXPECT_EQ("0 0 1 1", pp("__COUNTER__ __counter(a) __COUNTER__ __counter(a)"));
    EXPECT_TRUE(empty());
}

// N19: diagnostics at the closing-parenthesis line of the call.
TEST_F(NamedCounterTest, DiagnosticsAtClosingParenLine) {
    Issue::ContinueMode guard({});
    pp("x\n__counter(\n1\n)\n__counter_set(a,\n2147483647)__counter(a)__counter(\na\n)\ny");
    EXPECT_EQ(Strings({"<unknown location>:4.0: error: " +
                           pp39("__counter: counter name must be an identifier: 1"),
                       "<unknown location>:8.0: error: PP53: Counter value exceeds 2147483647; "
                       "'__counter(a)'"}),
              messages());
}

// Watchpoint: a [...] argument is not split at its commas.
TEST_F(NamedCounterTest, WatchpointBracketArgumentNotSplit) {
    EXPECT_EQ("a[x,y] 0 a[b[1,2], c[3,4]] 1",
              pp("{#define P(x) x __counter(c)}P(a[x,y]) P(a[b[1,2], c[3,4]])"));
    EXPECT_TRUE(empty());
}

// Watchpoint: an identical redefinition (different whitespace amount) of a
// macro using __counter is silent, in source and through -D.
TEST_F(NamedCounterTest, WatchpointIdenticalRedefinitionSilent) {
    // The later (equal) definition is the one in effect, spacing included.
    EXPECT_EQ("k   0", pp("{#define C(n) n __counter(n)}{#define C(n)  n   __counter(n)}C(k)"));
    EXPECT_TRUE(empty());
    Env env = setup({{"K", "__counter(k)"}, {"K", "__counter(k)"}, {"X", "1"}, {"X", "1"}});
    EXPECT_TRUE(empty());
    EXPECT_EQ("0 1", pp("K X", env));
    EXPECT_TRUE(empty());
}

// N7 (H1): the typed value is stored, never the 0/1 truth value.
TEST_F(NamedCounterTest, SetValueKinds) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("10 10 255 1 1 2147483647",
              pp("__counter_set(a, 10)__counter_peek(a) "
                 "__counter_set(a, -1)__counter_set(a, 2147483648)__counter_set(a, 2.5)__counter_peek(a) "
                 "__counter_set(a, BYTE#16#FF)__counter_peek(a) __counter_set(a, TRUE)__counter_peek(a) "
                 "__counter_set(a, LWORD#16#FFFFFFFFFFFFFFFF)__counter_peek(a) "
                 "__counter_set(a, 2147483647)__counter_peek(a)"));
    EXPECT_EQ(Strings({pp39("__counter_set: value out of range 0..2147483647: -1"),
                       pp39("__counter_set: value out of range 0..2147483647: 2147483648"),
                       "PP50: Type error in expression; '__counter_set: value must be an integer: 2.5'",
                       pp39("__counter_set: value out of range 0..2147483647: 18446744073709551615")}),
              diags());
}

// N7 (M1): literal errors raised inside constfold actions are failures.
TEST_F(NamedCounterTest, SetLiteralErrorsLeaveCounterUnchanged) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("3", pp("__counter_set(a, 3)__counter_set(a, 99999999999999999999)"
                      "__counter_set(a, 2#102)__counter_peek(a)"));
    EXPECT_EQ(Codes(2, Issue::Code::INVALID_EXPRESSION), codes());
}

#ifndef JIEPP_SANDBOX
// N7 (M2): a suppressed evaluator diagnostic still fails, with PP39. Division
// by zero is not a failure, so under {#ignore PP52} it is silent and stored.
TEST_F(NamedCounterTest, SetWithSuppressedDiagReportsPP39) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("3", pp("{#ignore PP52}__counter_set(a, 3)__counter_set(a, 1 x)__counter_peek(a)"));
    EXPECT_EQ(Strings({pp39("__counter_set: value could not be evaluated: 1 x")}), diags());
    // {#ignore} persists across pp() calls within one test: PP52 stays ignored.
    EXPECT_EQ("0", pp("{#ignore PP50}__counter_set(a, 2.5)__counter_peek(a)"));
    EXPECT_EQ(Strings({pp39("__counter_set: value must be an integer: 2.5")}), diags());
    EXPECT_EQ("0", pp("__counter_set(a, TRUE + 1)__counter_peek(a)"));
    EXPECT_EQ(Strings({pp39("__counter_set: value could not be evaluated: TRUE + 1")}), diags());
    EXPECT_EQ("4", pp("__counter_set(a, 1 / 0 + 4)__counter_peek(a)"));
    EXPECT_TRUE(empty());
}
#endif

// N7 (L4): division by zero reports PP52, continues with 0, and is stored.
TEST_F(NamedCounterTest, SetDivisionByZeroStoresValue) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("7", pp("__counter_set(a, 10 / 0 + 7)__counter_peek(a)"));
    EXPECT_EQ(Strings({"PP52: Invalid expression; 'division by zero'"}), diags());
}

// E27 (L4): a counter in an __assert expression counts once per evaluation.
TEST_F(NamedCounterTest, CounterInsideAssertExpr) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("2", pp("__assert(__counter(a) < 1, m)__assert(__counter(a) < 1, m)__counter_peek(a)"));
    EXPECT_EQ(Strings({"PP91: 'm'"}), diags());
}

// R2-C2: the values live in the Env, not in the built-in macro objects.
TEST_F(NamedCounterTest, ValuesSurviveUndefOfBuiltin) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("0 1__counter(a)", pp("__counter(a) __counter(a){#undef __counter}__counter(a)", env));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__counter: undefining built-in macro'"}), diags());
    EXPECT_EQ(2u, env.counter_value("a"));
    EXPECT_EQ("2", pp("__counter_peek(a)", env));
    EXPECT_TRUE(empty());
}

// N16 (M5): undefining or redefining a counter built-in warns PP35.
TEST_F(NamedCounterTest, UndefOrRedefineCounterBuiltinWarns) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("__counter_set(a, 1)", pp("{#undef __counter_set}__counter_set(a, 1)"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__counter_set: undefining built-in macro'"}), diags());
    EXPECT_EQ("1", pp("{#define __counter_peek 1}__counter_peek"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__counter_peek: redefining built-in macro'"}), diags());
}

// N20 (L5): a misspelt name is simply a new counter (by design, silent).
TEST_F(NamedCounterTest, MisspeltNameIsNewCounter) {
    EXPECT_EQ("0 0", pp("__counter(slot) __counter_peek(sloot)"));
    EXPECT_TRUE(empty());
}
