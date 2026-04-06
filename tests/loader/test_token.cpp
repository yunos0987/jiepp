#include "test_helper.hpp"
#include "loader/token.hpp"

namespace {

const Token WS = Token::create(Token::WS,  " ");
const Token ID = Token::create(Token::ANY, "abc");

using TS = std::vector<Token>;

} // namespace

class TokenTrimTest : public JieppTest {};

// ---- ts_ltrim ----

TEST_F(TokenTrimTest, LtrimRemovesLeading) {
    // Fact: leading whitespace tokens are stripped; trailing tokens are preserved
    EXPECT_EQ((TS{ID, WS, WS}), ts_ltrim({WS, WS, ID, WS, WS}));
    EXPECT_EQ((TS{ID, WS, WS}), ts_ltrim({WS,     ID, WS, WS}));
    EXPECT_EQ((TS{ID, WS, WS}), ts_ltrim({        ID, WS, WS}));

    EXPECT_EQ((TS{ID, WS}),     ts_ltrim({WS, WS, ID, WS}));
    EXPECT_EQ((TS{ID, WS}),     ts_ltrim({WS,     ID, WS}));
    EXPECT_EQ((TS{ID, WS}),     ts_ltrim({        ID, WS}));

    EXPECT_EQ((TS{ID}),         ts_ltrim({WS, WS, ID}));
    EXPECT_EQ((TS{ID}),         ts_ltrim({WS,     ID}));
    EXPECT_EQ((TS{ID}),         ts_ltrim({        ID}));
}

TEST_F(TokenTrimTest, LtrimAllWhitespaceOrEmpty) {
    // Fact: an all-whitespace or empty list becomes empty
    EXPECT_EQ((TS{}), ts_ltrim({WS, WS}));
    EXPECT_EQ((TS{}), ts_ltrim({WS}));
    EXPECT_EQ((TS{}), ts_ltrim({}));
}

TEST_F(TokenTrimTest, LtrimInternalPreserved) {
    // Fact: whitespace between non-whitespace tokens is preserved
    EXPECT_EQ((TS{ID, WS, ID, WS, WS}), ts_ltrim({WS, WS, ID, WS, ID, WS, WS}));
}

// ---- ts_rtrim ----

TEST_F(TokenTrimTest, RtrimRemovesTrailing) {
    // Fact: trailing whitespace tokens are stripped; leading tokens are preserved
    EXPECT_EQ((TS{WS, WS, ID}), ts_rtrim({WS, WS, ID, WS, WS}));
    EXPECT_EQ((TS{WS,     ID}), ts_rtrim({WS,     ID, WS, WS}));
    EXPECT_EQ((TS{        ID}), ts_rtrim({        ID, WS, WS}));

    EXPECT_EQ((TS{WS, WS, ID}), ts_rtrim({WS, WS, ID, WS}));
    EXPECT_EQ((TS{WS,     ID}), ts_rtrim({WS,     ID, WS}));
    EXPECT_EQ((TS{        ID}), ts_rtrim({        ID, WS}));

    EXPECT_EQ((TS{WS, WS, ID}), ts_rtrim({WS, WS, ID}));
    EXPECT_EQ((TS{WS,     ID}), ts_rtrim({WS,     ID}));
    EXPECT_EQ((TS{        ID}), ts_rtrim({        ID}));
}

TEST_F(TokenTrimTest, RtrimAllWhitespaceOrEmpty) {
    // Fact: an all-whitespace or empty list becomes empty
    EXPECT_EQ((TS{}), ts_rtrim({WS, WS}));
    EXPECT_EQ((TS{}), ts_rtrim({WS}));
    EXPECT_EQ((TS{}), ts_rtrim({}));
}

TEST_F(TokenTrimTest, RtrimInternalPreserved) {
    // Fact: whitespace between non-whitespace tokens is preserved
    EXPECT_EQ((TS{WS, WS, ID, WS, ID}), ts_rtrim({WS, WS, ID, WS, ID, WS, WS}));
}

// ---- ts_trim ----

TEST_F(TokenTrimTest, TrimRemovesBothEnds) {
    // Fact: both leading and trailing whitespace tokens are removed
    EXPECT_EQ((TS{ID}), ts_trim({WS, WS, ID, WS, WS}));
    EXPECT_EQ((TS{ID}), ts_trim({WS,     ID, WS, WS}));
    EXPECT_EQ((TS{ID}), ts_trim({        ID, WS, WS}));

    EXPECT_EQ((TS{ID}), ts_trim({WS, WS, ID, WS}));
    EXPECT_EQ((TS{ID}), ts_trim({WS,     ID, WS}));
    EXPECT_EQ((TS{ID}), ts_trim({        ID, WS}));

    EXPECT_EQ((TS{ID}), ts_trim({WS, WS, ID}));
    EXPECT_EQ((TS{ID}), ts_trim({WS,     ID}));
    EXPECT_EQ((TS{ID}), ts_trim({        ID}));
}

TEST_F(TokenTrimTest, TrimAllWhitespaceOrEmpty) {
    // Fact: an all-whitespace or empty list becomes empty
    EXPECT_EQ((TS{}), ts_trim({WS, WS}));
    EXPECT_EQ((TS{}), ts_trim({WS}));
    EXPECT_EQ((TS{}), ts_trim({}));
}

TEST_F(TokenTrimTest, TrimInternalPreserved) {
    // Fact: whitespace between non-whitespace tokens is preserved
    EXPECT_EQ((TS{ID, WS, ID}), ts_trim({WS, WS, ID, WS, ID, WS, WS}));
}

