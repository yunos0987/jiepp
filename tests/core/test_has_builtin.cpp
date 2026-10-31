#include "test_helper.hpp"

#include <string>
#include <vector>

// __has_builtin(NAME) -- SPEC 5.2. Value 1 iff NAME is an operator or a
// symbol-table entry that is still a built-in kind; recognised only where the
// transient 'defined' operator is active.
//
// Messages in the inputs have no apostrophes (they would end the quoted
// context of the diagnostic).

using ::testing::HasSubstr;

class HasBuiltinTest : public JieppTest {
protected:
    // "y" / "n" for a condition, evaluated in `env`.
    std::string yn(const std::string& cond, Env& env) {
        return pp("{#if " + cond + "}y{#else}n{#endif}", env);
    }
    std::string yn(const std::string& cond) {
        Env env = setup();
        return yn(cond, env);
    }
};

// The names whose entry is the object setup() installs, plus the operators.
TEST_F(HasBuiltinTest, InitialBuiltinsAreOne) {
    std::vector<std::string> names = {
        "__COUNTER__", "__LINE__", "__FILE__", "__DATE__", "__TIME__",
        "__TIMESTAMP__", "__INCLUDE_LEVEL__", "__BASE_FILE__", "__FILE_NAME__",
        "__error", "__assert", "__counter", "__counter_peek", "__counter_set",
        "__nosplit", "__xmlattr", "__xmltext", "__arg_lhs", "__arg_rhs", "__arg_dir",
        "__has_builtin", "defined",
#ifndef JIEPP_SANDBOX
        "__has_include",
#endif
    };
    Env env = setup();
    for (const auto& n : names) {
        SCOPED_TRACE(n);
        EXPECT_EQ("y", yn("__has_builtin(" + n + ")", env));
        EXPECT_TRUE(empty());
    }
}

TEST_F(HasBuiltinTest, OtherNamesAreZero) {
    Env env = setup();
    pp("{#define FOO 1}{#define F(x) x}", env);
    for (const char* n : {"__INT_MAX__", "_JIEPP", "_JIEPP_VER", "FOO", "F", "UNKNOWN",
                          "__VA_ARGS__"}) {
        SCOPED_TRACE(n);
        EXPECT_EQ("n", yn(std::string("__has_builtin(") + n + ")", env));
        EXPECT_TRUE(empty());
    }
}

TEST_F(HasBuiltinTest, UndefMakesZero) {
    Env env = setup();
    EXPECT_EQ("n", pp("{#undef __assert}{#if __has_builtin(__assert)}y{#else}n{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__assert: undefining built-in macro"));

    // -U goes through apply_undef_option()
    Env env2 = setup();
    apply_undef_option("__assert", env2);
    msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("__assert: undefining built-in macro"));
    EXPECT_EQ("n", yn("__has_builtin(__assert)", env2));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, RedefineMakesZero) {
    {
        Env env = setup();
        EXPECT_EQ("n", pp("{#define __LINE__ 5}{#if __has_builtin(__LINE__)}y{#else}n{#endif}", env));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_THAT(msgs[0], HasSubstr("__LINE__: redefining built-in macro"));
    }
    {
        // -D __assert=
        Env env = setup({{"__assert", ""}});
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_THAT(msgs[0], HasSubstr("__assert: redefining built-in macro"));
        EXPECT_EQ("n", yn("__has_builtin(__assert)", env));
        EXPECT_TRUE(empty());
    }
}

