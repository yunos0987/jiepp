#include "test_helper.hpp"

// ---- one space at a macro-expansion edge (SPEC §3.9) ----
//
// Where a macro's replacement, a substituted argument or an empty expansion
// puts two tokens next to each other that would read as other tokens, one
// space goes between them. The inputs are those measured with clang 22.1
// (--target=x86_64-pc-linux-gnu -E -P) and gcc 13.3 (-E -P); the expected
// spacing is the clang output except where IEC 61131-3 and C read the pair
// differently (noted per case).

class ExpansionEdgeTest : public JieppTest {};

TEST_F(ExpansionEdgeTest, MeasuredTable) {
    const std::string defs =
        "{#define F(a) a}{#define EMPTY}{#define LT <}{#define SL /}{#define ST *}";
    // clang/gcc: the same.
    EXPECT_EQ("a b", pp(defs + "F(a)b"));
    EXPECT_EQ("1 2", pp(defs + "F(1)F(2)"));
    EXPECT_EQ("1 .5", pp(defs + "F(1).5"));
    EXPECT_EQ("a < = b", pp(defs + "a LT= b"));
    EXPECT_EQ("/ / x", pp(defs + "/SL x"));
    EXPECT_EQ("/ * x", pp(defs + "/ST x"));
    // clang: `(* x` (two tokens in C); in IEC `(*` opens a comment.
    EXPECT_EQ("( * x", pp(defs + "(ST x"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, OnlyAtExpansionEdges) {
    // Text written together in the source, in a macro body or in an
    // argument keeps its spelling.
    EXPECT_EQ("ab <= 1.5 x:=y", pp("ab <= 1.5 x:=y"));
    EXPECT_EQ("a<=b", pp("{#define O a<=b}O"));
    EXPECT_EQ("[x<=y]", pp("{#define W(x) [x]}W(x<=y)"));
    // Each edge: before the replacement, after it, around an argument and
    // around an empty expansion.
    EXPECT_EQ("< =", pp("{#define F(a) a}<F(=)"));
    EXPECT_EQ("< =", pp("{#define H(x) x=}H(<)"));
    EXPECT_EQ(": =", pp("{#define H(x) x=}H(:)"));
    EXPECT_EQ("= >", pp("{#define G(x) =x}G(>)"));
    EXPECT_EQ(". .", pp("{#define EMPTY}.EMPTY."));
    EXPECT_EQ("< >", pp("{#define EMPTY}<EMPTY>"));
    EXPECT_EQ("* *", pp("{#define EMPTY}*EMPTY*"));
    EXPECT_EQ("( *", pp("{#define EMPTY}(EMPTY*"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, PairsThatAreOneTokenOnlyInC) {
    // clang puts a space in each of these (`+ +`, `- -`, `& &`, `= =`, ...);
    // in IEC 61131-3 they stay two tokens, so jiepp keeps them together.
    const std::string defs = "{#define EMPTY}{#define F(x) x}";
    EXPECT_EQ("++ -- && || == != << >> -> +-",
              pp(defs + "+EMPTY+ -EMPTY- &EMPTY& |EMPTY| =EMPTY= !EMPTY= <EMPTY< >EMPTY> "
                        "F(-)F(>) F(+)F(-)"));
    // `:=`, `<>`, `**`, `..` are one token in IEC but not in C: clang keeps
    // them together, jiepp separates them.
    EXPECT_EQ(": = < > * * . .", pp(defs + ":EMPTY= <EMPTY> *EMPTY* .EMPTY."));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, NumbersSplitByAnEdge) {
    const std::string defs = "{#define ONE 1}{#define TWO 2}{#define D .5}{#define F(x) x}";
    // clang: `1 . 2`, `1 . 1`, `. 1`, `1 .`, `1 .5`, `1 ..5` -- the same
    // tokens; jiepp puts one space where the number would otherwise grow.
    EXPECT_EQ("1. 2 1. 1 .1 1. 1 .5 1. .5",
              pp(defs + "ONE.TWO ONE.ONE .ONE ONE. ONE D ONE.D"));
    EXPECT_EQ("1 .5 1 .5", pp(defs + "F(1).5 F(1)F(.5)"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, IecOperatorsAcrossAnEdge) {
    const std::string defs =
        "{#define ASSIGN :=}{#define ARROW =>}{#define NE <>}{#define POW **}{#define RNG ..}";
    // Already read the same way on each side: no space (clang separates the
    // C tokens `:= =` and `<> =`).
    EXPECT_EQ(":== =>> <>= ::= <:= *** ...", pp(defs + "ASSIGN= ARROW> NE= :ASSIGN <ASSIGN POW* RNG."));
    // `*` `**` would read as `**` `*`, `.` `..` as `..` `.`.
    EXPECT_EQ("* ** . ..", pp(defs + "*POW .RNG"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, TypedLiteralHashIsNotSeparated) {
    // The lexer splits a typed literal at its '#': no space next to it.
    EXPECT_EQ("16#FF T#1s", pp("{#define N 16}{#define U 1s}N#FF T#U"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, LiteralsAndComments) {
    // Two literals with the same quote are merged with or without a space
    // (SPEC §2), so none is put between them (clang: `'x''y'`).
    EXPECT_EQ("'x''y'", pp("{#define EMPTY}'x'EMPTY'y'"));
    // A comment after a '/' would become `//`.
    EXPECT_EQ("/ /* c */y", pp("{#define C(x) x/* c */y}C(/)"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, AcrossADirective) {
    // A directive between the two outputs nothing: the token after it
    // meets the replacement.
    EXPECT_EQ("2 1", pp("{#define X 2}{#define Y 1}X{#define Z}Y"));
    EXPECT_EQ("1 n", pp("{#define X 1}X{#ifdef X}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, TokenDirectiveIsSplicedAsWritten) {
    // {#token} joins its output to the text around it as written, on both
    // sides (`Main{### __COUNTER__}` is `Main0`); the expansion edges inside
    // its operand follow the rule.
    EXPECT_EQ("Main0 x", pp("Main{### __COUNTER__} x"));
    EXPECT_EQ("Main1 2", pp("{#define F(a) a}Main{### F(1)F(2)}"));
    // After a replacement, and before text or a replacement.
    EXPECT_EQ("fb0_t fb1 x fb_a_b fb_c",
              pp("{#define PFX fb}PFX{### __COUNTER__}_t PFX{### __COUNTER__} x "
                 "PFX{#token _a}_b {### PFX}_c"));
    EXPECT_EQ("x12 1x 1_t", pp("{#define F(a) a}{#define PFX 1}F(x){### 1}2 {### 1}F(x) PFX{### _}t"));
    // {#string} likewise; an operand that outputs nothing passes the edge on.
    EXPECT_EQ("x'a'y", pp("{#define F(a) a}F(x){#string a}F(y)"));
    EXPECT_EQ("x y", pp("{#define F(a) a}F(x){#token}F(y)"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, TextOperandsLeaveTheSpaceOut) {
    // A message is the expansion's text without the edge spaces (as
    // stringizing; `MAJOR.MINOR` is `1.2`, `F(a)b` is `ab`).
    const std::string defs = "{#define MAJOR 1}{#define MINOR 2}{#define F(a) a}";
    pp(defs + "{#warning v MAJOR.MINOR F(a)b}");
    EXPECT_EQ(std::vector<std::string>{"<unknown location>:1.0: warning: PP92: 'v 1.2 ab'"},
              messages());
    Issue::ContinueMode guard({});
    pp(defs + "__error(v MAJOR.MINOR F(a)b)");
    EXPECT_EQ(std::vector<std::string>{"<unknown location>:1.0: error: PP91: 'v 1.2 ab'"},
              messages());
}

TEST_F(ExpansionEdgeTest, StringizingDoesNotCountTheSpace) {
    // gcc/clang spell the tokens of an expansion without the space they
    // print between them: `"12"`, `"0.1"`.
    EXPECT_EQ("'12'", pp("{#define F(a) a}{#string F(1)F(2)}"));
    EXPECT_EQ("'0.1'", pp("{## __COUNTER__.__COUNTER__}"));
    EXPECT_EQ("'12'", pp("{#define F(a) a}{#define S(x) @x}{#define XS(x) S(x)}XS(F(1)F(2))"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, MeasuredIdentifierAndNumberCases) {
    // clang/gcc: `OBJy a b a b OBJ1 x.5 1OBJ` -- the same.
    EXPECT_EQ("OBJy a b a b OBJ1 x.5 1OBJ",
              pp("{#define F(a) a}{#define OBJ x}OBJy F(a)b F(a)F(b) OBJ1 OBJ.5 1OBJ"));
    EXPECT_TRUE(empty());
    Issue::ContinueMode guard({});
    // clang: `1e 1e5 1e+ 5 12 .5 0x1F 1.5 1.5 1e + a1`. `1e+`, `.5`, `1.` are
    // not IEC tokens: those pastes fail (PP32) and the space keeps the
    // remaining tokens from reading as one number (`1e +5`, `1 .5`, `1. 5`).
    EXPECT_EQ("1e 1e5 1e +5 12 .5 0x1F 1 .5 1. 5 1e+ a1",
              pp("{#define C(a,b) a@@b}C(1,e) C(1e,5) C(1e,+5) C(1,2) C(.,5) C(0x,1F) C(1,.5) "
                 "C(1.,5) C(1,e+) C(a,1)"));
    EXPECT_EQ(4u, messages().size());
    // clang: `1.0e- 5 1.0e5 1.0e5 1p+ 2 0x1p+ 2 0x1p2`.
    EXPECT_EQ("1.0e -5 1.0e5 1.0e5 1p+2 0x1p+2 0x1p2",
              pp("{#define C(a,b) a@@b}C(1.0e,-5) C(1.0e,5) C(1.0,e5) C(1p,+2) C(0x1p,+2) "
                 "C(0x1p,2)"));
    EXPECT_EQ(3u, messages().size());
}

TEST_F(ExpansionEdgeTest, FailedPasteKeepsTheTokensApart) {
    // clang 22: `ID(a, b) ## y` gives `a b y`; `/ ## /` gives `/ /`.
    Issue::ContinueMode guard({});
    EXPECT_EQ("a b y", pp("{#define ID(a, b) a b}{#define Q ID(a, b) @@ y}Q"));
    EXPECT_EQ("/ /", pp("{#define P(a, b) a @@ b}P(/, /)"));
    EXPECT_EQ(2u, messages().size());
}

TEST_F(ExpansionEdgeTest, ConditionOperandsFollowTheRule) {
    // `1 LT= 2` is `1 < = 2` in a condition too, not `1 <= 2` (clang: "invalid
    // token at start of a preprocessor expression", gcc: token "=" is not
    // valid).
    EXPECT_THROW(pp("{#define LT <}{#if 1 LT= 2}y{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    EXPECT_EQ("y", pp("{#define LT <}{#if 1 LT 2}y{#endif}"));
}

// ---- a number continued by letters is one token (SPEC §2) ----

TEST_F(ExpansionEdgeTest, NumberWithLettersIsNotExpanded) {
    // clang 22 / gcc 13: the pp-number is not expanded inside.
    const std::string defs =
        "{#define abc X}{#define ms *1000}{#define e X}{#define s X}{#define A X}{#define f X}";
    EXPECT_EQ("12abc 10ms 1e 1s 1A 1.5f 1e+5 1_000 1__0",
              pp(defs + "12abc 10ms 1e 1s 1A 1.5f 1e+5 1_000 1__0"));
    // Typed literals stay as they were.
    EXPECT_EQ("T#10ms 16#abc T#1s", pp(defs + "T#10ms 16#abc T#1s"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionEdgeTest, RangesStillExpand) {
    // Unlike a C pp-number, '.' and a sign end the number: the name after
    // `..` or `.` is still expanded (clang: `0..MAX`, `1.MAX` unexpanded).
    const std::string defs = "{#define MAX 9}{#define N 5}{#define f X}";
    EXPECT_EQ("ARRAY[0..9] 1..5 X[1..5] 1.5..5", pp(defs + "ARRAY[0..MAX] 1..N X[1..N] 1.5..N"));
    // `1.9` would read as one number: the edge keeps them apart.
    EXPECT_EQ("1. 9 2.X 1e+X", pp(defs + "1.MAX 2.f 1e+f"));
    EXPECT_TRUE(empty());
}
