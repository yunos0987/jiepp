#include "test_helper.hpp"

#include "loader/lexer.hpp"

// ---- helpers ----

static std::vector<Token> ts(const std::string& text, bool remove_comments = false) {
    return iec3_tokens_from_string(text, remove_comments);
}

static Token make(int type, const std::string& text, int line = 0) {
    return Token::create(type, text, line);
}

// ---- remove_comments ----

class LexerTest : public JieppTest {};

TEST_F(LexerTest, RemoveComments) {
    EXPECT_EQ((std::vector<Token>{make(Token::WS, " "), make(Token::WS, " ")}),
              ts("(*a*)(*b*)", true));
    auto result = ts("(*\ncomment\n*)\n/*\ncomment\n*/\n// comment\n", true);
    ASSERT_EQ(3u, result.size());
    EXPECT_EQ(Token::WS, result[0].type);
    EXPECT_EQ(3, result[0].num_of_lines);
    EXPECT_EQ(Token::WS, result[1].type);
    EXPECT_EQ(3, result[1].num_of_lines);
    EXPECT_EQ(Token::WS, result[2].type);
    EXPECT_EQ(1, result[2].num_of_lines);
    EXPECT_TRUE(empty());
}

// ---- bracket tokens ----

TEST_F(LexerTest, BracketTokens) {
    auto result = ts("[0]");
    ASSERT_EQ(3u, result.size());
    EXPECT_EQ(make(Token::LB, "["), result[0]);
    EXPECT_EQ(make(Token::ANY, "0"), result[1]);
    EXPECT_EQ(make(Token::RB, "]"), result[2]);

    result = ts("[0,1]");
    ASSERT_EQ(5u, result.size());
    EXPECT_EQ(make(Token::LB, "["),  result[0]);
    EXPECT_EQ(make(Token::ANY, "0"), result[1]);
    EXPECT_EQ(make(Token::SEP, ","), result[2]);
    EXPECT_EQ(make(Token::ANY, "1"), result[3]);
    EXPECT_EQ(make(Token::RB, "]"),  result[4]);

    EXPECT_TRUE(empty());
}

// ---- @@ and @ tokens ----

TEST_F(LexerTest, GlueAndStringizeTokens) {
    auto expect_tokens = [&](const std::vector<Token>& expected, const std::string& text) {
        EXPECT_EQ(expected, ts(text));
    };
    expect_tokens({make(Token::WS, " "), make(Token::ANY, "x"), make(Token::WS, "  "),
                   make(Token::ANY, "+"), make(Token::WS, "   "), make(Token::ANY, "y"),
                   make(Token::WS, "    ")},
                  " x  +   y    ");
    expect_tokens({make(Token::WS, " "), make(Token::ANY, "x"), make(Token::WS, "  "),
                   make(Token::GLUE, "@@"), make(Token::WS, "   "), make(Token::ANY, "y"),
                   make(Token::WS, "    ")},
                  " x  @@   y    ");
    expect_tokens({make(Token::WS, " "), make(Token::STRINGIZE, "@"), make(Token::WS, "  "),
                   make(Token::ANY, "x"), make(Token::WS, "   ")},
                  " @  x   ");
    expect_tokens({make(Token::WS, " "), make(Token::STRINGIZE, "@"), make(Token::WS, "  "),
                   make(Token::ANY, "xy"), make(Token::WS, "   "), make(Token::GLUE, "@@"),
                   make(Token::WS, "    "), make(Token::ANY, "zw"), make(Token::WS, "     "),
                   make(Token::GLUE, "@@"), make(Token::WS, "      "), make(Token::ANY, "ab"),
                   make(Token::WS, "       "), make(Token::STRINGIZE, "@"), make(Token::WS, "        "),
                   make(Token::ANY, "cd"), make(Token::WS, "         ")},
                  " @  xy   @@    zw     @@      ab       @        cd         ");
    expect_tokens({make(Token::ANY, "x"), make(Token::ANY, "+"), make(Token::ANY, "y")},
                  "x+y");
    expect_tokens({make(Token::ANY, "x"), make(Token::GLUE, "@@"), make(Token::ANY, "y")},
                  "x@@y");
    expect_tokens({make(Token::STRINGIZE, "@"), make(Token::ANY, "x")},
                  "@x");
    expect_tokens({make(Token::STRINGIZE, "@"), make(Token::ANY, "xy"),
                   make(Token::GLUE, "@@"), make(Token::ANY, "zw"),
                   make(Token::GLUE, "@@"), make(Token::ANY, "ab"),
                   make(Token::STRINGIZE, "@"), make(Token::ANY, "cd")},
                  "@xy@@zw@@ab@cd");
    EXPECT_TRUE(empty());
}

// ---- error line number tracking ----

