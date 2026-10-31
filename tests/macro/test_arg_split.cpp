#include "test_helper.hpp"

#include <sstream>
#include <string>
#include <vector>

// =====================================================================
// __arg_lhs(args...), __arg_rhs(args...) and __arg_dir(args...)
// (SPECIFICATION.md 12.13): the arguments joined like __VA_ARGS__ and
// macro-expanded once, then split at the first ':=' or '=>' outside
// (...) and [...]. lhs / rhs give the tokens before / after it without the
// whitespace at their ends; dir gives 0 for ':=' and 1 for '=>'. A call
// without a separator is PP81 and gives no output.
// =====================================================================

using ::testing::HasSubstr;

class ArgSplitTest : public JieppTest {};

// The RobustnessTest* name matches the sandbox test presets' filter
// ^(Sandbox|RobustnessTest), so these run in sandbox builds too.
class RobustnessTestArgSplit : public JieppTest {};

namespace {
const std::vector<std::string> kNames = {"__arg_lhs", "__arg_rhs", "__arg_dir"};

// A caller like rungst's FB argument macros: the macro body names the
// parameter inside the calls.
const std::string kIn =
    "{#define IN(a) <InputVariable parameterName=__xmlattr(__arg_lhs(a))/>}";
} // namespace

// ---- the examples ----

