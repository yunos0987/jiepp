#include "test_helper.hpp"

// Macro-expansion rules aligned with gcc/clang. Every input below was run
// through clang 22.1 (--target=x86_64-pc-linux-gnu -E -P) and gcc 13.3 (-E -P)
// with `#`/`##`/`"` for jiepp's `@`/`@@`/`'`; the expected values are the
// clang output (gcc agrees unless noted).

class ExpansionRulesTest : public JieppTest {
protected:
    // Runs `src` as the jiepp command does (continue mode) and returns the
    // output; the diagnostics are left for diags().
    std::string pp_continue(const std::string& src) {
        Issue::ContinueMode guard({});
        auto env = setup();
        return pp(src, env);
    }
    // The diagnostics without their location ("PP31: ...").
    std::vector<std::string> diags() {
        std::vector<std::string> out;
        for (const auto& m : messages()) {
            auto p = m.find(": PP");
            out.push_back(p == std::string::npos ? m : m.substr(p + 2));
        }
        return out;
    }
    static std::vector<std::string> codes_of(const std::vector<std::string>& ds) {
        std::vector<std::string> out;
        for (const auto& d : ds)
            out.push_back(d.substr(0, 4));
        return out;
    }
};

using Strings = std::vector<std::string>;

// ---- `a @@ @b`: the operand is stringized, then pasted (SPEC §4.3) ----

TEST_F(ExpansionRulesTest, PasteWithStringizedRightOperand) {
    const std::string defs = "{#define A(a,b) a @@ @b}{#define B(a,b) @a @@ b}"
                             "{#define C(a,b) @a @@ @b}{#define D(a) @ a @@ a}";
    // clang: `"y" "x"` (A(,y): the empty left operand is a placemarker).
    EXPECT_EQ("'y' 'x'", pp(defs + "A(,y) B(x,)"));
    EXPECT_TRUE(empty());
    // clang: `"x""y" """" "q"q`, each a failed paste.
    EXPECT_EQ("'x''y' '''' 'q'q", pp_continue(defs + "C(x,y) C(,) D(q)"));
    EXPECT_EQ(codes_of(diags()), (Strings{"PP32", "PP32", "PP32"}));
    // clang: `L"y"` (a wide literal in C); `L` and `'y'` are two IEC tokens.
    EXPECT_EQ("L'y' 1'y'", pp_continue(defs + "A(L,y) A(1,y)"));
    EXPECT_EQ(diags(), (Strings{"PP32: Invalid token pasting (@@); 'L @@ 'y''",
                                "PP32: Invalid token pasting (@@); '1 @@ 'y''"}));
}

// ---- __VA_OPT__ looks at the expanded variable arguments (SPEC §3.4) ----

