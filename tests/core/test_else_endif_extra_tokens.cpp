#include "test_helper.hpp"

// PP49 (EXTRA_TOKENS_AT_END_OF_DIRECTIVE) for {#else}/{#endif}: like
// gcc/clang's "extra tokens at end of #else/#endif directive", a warning
// only; the directive still has its usual effect. Comments are whitespace
// (silent); a document comment `(*! *)` counts as a token. See the design
// doc (task_slug: else-endif-extra) for the clang/gcc evidence this mirrors.
//
// U1 = option (a): the PP21 (invalid escape) gate for {#else}/{#endif}
// operands uses the same "reached"/"parent active" gate as PP29/PP49
// (R1/R2/R3 in the design doc), instead of the older, wider
// ctrl_parent_active()-only gate. This also newly reports PP29 for an
// unmatched {#else}/{#endif}'s operand (a pre-existing gap vs. gcc/clang).

namespace {
class ElseEndifExtraTokensTest : public JieppTest {};

// The exact PP49 message text at line `line` for context `ctx`, matching
// PlainTextMessage::message()'s "<unknown location>:L.0: ..." form used by
// pp()/pp_file() when no real file has been pushed.
std::string w49(int line, const std::string& ctx) {
    return "<unknown location>:" + std::to_string(line) +
           ".0: warning: PP49: Extra tokens at end of directive; '" + ctx + "'";
}

// The exact PP29 message text, matching test_unterminated_string.cpp's
// w29() helper (duplicated here to keep this file self-contained).
std::string w29(int line, const std::string& ctx) {
    return "<unknown location>:" + std::to_string(line) +
           ".0: warning: PP29: Missing terminating quote character; '" + ctx + "'";
}

} // namespace

