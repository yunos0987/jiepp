#include "test_helper.hpp"

#include <string>
#include <vector>

// =====================================================================
// __nosplit(args...) (SPECIFICATION.md 12.11): the arguments joined like
// __VA_ARGS__ and macro-expanded once; every comma of the result outside
// (...)/[...] stops separating the arguments of a later function-macro call
// and is not counted by __VA_ARGC__. The text is that of the identity macro
// ID(...) below, which is the control case: replacing __nosplit by ID (or
// by a plain __VA_ARGS__) changes the results checked here.
// =====================================================================

class NosplitTest : public JieppTest {
protected:
    // Each diagnostic line without its location prefix, e.g. "PP34: ...".
    std::vector<std::string> diags() {
        std::vector<std::string> out;
        for (const auto& m : messages()) {
            auto p = m.find(": PP");
            out.push_back(p == std::string::npos ? m : m.substr(p + 2));
        }
        return out;
    }
};

// The RobustnessTest* name matches the sandbox test presets' filter
// ^(Sandbox|RobustnessTest), so these run in sandbox builds too.
class RobustnessTestNosplit : public JieppTest {};

using Strings = std::vector<std::string>;

namespace {

// The control: the same text as __nosplit, with ordinary commas.
const std::string kId = "{#define ID(...) __VA_ARGS__}";
// The number of arguments of a call, and a second call that re-collects
// its own arguments (G(x) calls F(x)), where the protection shows.
const std::string kF = "{#define F(...) __VA_ARGC__}";
const std::string kG = "{#define G(...) F(__VA_ARGS__)}";
// A statement wrapper like rungst's ST_EXEC, and the same without __nosplit.
const std::string kStx = "{#define STX(...) stx: __nosplit(__VA_ARGS__) ;}";
const std::string kStxr = "{#define STXR(...) stx: __VA_ARGS__ ;}";
const std::string kTwoStage =
    "{#define IN(...) <__VA_ARGC__>}{#define OUT(...) <__VA_ARGC__> IN(__VA_ARGS__)}";

} // namespace

// T1: __VA_ARGC__ counts the raw actuals. F(__nosplit(a, b)) is 1 through
// the parentheses alone; the effect shows when the result is re-collected
// (G), where the control ID gives 2.
TEST_F(NosplitTest, VaArgcAfterRescan) {
    EXPECT_EQ("1", pp(kF + "F(__nosplit(a, b))"));
    EXPECT_EQ("2", pp(kF + "F(a, b)"));
    EXPECT_EQ("1", pp(kF + kG + "G(__nosplit(a, b))"));
    EXPECT_EQ("2", pp(kF + kG + kId + "G(ID(a, b))")); // control
    EXPECT_TRUE(empty());
}

// T2: two-stage distribution (the rungst RUNG case).
TEST_F(NosplitTest, TwoStageDistribution) {
    EXPECT_EQ("<2> <2>", pp(kTwoStage + kStx + "OUT(A, STX(case N of 1, 2: Y := 1;))"));
    EXPECT_EQ("<2> <3>", pp(kTwoStage + kStxr + "OUT(A, STXR(case N of 1, 2: Y := 1;))")); // control
    EXPECT_TRUE(empty());
}

// T3
TEST_F(NosplitTest, StExecLikeWrapper) {
    EXPECT_EQ("stx: case N of 1, 2: Y := 1; ;", pp(kStx + "STX(case N of 1, 2: Y := 1;)"));
    EXPECT_TRUE(empty());
}

// T4: @ does not pre-expand its operand; a relay macro does.
TEST_F(NosplitTest, Stringize) {
    const std::string s = "{#define S(x) @x}";
    const std::string xs = "{#define XS(x) S(x)}";
    EXPECT_EQ("'__nosplit(a, b)'", pp(s + "S(__nosplit(a, b))"));
    EXPECT_EQ("'a, b'", pp(s + xs + "XS(__nosplit(a, b))"));
    EXPECT_EQ("'a, b'", pp("{#string __nosplit(a, b)}"));
    EXPECT_EQ("'a, b, c'",
              pp("{#define SV(...) @__VA_ARGS__}{#define XSV(...) SV(__VA_ARGS__)}"
                 "XSV(a, __nosplit(b, c))"));
    EXPECT_TRUE(empty());
    {
        // Control: with ID, S receives two arguments.
        Issue::ContinueMode guard({});
        EXPECT_EQ("S", pp(s + xs + kId + "XS(ID(a, b))"));
        EXPECT_EQ(Strings({"PP34: Argument count mismatch; 'S: expected 1, got 2'"}), diags());
    }
}

