#include "test_helper.hpp"

#include "loader/lexer.hpp"

// ---- helpers ----

static std::vector<Token> ts(const std::string& text, bool remove_comments = false) {
    return iec3_tokens_from_string(text, remove_comments);
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

// ---- '//' inside a directive body ends at a raw newline, like C ----
//
// read_pragma_body() folds a raw newline into whitespace before the body is
// lexed, so without tracking '//' as it scans, a line comment that reaches
// a raw newline would otherwise keep eating characters past it (silently
// dropping the rest of the directive). These check the DIRECTIVE token's
// folded text directly, so the comment-truncation rule can be verified
// independently of any one directive handler.
TEST_F(LexerPragmaTest, DirectiveLineCommentEndsAtRawNewline) {
    // L1: a '//' comment ends at the raw newline (LF, CRLF, CR alike); the
    // rest of the directive is not swallowed.
    for (const std::string& nl : {"\n", "\r\n", "\r"}) {
        SCOPED_TRACE(nl);
        auto r = ts("{#define X 1 // c" + nl + "+2}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ(Token::DIRECTIVE, r[0].type);
        EXPECT_EQ("{#define X 1 +2}", r[0].text);
    }

    // L2: an empty comment body also ends at the raw newline.
    {
        auto r = ts("{#define X 1 //\n+2}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X 1 +2}", r[0].text);
    }

    // L3: '$'+newline inside a '//' comment extends it to the next raw
    // newline (like C's backslash-newline continuation), so only the
    // second raw newline ends it.
    {
        auto r = ts("{#define X 1 // c$\n+2\n+3}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X 1 +3}", r[0].text);
    }

    // L4: a decoded '//' ($2F$2F) also opens a comment and is ended by a
    // raw newline the same way.
    {
        auto r = ts("{#define X 1 $2F$2F c\n+2}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X 1 +2}", r[0].text);
    }

    // L5: a '//' inside a string literal is not a comment opener.
    {
        auto r = ts("{#define U 'a//b'\n+1}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define U 'a//b' +1}", r[0].text);
    }

    // L6: a '//' inside a block comment ('(* ... *)') is not a comment
    // opener; the block comment may span the raw newline unaffected.
    {
        auto r = ts("{#define X 1 (* a // b\n c *) +2}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X 1 (* a // b  c *) +2}", r[0].text);
    }

    // L7: a single '/' followed later by another '/' across a raw newline
    // is not a '//' opener -- each stays an ordinary character.
    {
        auto r = ts("{#define X a/\n/b}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X a/ /b}", r[0].text);
    }

    // L8: a decoded '$n' (real newline) inside a '//' comment ends the
    // comment right there -- unlike a raw newline, it is not folded to
    // whitespace, so the comment text up to it is kept, followed by a
    // literal newline character, and the directive continues normally.
    {
        auto r = ts("{#define X 1 // c$n+2\n+3}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X 1 // c$n+2 +3}", r[0].text);
    }

    // L9: a '//' comment that ends at the closing '}' (no raw newline in
    // between) is unaffected by this fix and stays in the body.
    {
        auto r = ts("{#define X 1 // c}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ("{#define X 1 // c}", r[0].text);
    }

    // L10: an ordinary (non-directive) pragma body follows the same rule.
    {
        auto r = ts("{x // c\ny}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ(Token::PRAGMA, r[0].type);
        EXPECT_EQ("{x // c y}", r[0].text);
    }
}

// ---- v2: '{' inside a directive body never opens a nested pragma ----
//
// read_pragma_body() always raises PP22 for a raw '{' inside a directive
// body and keeps it as a literal character (the directive itself is only
// closed by the next '}'), so a '{' seen while scanning for '//'/'(*'/'/*'
// is ordinary content, not a pragma opener.
TEST_F(LexerPragmaTest, DirectiveLineCommentBraceIsLiteral) {
    // L11: a '{' inside a '//' comment is still just a literal character
    // (one PP22), and the comment itself still ends at the raw newline.
    {
        Issue::ContinueMode guard({});
        auto r = ts("{#define X 1 // see {\n+2}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ(Token::DIRECTIVE, r[0].type);
        EXPECT_EQ("{#define X 1 +2}", r[0].text);
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, cs[0]);
    }

    // L12: a '{' inside a block comment ('(* ... *)') is likewise a literal
    // character (one PP22); the block comment is not truncated by the raw
    // newline and keeps spanning it.
    {
        Issue::ContinueMode guard({});
        auto r = ts("{#define X 1 (* { *)\n+2}");
        ASSERT_LE(1u, r.size());
        EXPECT_EQ(Token::DIRECTIVE, r[0].type);
        EXPECT_EQ("{#define X 1 (* { *) +2}", r[0].text);
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, cs[0]);
    }
}