TEST_F(ElseEndifExtraTokensTest, ElseExtraTokenWarns) {
    EXPECT_EQ("y;", pp("{#if 0}{#else X}y;{#endif}"));
    EXPECT_EQ((std::vector<std::string>{w49(1, "{#else X}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, EndifExtraTokenWarns) {
    EXPECT_EQ("y;", pp("{#if 1}y;{#endif Y}"));
    EXPECT_EQ((std::vector<std::string>{w49(1, "{#endif Y}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, ElseAndEndifInActiveChain) {
    pp("{#if 1}\n{#else X}\n{#endif Y}\n");
    EXPECT_EQ((std::vector<std::string>{w49(2, "{#else X}"), w49(3, "{#endif Y}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, TakenElseAfterFalseBranchesWarns) {
    pp("{#if 0}{#elif 0}{#else X}{#endif}");
    EXPECT_EQ((std::vector<std::string>{w49(1, "{#else X}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, ElseAfterNonAdjacentTakenBranchSilent) {
    EXPECT_EQ("", pp("{#if 1}{#elif 1}{#else X}{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("", pp("{#if 1}{#elif 0}{#else X}{#endif}"));
    EXPECT_TRUE(empty());
    pp("{#if 1}{#elif 1}{#else X}{#endif Y}");
    EXPECT_EQ((std::vector<std::string>{w49(1, "{#endif Y}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, InactiveParentSilent) {
    EXPECT_EQ("", pp("{#if 0}{#if 1}{#else X}{#endif Y}{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(ElseEndifExtraTokensTest, OuterElseWarnsInnerSilent) {
    pp("{#if 0}{#if 1}{#elif Z}{#endif}{#else W}{#endif}");
    EXPECT_EQ((std::vector<std::string>{w49(1, "{#else W}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, CommentsAndWhitespaceSilent) {
    const char* cases[] = {
        "{#if 1}{#else (* c *)}{#endif}",
        "{#if 1}{#else /* c */}{#endif}",
        "{#if 1}{#endif // A & B()}",
        "{#if 1}{#endif /* defined(x) \\and\\ y > 1 */}",
        "{#if 1}{# endif (* c *)}",
        "{#if 1}{#else:}{#endif}",
        "{#if 1}{#else $n}{#endif}",
        "{#if 1}{#endif   }",
    };
    for (const char* c : cases) {
        SCOPED_TRACE(c);
        pp(c);
        EXPECT_TRUE(empty());
    }
}

TEST_F(ElseEndifExtraTokensTest, TokenVariantsWarnOnce) {
    const char* cases[] = {
        "{#if 1}{#else ;}{#endif}",
        "{#if 1}{#endif (}",
        "{#if 1}{#else X Y Z}{#endif}",
        "{#if 1}{#endif (*! d *)}",
        "{#if 1}{#else: X}{#endif}",
    };
    for (const char* c : cases) {
        SCOPED_TRACE(c);
        pp(c);
        EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes());
    }
}

TEST_F(ElseEndifExtraTokensTest, MultiLineOperand) {
    EXPECT_EQ("\n\n\n\nz;", pp("{#if 1}\n{#else X\nY}\n{#endif}\nz;"));
    EXPECT_EQ((std::vector<std::string>{w49(2, "{#else X Y}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, UnterminatedThenExtra) {
    // The design calls for a double-quoted literal here (distinct from
    // {#else}'s single-quoted one above), not another single-quoted one --
    // verified: {#endif "xyz} -> PP29 '"xyz' then PP49 at line 3.
    pp("{#if 1}\n{#else 'abc}\n{#endif \"xyz}\n");
    EXPECT_EQ((std::vector<std::string>{
                  w29(2, "'abc"), w49(2, "{#else 'abc}"),
                  w29(3, "\"xyz"), w49(3, "{#endif \"xyz}")}),
              messages());
}

TEST_F(ElseEndifExtraTokensTest, NonAdjacentTakenElseNotScanned) {
    pp("{#if 1}\n{#elif 1}\n{#else 'a}\n{#endif 'b}\n");
    EXPECT_EQ((std::vector<std::string>{w29(4, "'b"), w49(4, "{#endif 'b}")}), messages());
}

TEST_F(ElseEndifExtraTokensTest, ElseAfterElseOrder) {
    {
        Issue::ContinueMode cm({});
        pp("{#if 0}{#else X}{#else Y}{#endif}");
        EXPECT_EQ((std::vector<Issue::Code>{
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
                      Issue::Code::ELSE_ERROR}),
                  codes());
    }
    {
        Issue::ContinueMode cm({});
        pp("{#if 1}{#else X}{#else Y}{#endif}");
        EXPECT_EQ((std::vector<Issue::Code>{
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
                      Issue::Code::ELSE_ERROR}),
                  codes());
    }
}

TEST_F(ElseEndifExtraTokensTest, UnmatchedOrderContinueMode) {
    {
        Issue::ContinueMode cm({});
        pp("{#endif 'q}");
        EXPECT_EQ((std::vector<Issue::Code>{
                      Issue::Code::UNTERMINATED_STRING_LITERAL,
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
                      Issue::Code::ENDIF_ERROR}),
                  codes());
    }
    {
        Issue::ContinueMode cm({});
        pp("{#else X}");
        EXPECT_EQ((std::vector<Issue::Code>{
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
                      Issue::Code::ELSE_ERROR}),
                  codes());
    }
}

TEST_F(ElseEndifExtraTokensTest, UnmatchedOrderNormalMode) {
    EXPECT_THROW(pp("{#endif X}"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[0]);
    EXPECT_EQ(Issue::Code::ENDIF_ERROR, cs[1]);
}

// U1 = option (a): the PP21 gate for {#else}/{#endif} matches the
// PP29/PP49 "reached" gate (R1/R2/R3), not the plain ctrl_parent_active()
// gate used before this change.
TEST_F(ElseEndifExtraTokensTest, InvalidEscape) {
    {
        Issue::ContinueMode cm({});
        pp("{#if 1}{#else $q}{#endif}");
        EXPECT_EQ((std::vector<Issue::Code>{
                      Issue::Code::INVALID_ESCAPE_SEQUENCE,
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}),
                  codes());
    }
    {
        // U1=(a): the earlier {#elif 1} already took the branch, so this
        // {#else} is not "reached" -- no PP21, no PP49.
        pp("{#if 1}{#elif 1}{#else $q}{#endif}");
        EXPECT_TRUE(empty());
    }
    {
        // U1=(a): an unmatched {#endif $q} is now scanned before ENDIF_ERROR.
        Issue::ContinueMode cm({});
        pp("{#endif $q}");
        EXPECT_EQ((std::vector<Issue::Code>{
                      Issue::Code::INVALID_ESCAPE_SEQUENCE,
                      Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE,
                      Issue::Code::ENDIF_ERROR}),
                  codes());
    }
}

TEST_F(ElseEndifExtraTokensTest, IgnoredPP49Silent) {
    EXPECT_EQ("", pp("{#ignore PP49}{#if 1}{#else X}{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(ElseEndifExtraTokensTest, WerrorPromotesElsePP49) {
    Issue::werror_ = true;
    EXPECT_THROW(pp("{#if 1}{#else X}{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, code());
    Issue::werror_ = false;
}

TEST_F(ElseEndifExtraTokensTest, IncludedTwiceReportsTwice) {
    fs::path tmp = fs::temp_directory_path() / "else_endif_extra_tokens_include_twice.iec";
    {
        std::ofstream f(tmp);
        f << "{#if 1}{#endif X}";
    }
    std::string abs = fs::absolute(tmp).generic_string();
    std::string src = "{#include '" + abs + "'}{#include '" + abs + "'}";
    pp(src);
    std::error_code ec;
    fs::remove(tmp, ec);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[0]);
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[1]);
}

TEST_F(ElseEndifExtraTokensTest, IdenticalRedefinitionWatchpointNoPP35) {
    pp("{#define A 1}{#define A 1}{#if 1}{#else X}{#endif}");
    EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE}), codes());
}

TEST_F(ElseEndifExtraTokensTest, BracketArgumentWatchpointNotSplit) {
    EXPECT_EQ("a[x,y];", pp("{#define F(a) a}{#if 0}{#else X}F(a[x,y]);{#endif}"));
    EXPECT_EQ((std::vector<std::string>{w49(1, "{#else X}")}), messages());
}