// T5: @@ does not pre-expand its operands; through a relay (XCAT) the
// result is pasted. A protected comma at the edge is an ANY token, so
// pasting it to an identifier is not PP32 (it would be for an ordinary
// comma). `__nosplit(a, b) @@ y` pastes the closing ')' to 'y', which is
// PP32 for any macro; the failed paste keeps both operands, so 'y' is not
// lost (PasteToClosingParenKeepsBothOperands below).
TEST_F(NosplitTest, PasteAdjacency) {
    const std::string cat = "{#define CAT(p, q) p @@ q}{#define XCAT(p, q) CAT(p, q)}";
    EXPECT_EQ("x__nosplit(a, b)", pp("{#define P1() x @@ __nosplit(a, b)}P1()"));
    EXPECT_EQ("xa, b", pp(cat + "XCAT(x, __nosplit(a, b))"));
    EXPECT_EQ("a, by", pp(cat + "XCAT(__nosplit(a, b), y)"));
    EXPECT_EQ("x, b", pp(cat + "XCAT(x, __nosplit(, b))"));
    EXPECT_TRUE(empty());
    {
        // Control: with ID, CAT receives three arguments.
        Issue::ContinueMode guard({});
        EXPECT_EQ("CAT", pp(cat + kId + "XCAT(x, ID(a, b))"));
        EXPECT_EQ(Strings({"PP34: Argument count mismatch; 'CAT: expected 2, got 3'"}), diags());
    }
}

// The closing ')' of __nosplit(...) pasted to 'y' is PP32 (a structural
// token cannot be pasted); both operands stay, so 'y' is kept.
TEST_F(NosplitTest, PasteToClosingParenKeepsBothOperands) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("a, by", pp("{#define P2() __nosplit(a, b) @@ y}P2()"));
    EXPECT_EQ(Strings({"PP32: Invalid token pasting (@@); ') @@ y'"}), diags());
}

// T6: commas inside (...)/[...] are left as they are. (d) fails with PP34
// if every comma were changed regardless of depth: the deferred call
// P EMPTY (1, 2) would then see one argument.
TEST_F(NosplitTest, BracketsAndParens) {
    EXPECT_EQ("[a, b], c", pp("__nosplit([a, b], c)"));
    EXPECT_EQ("f(a, b), c", pp("__nosplit(f(a, b), c)"));
    EXPECT_EQ("1", pp(kF + kG + "G(__nosplit(f(a, b), c))"));
    EXPECT_EQ("<1|2>", pp("{#define P(x, y) <x|y>}{#define EMPTY}__nosplit(P EMPTY (1, 2))"));
    EXPECT_TRUE(empty());
}

// T7: empty elements are kept (the text of __VA_ARGS__).
TEST_F(NosplitTest, ZeroOneAndEmptyElements) {
    EXPECT_EQ("[]", pp("[__nosplit()]"));
    EXPECT_EQ("[]", pp("[__nosplit( )]"));
    EXPECT_EQ("a", pp("__nosplit(a)"));
    EXPECT_EQ("a, , b", pp("__nosplit(a, , b)"));
    EXPECT_EQ("a,", pp("__nosplit(a,)"));
    EXPECT_EQ(",", pp("__nosplit(,)"));
    EXPECT_EQ("1", pp(kF + kG + "G(__nosplit(a, , b))"));
    EXPECT_EQ("1", pp(kF + kG + "G(__nosplit(,))"));
    EXPECT_EQ("0", pp(kF + kG + "G(__nosplit())"));
    EXPECT_TRUE(empty());
}