TEST_F(LexerTest, ErrorLineno) {
    // Fact: an unterminated pragma at line 51 reports the correct line number
    static const char* s = R"(
;



;
{#nop}
;
(*{#nop}*)
;
/*{#nop}*/
;
//{#nop}
;
{nop}
;
(*{nop}*)
;
/*{nop}*/
;
//{nop}
;
(*
 *
 *)
;
(*!
 *
 *)
;
/*
 *
 */
;
/*!
 *
 */
;
//
;
//!
;
{#nop $
$
}
;
{nop $
$
}
;
{
    )";
    EXPECT_THROW(ts(s), Issue::Exception);
    EXPECT_EQ("<unknown location>:52.0: error: PP22: Invalid syntax for pragma; '{$n    '", message());
    
    std::string s2 = s;
    for (std::size_t pos = 0; (pos = s2.find("$\n", pos)) != std::string::npos;) {
        s2.replace(pos, 2, "\n");
        ++pos;
    }
    EXPECT_THROW(ts(s2), Issue::Exception);
    EXPECT_EQ("<unknown location>:52.0: error: PP22: Invalid syntax for pragma; '{$n    '", message());
}

// ---- nested brackets ----

TEST_F(LexerTest, NestedBrackets) {
    // Fact: nested bracket tokens [0,[1],[2,3]] produce 13 tokens
    auto result = ts("[0,[1],[2,3]]");
    ASSERT_EQ(13u, result.size());
    EXPECT_EQ(make(Token::LB, "["),  result[0]);
    EXPECT_EQ(make(Token::ANY, "0"), result[1]);
    EXPECT_EQ(make(Token::SEP, ","), result[2]);
    EXPECT_EQ(make(Token::LB, "["),  result[3]);
    EXPECT_EQ(make(Token::ANY, "1"), result[4]);
    EXPECT_EQ(make(Token::RB, "]"),  result[5]);
    EXPECT_EQ(make(Token::SEP, ","), result[6]);
    EXPECT_EQ(make(Token::LB, "["),  result[7]);
    EXPECT_EQ(make(Token::ANY, "2"), result[8]);
    EXPECT_EQ(make(Token::SEP, ","), result[9]);
    EXPECT_EQ(make(Token::ANY, "3"), result[10]);
    EXPECT_EQ(make(Token::RB, "]"),  result[11]);
    EXPECT_EQ(make(Token::RB, "]"),  result[12]);
    EXPECT_TRUE(empty());
}

// ---- iec3_is_identifier ----

// R3: iec3_is_identifier accepts exactly what tokenize() lexes as a single
// identifier: [A-Za-z_][A-Za-z0-9_]*, ASCII only.
TEST_F(LexerTest, IsIdentifier) {
    EXPECT_TRUE(iec3_is_identifier("a"));
    EXPECT_TRUE(iec3_is_identifier("_x1"));
    EXPECT_TRUE(iec3_is_identifier("X_2"));
    EXPECT_TRUE(iec3_is_identifier("__VA_ARGS__"));
    EXPECT_TRUE(iec3_is_identifier("if"));
    EXPECT_FALSE(iec3_is_identifier(""));
    EXPECT_FALSE(iec3_is_identifier("1"));
    EXPECT_FALSE(iec3_is_identifier("1a"));
    EXPECT_FALSE(iec3_is_identifier("a+b"));
    EXPECT_FALSE(iec3_is_identifier("."));
    EXPECT_FALSE(iec3_is_identifier("%IX0"));
    EXPECT_FALSE(iec3_is_identifier("$"));
    EXPECT_FALSE(iec3_is_identifier("\xE5"));
}

TEST_F(LexerTest, BlankOutComments) {
    // Each comment form becomes one space, like translation phase 3 in C.
    EXPECT_EQ("1  ", iec3_blank_out_comments("1 // c"));
    EXPECT_EQ("1 2", iec3_blank_out_comments("1(*c*)2"));
    // Unclosed block comment: runs to the end of the string.
    EXPECT_EQ("1  ", iec3_blank_out_comments("1 (* c"));
    // A document comment is a token, kept verbatim.
    EXPECT_EQ("1 (*! d *)", iec3_blank_out_comments("1 (*! d *)"));
    // A comment opener inside a string literal is not a comment.
    EXPECT_EQ("'a(*b*)c'  ", iec3_blank_out_comments("'a(*b*)c' (* x *)"));
    // A $-escaped quote inside the literal does not end it early.
    EXPECT_EQ("'it$'s'  ", iec3_blank_out_comments("'it$'s' // c"));
    // A '//' comment runs only to the end of the line, not past it.
    EXPECT_EQ("1  \n+1", iec3_blank_out_comments("1 // c\n+1"));
    EXPECT_EQ(" ", iec3_blank_out_comments("(*)"));
    // A pragma opener ("(*{ ... }") is kept, not blanked out.
    EXPECT_EQ("1 (*{p}*)", iec3_blank_out_comments("1 (*{p}*)"));
    // No comment opener at all: unchanged.
    EXPECT_EQ("a */ b", iec3_blank_out_comments("a */ b"));
}

// ---- ellipsis tokenization ----

// R3: '...' is three adjacent one-character Token::ANY "." tokens, not a
// single "..." token; whitespace between dots breaks the run.
TEST_F(LexerTest, EllipsisIsThreeDotTokens) {
    auto result = ts("...");
    ASSERT_EQ(3u, result.size());
    EXPECT_EQ(make(Token::ANY, "."), result[0]);
    EXPECT_EQ(make(Token::ANY, "."), result[1]);
    EXPECT_EQ(make(Token::ANY, "."), result[2]);

    auto result2 = ts(". .");
    ASSERT_EQ(3u, result2.size());
    EXPECT_EQ(Token::ANY, result2[0].type);
    EXPECT_EQ(".", result2[0].text);
    EXPECT_TRUE(result2[1].type & Token::MASK_WS);
    EXPECT_EQ(Token::ANY, result2[2].type);
    EXPECT_EQ(".", result2[2].text);
}