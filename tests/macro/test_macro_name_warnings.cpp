// Tests for R4 follow-up Q1 (PP49 EXTRA_TOKENS_AT_END_OF_DIRECTIVE).
// See design-r4q.md.
#include "test_helper.hpp"
#include "core/preprocessor.hpp"

class MacroNameWarningTest : public JieppTest {};

// ---- Q1: {#undef}/{#ifdef}/{#ifndef} extra tokens after NAME (PP49) ----

// W1
TEST_F(MacroNameWarningTest, UndefExtraTokensWarnsAndUndefines) {
    auto r = pp("{#define A 1}{#undef A B};A;");
    EXPECT_EQ(";A;", r);
    // message()/codes() both drain the same underlying buffer (DiagBox::
    // release()), so only one of them can be queried per emitted diagnostic
    // set; the exact message text below also confirms there is exactly one
    // PP49 and nothing else.
    EXPECT_EQ("<unknown location>:1.0: warning: PP49: Extra tokens at end of directive; '{#undef A B}'",
              message());
}

// W2
TEST_F(MacroNameWarningTest, UndefExtraTokenVariantsWarnAndUndefine) {
    for (const std::string& variant : {"{#undef A(x)}", "{#undef A 1}", "{#undef A $n B}", "{#undef A (*! d *)}"}) {
        auto r = pp("{#define A 1}" + variant + ";A;");
        EXPECT_EQ(";A;", r) << "variant: " << variant;
        EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes())
            << "variant: " << variant;
    }
}

// W3: regression -- a plain comment or extra whitespace after NAME is not
// "extra tokens" (comments/whitespace are stripped before the check).
TEST_F(MacroNameWarningTest, UndefCommentOrWhitespaceOnlyStaysSilent) {
    auto r1 = pp("{#define A 1}{#undef A (* c *)};A;");
    EXPECT_EQ(";A;", r1);
    EXPECT_TRUE(empty());

    auto r2 = pp("{#define A 1}{#undef  A  };A;");
    EXPECT_EQ(";A;", r2);
    EXPECT_TRUE(empty());
}

// W4
TEST_F(MacroNameWarningTest, IfdefExtraTokensWarnsAndTestsNameOnly) {
    struct Case { std::string src; std::string expected; };
    const Case cases[] = {
        {"{#define A}{#ifdef A B}y{#else}n{#endif}", "y"},
        {"{#ifdef A B}y{#else}n{#endif}", "n"},
        {"{#ifndef A B}y{#else}n{#endif}", "y"},
        {"{#define A}{#ifndef A B}y{#else}n{#endif}", "n"},
        {"{#define A}{#ifdef A.B}y{#else}n{#endif}", "y"},
    };
    for (const auto& c : cases) {
        auto r = pp(c.src);
        EXPECT_EQ(c.expected, r) << "src: " << c.src;
        EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes())
            << "src: " << c.src;
    }
}

// W5: the extra tokens must never be evaluated as an expression (formerly
// spliced into "defined(...)").
TEST_F(MacroNameWarningTest, IfdefExtraTokensNeverEvaluatedAsExpression) {
    {
        auto r = pp("{#define X}{#ifndef X) \\or\\ (1}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes());
    }
    {
        auto r = pp("{#ifdef X) \\or\\ (1}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes());
    }
}

// W6
TEST_F(MacroNameWarningTest, IfdefIfndefMessageEchoesDirective) {
    {
        pp("{#ifdef A B}y{#endif}");
        EXPECT_NE(std::string::npos, message().find("'{#ifdef A B}'"));
    }
    {
        pp("{#ifndef A B}y{#endif}");
        EXPECT_NE(std::string::npos, message().find("'{#ifndef A B}'"));
    }
}

// W7: regression -- an inactive group's {#ifdef} is never evaluated.
TEST_F(MacroNameWarningTest, IfdefInInactiveGroupNotEvaluated) {
    auto r = pp("{#if 0}{#ifdef A B}{#endif}{#endif}ok");
    EXPECT_EQ("ok", r);
    EXPECT_TRUE(empty());
}

// W8: line count must be preserved (the extra-token warning path must not
// consume/duplicate lines differently than before).
TEST_F(MacroNameWarningTest, IfdefExtraTokensPreservesLineCount) {
    auto r = pp("{#ifdef A\nB}y{#endif}\n__LINE__");
    EXPECT_NE(std::string::npos, r.find("3"));
}

// W9: -Werror / {#ignore PP49}.
TEST_F(MacroNameWarningTest, WerrorPromotesUndefExtraTokens) {
    Issue::werror_ = true;
    EXPECT_THROW(pp("{#undef A B}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, code());
    Issue::werror_ = false;
}

TEST_F(MacroNameWarningTest, IgnoredPP49Silent) {
    auto r = pp("{#ignore PP49}{#define A 1}{#undef A B};A;");
    EXPECT_EQ(";A;", r);
    EXPECT_TRUE(empty());
}

// W10
TEST_F(MacroNameWarningTest, ApplyUndefOptionExtraTokensWarnsAndUndefines) {
    Env env = setup({{"A", "1"}});
    apply_undef_option("A B", env);
    EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes());
    EXPECT_EQ("[A];", pp("[A];", env));
}