// T8a
TEST_F(NosplitTest, HasBuiltinAndIfdef) {
    EXPECT_EQ("y", pp("{#if __has_builtin(__nosplit)}y{#else}n{#endif}"));
    EXPECT_EQ("y", pp("{#ifdef __nosplit}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

// T8b: undefining or redefining the built-in warns PP35; afterwards
// __nosplit(...) is plain text (or the user's definition).
TEST_F(NosplitTest, UndefWarnsAndDisables) {
    const std::string cond = "{#if __has_builtin(__nosplit)}y{#else}n{#endif}";
    EXPECT_EQ("n__nosplit(a, b)", pp("{#undef __nosplit}" + cond + "__nosplit(a, b)"));
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__nosplit: undefining built-in macro'"}), diags());

    // -D __nosplit=
    Env env = setup({{"__nosplit", ""}});
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__nosplit: redefining built-in macro'"}), diags());
    EXPECT_EQ("n(a, b)", pp(cond + "__nosplit(a, b)", env));
    EXPECT_TRUE(empty());

    // -U __nosplit
    Env env2 = setup();
    apply_undef_option("__nosplit", env2);
    EXPECT_EQ(Strings({"PP35: Macro redefined; '__nosplit: undefining built-in macro'"}), diags());
    EXPECT_EQ("n", pp(cond, env2));
    EXPECT_TRUE(empty());
}

// T9: an unterminated call outputs nothing of the call.
TEST_F(NosplitTest, MissingCloseParenIsPP34) {
    Issue::ContinueMode guard({});
    std::string out = pp("x __nosplit(a, b");
    EXPECT_EQ("x ", out);
    EXPECT_EQ(Strings({"PP34: Argument count mismatch; '__nosplit: missing closing parenthesis'"}),
              diags());
}

// T10: in a condition, 'defined' works inside the arguments (they are
// expanded like a function-macro argument, not like a message).
TEST_F(NosplitTest, InsideIfCondition) {
    EXPECT_EQ("y", pp("{#if __nosplit(1)}y{#else}n{#endif}"));
    {
        // X must stay an EMPTY macro: an implementation that suspended
        // 'defined' while expanding the arguments would expand X to nothing,
        // leaving 'defined ( )' for the rescan (PP40, false). With X
        // undefined, or written 'defined X', the rescan would evaluate it
        // correctly and this check would no longer tell the two apart.
        Env env = setup();
        EXPECT_EQ("y", pp("{#define X}{#if __nosplit(defined(X))}y{#else}n{#endif}", env));
        EXPECT_EQ("y", pp(kId + "{#if ID(defined(X))}y{#else}n{#endif}", env));
        EXPECT_TRUE(empty());
    }
    {
        Issue::ContinueMode guard({});
        pp("{#if __nosplit(1, 2)}y{#endif}");
        auto with_nosplit = codes();
        pp("{#if 1, 2}y{#endif}");
        auto plain = codes();
        EXPECT_FALSE(plain.empty());
        EXPECT_EQ(plain, with_nosplit);
    }
    {
        // Inside a message, the suspension set by __error carries over.
        Issue::ContinueMode guard({});
        EXPECT_EQ("a", pp("{#if __error(__nosplit(defined)) 1}a{#endif}"));
        EXPECT_EQ(Strings({"PP91: 'defined'"}), diags());
    }
    EXPECT_EQ("", pp("__assert(__nosplit(1), m)"));
    EXPECT_TRUE(empty());
}

// T11: the text (spaces, line breaks, comments, line numbers) is that of
// __VA_ARGS__, checked against the identity macro.
TEST_F(NosplitTest, TextEqualsVaArgsOracle) {
    const std::vector<std::string> inputs = {
        "a, b", "a ,b", "a,b", " a , b ", "a, , b", ",", "", " ",
        "a,\nb", "(*c*) a, b (*d*)", "a (*c*), b", "f(x, y), z", "a[1, 2], b",
        "'s,t', u", "__LINE__, b", "__LINE__,\n__LINE__", "a // c\n, b",
    };
    for (const auto& x : inputs) {
        SCOPED_TRACE("input: [" + x + "]");
        std::string expected = pp(kId + "ID(" + x + ")");
        EXPECT_EQ(expected, pp(kId + "__nosplit(" + x + ")"));
        EXPECT_TRUE(empty());
    }
}

// The line number inside a multi-line call is that of the closing
// parenthesis, as for a function macro. As with __VA_ARGS__, a line break
// in the arguments is not output in place (no space after the comma); the
// call's line break follows the result, keeping the line count.
TEST_F(NosplitTest, LineInMultiLineArguments) {
    EXPECT_EQ("2,2\n", pp("__nosplit(__LINE__,\n__LINE__)"));
    EXPECT_EQ("2,2\n", pp(kId + "ID(__LINE__,\n__LINE__)")); // same as the control
    EXPECT_TRUE(empty());
}

// An IEC line comment at the end of an argument line is dropped like the
// other comments of a call; its line break follows the result.
TEST_F(NosplitTest, LineCommentInArguments) {
    EXPECT_EQ("a, b\n", pp("__nosplit(a // c\n, b)"));
    EXPECT_EQ("a, b\n", pp(kId + "ID(a // c\n, b)")); // same as the control
    EXPECT_TRUE(empty());
}

// The protection lasts for one preprocessing run only: the output text
// fed to jiepp again has ordinary commas.
TEST_F(NosplitTest, OutputFedAgainHasOrdinaryCommas) {
    std::string once = pp("__nosplit(a, b)");
    EXPECT_EQ("a, b", once);
    EXPECT_EQ("2", pp(kF + kG + "G(" + once + ")"));
    EXPECT_TRUE(empty());
}

// T12: commas produced by expanding the arguments are protected too.
TEST_F(NosplitTest, CommasFromExpandedArgumentsDoNotSplit) {
    const std::string ab = "{#define AB a, b}";
    EXPECT_EQ("1", pp(kF + kG + ab + "G(__nosplit(AB))"));
    EXPECT_EQ("2", pp(kF + kG + ab + "G(AB)")); // control
    EXPECT_EQ("a, b", pp(ab + "__nosplit(AB)"));
    EXPECT_TRUE(empty());
}

// T13: the protection survives several pre-expansions and the memoised
// expansion of a variadic argument used twice.
TEST_F(NosplitTest, MultiLevelAndMemoised) {
    const std::string defs =
        "{#define L3(...) <__VA_ARGC__>}{#define L2(...) <__VA_ARGC__> L3(__VA_ARGS__)}"
        "{#define L1(...) <__VA_ARGC__> L2(__VA_ARGS__)}"
        "{#define TW(...) L3(__VA_ARGS__) L3(__VA_ARGS__)}";
    EXPECT_EQ("<3> <3> <3>", pp(defs + kStx + "L1(A, STX(case N of 1, 2: Y := 1;), B)"));
    EXPECT_EQ("<2> <2>", pp(defs + "TW(A, __nosplit(b, c))"));
    EXPECT_TRUE(empty());
}

// T14: no hide set is added, and expansion still stops.
TEST_F(NosplitTest, HideSetsAndSelfReference) {
    EXPECT_EQ("A", pp("{#define A B}{#define B A}__nosplit(A)"));
    EXPECT_EQ("R", pp("{#define R __nosplit(R)}R"));
    EXPECT_EQ("a, b", pp("__nosplit(__nosplit)(a, b)"));
    EXPECT_EQ("1", pp("{#define K(x) x}__nosplit(K)(1)"));
    EXPECT_TRUE(empty());
}

// T15: a state directive in the arguments runs once at collection; a
// control directive there is PP02 (SEVERE: it stops the run).
TEST_F(NosplitTest, DirectivesInArguments) {
    EXPECT_EQ("1, b", pp("__nosplit({#define Q 1}Q, b)"));
    EXPECT_TRUE(empty());
    EXPECT_THROW(pp("__nosplit({#if 1}a{#endif})"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

// T16
TEST_F(NosplitTest, NoParenIsPlainText) {
    EXPECT_EQ("__nosplit;", pp("__nosplit;"));
    EXPECT_TRUE(empty());
}

// T17: each nesting level is one expand() call, bounded by the expansion
// depth limit (lowered here: library mode has no stack guard, and a deep
// nesting would overflow the 1 MiB main-thread stack of a Debug build).
TEST_F(RobustnessTestNosplit, DeepNestingReportsPP60) {
    Env env = setup();
    env.set_max_expansion_depth(8);
    const int k = 12;
    std::string src;
    for (int i = 0; i < k; ++i)
        src += "__nosplit(";
    src += "x";
    src += std::string(static_cast<std::size_t>(k), ')');
    try {
        pp(src, env);
        FAIL() << "expected Issue::Exception";
    } catch (const Issue::Exception& e) {
        EXPECT_EQ(Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED, e.code);
    }
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED, code());
    EXPECT_EQ(0, env.expansion_depth());
}

// T19: the core behaviour, also in sandbox builds.
TEST_F(RobustnessTestNosplit, CoreBehaviour) {
    EXPECT_EQ("<2> <2>", pp(kTwoStage + kStx + "OUT(A, STX(case N of 1, 2: Y := 1;))"));
    EXPECT_EQ("<2> <3>", pp(kTwoStage + kStxr + "OUT(A, STXR(case N of 1, 2: Y := 1;))"));
    EXPECT_EQ("a, , b", pp("__nosplit(a, , b)"));
    EXPECT_EQ("y", pp("{#if __has_builtin(__nosplit)}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}
