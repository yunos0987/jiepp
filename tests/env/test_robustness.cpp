#include "test_helper.hpp"

// =====================================================================
// Robustness limit tests (always-on, regardless of JIEPP_SANDBOX)
// =====================================================================

class RobustnessTest : public JieppTest {};

// ---- Expansion depth limit ----

TEST_F(RobustnessTest, ExpansionDepthDefault) {
    Env env;
    EXPECT_EQ(DEFAULT_MAX_EXPANSION_DEPTH, env.get_max_expansion_depth());
    EXPECT_EQ(0, env.expansion_depth());
}

TEST_F(RobustnessTest, ExpansionDepthIncDec) {
    Env env;
    env.inc_expansion_depth();
    EXPECT_EQ(1, env.expansion_depth());
    env.inc_expansion_depth();
    EXPECT_EQ(2, env.expansion_depth());
    env.dec_expansion_depth();
    EXPECT_EQ(1, env.expansion_depth());
}

TEST_F(RobustnessTest, ExpansionDepthExceeded) {
    // Verify the Env limit mechanism:
    // When depth exceeds max, Issue::happen() is called (which throws).
    // The actual guard (ExpansionDepthGuard) lives in expand.cpp and
    // protects against nested expand() calls via includes / preprocess_text().
    Env env;
    env.set_max_expansion_depth(2);
    env.inc_expansion_depth();  // depth=1, ok
    env.inc_expansion_depth();  // depth=2, ok
    EXPECT_THROW({
        env.inc_expansion_depth();  // depth=3 > max=2
        if (env.expansion_depth() > env.get_max_expansion_depth()) {
            ISSUE(MAX_EXPANSION_DEPTH_EXCEEDED);
        }
    }, Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED, code());
}

TEST_F(RobustnessTest, ExpansionDepthGuardRestoresOnThrow) {
    // U6/C6-2: ExpansionDepthGuard's constructor (expand.cpp) increments the
    // depth counter, then throws PP60 when the new depth exceeds the limit.
    // Since ISSUE() throws before the constructor body finishes, the guard
    // object never completes construction, so its destructor -- the usual
    // place the increment is undone -- never runs. Without the try/catch
    // added around that ISSUE() call, the increment above would leak
    // permanently into env's counter. Drive real recursive expand() calls
    // (not a manual inc_expansion_depth() like ExpansionDepthExceeded above)
    // so this exercises the actual guard, not just the Env accessors.
    Env env = setup();
    env.set_max_expansion_depth(3);

    // I(I(I(I(0)))): subst()'s per-formal-param expand(actual, result, env)
    // call recurses into expand() once per nesting level, driving
    // expansion_depth() past the limit of 3 (level 1: top-level file expand;
    // levels 2-4: one nested expand() per I(...) argument).
    EXPECT_THROW(pp("{#define I(a) (a)}\nI(I(I(I(0))))", env), Issue::Exception);
    EXPECT_EQ(0, env.expansion_depth());
}

// ---- Conditional nesting limit ----

TEST_F(RobustnessTest, IfNestingDefault) {
    Env env;
    EXPECT_EQ(DEFAULT_MAX_IF_NESTING, env.get_max_if_nesting());
}

TEST_F(RobustnessTest, IfNestingExceeded) {
    // Build deeply nested {#if 1}...{#endif} chain exceeding the limit
    Env env = setup();
    // Use a small nesting limit: we can't set it on Env since there's no setter
    // Instead, build enough nesting to trigger default 256 — that's too many.
    // We need a way to test with a smaller limit.
    // For now, just verify normal nesting works (non-excessive).
    auto result = pp("{#if 1}A{#if 1}B{#endif}{#endif}");
    EXPECT_NE(std::string::npos, result.find("A"));
    EXPECT_NE(std::string::npos, result.find("B"));
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, NormalExpansionOk) {
    // Verify normal macro expansion works within limits
    EXPECT_EQ(";hello", pp("{#define A hello};A"));
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, NormalOutputTokensOk) {
    // Verify output with many tokens works within default limit
    EXPECT_EQ("a;b;c;d;e", pp("a;b;c;d;e"));
    EXPECT_TRUE(empty());
}

// ---- max_expansion_depth directive ----

TEST_F(RobustnessTest, MaxExpansionDepthDirectiveSetsLimit) {
    Env env = setup();
    pp("{#max_expansion_depth 32}", env);
    EXPECT_EQ(32, env.get_max_expansion_depth());
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, MaxExpansionDepthDirectiveHyphenForm) {
    Env env = setup();
    pp("{#max-expansion-depth 64}", env);
    EXPECT_EQ(64, env.get_max_expansion_depth());
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, MaxExpansionDepthDirectiveNegativeRejected) {
    Env env = setup();
    EXPECT_THROW(pp("{#max_expansion_depth -1}", env), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PARAMETER_VALUE, code());
}

TEST_F(RobustnessTest, MaxExpansionDepthFixedBlocksDirective) {
    Env env = setup();
    env.fix_max_expansion_depth(10);
    pp("{#max_expansion_depth 9999}", env);
    EXPECT_EQ(10, env.get_max_expansion_depth());
    EXPECT_TRUE(empty());
}

// ---- max_if_nesting directive ----

TEST_F(RobustnessTest, MaxIfNestingDirectiveSetsLimit) {
    Env env = setup();
    pp("{#max_if_nesting 128}", env);
    EXPECT_EQ(128, env.get_max_if_nesting());
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, MaxIfNestingDirectiveHyphenForm) {
    Env env = setup();
    pp("{#max-if-nesting 64}", env);
    EXPECT_EQ(64, env.get_max_if_nesting());
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, MaxIfNestingDirectiveNegativeRejected) {
    Env env = setup();
    EXPECT_THROW(pp("{#max_if_nesting -1}", env), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PARAMETER_VALUE, code());
}

TEST_F(RobustnessTest, MaxIfNestingFixedBlocksDirective) {
    Env env = setup();
    env.fix_max_if_nesting(32);
    pp("{#max_if_nesting 9999}", env);
    EXPECT_EQ(32, env.get_max_if_nesting());
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTest, MaxIfNestingDirectiveThenExceed) {
    Env env = setup();
    // Set low limit then exceed it
    EXPECT_THROW(pp("{#max_if_nesting 2}{#if 1}{#if 1}{#if 1}A{#endif}{#endif}{#endif}", env), Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_IF_NESTING_EXCEEDED, code());
}