// The operator cannot be replaced by a macro; the queried name stays
// state-sensitive.
TEST_F(HasBuiltinTest, CannotBeFaked) {
    Env env = setup({{"__has_builtin(x)", "1"}});
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__has_builtin: redefining built-in macro"));

    EXPECT_EQ("n", pp("{#undef __assert}{#if __has_builtin(__assert)}y{#else}n{#endif}", env));
    msgs = messages();
    ASSERT_EQ(1u, msgs.size()); // the PP35 of {#undef __assert} only
    EXPECT_THAT(msgs[0], HasSubstr("__assert: undefining built-in macro"));

    // Outside a condition the user definition applies.
    EXPECT_EQ("1", pp("__has_builtin(q)", env));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, UndefHasBuiltinKeepsOperator) {
    Env env = setup();
    EXPECT_EQ("a", pp("{#undef __has_builtin}{#ifdef __has_builtin}a{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__has_builtin: undefining built-in macro"));
    EXPECT_EQ("y", yn("__has_builtin(__LINE__)", env));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, FeatureTest) {
    EXPECT_EQ("a", pp("{#ifdef __has_builtin}a{#endif}"));
    EXPECT_EQ("y", yn("defined(__has_builtin)"));
    EXPECT_EQ("y", yn("defined __has_builtin"));
    EXPECT_EQ("n", pp("{#ifndef __has_builtin}a{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, OperandNotExpanded) {
    Env env = setup();
    pp("{#define N __LINE__}", env);
    EXPECT_EQ("n", yn("__has_builtin(N)", env));
    EXPECT_EQ("y", yn("__has_builtin(__LINE__)", env));
    EXPECT_EQ("y", yn("__has_builtin(__assert)", env));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, OperandErrors) {
    Issue::ContinueMode guard({});
    struct Case { const char* cond; const char* code; const char* text; };
    const Case cases[] = {
        {"__has_builtin", "PP52", "missing '(' after '__has_builtin'"},
        {"__has_builtin ", "PP52", "missing '(' after '__has_builtin'"},
        {"__has_builtin()", "PP39", "__has_builtin: name missing"},
        {"__has_builtin(1)", "PP39", "__has_builtin: name must be an identifier: 1"},
        {"__has_builtin(A B)", "PP52", "missing ')' after '__has_builtin'"},
        {"__has_builtin(A", "PP52", "missing ')' after '__has_builtin'"},
        {"__has_builtin(1) \\or\\ __has_builtin(2)", "PP39",
         "__has_builtin: name must be an identifier: 1"},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.cond);
        Env env = setup();
        EXPECT_EQ("n", yn(c.cond, env));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_THAT(msgs[0], HasSubstr(c.code));
        EXPECT_THAT(msgs[0], HasSubstr(c.text));
    }
}

TEST_F(HasBuiltinTest, CommentsInOperand) {
    EXPECT_EQ("y", yn("__has_builtin( (* c *) __LINE__ /* d */ )"));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, InAssertAndCounterSet) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("", pp("__assert(__has_builtin(__counter), m)", env));
    EXPECT_TRUE(empty());

    pp("__assert(__has_builtin(FOO), m)", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP91"));
    EXPECT_THAT(msgs[0], HasSubstr("'m'"));

    EXPECT_EQ("5", pp("__counter_set(c, __has_builtin(__LINE__) + 4)__counter_peek(c)", env));
    EXPECT_TRUE(empty());
}

// An operand error is an evaluation failure: the assertion reports only that
// diagnostic, unless it was ignored (then PP91 stands in for it).
TEST_F(HasBuiltinTest, OperandErrorIsAssertFailure) {
    Issue::ContinueMode guard({});
    Env env = setup();
    pp("__assert(__has_builtin(1), m)", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP39"));
#ifndef JIEPP_SANDBOX
    pp("{#ignore PP39}__assert(__has_builtin(1), m)", env);
    msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP91"));
    EXPECT_THAT(msgs[0], HasSubstr("'m'"));
#endif
}

// Like 'defined': recognised when a macro expansion produces it; an operand
// that comes through a macro parameter was pre-expanded first.
TEST_F(HasBuiltinTest, MacroProducedOperator) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("y", pp("{#define HB(x) __has_builtin(x)}{#if HB(__assert)}y{#endif}", env));
    EXPECT_TRUE(empty());

    pp("{#if HB(__LINE__)}y{#endif}", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP39"));
}

// Outside the places where 'defined' works it is an ordinary identifier.
TEST_F(HasBuiltinTest, PlainTextOutsideConditions) {
    EXPECT_EQ("a __has_builtin(q) b", pp("a __has_builtin(q) b"));
    EXPECT_TRUE(empty());
}

TEST_F(HasBuiltinTest, SuspendedInMessage) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("a", pp("{#if __error(__has_builtin(x)) 1}a{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP91"));
    EXPECT_THAT(msgs[0], HasSubstr("'__has_builtin(x)'"));
}

// When the operand error is itself PP39, '{#ignore PP39}' also hides the
// stand-in PP39 of __counter_set, so the counter silently stays unchanged.
TEST_F(HasBuiltinTest, CounterSetOperandErrorPP39IgnoredIsSilent) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("5", pp("__counter_set(c, 5)__counter_peek(c)", env));
    EXPECT_TRUE(empty());

    pp("__counter_set(c, __has_builtin(1))", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP39"));
    EXPECT_EQ("5", pp("__counter_peek(c)", env));

#ifndef JIEPP_SANDBOX
    pp("{#ignore PP39}__counter_set(c, __has_builtin(1))", env);
    EXPECT_TRUE(empty());
    EXPECT_EQ("5", pp("__counter_peek(c)", env));
#endif
}

// The operator spans lines; line numbers after it stay correct.
TEST_F(HasBuiltinTest, MultiLineOperandKeepsLineNumbers) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("\n\ny\n4\n", pp("{#if __has_builtin(\n__LINE__\n)}y{#endif}\n__LINE__\n", env));
    EXPECT_TRUE(empty());

    Env env2 = setup();
    pp("{#if __has_builtin(\n__LINE__\n)}{#endif}\n{#error boom}", env2);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr(":4.0:"));
    EXPECT_THAT(msgs[0], HasSubstr("PP91"));
}

// The pattern that motivated the operator: detect a disabled __assert.
TEST_F(HasBuiltinTest, RungstGuardPattern) {
    Issue::ContinueMode guard({});
    Env env = setup({{"__assert", ""}});
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));

    pp("{#if \\not\\ __has_builtin(__assert)}{#error assert disabled}{#endif}", env);
    msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP91"));
    EXPECT_THAT(msgs[0], HasSubstr("'assert disabled'"));
}

#ifndef JIEPP_SANDBOX
TEST_F(HasBuiltinTest, DefineHasIncludeWarns) {
    Env env = setup();
    EXPECT_EQ("n", pp("{#define __has_include 1}"
                      "{#if __has_include('nonexistent.iec')}y{#else}n{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__has_include: redefining built-in macro"));
}
#endif

// The ": redefining built-in macro" suffix only marks the first definition
// over the operator (no symbol-table entry yet); after that the usual rules
// apply: identical is silent, a different body is the plain PP35.
TEST_F(HasBuiltinTest, OperatorRedefinitionFollowsSymbolTable) {
    Env env = setup();
    pp("{#define __has_builtin 1}", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("'__has_builtin: redefining built-in macro'"));

    pp("{#define __has_builtin 1}", env);
    EXPECT_TRUE(empty());

    pp("{#define __has_builtin 2}", env);
    msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("'__has_builtin'"));
    EXPECT_THAT(msgs[0], testing::Not(HasSubstr("redefining")));
}