TEST_F(ExpansionRulesTest, VaOptOfArgumentsThatExpandToNothing) {
    const std::string defs = "{#define EMPTY}{#define VO(...) [__VA_OPT__(a)]}";
    EXPECT_EQ("[] [] [] [] [] [a] [a] [a] [a]",
              pp(defs + "VO() VO( ) VO((*c*)) VO(EMPTY) VO(EMPTY EMPTY) VO(1) VO(,) VO( , ) "
                        "VO(EMPTY,EMPTY)"));
    // The other forms, a named variadic, and a non-variadic first parameter.
    EXPECT_EQ("'' x z [ ] [] []",
              pp(defs + "{#define VS(...) @__VA_OPT__(a)}{#define VP(...) x @@ __VA_OPT__(y)}"
                        "{#define VQ(...) __VA_OPT__(y) @@ z}{#define VE(...) [__VA_OPT__(a) __VA_ARGS__]}"
                        "{#define VN(x,...) [__VA_OPT__(a)]}{#define VA(args...) [__VA_OPT__(a)]}"
                        "VS(EMPTY) VP(EMPTY) VQ(EMPTY) VE(EMPTY) VN(1,EMPTY) VA(EMPTY)"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionRulesTest, VaOptExpandsTheArgumentsOnce) {
    // The expansion that decides __VA_OPT__ is the one __VA_ARGS__ uses:
    // __COUNTER__ moves once per call (clang: `[a 0] [a 1]`).
    EXPECT_EQ("[a 0] [a 1]",
              pp("{#define V(...) [__VA_OPT__(a) __VA_ARGS__]}V(__COUNTER__) V(__COUNTER__)"));
    EXPECT_EQ("[0 a] 1", pp("{#define V(...) [__VA_ARGS__ __VA_OPT__(a)]}V(__COUNTER__) __COUNTER__"));
    // Two or more variable arguments are expanded too, though they are not
    // empty anyway (clang 22 / gcc 13: `[x] [x] 2 [x] 4 [x] 7`).
    EXPECT_EQ("[x] [x] 2 [x] 4 [x] 7",
              pp("{#define U(...) [__VA_OPT__(x)]}{#define C __COUNTER__}{#define E}"
                 "U(C) U(C) C U(E C) C U(C,C) C"));
    // __nosplit() expands to nothing: empty, like EMPTY.
    EXPECT_EQ("[]", pp("{#define V(...) [__VA_OPT__(x)]}V(__nosplit())"));
    // __VA_ARGC__ still counts the arguments as written.
    EXPECT_EQ("1 0", pp("{#define EMPTY}{#define N(...) __VA_ARGC__}N(EMPTY) N()"));
    EXPECT_TRUE(empty());
}

// ---- `@` in a function-like body must name a parameter (SPEC §3.2) ----

TEST_F(ExpansionRulesTest, StrayStringizeRejectsTheDefinition) {
    struct Case { const char* def; const char* use; const char* out; };
    for (const Case& c : {
             // clang: error at the definition; the name stays undefined.
             Case{"F(x) @y", "F(1) end", "F(1) end"},
             Case{"F(x) x @", "F(1) end", "F(1) end"},
             Case{"F() @y", "F() end", "F() end"},
             Case{"F(x) @__VA_ARGS__", "F(1) end", "F(1) end"},
             Case{"V4(args...) @__VA_ARGS__", "V4(1) end", "V4(1) end"},
             Case{"F(...) __VA_OPT__(@z)", "F(1) end", "F(1) end"},
             Case{"F(x) @ @@ x", "F(1) end", "F(1) end"},
             Case{"F(x) x @@ @", "F(1) end", "F(1) end"},
         }) {
        SCOPED_TRACE(c.def);
        EXPECT_EQ(c.out, pp_continue(std::string("{#define ") + c.def + "}" + c.use));
        EXPECT_EQ(diags(), (Strings{std::string("PP31: Invalid stringizing (@); ''@' is not "
                                                "followed by a macro parameter: ") + c.def + "'"}));
    }
    // A parameter, __VA_ARGC__ or __VA_OPT__ after `@` (whitespace and
    // comments between allowed): clang accepts these.
    EXPECT_EQ("'1' '2' '1' ''",
              pp("{#define F(x) @   x}{#define G(x) @ (*c*) x}{#define N(...) @__VA_ARGC__}"
                 "{#define O(a) @__VA_OPT__(b)}F(1) G(2) N(z) O(1)"));
    EXPECT_TRUE(messages().size() == 1u); // PP80 for O
}

TEST_F(ExpansionRulesTest, StrayStringizeKeepsThePreviousDefinition) {
    // clang: error, no "macro redefined", the first F stays.
    EXPECT_EQ("[1] end", pp_continue("{#define F(x) [x]}{#define F(x) @y}F(1) end"));
    EXPECT_EQ(codes_of(diags()), (Strings{"PP31"}));
    EXPECT_EQ("1 end", pp_continue("{#define F(x, y) @z}{#define F(x, y) x}F(1,2) end"));
    EXPECT_EQ(codes_of(diags()), (Strings{"PP31"}));
}

TEST_F(ExpansionRulesTest, BodyErrorsInTheOrderClangReadsThem) {
    // The body is read left to right: the first `@` error or malformed
    // __VA_OPT__ is reported; `@@` at an end is noticed after the body.
    struct Case { const char* def; Strings codes; };
    for (const Case& c : {
             Case{"F(x) @@ @y", {"PP31"}},
             Case{"F(x) @y @@", {"PP31"}},
             Case{"F(x,x) @y", {"PP36"}},
             Case{"F(...) @y __VA_OPT__", {"PP31"}},
             Case{"F(...) __VA_OPT__ @y", {"PP37"}},
             Case{"F(...) @y __VA_OPT__(", {"PP31"}},
             Case{"F(...) __VA_OPT__( @y", {"PP31"}},
             Case{"F(...) @@ __VA_OPT__ x", {"PP37"}},
             Case{"F(...) __VA_OPT__ x @@", {"PP37"}},
             // A non-variadic body warns up to the token clang is reading.
             Case{"F(x) __VA_OPT__(a) @y __VA_OPT__(b)", {"PP80", "PP31", "PP80"}},
         }) {
        SCOPED_TRACE(c.def);
        EXPECT_EQ("F(1) end", pp_continue(std::string("{#define ") + c.def + "}F(1) end"));
        EXPECT_EQ(codes_of(diags()), c.codes);
    }
}

TEST_F(ExpansionRulesTest, ObjectMacroAtIsAnOrdinaryToken) {
    // clang: `# x #x # -#`, `#__VA_ARGS__ ##`, no diagnostic.
    EXPECT_EQ("@ x @x @ -@", pp("{#define H @ x}{#define H2 @x}{#define H3 @}H H2 H3 -H3"));
    EXPECT_EQ("@__VA_ARGS__ @@", pp("{#define O @__VA_ARGS__}{#define P @ @@ @}O P"));
    EXPECT_TRUE(empty());
}

// ---- the whitespace around `@` and `@@` in a redefinition (SPEC §3.6) ----

TEST_F(ExpansionRulesTest, RedefinitionComparesWhitespaceAroundOperators) {
    // clang/gcc warn for F, G and H, not for K and L.
    pp("{#define F(a) @ a}{#define F(a) @a}"
       "{#define G(a,b) a @@ b}{#define G(a,b) a@@b}"
       "{#define H(a,b) a @@b}{#define H(a,b) a@@ b}"
       "{#define K(a) @a}{#define K(a) @a}"
       "{#define L(a,b) a @@ b}{#define L(a,b) a  @@  b}"
       "{#define O x @@ y}{#define O x@@y}");
    EXPECT_EQ(diags(), (Strings{"PP35: Macro redefined; 'F'", "PP35: Macro redefined; 'G'",
                                "PP35: Macro redefined; 'H'", "PP35: Macro redefined; 'O'"}));
}

// ---- deferred expansion: a finished macro is not hidden (SPEC §3.7) ----

TEST_F(ExpansionRulesTest, DeferredExpansion) {
    const std::string defs = "{#define EMPTY}{#define DEFER(id) id EMPTY}"
                             "{#define EXPAND(...) __VA_ARGS__}"
                             "{#define R() x DEFER(R_I)()()}{#define R_I() R}";
    EXPECT_EQ("x x R_I ()()\nx x x R_I ()()\nx R_I ()()",
              pp(defs + "EXPAND(R())\nEXPAND(EXPAND(R()))\nR()"));
    EXPECT_TRUE(empty());
}

namespace {

const std::string kEvalDefs =
    "{#define EMPTY()}{#define DEFER(id) id EMPTY()}"
    "{#define OBSTRUCT(...) __VA_ARGS__ DEFER(EMPTY)()}"
    "{#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))}"
    "{#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))}"
    "{#define EVAL2(...) __VA_ARGS__}"
    "{#define CAT(a, ...) a @@ __VA_ARGS__}{#define INC(x) CAT(INC_, x)}"
    "{#define INC_0 1}{#define INC_1 2}{#define INC_2 3}{#define INC_3 4}"
    "{#define REPEAT(n) n OBSTRUCT(REPEAT_INDIRECT) () (INC(n))}"
    "{#define REPEAT_INDIRECT() REPEAT2}{#define REPEAT2(n) n}"
    "{#define LOOP(n) n DEFER(LOOP_I)()(INC(n))}{#define LOOP_I() LOOP}";

} // namespace

TEST_F(ExpansionRulesTest, DeferredRecursionWithEval) {
    EXPECT_EQ("0 1", pp(kEvalDefs + "EVAL(REPEAT(0))"));
    // The loop runs as many times as EVAL rescans, then stops.
    EXPECT_EQ("0 1 2 3 4 INC_4 INC_INC_4 INC_INC_INC_4 INC_INC_INC_INC_4 INC_INC_INC_INC_INC_4 "
              "INC_INC_INC_INC_INC_INC_4 INC_INC_INC_INC_INC_INC_INC_4 "
              "INC_INC_INC_INC_INC_INC_INC_INC_4 INC_INC_INC_INC_INC_INC_INC_INC_INC_4 "
              "LOOP_I ()(INC_INC_INC_INC_INC_INC_INC_INC_INC_INC_4)",
              pp(kEvalDefs + "EVAL(LOOP(0))"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionRulesTest, RecursionStillStops) {
    // Self and mutual reference, and a painted name passed through EXPAND.
    const std::string defs = "{#define A A B}{#define B B A}{#define f(x) f(x) g}{#define g(x) f(x)}"
                             "{#define EXPAND(...) __VA_ARGS__}{#define AA(x) x AA}";
    EXPECT_EQ("A B A B A B f(1) f(2) g(3) A B A f(1) f(2) g",
              pp(defs + "A B f(1)(2)(3) EXPAND(A) EXPAND(f(1)(2))"));
    EXPECT_EQ("1 AA(2) 1 AA(2) 1 AA(2)", pp(defs + "AA(1)(2) EXPAND(AA(1))(2) EXPAND(AA(1)(2))"));
    EXPECT_EQ("Z", pp("{#define Z EXPAND(Z)}{#define EXPAND(...) __VA_ARGS__}Z"));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionRulesTest, DeferredExpansionStillHitsTheLimits) {
    // Every EXPAND around R() expands it once more: the nesting is bounded
    // by the expansion depth, and the work by the step limit.
    const std::string defs = "{#define EMPTY}{#define DEFER(id) id EMPTY}"
                             "{#define EXPAND(...) __VA_ARGS__}"
                             "{#define R() x DEFER(R_I)()()}{#define R_I() R}";
    std::string call = "R()";
    for (int i = 0; i < 100; ++i)
        call = "EXPAND(" + call + ")";
    {
        auto env = setup();
        env.set_max_expansion_depth(64);
        EXPECT_THROW(pp(defs + call, env), Issue::Exception);
        EXPECT_EQ(Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED, code());
    }
    {
        auto env = setup();
        env.set_max_expansion_steps(500);
        EXPECT_THROW(pp(kEvalDefs + "EVAL(LOOP(0))", env), Issue::Exception);
        EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
    }
}
// ---- comments at either end of a macro body (SPEC §2) ----

TEST_F(ExpansionRulesTest, BodyEdgeCommentsAreKept) {
    // clang -CC: `[1 /* c */] [/* c */ 1] [/* a */ 1 /* b */ 2 /* c */] [/* l */ 1 /* r */]`.
    EXPECT_EQ("[1 /* c */] [/* c */ 1] [/* a */ 1 /* b */ 2 /* c */] [/* l */ 1 /* r */]",
              pp("{#define O 1 /* c */}{#define P /* c */ 1}{#define Q /* a */ 1 /* b */ 2 /* c */}"
                 "{#define F(x) /* l */ x /* r */}[O] [P] [Q] [F(1)]"));
    // clang -CC: `1 /* c */2 /* c */`.
    EXPECT_EQ("1 /* c */2 /* c */", pp("{#define F(x) x /* c */}F(1)F(2)"));
    EXPECT_TRUE(empty());
    // -nC removes them, as before.
    Env env = setup({}, /*remove_comments=*/true);
    EXPECT_EQ("[1] [1]", pp("{#define O 1 /* c */}{#define P /* c */ 1}[O] [P]", env));
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionRulesTest, BodyEdgeCommentsAreWhitespaceInARedefinition) {
    // A comment is whitespace in the comparison (SPEC §3.6), and whitespace
    // at either end is not part of the replacement list: no PP35. (clang -CC
    // keeps comments as tokens and warns for O, P and R.)
    pp("{#define O 1 /* c */}{#define O 1}{#define P 1}{#define P /* c */ 1}"
       "{#define R 1 /* c */}{#define R 1 /* d */}");
    EXPECT_TRUE(empty());
}

TEST_F(ExpansionRulesTest, IdenticalRedefinitionKeepsTheFirstDefinition) {
    // Identical (comments are whitespace): nothing changes, the first
    // definition stays with its own comments. Before, the later one replaced
    // it silently (with an inner comment as well).
    EXPECT_EQ("1 1 + 2 (* c *)1",
              pp("{#define O 1}{#define O 1 (* other *)}O "
                 "{#define P 1 + 2}{#define P 1 (* c *) + 2}P "
                 "{#define Q (* c *)1}{#define Q 1}Q"));
    EXPECT_TRUE(empty());
    // -dD still echoes every {#define} as written.
    Env env = setup();
    env.set_dd_mode(true);
    const std::string out = pp("{#define O 1}{#define O 1 (* other *)}O", env);
    EXPECT_NE(std::string::npos, out.find("{#define O 1}{#define O 1 (* other *)}1")) << out;
}

TEST_F(ExpansionRulesTest, UnclosedCommentEndingABodyIsLeftOut) {
    // PP20 as before; the comment is not kept in the body, where it would
    // swallow the text after each use.
    Issue::ContinueMode guard({});
    EXPECT_EQ("1 tail\n", pp("{#define U 1 (* open}\nU tail\n").substr(1));
    EXPECT_EQ(codes_of(diags()), (Strings{"PP20"}));
}

TEST_F(ExpansionRulesTest, BodyEdgeCommentsInDumps) {
    // -dM prints the comments of the body, at either end too (gcc -CC -dM
    // does; clang -CC -dM prints none).
    Env env = setup();
    pp("{#define O 1 /* c */}{#define P /* c */ 1}", env);
    std::ostringstream o;
    dump_macros(env, o);
    EXPECT_NE(std::string::npos, o.str().find("{#define O 1 /* c */}\n")) << o.str();
    EXPECT_NE(std::string::npos, o.str().find("{#define P /* c */ 1}\n")) << o.str();
}
