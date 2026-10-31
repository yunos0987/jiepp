#include "test_helper.hpp"

#include <sstream>
#include <string>
#include <vector>

// =====================================================================
// __xmltext(args...) (SPECIFICATION.md 12.12): the same text as
// __xmlattr(args...) (joined like @__VA_ARGS__, expanded once, built like
// `@`, no IEC '$' escaping), for XML element content: not wrapped in quotes
// and only & < > are escaped (" and ' stay).
// =====================================================================

using ::testing::HasSubstr;

class XmltextTest : public JieppTest {};

// The RobustnessTest* name matches the sandbox test presets' filter
// ^(Sandbox|RobustnessTest), so these run in sandbox builds too.
class RobustnessTestXmltext : public JieppTest {};

namespace {
// A caller like rungst's element writers: the parameter is named inside the
// call.
const std::string kElem = "{#define ELEM(t) <Text>__xmltext(t)</Text>}";
} // namespace

// ---- the examples ----

TEST_F(XmltextTest, AssignmentWithLessThan) {
    EXPECT_EQ("a := b; c &lt; d", pp("__xmltext(a := b; c < d)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, QuotesStayAsTheyAre) {
    EXPECT_EQ("Mode = 'RUN'", pp("__xmltext(Mode = 'RUN')"));
    EXPECT_EQ("say \"hi\"", pp("__xmltext(say \"hi\")"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, EmptyCall) {
    EXPECT_EQ("[]", pp("[__xmltext()]"));
    EXPECT_EQ("[]", pp("[__xmltext(   )]"));
    EXPECT_EQ("[]", pp("[__xmltext((* c *))]"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, AmpersandLessGreater) {
    EXPECT_EQ("A &lt; B &amp; C &gt; D", pp("__xmltext(A < B & C > D)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, DollarIsNotEscaped) {
    EXPECT_EQ("'it$'s'", pp("__xmltext('it$'s')"));
    EXPECT_EQ("$", pp("__xmltext($)"));
    EXPECT_EQ("'$$'", pp("__xmltext('$$')"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, MultipleArgumentsJoinedLikeStringize) {
    const std::string s = "{#define S(...) @__VA_ARGS__}";
    EXPECT_EQ("a, b", pp("__xmltext(a, b)"));
    EXPECT_EQ("a , b", pp("__xmltext(a ,  b)"));
    EXPECT_EQ("'a , b'", pp(s + "S(a ,  b)"));
    EXPECT_EQ("a,b", pp("__xmltext(a,b)"));
    EXPECT_EQ("'a,b'", pp(s + "S(a,b)"));
    EXPECT_TRUE(empty());
}

// ---- name, availability ----

TEST_F(XmltextTest, NameIsCaseSensitive) {
    EXPECT_EQ("a", pp("__xmltext(a)"));
    EXPECT_EQ("__XMLTEXT(a)", pp("__XMLTEXT(a)"));
    EXPECT_EQ("__Xmltext(a)", pp("__Xmltext(a)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, FeatureTests) {
    EXPECT_EQ("yz", pp("{#ifdef __xmltext}y{#endif}{#if __has_builtin(__xmltext)}z{#endif}"));
    EXPECT_EQ("n", pp("{#if __has_builtin(__XMLTEXT)}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, UndefWarnsPP35AndRemovesIt) {
    Env env = setup();
    EXPECT_EQ("n", pp("{#undef __xmltext}{#if __has_builtin(__xmltext)}y{#else}n{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmltext: undefining built-in macro"));
    EXPECT_EQ("__xmltext(a)", pp("__xmltext(a)", env));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, RedefineWarnsPP35) {
    Env env = setup();
    EXPECT_EQ("n", pp("{#define __xmltext 1}{#if __has_builtin(__xmltext)}y{#else}n{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmltext: redefining built-in macro"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, DefineOptionWarnsPP35) {
    Env env = setup({{"__xmltext", "1"}});
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmltext: redefining built-in macro"));
    EXPECT_EQ("n", pp("{#if __has_builtin(__xmltext)}y{#else}n{#endif}", env));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, UndefOptionWarnsPP35) {
    Env env = setup();
    apply_undef_option("__xmltext", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("__xmltext: undefining built-in macro"));
    EXPECT_EQ("n", pp("{#if __has_builtin(__xmltext)}y{#else}n{#endif}", env));
    EXPECT_TRUE(empty());
}

// Guard: passes on the old source too (nothing was listed before either).
TEST_F(XmltextTest, GuardNotListedByDumpMacros) {
    Env env = setup();
    std::ostringstream os;
    dump_macros(env, os);
    EXPECT_EQ(std::string::npos, os.str().find("__xmltext"));
    EXPECT_TRUE(empty());
}

// ---- errors and edge cases ----

TEST_F(XmltextTest, UnclosedParenthesisIsPP34) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("x ", pp("x __xmltext(a"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP34"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmltext: missing closing parenthesis"));
}

TEST_F(XmltextTest, WithoutParenthesisPassesThrough) {
    EXPECT_EQ("a", pp("__xmltext(a)"));
    EXPECT_EQ("__xmltext + 1", pp("__xmltext + 1"));
    EXPECT_EQ("x __xmltext", pp("x __xmltext"));
    EXPECT_TRUE(empty());
}

// ---- argument expansion ----

TEST_F(XmltextTest, DirectUseExpandsArgument) {
    EXPECT_EQ("RUN", pp("{#define MODE RUN}__xmltext(MODE)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, ParameterIsPreExpandedThroughMacro) {
    EXPECT_EQ("<Text>RUN</Text>", pp("{#define MODE RUN}" + kElem + "ELEM(MODE)"));
    EXPECT_EQ("<Text>a &amp; b</Text>", pp(kElem + "ELEM(a & b)"));
    EXPECT_TRUE(empty());
}

// Guard: `@` is unchanged (passes on the old source too).
TEST_F(XmltextTest, GuardStringizeGivesUnexpandedText) {
    EXPECT_EQ("<x v='MODE'/>",
              pp("{#define MODE RUN}{#define S(op) <x v=@op/>}S(MODE)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, CounterAdvancesOncePerCall) {
    EXPECT_EQ("0 1", pp("__xmltext(__COUNTER__) __COUNTER__"));
    EXPECT_EQ("0 1 2", pp("__xmltext(__COUNTER__ __COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, CounterThroughMacroAdvancesOnce) {
    EXPECT_EQ("0 1", pp("{#define C(x) __xmltext(x)}C(__COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, ParameterUsedTwiceInBody) {
    EXPECT_EQ("0 0 1", pp("{#define C(x) __xmltext(x x)}C(__COUNTER__) __COUNTER__"));
    EXPECT_EQ("a-a", pp("{#define D(x) __xmltext(x)-__xmltext(x)}D(a)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, VaArgs) {
    const std::string v = "{#define V(...) __xmltext(__VA_ARGS__)}";
    EXPECT_EQ("a , b", pp(v + "V(a ,  b)"));
    EXPECT_EQ("[]", pp("[" + v + "V()]"));
    EXPECT_EQ("x,y", pp(v + "V(x,y)"));
    EXPECT_EQ("x, y, z", pp(v + "V(x, y, z)"));
    EXPECT_TRUE(empty());
}

// The inner call gives the escaped text, which the outer call escapes again.
TEST_F(XmltextTest, Nesting) {
    EXPECT_EQ("a &amp;lt; b", pp("__xmltext(__xmltext(a < b))"));
    EXPECT_EQ("\"x\"", pp("__xmltext(__xmlattr(x))"));
    EXPECT_EQ("\"a &amp;lt; b\"", pp("__xmlattr(__xmltext(a < b))"));
    EXPECT_TRUE(empty());
}

// ---- the result token ----

// One token: its commas do not separate arguments of a later call.
TEST_F(XmltextTest, ResultIsOneArgumentOfLaterCall) {
    EXPECT_EQ("1", pp("{#define F(...) __VA_ARGC__}F(__xmltext(a, b))"));
    EXPECT_EQ("a, b", pp("{#define I(x) x}I(__xmltext(a, b))"));
    EXPECT_EQ("p,q+r", pp("{#define A(x, y) x+y}A(__xmltext(p,q), r)"));
    EXPECT_TRUE(empty());
}

// The text is not scanned again: Q is a function-like macro, its name alone
// is left as text, and the result must not turn into a call of Q.
TEST_F(XmltextTest, ResultIsNotRescanned) {
    EXPECT_EQ("Q(1)", pp("{#define Q(x) <x>}__xmltext(Q)(1)"));
    EXPECT_EQ("<x>", pp("{#define Q(x) <x>}Q(x)"));
    EXPECT_EQ("ab", pp("__xmltext(a)__xmltext(b)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, AdjacentLiteralsAreNotMerged) {
    EXPECT_EQ("a 'x'", pp("__xmltext(a) 'x'"));
    EXPECT_EQ("'x' a", pp("'x' __xmltext(a)"));
    EXPECT_EQ("'x'a'y'", pp("'x'__xmltext(a)'y'"));
    EXPECT_EQ("a \"w\"", pp("__xmltext(a) \"w\""));
    EXPECT_TRUE(empty());
}

// ---- text building ----

TEST_F(XmltextTest, CommentsAreDropped) {
    EXPECT_EQ("a b", pp("__xmltext(a (* c *) b)"));
    EXPECT_EQ("a b", pp("__xmltext(a/**/b)"));
    EXPECT_EQ("a", pp("__xmltext( (*c*) a (*d*) )"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, WhitespaceRunsBecomeOneSpace) {
    EXPECT_EQ("a b", pp("__xmltext(a   b)"));
    EXPECT_EQ("a b", pp("__xmltext(a\t\tb)"));
    EXPECT_EQ("a b", pp("__xmltext(  a  b  )"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, PragmaIsKeptAndEscaped) {
    // In the library's default annotated style a pragma is spelled (*{...}*).
    EXPECT_EQ("a (*{attribute   'x' &lt; y}*) b", pp("__xmltext(a   {attribute   'x' < y}   b)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, DocCommentIsKeptAndEscaped) {
    EXPECT_EQ("a (*! d &lt;  e *) b", pp("__xmltext(a   (*! d <  e *)   b)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, RawTabInStringBecomesSpace) {
    EXPECT_EQ("'a b'", pp("__xmltext('a\tb')"));
    EXPECT_EQ("'a  b'", pp("__xmltext('a\t\tb')"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, RawLineBreaksInPragmaBecomeSpaces) {
    // The pragma's own line break is printed after the call.
    EXPECT_EQ("(*{attribute x  y}*)\n", pp("__xmltext({attribute x\r\n y})"));
    EXPECT_EQ("(*{attribute x  y}*)\n", pp("__xmltext({attribute x\n\ty})"));
    EXPECT_TRUE(empty());
}

TEST_F(XmltextTest, ControlAndNonAsciiBytesPassThrough) {
    const std::string in = std::string("__xmltext('a\001") + "\177\303\251\377" + "z')";
    const std::string out = std::string("'a\001") + "\177\303\251\377" + "z'";
    EXPECT_EQ(out, pp(in));
    EXPECT_TRUE(empty());
}

// ---- sandbox coverage ----

TEST_F(RobustnessTestXmltext, WorksAndIsLinear) {
    EXPECT_EQ("A &lt; B &amp; 'C'", pp("__xmltext(A < B & 'C')"));
    EXPECT_EQ("x", pp("{#define V(...) __xmltext(__VA_ARGS__)}V(x)"));
    EXPECT_EQ("yz", pp("{#ifdef __xmltext}y{#endif}{#if __has_builtin(__xmltext)}z{#endif}"));
    EXPECT_TRUE(empty());
}

// The output is at most four times the input (every < becomes &lt;).
TEST_F(RobustnessTestXmltext, LongArgumentStaysBounded) {
    const std::string lts(10000, '<');
    const std::string in = "__xmltext(" + lts + ")";
    const std::string out = pp(in);
    EXPECT_EQ(10000u * 4, out.size());
    EXPECT_LE(out.size(), 6 * in.size());
    EXPECT_TRUE(empty());
}
