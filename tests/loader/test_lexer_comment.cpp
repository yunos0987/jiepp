#include "test_helper.hpp"

#include "loader/lexer.hpp"

// ---- helpers ----

static std::vector<Token> ts(const std::string& text, bool remove_comments = false) {
    return iec3_tokens_from_string(text, remove_comments);
}

static Token make(int type, const std::string& text, int line = 0) {
    return Token::create(type, text, line);
}

static void expect_tokens(const std::vector<Token>& expected,
                          const std::string& text,
                          bool remove_comments = false) {
    EXPECT_EQ(expected, ts(text, remove_comments));
}

// ---- comment errors ----

class LexerCommentTest : public JieppTest {};

TEST_F(LexerCommentTest, BlockCommentError) {
    EXPECT_THROW(ts("(* comment"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP20: Unclosed comment; '(* comment'", message());
    
    EXPECT_THROW(ts("(* comment */"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP20: Unclosed comment; '(* comment */'", message());
    
    EXPECT_THROW(ts("/* comment"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP20: Unclosed comment; '/* comment'", message());
    
    EXPECT_THROW(ts("/* comment *)"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP20: Unclosed comment; '/* comment *)'", message());
}

// ---- line comment ----

TEST_F(LexerCommentTest, LineComment) {
    expect_tokens({make(Token::C, "// comment\n", 1)}, "// comment\n");
    expect_tokens({make(Token::C, "// comment\n", 1)}, "// comment\r\n");
    expect_tokens({make(Token::C, "// comment\n", 1)}, "// comment\r");
    EXPECT_TRUE(empty());
}

// ---- block comment newlines ----

TEST_F(LexerCommentTest, BlockComment) {
    expect_tokens({make(Token::C, "(*\ncomment\n*)", 2)}, "(*\ncomment\n*)");
    expect_tokens({make(Token::C, "(*\ncomment\n*)", 2)}, "(*\r\ncomment\r\n*)");
    expect_tokens({make(Token::C, "(*\ncomment\n*)", 2)}, "(*\rcomment\r*)");
    expect_tokens({make(Token::C, "(*\ncomment\n*)\n", 3)}, "(*\ncomment\n*)\n");
    expect_tokens({make(Token::C, "(*\ncomment\n*)\n", 3)}, "(*\r\ncomment\r\n*)\r\n");
    expect_tokens({make(Token::C, "(*\ncomment\n*)\n", 3)}, "(*\rcomment\r*)\r");
    EXPECT_TRUE(empty());
}

// ---- document block comments ----

TEST_F(LexerCommentTest, DocBlockComment) {
    // (*! doc *) — IEC doc comment
    auto r1 = ts("(*!doc*)");
    ASSERT_EQ(1u, r1.size());
    EXPECT_EQ(Token::DOCUMENT, r1[0].type);
    EXPECT_EQ("(*!doc*)", r1[0].text);

    // /*! doc */ — C-style doc comment
    auto r2 = ts("/*!doc*/");
    ASSERT_EQ(1u, r2.size());
    EXPECT_EQ(Token::DOCUMENT, r2[0].type);
    EXPECT_EQ("/*!doc*/", r2[0].text);

    // multiline doc comment with newline count
    auto r3 = ts("(*!\nline1\nline2\n*)");
    ASSERT_EQ(1u, r3.size());
    EXPECT_EQ(Token::DOCUMENT, r3[0].type);
    EXPECT_EQ(3, r3[0].num_of_lines);

    EXPECT_TRUE(empty());
}

// ---- document line comments ----

TEST_F(LexerCommentTest, DocLineComment) {
    // //! doc — line doc comment
    auto r1 = ts("//!doc\n");
    ASSERT_EQ(1u, r1.size());
    EXPECT_EQ(Token::DOCUMENT, r1[0].type);
    EXPECT_EQ("//!doc\n", r1[0].text);
    EXPECT_EQ(1, r1[0].num_of_lines);

    // //! followed by EOF (no trailing newline)
    auto r2 = ts("//!doc");
    ASSERT_EQ(1u, r2.size());
    EXPECT_EQ(Token::DOCUMENT, r2[0].type);
    EXPECT_EQ("//!doc", r2[0].text);
    EXPECT_EQ(0, r2[0].num_of_lines);

    EXPECT_TRUE(empty());
}