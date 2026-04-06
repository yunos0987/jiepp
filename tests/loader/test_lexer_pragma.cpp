#include "test_helper.hpp"

#include "loader/lexer.hpp"

// ---- helpers ----

static std::vector<Token> ts(const std::string& text, bool remove_comments = false) {
    return iec3_tokens_from_string(text, remove_comments);
}

static Token make(int type, const std::string& text, int line = 0) {
    return Token::create(type, text, line);
}

// ---- pragma errors ----

class LexerPragmaTest : public JieppTest {};

TEST_F(LexerPragmaTest, Error) {
    EXPECT_THROW(ts("{id"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP22: Invalid syntax for pragma; '{id'", message());
    
    EXPECT_THROW(ts("{id:"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP22: Invalid syntax for pragma; '{id:'", message());
}

// ---- pragma closer normal cases ----

TEST_F(LexerPragmaTest, CloserNormal) {
    // {body} — bare pragma
    auto r1 = ts("{body}");
    ASSERT_EQ(1u, r1.size());
    EXPECT_EQ(Token::PRAGMA, r1[0].type);
    EXPECT_EQ("{body}", r1[0].text);

    // (*{body}*) — IEC block pragma
    auto r2 = ts("(*{body}*)");
    ASSERT_EQ(1u, r2.size());
    EXPECT_EQ(Token::PRAGMA, r2[0].type);
    EXPECT_EQ("{body}", r2[0].text);

    // /*{body}*/ — C-style block pragma
    auto r3 = ts("/*{body}*/");
    ASSERT_EQ(1u, r3.size());
    EXPECT_EQ(Token::PRAGMA, r3[0].type);
    EXPECT_EQ("{body}", r3[0].text);

    EXPECT_TRUE(empty());
}

// ---- pragma closer mismatch ----

TEST_F(LexerPragmaTest, CloserMismatch) {
    // (*{body}*/ — IEC opener with C closer → error
    EXPECT_THROW(ts("(*{body}*/"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP22: Invalid syntax for pragma; '}'", message());

    // /*{body}*) — C opener with IEC closer → error
    EXPECT_THROW(ts("/*{body}*)"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP22: Invalid syntax for pragma; '}'", message());
}

// ---- pragma directive ----

TEST_F(LexerPragmaTest, Directive) {
    // {#define X 1} should produce a DIRECTIVE token
    auto r = ts("{#define X 1}");
    ASSERT_EQ(1u, r.size());
    EXPECT_EQ(Token::DIRECTIVE, r[0].type);
    EXPECT_TRUE(empty());
}