#include "test_helper.hpp"

#include <sstream>
#include <string>
#include <vector>

// =====================================================================
// __xmlattr(args...) (SPECIFICATION.md 12.12): the arguments joined like
// @__VA_ARGS__ and macro-expanded once, turned into text like `@`, escaped
// for an XML attribute value (& < > " ' become entity references, no IEC '$'
// escaping) and returned as one double-quoted WSTRING token.
// =====================================================================

using ::testing::HasSubstr;

class XmlattrTest : public JieppTest {};

// The RobustnessTest* name matches the sandbox test presets' filter
// ^(Sandbox|RobustnessTest), so these run in sandbox builds too.
class RobustnessTestXmlattr : public JieppTest {};

namespace {
// A caller like rungst's CONTACT: the macro body names the parameter inside
// the call.
const std::string kContact =
    "{#define CONTACT(op) <LdObject xsi:type=\"Contact\" operand=__xmlattr(op)/>}";
} // namespace

// ---- the examples ----

TEST_F(XmlattrTest, QuotesBecomeApos) {
    EXPECT_EQ("\"Mode = &apos;RUN&apos;\"", pp("__xmlattr(Mode = 'RUN')"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, LessThanAndAmpersand) {
    EXPECT_EQ("\"A &lt; B &amp; C\"", pp("__xmlattr(A < B & C)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, TimeLiteralUnchanged) {
    EXPECT_EQ("\"T#2s\"", pp("__xmlattr(T#2s)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, EmptyCall) {
    EXPECT_EQ("\"\"", pp("__xmlattr()"));
    EXPECT_EQ("\"\"", pp("__xmlattr(   )"));
    EXPECT_EQ("\"\"", pp("__xmlattr((* c *))"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, DollarIsNotEscaped) {
    EXPECT_EQ("\"&apos;it$&apos;s&apos;\"", pp("__xmlattr('it$'s')"));
    EXPECT_EQ("\"$\"", pp("__xmlattr($)"));
    EXPECT_EQ("\"&apos;$$&apos;\"", pp("__xmlattr('$$')"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, TwoArgumentsJoinedWithComma) {
    EXPECT_EQ("\"a, b\"", pp("__xmlattr(a, b)"));
    EXPECT_TRUE(empty());
}

// The comma spacing is that of `@__VA_ARGS__`.
TEST_F(XmlattrTest, CommaSpacingMatchesStringize) {
    const std::string s = "{#define S(...) @__VA_ARGS__}";
    EXPECT_EQ("\"a , b\"", pp("__xmlattr(a ,  b)"));
    EXPECT_EQ("'a , b'", pp(s + "S(a ,  b)"));
    EXPECT_EQ("\"a,b\"", pp("__xmlattr(a,b)"));
    EXPECT_EQ("'a,b'", pp(s + "S(a,b)"));
    EXPECT_EQ("\"a , b\"", pp("__xmlattr( a , b )"));
    EXPECT_EQ("'a , b'", pp(s + "S( a , b )"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, QuotAndGreaterThanAreEscaped) {
    EXPECT_EQ("\"a&gt;b &quot;q&quot;\"", pp("__xmlattr(a>b \"q\")"));
    EXPECT_TRUE(empty());
}

// ---- name, availability ----

TEST_F(XmlattrTest, NameIsCaseSensitive) {
    EXPECT_EQ("\"a\"", pp("__xmlattr(a)"));
    EXPECT_EQ("__XMLATTR(a)", pp("__XMLATTR(a)"));
    EXPECT_EQ("__Xmlattr(a)", pp("__Xmlattr(a)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, FeatureTests) {
    EXPECT_EQ("yz", pp("{#ifdef __xmlattr}y{#endif}{#if __has_builtin(__xmlattr)}z{#endif}"));
    EXPECT_EQ("n", pp("{#if __has_builtin(__XMLATTR)}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, UndefWarnsPP35AndRemovesIt) {
    Env env = setup();
    EXPECT_EQ("n", pp("{#undef __xmlattr}{#if __has_builtin(__xmlattr)}y{#else}n{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmlattr: undefining built-in macro"));
    EXPECT_EQ("__xmlattr(a)", pp("__xmlattr(a)", env));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, RedefineWarnsPP35) {
    Env env = setup();
    EXPECT_EQ("n", pp("{#define __xmlattr 1}{#if __has_builtin(__xmlattr)}y{#else}n{#endif}", env));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmlattr: redefining built-in macro"));
    EXPECT_TRUE(empty());
}

// -D goes through setup()'s predefined macros, -U through apply_undef_option().
TEST_F(XmlattrTest, DefineOptionWarnsPP35) {
    Env env = setup({{"__xmlattr", "1"}});
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP35"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmlattr: redefining built-in macro"));
    EXPECT_EQ("n", pp("{#if __has_builtin(__xmlattr)}y{#else}n{#endif}", env));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, UndefOptionWarnsPP35) {
    Env env = setup();
    apply_undef_option("__xmlattr", env);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("__xmlattr: undefining built-in macro"));
    EXPECT_EQ("n", pp("{#if __has_builtin(__xmlattr)}y{#else}n{#endif}", env));
    EXPECT_TRUE(empty());
}

// Guard: passes on the old source too (nothing was listed before either).
TEST_F(XmlattrTest, GuardNotListedByDumpMacros) {
    Env env = setup();
    std::ostringstream os;
    dump_macros(env, os);
    EXPECT_EQ(std::string::npos, os.str().find("__xmlattr"));
    EXPECT_TRUE(empty());
}

// Guard (watchpoint): an identical redefinition stays silent.
TEST_F(XmlattrTest, GuardIdenticalRedefinitionIsSilent) {
    EXPECT_EQ("1", pp("{#define A 1}{#define A 1}A"));
    EXPECT_TRUE(empty());
}

// Guard (watchpoint): a comma inside [...] stays in one argument.
TEST_F(XmlattrTest, GuardBracketCommaStaysOneArgument) {
    EXPECT_EQ("<a[x,y]>", pp("{#define F(x) <x>}F(a[x,y])"));
    EXPECT_EQ("\"a[x,y]\"", pp("__xmlattr(a[x,y])"));
    EXPECT_TRUE(empty());
}

// ---- errors and edge cases ----

TEST_F(XmlattrTest, UnclosedParenthesisIsPP34) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("x ", pp("x __xmlattr(a"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr("PP34"));
    EXPECT_THAT(msgs[0], HasSubstr("__xmlattr: missing closing parenthesis"));
}

TEST_F(XmlattrTest, WithoutParenthesisPassesThrough) {
    EXPECT_EQ("\"a\"", pp("__xmlattr(a)"));
    EXPECT_EQ("__xmlattr + 1", pp("__xmlattr + 1"));
    EXPECT_EQ("x __xmlattr", pp("x __xmlattr"));
    EXPECT_TRUE(empty());
}

// ---- argument expansion ----

// The built-in expands its arguments once, like __nosplit.
TEST_F(XmlattrTest, DirectUseExpandsArgument) {
    EXPECT_EQ("\"RUN\"", pp("{#define MODE RUN}__xmlattr(MODE)"));
    EXPECT_TRUE(empty());
}

// A parameter inside the call is pre-expanded like any other parameter.
TEST_F(XmlattrTest, ParameterIsPreExpandedThroughMacro) {
    EXPECT_EQ("<LdObject xsi:type=\"Contact\" operand=\"RUN\"/>",
              pp("{#define MODE RUN}" + kContact + "CONTACT(MODE)"));
    EXPECT_EQ("<LdObject xsi:type=\"Contact\" operand=\"a &lt; b\"/>",
              pp(kContact + "CONTACT(a < b)"));
    EXPECT_TRUE(empty());
}

// `@op` is the way to get the unexpanded text.
// Guard: `@` is unchanged (passes on the old source too).
TEST_F(XmlattrTest, GuardStringizeGivesUnexpandedText) {
    EXPECT_EQ("<x v='MODE'/>",
              pp("{#define MODE RUN}{#define S(op) <x v=@op/>}S(MODE)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, CounterAdvancesOncePerCall) {
    EXPECT_EQ("\"0\" 1", pp("__xmlattr(__COUNTER__) __COUNTER__"));
    EXPECT_EQ("\"0 1\" 2", pp("__xmlattr(__COUNTER__ __COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, CounterThroughMacroAdvancesOnce) {
    EXPECT_EQ("\"0\" 1", pp("{#define C(x) __xmlattr(x)}C(__COUNTER__) __COUNTER__"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, ParameterUsedTwiceInBody) {
    EXPECT_EQ("\"0 0\" 1", pp("{#define C(x) __xmlattr(x x)}C(__COUNTER__) __COUNTER__"));
    EXPECT_EQ("\"a\"-\"a\"", pp("{#define D(x) __xmlattr(x)-__xmlattr(x)}D(a)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, VaArgs) {
    const std::string v = "{#define V(...) __xmlattr(__VA_ARGS__)}";
    EXPECT_EQ("\"a , b\"", pp(v + "V(a ,  b)"));
    EXPECT_EQ("\"\"", pp(v + "V()"));
    EXPECT_EQ("\"x,y\"", pp(v + "V(x,y)"));
    EXPECT_EQ("\"x, y, z\"", pp(v + "V(x, y, z)"));
    EXPECT_TRUE(empty());
}

// Nesting: the inner call gives "x" (a WSTRING), whose text the outer call
// then escapes.
TEST_F(XmlattrTest, Nesting) {
    EXPECT_EQ("\"&quot;x&quot;\"", pp("__xmlattr(__xmlattr(x))"));
    EXPECT_TRUE(empty());
}

// ---- the result token ----

TEST_F(XmlattrTest, ResultIsOneArgumentOfLaterCall) {
    // The comma is inside one token, so it does not separate arguments.
    EXPECT_EQ("1", pp("{#define F(...) __VA_ARGC__}F(__xmlattr(a, b))"));
    EXPECT_EQ("\"a, b\"", pp("{#define I(x) x}I(__xmlattr(a, b))"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, DoesNotMergeWithAdjacentLiterals) {
    EXPECT_EQ("\"a\" 'x'", pp("__xmlattr(a) 'x'"));
    EXPECT_EQ("'x' \"a\"", pp("'x' __xmlattr(a)"));
    EXPECT_EQ("'x'\"a\"'y'", pp("'x'__xmlattr(a)'y'"));
    EXPECT_EQ("\"a\" \"w\"", pp("__xmlattr(a) \"w\""));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, ResultIsNotRescannedAsMacroName) {
    // Q is a function-like macro: its name alone is left as text, and the
    // result must not turn into a call of Q.
    EXPECT_EQ("\"Q\"(1)", pp("{#define Q(x) <x>}__xmlattr(Q)(1)"));
    EXPECT_EQ("<x>", pp("{#define Q(x) <x>}Q(x)"));
    EXPECT_TRUE(empty());
}

// ---- text building ----

TEST_F(XmlattrTest, CommentsAreDropped) {
    EXPECT_EQ("\"a b\"", pp("__xmlattr(a (* c *) b)"));
    EXPECT_EQ("\"a b\"", pp("__xmlattr(a/**/b)"));
    EXPECT_EQ("\"a\"", pp("__xmlattr( (*c*) a (*d*) )"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, WhitespaceRunsBecomeOneSpace) {
    EXPECT_EQ("\"a b\"", pp("__xmlattr(a   b)"));
    EXPECT_EQ("\"a b\"", pp("__xmlattr(a\t\tb)"));
    EXPECT_EQ("\"a b\"", pp("__xmlattr(  a  b  )"));
    EXPECT_TRUE(empty());
}

// A pragma token is kept as spelled (in the library's default annotated
// style that is (*{...}*)), then escaped like the rest.
TEST_F(XmlattrTest, PragmaIsKeptAndEscaped) {
    EXPECT_EQ("\"a (*{attribute   &apos;x&apos; &lt; y}*) b\"",
              pp("__xmlattr(a   {attribute   'x' < y}   b)"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, DocCommentIsKeptAndEscaped) {
    EXPECT_EQ("\"a (*! d &lt;  e *) b\"", pp("__xmlattr(a   (*! d <  e *)   b)"));
    EXPECT_TRUE(empty());
}

// A raw LF, CR or TAB inside a kept token becomes one space each.
TEST_F(XmlattrTest, RawTabInStringBecomesSpace) {
    EXPECT_EQ("\"&apos;a b&apos;\"", pp("__xmlattr('a\tb')"));
    EXPECT_EQ("\"&apos;a  b&apos;\"", pp("__xmlattr('a\t\tb')"));
    EXPECT_TRUE(empty());
}

TEST_F(XmlattrTest, RawLineBreaksInPragmaBecomeSpaces) {
    // The pragma's own line break is printed after the call.
    EXPECT_EQ("\"(*{attribute x  y}*)\"\n", pp("__xmlattr({attribute x\r\n y})"));
    EXPECT_EQ("\"(*{attribute x  y}*)\"\n", pp("__xmlattr({attribute x\n\ty})"));
    EXPECT_TRUE(empty());
}

// Control characters and non-ASCII bytes are not checked and pass through.
TEST_F(XmlattrTest, ControlAndNonAsciiBytesPassThrough) {
    const std::string in = std::string("__xmlattr('a\001") + "\177\303\251\377" + "z')";
    const std::string out = std::string("\"&apos;a\001") + "\177\303\251\377" + "z&apos;\"";
    EXPECT_EQ(out, pp(in));
    EXPECT_TRUE(empty());
}

// ---- sandbox coverage ----

TEST_F(RobustnessTestXmlattr, WorksAndIsLinear) {
    EXPECT_EQ("\"A &lt; B &amp; &apos;C&apos;\"", pp("__xmlattr(A < B & 'C')"));
    EXPECT_EQ("\"x\"", pp("{#define V(...) __xmlattr(__VA_ARGS__)}V(x)"));
    EXPECT_EQ("yz", pp("{#ifdef __xmlattr}y{#endif}{#if __has_builtin(__xmlattr)}z{#endif}"));
    EXPECT_TRUE(empty());
}

// The output is at most six times the input: every & becomes &amp; and every
// ' inside the string literal below becomes &apos;.
TEST_F(RobustnessTestXmlattr, LongArgumentStaysBounded) {
    std::string in = "__xmlattr(" + std::string(10000, '&') + ")";
    std::string out = pp(in);
    EXPECT_EQ(10000u * 5 + 2, out.size());
    EXPECT_LE(out.size(), 6 * in.size());
    in = "__xmlattr('" + std::string(10000, 'a') + "')";
    out = pp(in);
    EXPECT_EQ(10000u + 2 * 6 + 2, out.size());
    EXPECT_LE(out.size(), 6 * in.size());
    EXPECT_TRUE(empty());
}