TEST_F(ArgSplitTest, LhsOfAssignment) {
    EXPECT_EQ("PT", pp("__arg_lhs(PT := T#2s)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, RhsOfAssignment) {
    EXPECT_EQ("T#2s", pp("__arg_rhs(PT := T#2s)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, DirOfAssignmentIsZero) {
    EXPECT_EQ("0", pp("__arg_dir(PT := T#2s)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, DirOfOutputIsOne) {
    EXPECT_EQ("1", pp("__arg_dir(Q => Done)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, RhsKeepsNestedSeparatorInParentheses) {
    EXPECT_EQ("F(a := 1)", pp("__arg_rhs(IN := F(a := 1))"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, LhsKeepsNestedSeparatorInBrackets) {
    EXPECT_EQ("v[i := 1]", pp("__arg_lhs(v[i := 1] := 2)"));
    EXPECT_EQ("2", pp("__arg_rhs(v[i := 1] := 2)"));
    EXPECT_TRUE(empty());
}

// `<=>` is not an IEC operator: its `=>` part is the separator, so the left
// side keeps the `<` (SPECIFICATION.md 12.13).
TEST_F(ArgSplitTest, SpaceshipSplitsAtTheArrowPart) {
    EXPECT_EQ("a <", pp("__arg_lhs(a <=> b)"));
    EXPECT_EQ("b", pp("__arg_rhs(a <=> b)"));
    EXPECT_EQ("1", pp("__arg_dir(a <=> b)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, RhsOfOutput) {
    EXPECT_EQ("Done", pp("__arg_rhs(Q => Done)"));
    EXPECT_EQ("Q", pp("__arg_lhs(Q => Done)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, FeedsXmlattrLhs) {
    EXPECT_EQ("\"PT\"", pp("__xmlattr(__arg_lhs(PT := T#2s))"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, FeedsXmlattrRhsAndEscapes) {
    EXPECT_EQ("\"&apos;a&lt;b&apos;\"", pp("__xmlattr(__arg_rhs(S := 'a<b'))"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, DirInIfCondition) {
    EXPECT_EQ("y", pp("{#if __arg_dir(Q => D) = 1}y{#else}n{#endif}"));
    EXPECT_EQ("n", pp("{#if __arg_dir(Q := D) = 1}y{#else}n{#endif}"));
    EXPECT_EQ("y", pp("{#if __arg_dir(Q := D) = 0}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, DirInAssert) {
    EXPECT_EQ("", pp("__assert(__arg_dir(Q => D) = 1, m)"));
    EXPECT_TRUE(empty());
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__assert(__arg_dir(Q := D) = 1, wrong direction)"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("wrong direction"));
    EXPECT_TRUE(empty());
}

// The result is an ordinary integer token, so it is not an operand of @@:
// like any built-in there it is not evaluated (SPEC 12.13).
TEST_F(ArgSplitTest, GuardDirIsNotEvaluatedAsOperandOfGlue) {
    EXPECT_EQ("v__arg_dir(a := 1)", pp("{#define G(x) v@@__arg_dir(x)}G(a := 1)"));
    EXPECT_TRUE(empty());
}

// ---- through macros ----

TEST_F(ArgSplitTest, ThroughMacro) {
    EXPECT_EQ("<InputVariable parameterName=\"PT\"/>", pp(kIn + "IN(PT := T#2s)"));
    EXPECT_EQ("<InputVariable parameterName=\"Q\"/>", pp(kIn + "IN(Q => Done)"));
    EXPECT_TRUE(empty());
}

// A parameter inside the call is pre-expanded like any other parameter.
TEST_F(ArgSplitTest, ParameterIsPreExpanded) {
    EXPECT_EQ("<InputVariable parameterName=\"PT\"/>",
              pp("{#define PIN PT}" + kIn + "IN(PIN := 1)"));
    EXPECT_EQ("PT", pp("{#define ASSIGN PT := 1}__arg_lhs(ASSIGN)"));
    EXPECT_EQ("1", pp("{#define ASSIGN PT := 1}__arg_rhs(ASSIGN)"));
    EXPECT_EQ("0", pp("{#define ASSIGN PT := 1}__arg_dir(ASSIGN)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, SeparatorFromMacroBody) {
    EXPECT_EQ("a", pp("{#define SEP =>}__arg_lhs(a SEP b)"));
    EXPECT_EQ("1", pp("{#define SEP =>}__arg_dir(a SEP b)"));
    EXPECT_TRUE(empty());
}

// A separator glued by @@ in a macro body is one token ':=' / '=>'.
TEST_F(ArgSplitTest, GluedSeparator) {
    EXPECT_EQ("0", pp("{#define D(x) __arg_dir(x :@@= 1)}D(a)"));
    EXPECT_EQ("1", pp("{#define D(x) __arg_dir(x =@@> 1)}D(a)"));
    EXPECT_EQ("a", pp("{#define L(x) __arg_lhs(x :@@= 1)}L(a)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, VaArgs) {
    const std::string v = "{#define V(...) __arg_lhs(__VA_ARGS__)}{#define W(...) __arg_rhs(__VA_ARGS__)}"
                          "{#define X(...) __arg_dir(__VA_ARGS__)}";
    EXPECT_EQ("PT", pp(v + "V(PT := 1)"));
    EXPECT_EQ("a, b", pp(v + "V(a, b := c, d)"));
    EXPECT_EQ("c, d", pp(v + "W(a, b := c, d)"));
    EXPECT_EQ("1", pp(v + "X(a, b => c)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, TwoArgumentsAreJoinedWithComma) {
    EXPECT_EQ("a, b", pp("__arg_lhs(a, b := c, d)"));
    EXPECT_EQ("c, d", pp("__arg_rhs(a, b := c, d)"));
    // The comma of the second argument is split from the first.
    EXPECT_EQ("a", pp("__arg_lhs(a := b, c)"));
    EXPECT_EQ("b, c", pp("__arg_rhs(a := b, c)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, CounterAdvancesOncePerCall) {
    EXPECT_EQ("0 1", pp("__arg_lhs(__COUNTER__ := 9) __COUNTER__"));
    EXPECT_EQ("0 1", pp("__arg_rhs(x := __COUNTER__) __COUNTER__"));
    EXPECT_EQ("1 0", pp("__arg_dir(a => b) __COUNTER__"));
    EXPECT_TRUE(empty());
}

// The argument is expanded once, so a counter on both sides advances twice
// (once per __COUNTER__ written) and the call itself adds nothing.
TEST_F(ArgSplitTest, CounterOnBothSides) {
    EXPECT_EQ("1 2", pp("__arg_rhs(__COUNTER__ := __COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, CounterThroughMacroAdvancesOnce) {
    EXPECT_EQ("0 1", pp("{#define C(x) __arg_lhs(x)}C(__COUNTER__ := 5) __COUNTER__"));
    EXPECT_TRUE(empty());
}

// ---- where the separator is searched ----

TEST_F(ArgSplitTest, SeparatorInStringLiteralIsNotTaken) {
    EXPECT_EQ("S", pp("__arg_lhs(S := ':=')"));
    EXPECT_EQ("':='", pp("__arg_rhs(S := ':=')"));
    EXPECT_EQ("':='", pp("__arg_lhs(':=' => x)"));
    EXPECT_EQ("1", pp("__arg_dir(':=' => x)"));
    EXPECT_EQ("0", pp("__arg_dir(\"=>\" := x)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, SeparatorInCommentIsNotTaken) {
    EXPECT_EQ("a", pp("__arg_lhs(a (* := *) => b)"));
    EXPECT_EQ("1", pp("__arg_dir(a (* := *) => b)"));
    EXPECT_EQ("1", pp("__arg_dir(a /* := */ => b)"));
    EXPECT_EQ("b", pp("__arg_rhs(a (* := *) => b)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, SeparatorInPragmaIsNotTaken) {
    EXPECT_EQ("1", pp("__arg_dir(a {attribute x := y} => b)"));
    EXPECT_EQ("b", pp("__arg_rhs(a {attribute x := y} => b)"));
    EXPECT_EQ("a (*{attribute x := y}*)", pp("__arg_lhs(a {attribute x := y} => b)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, SeparatorInParenthesesOrBracketsIsNotTaken) {
    EXPECT_EQ("1", pp("__arg_dir(f(a := 1) => b)"));
    EXPECT_EQ("f(a := 1)", pp("__arg_lhs(f(a := 1) => b)"));
    EXPECT_EQ("1", pp("__arg_dir(v[i := 1] => b)"));
    EXPECT_EQ("0", pp("__arg_dir(f((a => 1)) [b => 2] := 3)"));
    EXPECT_EQ("f((a => 1)) [b => 2]", pp("__arg_lhs(f((a => 1)) [b => 2] := 3)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, FirstSeparatorWins) {
    EXPECT_EQ("0", pp("__arg_dir(a := b => c)"));
    EXPECT_EQ("1", pp("__arg_dir(a => b := c)"));
    EXPECT_EQ("b := c", pp("__arg_rhs(a := b := c)"));
    EXPECT_EQ("b => c", pp("__arg_rhs(a := b => c)"));
    EXPECT_TRUE(empty());
}

// Only a ':' directly followed by '=' (or '=' by '>') is a separator.
TEST_F(ArgSplitTest, SpacedCharactersAreNotASeparator) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__arg_dir(a : = b)"));
    EXPECT_EQ("", pp("__arg_dir(a = > b)"));
    EXPECT_EQ(2u, messages().size());
}

TEST_F(ArgSplitTest, OtherOperatorsAreNotASeparator) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__arg_dir(a <= b)"));
    EXPECT_EQ("", pp("__arg_dir(a >= b)"));
    EXPECT_EQ("", pp("__arg_dir(a = b)"));
    EXPECT_EQ(3u, messages().size());
}

// ---- spacing and empty sides ----

TEST_F(ArgSplitTest, EdgeWhitespaceIsTrimmedInnerSpacingKept) {
    EXPECT_EQ("a   b", pp("__arg_lhs(   a   b   :=   c   d   )"));
    EXPECT_EQ("c   d", pp("__arg_rhs(   a   b   :=   c   d   )"));
    EXPECT_EQ("a", pp("__arg_lhs((*c*) a (*d*) := b)"));
    EXPECT_EQ("b", pp("__arg_rhs(a := (*c*) b (*d*))"));
    EXPECT_EQ("a", pp("__arg_lhs(a:=b)"));
    EXPECT_EQ("b", pp("__arg_rhs(a:=b)"));
    EXPECT_EQ("0", pp("__arg_dir(a:=b)"));
    EXPECT_EQ("1", pp("__arg_dir(a=>b)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, EmptySideIsNotAnError) {
    EXPECT_EQ("", pp("__arg_lhs(:= x)"));
    EXPECT_EQ("x", pp("__arg_rhs(:= x)"));
    EXPECT_EQ("pin", pp("__arg_lhs(pin :=)"));
    EXPECT_EQ("", pp("__arg_rhs(pin :=)"));
    EXPECT_EQ("", pp("__arg_lhs(=> x)"));
    EXPECT_EQ("", pp("__arg_rhs(Q =>)"));
    EXPECT_EQ("0", pp("__arg_dir(:=)"));
    EXPECT_EQ("1", pp("__arg_dir(=>)"));
    EXPECT_TRUE(empty());
}

// ---- no separator: PP81 and no output ----

TEST_F(ArgSplitTest, MissingSeparatorIsPP81) {
    Issue::ContinueMode guard({});
    for (const auto& name : kNames) {
        EXPECT_EQ("[]", "[" + pp(name + "(a b)") + "]");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr("PP81"));
        EXPECT_THAT(msgs[0], HasSubstr(name + ": no ':=' or '=>' at the top level"));
    }
}

TEST_F(ArgSplitTest, EmptyArgumentIsPP81) {
    Issue::ContinueMode guard({});
    for (const auto& name : kNames) {
        EXPECT_EQ("[]", "[" + pp(name + "()") + "]");
        EXPECT_EQ("[]", "[" + pp(name + "(   )") + "]");
        EXPECT_EQ("[]", "[" + pp(name + "((* c *))") + "]");
        auto msgs = messages();
        ASSERT_EQ(3u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr("PP81"));
    }
}

TEST_F(ArgSplitTest, SeparatorOnlyInsideLiteralCommentOrGroupIsPP81) {
    Issue::ContinueMode guard({});
    const std::vector<std::string> inputs = {
        "'a := b'", "\"a => b\"", "a (* := *) b", "f(a := b)", "[a => b]", "f((a := b))",
    };
    for (const auto& name : kNames) {
        for (const auto& in : inputs) {
            EXPECT_EQ("[]", "[" + pp(name + "(" + in + ")") + "]") << name << " " << in;
            auto msgs = messages();
            ASSERT_EQ(1u, msgs.size()) << name << " " << in;
            EXPECT_THAT(msgs[0], HasSubstr("PP81"));
        }
    }
}

TEST_F(ArgSplitTest, MissingSeparatorOutputDoesNotDisturbText) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("x  y", pp("x __arg_lhs(a) y"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP81"));
}

TEST_F(ArgSplitTest, MissingSeparatorThroughMacroIsPP81) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("<InputVariable parameterName=\"\"/>", pp(kIn + "IN(PT)"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP81"));
    EXPECT_THAT(msgs[0], HasSubstr("__arg_lhs: no ':=' or '=>' at the top level"));
}

#ifndef JIEPP_SANDBOX
// {#ignore PP81} hides the diagnostic like any other; the call still gives
// no output.
TEST_F(ArgSplitTest, IgnoreSuppressesPP81) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("[]", "[" + pp("{#ignore PP81}__arg_lhs(a)", env) + "]");
    EXPECT_TRUE(empty());
}
#endif

// The code is an ERROR, so it also counts for the exit status in a build.
TEST_F(ArgSplitTest, CodeIsAnError) {
    EXPECT_TRUE(Issue::is_error(Issue::Code::ARG_SEPARATOR_MISSING));
    EXPECT_EQ("ARG_SEPARATOR_MISSING", Issue::codename(Issue::Code::ARG_SEPARATOR_MISSING));
}

// ---- the three names behave like the other built-in function macros ----

TEST_F(ArgSplitTest, NamesAreCaseSensitive) {
    EXPECT_EQ("a", pp("__arg_lhs(a := b)"));
    EXPECT_EQ("__ARG_LHS(a := b)", pp("__ARG_LHS(a := b)"));
    EXPECT_EQ("__Arg_rhs(a := b)", pp("__Arg_rhs(a := b)"));
    EXPECT_EQ("__arg_DIR(a := b)", pp("__arg_DIR(a := b)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, FeatureTests) {
    for (const auto& name : kNames) {
        EXPECT_EQ("yz", pp("{#ifdef " + name + "}y{#endif}{#if __has_builtin(" + name + ")}z{#endif}"))
            << name;
        EXPECT_EQ("n", pp("{#if __has_builtin(" + name + "x)}y{#else}n{#endif}")) << name;
    }
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, UndefWarnsPP35AndRemovesIt) {
    for (const auto& name : kNames) {
        Env env = setup();
        EXPECT_EQ("n", pp("{#undef " + name + "}{#if __has_builtin(" + name + ")}y{#else}n{#endif}", env));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr("PP35"));
        EXPECT_THAT(msgs[0], HasSubstr(name + ": undefining built-in macro"));
        EXPECT_EQ(name + "(a := b)", pp(name + "(a := b)", env));
        EXPECT_TRUE(empty());
    }
}

TEST_F(ArgSplitTest, RedefineWarnsPP35) {
    for (const auto& name : kNames) {
        Env env = setup();
        EXPECT_EQ("n", pp("{#define " + name + " 1}{#if __has_builtin(" + name + ")}y{#else}n{#endif}", env));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr("PP35"));
        EXPECT_THAT(msgs[0], HasSubstr(name + ": redefining built-in macro"));
        EXPECT_TRUE(empty());
    }
}

// -D goes through setup()'s predefined macros, -U through apply_undef_option().
TEST_F(ArgSplitTest, DefineOptionWarnsPP35) {
    for (const auto& name : kNames) {
        Env env = setup({{name, "1"}});
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr("PP35"));
        EXPECT_THAT(msgs[0], HasSubstr(name + ": redefining built-in macro"));
        EXPECT_EQ("n", pp("{#if __has_builtin(" + name + ")}y{#else}n{#endif}", env));
        EXPECT_TRUE(empty());
    }
}

TEST_F(ArgSplitTest, UndefOptionWarnsPP35) {
    for (const auto& name : kNames) {
        Env env = setup();
        apply_undef_option(name, env);
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr(name + ": undefining built-in macro"));
        EXPECT_EQ("n", pp("{#if __has_builtin(" + name + ")}y{#else}n{#endif}", env));
        EXPECT_TRUE(empty());
    }
}

// Guard: passes on the old source too (nothing was listed before either).
TEST_F(ArgSplitTest, GuardNotListedByDumpMacros) {
    Env env = setup();
    std::ostringstream os;
    dump_macros(env, os);
    EXPECT_EQ(std::string::npos, os.str().find("__arg_"));
    EXPECT_TRUE(empty());
}

// Guard (watchpoint): an identical redefinition stays silent.
TEST_F(ArgSplitTest, GuardIdenticalRedefinitionIsSilent) {
    EXPECT_EQ("1", pp("{#define A 1}{#define A 1}A"));
    EXPECT_TRUE(empty());
}

// Guard (watchpoint): a comma inside [...] stays in one argument.
TEST_F(ArgSplitTest, GuardBracketCommaStaysOneArgument) {
    EXPECT_EQ("<a[x,y]>", pp("{#define F(x) <x>}F(a[x,y])"));
    EXPECT_EQ("a[x,y]", pp("__arg_lhs(a[x,y] := 1)"));
    EXPECT_TRUE(empty());
}

TEST_F(ArgSplitTest, UnclosedParenthesisIsPP34) {
    Issue::ContinueMode guard({});
    for (const auto& name : kNames) {
        EXPECT_EQ("x ", pp("x " + name + "(a := b"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << name;
        EXPECT_THAT(msgs[0], HasSubstr("PP34"));
        EXPECT_THAT(msgs[0], HasSubstr(name + ": missing closing parenthesis"));
    }
}

TEST_F(ArgSplitTest, GuardWithoutParenthesisPassesThrough) {
    for (const auto& name : kNames) {
        EXPECT_EQ(name + " + 1", pp(name + " + 1")) << name;
        EXPECT_EQ("x " + name, pp("x " + name)) << name;
    }
    EXPECT_TRUE(empty());
}

// ---- sandbox coverage ----

TEST_F(RobustnessTestArgSplit, WorksAndReportsMissingSeparator) {
    EXPECT_EQ("PT", pp("__arg_lhs(PT := T#2s)"));
    EXPECT_EQ("T#2s", pp("__arg_rhs(PT := T#2s)"));
    EXPECT_EQ("1", pp("__arg_dir(Q => Done)"));
    EXPECT_EQ("yz", pp("{#ifdef __arg_lhs}y{#endif}{#if __has_builtin(__arg_dir)}z{#endif}"));
    EXPECT_TRUE(empty());

    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__arg_rhs(a)"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP81"));
}

// A long argument is scanned once: the separator is found by one pass, and
// the side tokens are copied once.
TEST_F(RobustnessTestArgSplit, LongArgumentStaysBounded) {
    std::string left;
    for (int i = 0; i < 20000; ++i)
        left += "a ";
    EXPECT_EQ(left.substr(0, left.size() - 1), pp("__arg_lhs(" + left + ":= b)"));
    EXPECT_EQ("b", pp("__arg_rhs(" + left + ":= b)"));
    EXPECT_EQ("0", pp("__arg_dir(" + left + ":= b)"));

    // Nested groups with no separator at the top level: a depth counter, not
    // a stack, so deep nesting is not a problem.
    std::string deep = std::string(200, '(') + ":=" + std::string(200, ')');
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("__arg_dir(" + deep + ")"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP81"));
}
