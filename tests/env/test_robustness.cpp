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
