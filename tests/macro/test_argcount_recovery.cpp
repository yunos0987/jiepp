#include "test_helper.hpp"

// PP34 error recovery (task_slug argcount-error-output): a function-macro
// call with the wrong argument count is NOT expanded, like gcc/clang. The
// call is replaced by the macro name only (its own hide set, not painted),
// and the argument list ('(' through the matching ')') is dropped
// unexpanded: no diagnostics or __COUNTER__ increments come from inside it.
// An unterminated call (no ')' before the end of input) follows clang
// instead: nothing is output for it, not even the macro name, and the
// count check itself is skipped so only ONE PP34 is reported per call.

class ArgCountRecoveryTest : public JieppTest {};

// R1: the painted recursion example -- the erroneous inner call becomes its
// own bare name, and the outer macro still substitutes it twice.
TEST_F(ArgCountRecoveryTest, R1_NestedBodySubstitution) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("P P", pp("{#define P(x) x+1}{#define F(a) a a}F(P(2,3))"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

// R2: top-level erroneous call surrounded by plain text.
TEST_F(ArgCountRecoveryTest, R2_TopLevelSurroundedByText) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("X P Y", pp("{#define P(x) x+1}X P(2,3) Y"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

// R3: too few / zero / too many arguments, one PP34 per erroneous call.
TEST_F(ArgCountRecoveryTest, R3_TooFewZeroTooMany) {
    const char* def = "{#define F2(x,y) x+y}";
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("A F2 B", pp(std::string(def) + "A F2(1) B"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("C F2 D", pp(std::string(def) + "C F2() D"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("E F2 G", pp(std::string(def) + "E F2(1,2,3) G"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
}

// R4: a 0-param macro called with one argument is an error (RED); GUARD
// cases (F0( ) and F1() with an empty arg) must stay unaffected.
TEST_F(ArgCountRecoveryTest, R4_ZeroParamWithArgument) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("E F0 G", pp("{#define F0() zero}E F0(1) G"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

TEST_F(ArgCountRecoveryTest, R4_Guard_ZeroParamEmptyCallsUnaffected) {
    EXPECT_EQ("H zero I", pp("{#define F0() zero}H F0( ) I"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("A [] B", pp("{#define F1(x) [x]}A F1() B"));
    EXPECT_TRUE(empty());
}

// R5: variadic argument-count rules; RED cases for too-few required
// arguments, GUARD cases for the already-correct boundary forms.
TEST_F(ArgCountRecoveryTest, R5_VariadicTooFew) {
    const char* def = "{#define V(a,b,...) <a|b|__VA_ARGS__>}";
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("A V B", pp(std::string(def) + "A V(1) B"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("E V G", pp(std::string(def) + "E V() G"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
}

TEST_F(ArgCountRecoveryTest, R5_Guard_VariadicBoundaryFormsUnaffected) {
    EXPECT_EQ("C <1|2|> D",
              pp("{#define V(a,b,...) <a|b|__VA_ARGS__>}C V(1,2) D"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("<1|> <|>",
              pp("{#define V1(a,...) <a|__VA_ARGS__>}V1(1) V1()"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("<|>", pp("{#define N(a,args...) <a|args>}N()"));
    EXPECT_TRUE(empty());
}

// R6: body rescan -- the emitted name is final within the scan it appears
// in, but can form a new call once more tokens (from the source, or from
// another macro's body) are pushed after it.
TEST_F(ArgCountRecoveryTest, R6_BodyRescan) {
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("A P tail Z",
                  pp("{#define P(x) x+1}{#define B1 P(1,2) tail}A B1 Z"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("P after",
                  pp("{#define P(x) x+1}{#define H P(1,}H 2) after"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("P(5)", pp("{#define P(x) x+1}{#define Q P(1,2)}Q(5)"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("P(5)", pp("{#define P(x) x+1}P(1,2)(5)"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
}

// R7: when the error happens while pre-expanding an argument, the emitted
// name becomes part of that argument and is NOT painted -- it forms a new
// call once the outer replacement is rescanned.
TEST_F(ArgCountRecoveryTest, R7_ArgumentPreExpansionNameNotPainted) {
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("K q", pp("{#define K(a) a}K(K(1,2)) q"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("5+1",
                  pp("{#define P(x) x+1}{#define K(a) a}K(P(1,2))(5)"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
}

// R8: the arguments of an erroneous call are never expanded -- no
// diagnostics (here, a nested PP34 that would come from P(1,2)) and no
// substitution escape from inside them.
TEST_F(ArgCountRecoveryTest, R8_ArgumentsNeverExpanded) {
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("F2",
                  pp("{#define P(x) x+1}{#define F2(x,y) x y}F2(P(1,2))"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
                  PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("expected 2, got 1")) << msgs[0];
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("F2 z",
                  pp("{#define P(x) x+1}{#define F2(x,y) x y}F2(1,2,P(3,4)) z"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
}

// R9: __COUNTER__ inside a dropped (unexpanded) argument list does not
// increment the counter.
TEST_F(ArgCountRecoveryTest, R9_CounterNotIncrementedInsideDroppedArgs) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("F2 0",
              pp("{#define F2(x,y) x y}F2(__COUNTER__) __COUNTER__"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

// R10: unterminated calls (no closing ')' before the end of input / the
// enclosing argument) follow clang -- nothing is output for the call, not
// even the macro name -- and only ONE PP34 is reported (the count check is
// skipped).
TEST_F(ArgCountRecoveryTest, R10_UnterminatedCallOutputsNothing) {
    {
        Issue::ContinueMode guard({});
        // The trailing space is the ordinary whitespace token between "A"
        // and "F", unrelated to the call itself -- it is emitted as usual;
        // only the call's own tokens ('F' through the unterminated '(1')
        // produce nothing.
        EXPECT_EQ("A ", pp("{#define F(x) <x>}A F(1"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
                  PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("missing closing parenthesis"))
            << msgs[0];
    }
    {
        Issue::ContinueMode guard({});
        EXPECT_EQ("A ", pp("{#define P(x) x+1}A P(1,2"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
    {
        // Unterminated reached through the shared '('/'[' depth counter: the
        // outer call G(...) is well-formed, but the inner F(1] never finds
        // its ')' before the ']' (part of G's own argument) closes the
        // bracket depth back to 0 and G's ')' is found -- F's call is
        // therefore unterminated within the token stream handed to it.
        Issue::ContinueMode guard({});
        EXPECT_EQ("A [] z",
                  pp("{#define F(x) <x>}{#define G(a) [a]}A G(F(1]) z"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
    }
}

// R11: line-number fidelity -- an erroneous call must advance the line
// counter by exactly the same amount as the same call shape would if it
// succeeded (same tail code path), so a __LINE__ after it is unaffected by
// whether the call errored.
//
// Each case below is its own TEST_F (rather than three blocks in one test):
// the PP34/#warning location is tracked through a process-global location
// stack (Issue::push/pop via err_set_lineno(), see expand.cpp's
// advance_lineno()) that JieppTest::SetUp() resets once per TEST_F but that
// a second pp() call within the same test would NOT reset on its own, so
// multiple sequential pp() calls in one test can leak a stale line number
// from the previous call into a diagnostic reported before this call's own
// first advance_lineno() (exactly what happens here, since each ISSUE(...)
// for ARGUMENT_COUNT_MISMATCH is deliberately reported before this call's
// own lines are added to the counter -- see D4/C2 -- unchanged pre-existing
// behavior, not something this change introduces).

TEST_F(ArgCountRecoveryTest, R11a_LineNumbersMatchSuccessfulTwin) {
    Issue::ContinueMode guard({});
    // Twin (success): {#define P(x,y,z) x}A P(1,\n2,\n3) B\n__LINE__
    //   -> "A 1\n\n B\n4" (verified on this build: the call result is
    //   the single body token "x" -> "1", followed by the 2-newline
    //   tail for the 2 newlines inside the call's argument list).
    // Error twin: same call shape, wrong arg count against a 1-param P
    // -> the call result is the bare name "P" in place of "1", with an
    // identical newline tail, so __LINE__ is still 4.
    EXPECT_EQ("A P\n\n B\n4",
              pp("{#define P(x) x+1}A P(1,\n2,\n3) B\n__LINE__"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

TEST_F(ArgCountRecoveryTest, R11b_OutputOnlyNewlineVariant) {
    // H's body call to P is one source line, but P(1$n,2) contains a
    // decoded-newline ($n) that becomes an output-only newline when H's own
    // call is rescanned. __LINE__ after H's single source line must still
    // be 2, whether or not H's embedded P(...) call errors.
    Issue::ContinueMode guard({});
    // Matches the successful twin's shape exactly ("1" -> "P"), including
    // the blank-line-compaction marker line pp()'s default settings emit
    // for the swallowed {#define} lines above the threshold.
    EXPECT_EQ("A P\n B\n(*{#:1}*)\n2",
              pp("{#define P(x) x+1}{#define H P(1$n,2)}A H B\n__LINE__"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, cs[0]);
}

TEST_F(ArgCountRecoveryTest, R11c_MultiLineCallDoesNotShiftLaterDiagnosticLine) {
    // A multi-line erroneous call does not shift a later diagnostic's line
    // off by the lines it spans. C2: the "too many arguments" PP34 itself no
    // longer stays on the macro name's own line -- it follows clang, which
    // reports the first non-whitespace token after the comma ending the
    // last allowed argument (here, P's 1 parameter: the comma right after
    // "1,", whose first following token is "2" on line 2).
    Issue::ContinueMode guard({});
    pp("{#define P(x) x+1}A P(1,\n2,\n3) B\n{#warning w}");
    auto msgs = messages();
    ASSERT_EQ(2u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":2.")) << msgs[0];
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, PlainTextMessage::parse_code(msgs[1]));
    EXPECT_NE(std::string::npos, msgs[1].find(":4.")) << msgs[1];
}

// R12: inside #if, the leftover macro name is an ordinary identifier (0).
TEST_F(ArgCountRecoveryTest, R12_IfConditionLeftoverNameIsZero) {
    Issue::ContinueMode guard({});
    std::string out = pp(
        "{#define P(x) x+1}"
        "{#if P(1,2) + 1}yes{#else}no{#endif} "
        "{#if P(1,2)}y2{#else}n2{#endif}");
    EXPECT_NE(out.find("yes"), std::string::npos) << out;
    EXPECT_EQ(out.find("y2"), std::string::npos) << out;  // "y2" never appears
    EXPECT_NE(out.find("n2"), std::string::npos) << out;
    auto cs = codes();
    EXPECT_EQ(2u, cs.size());
    for (auto c : cs)
        EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, c);
}

// R13: {#ignore PP34} silences the diagnostic (even in strict/library mode,
// like {#ignore PP30}/{#ignore PP36}), but the call is still not expanded --
// an unterminated ignored call still outputs nothing.
TEST_F(ArgCountRecoveryTest, R13_IgnoredStillNotExpanded) {
    // "F(1,2)" is terminated (wrong count) -> emits the name "F"; the
    // trailing " F(1" is unterminated -> emits nothing, leaving just the
    // ordinary space between the two calls.
    EXPECT_EQ("F ",
              pp("{#ignore PP34}{#define F(x) <x>}F(1,2) F(1"));
    EXPECT_TRUE(empty());
}

// R16: a macro name produced BY substitution (not left over from an
// erroneous call) is not itself mistaken for a dropped call -- it is
// rescanned normally and expands if followed by '('.
TEST_F(ArgCountRecoveryTest, R16_Guard_SubstitutedNameIsNotTreatedAsError) {
    EXPECT_EQ("P(1,1)", pp("{#define P(x) P(x,x)}P(1)"));
    EXPECT_TRUE(empty());
}

// ---- C2: PP34's reported line for a multi-line call follows clang ----
//
// clang reports "too many arguments" at the first non-whitespace,
// non-comment token after the comma ending the last allowed argument (the
// closing ')' itself if nothing but whitespace/comments remain, in which
// case it is the comma's own line instead); "too few arguments" at the
// closing ')'. gcc instead always uses the ')' line for both kinds; jiepp
// follows clang. Every expected line below was measured directly with
// `clang -E -fsyntax-only` (clang 22.1.3) on an equivalent two-parameter
// `#define F(a,b) a+b` / `#define G(a,b) a+b`, not copied from a table.

TEST_F(ArgCountRecoveryTest, C2_TooManyNextLineAfterTriggerComma) {
    // clang (case A, `x F(1,\n2,\n3) y;` with F(a,b), call starting on the
    // line right after the #define): reports line 4 -- the "3" after the
    // comma that ends the 2nd (last allowed) argument, not the macro name's
    // own line (2) and not the comma's own line (3).
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}\nx F(1,\n2,\n3) y;");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":4.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_TooManyExcessTokenFarFromTriggerComma) {
    // clang (case B, `F(\n1,\n2,\n3\n);`): reports line 5 -- the excess
    // argument "3" sits on its own line, two lines after the triggering
    // comma (end of "2,").
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}\nF(\n1,\n2,\n3\n);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":5.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_TooManySingleLineCallUnaffected) {
    // clang (case C, one-line `F(1,2,3);`): a single-line call still reports
    // on its own line -- the fix only changes which *column/position within
    // a span* the line comes from, not single-line behavior.
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}\nF(1,2,3);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":2.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_TooFewReportsClosingParenLine) {
    // clang (case D, `G(1\n\n);` with 2-param G): "too few" always reports
    // the closing ')' line, same as gcc -- unlike "too many", there is no
    // excess-token search.
    Issue::ContinueMode guard({});
    pp("{#define G(a,b) a}\nG(1\n\n);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":4.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_TooManyExcessIsTheSecondArgumentAfterAnEmptyOne) {
    // clang (case E, `F(1\n,\n\n\n,\nx);`): the 2nd argument (between the two
    // commas) is empty, so the triggering comma ending it is the *second*
    // one; the excess token is "x" on line 7, not the second comma's own
    // line (6) -- the binding review correction for this case.
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}\nF(1\n,\n\n\n,\nx);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":7.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_TooManyExcessArgumentIsEmptyFallsBackToCommaLine) {
    // clang (case F, `F(1,2,\n\n);`): the excess (3rd) argument is empty --
    // nothing but whitespace stands between the triggering comma and the
    // closing ')' -- so the report falls back to the comma's own line (2),
    // not the ')' line (4).
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}\nF(1,2,\n\n);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":2.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_TooManySkipsCommentBeforeExcessToken) {
    // clang (case G, `F(1,2, /*c*/\n\n3);`): a comment right after the
    // triggering comma is skipped like whitespace; the excess token "3" is
    // on line 4.
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}\nF(1,2, (*c*)\n\n3);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":4.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_ZeroParamMacroExcessStartsAfterOpenParen) {
    // 0-parameter macro: the excess search starts right after '(' itself
    // (there is no "last allowed argument" comma); the first argument given
    // is already excess.
    Issue::ContinueMode guard({});
    pp("{#define F() a}\nF(\n\n1);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":4.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_VariadicTooFewStillReportsClosingParenLine) {
    // A variadic macro's "too few" case uses the same closing-')' rule as a
    // non-variadic one.
    Issue::ContinueMode guard({});
    pp("{#define V(a,b,...) <a|b|__VA_ARGS__>}\nV(1\n\n);");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":4.")) << msgs[0];
}

TEST_F(ArgCountRecoveryTest, C2_OutputOnlyLinesInRescannedArgumentsDoNotMoveTheLine) {
    // The excess-token line marker reuses the same sum_uncounted accounting
    // as advance_lineno() itself, so an output-only newline (from a $n
    // decode, rescanned from another macro's body) must not shift the
    // reported line any more than it shifts __LINE__ -- BODY's invocation is
    // one source line, even though F's rescanned argument list embeds
    // decoded newlines.
    Issue::ContinueMode guard({});
    pp("{#define F(a,b) a}{#define BODY F(1$n,2$n,3)}\nBODY");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH,
              PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find(":2.")) << msgs[0];
}
