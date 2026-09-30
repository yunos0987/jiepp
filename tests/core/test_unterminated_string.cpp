#include "test_helper.hpp"

#include "loader/lexer.hpp"

// PP29 (UNTERMINATED_STRING_LITERAL): a string literal the lexer ended at a
// newline or at the end of the input, not at its closing quote (D1: such a
// literal is also never merged into the following one -- see
// push_string_token() in lexer_literal.cpp).

namespace {
class UnterminatedStringTest : public JieppTest {};

// STRING/WSTRING tokens from `text`, as (text, unterminated) pairs; any
// other token type (whitespace, identifiers, ...) is not of interest here.
std::vector<std::pair<std::string, bool>> str_marks(const std::string& text) {
    std::vector<std::pair<std::string, bool>> marks;
    for (auto& t : iec3_tokens_from_string(text, /*remove_comments=*/false)) {
        if (t.type == Token::STRING || t.type == Token::WSTRING)
            marks.emplace_back(t.text, t.unterminated);
    }
    return marks;
}

} // namespace

// ---- lexer marks (D1) ----

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAtEof) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}}), str_marks("'a"));
}

TEST_F(UnterminatedStringTest, LexerDoesNotMarkClosedLiteral) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a'", false}}), str_marks("'a'"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAtLf) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}}), str_marks("'a\nb"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAtCrLf) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}}), str_marks("'a\r\nb"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAfterEscapedQuote) {
    // $' is an escaped quote, not a closing one: still unterminated.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a$'", true}}), str_marks("'a$'\n"));
}

TEST_F(UnterminatedStringTest, LexerDoesNotMarkDollarNewlineContinuation) {
    // $ + newline is a line continuation, not an end-of-literal newline.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a$\nb'", false}}), str_marks("'a$\nb'"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedWstring) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"\"a", true}}), str_marks("\"a"));
}

// ---- D1: no merge into an unterminated literal ----

TEST_F(UnterminatedStringTest, NoMergeIntoUnterminatedPrecedingLiteral) {
    // 'a' (closed) followed by 'b (unterminated): the closed literal still
    // merges the way any adjacent pair does (Python-compatible splicing),
    // and the merged token inherits the new piece's unterminated mark.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'ab", true}}), str_marks("'a' 'b\n"));
}

TEST_F(UnterminatedStringTest, NoMergeAfterUnterminatedLiteral) {
    // 'a (unterminated) must not absorb the 'b' that follows it on the next
    // line: before the fix, push_string_token() merged them into a single
    // "'b'" token, silently dropping the "a" content.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}, {"'b'", false}}),
              str_marks("'a\n'b'"));
}

// ---- D1: output and line count unaffected, no diagnostics ----

TEST_F(UnterminatedStringTest, OutputUnchangedNoMergeSingleQuote) {
    // Before the fix: "x := 'abdef';\nz := 'r\n" -- the lexer's
    // Python-compatible merge dropped the unterminated literal's own
    // content into the following one's.
    EXPECT_EQ("x := 'abc\n'def';\n", pp("x := 'abc\n'def';\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, OutputUnchangedNoMergeDoubleQuote) {
    EXPECT_EQ("y := \"ab\n\"cd\";\n", pp("y := \"ab\n\"cd\";\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, OutputUnchangedBothLiteralsUnterminated) {
    EXPECT_EQ("z := 'q\n'r\n", pp("z := 'q\n'r\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, LineCounterUnaffectedByUnmergedLiteral) {
    EXPECT_EQ("'a\n'b'\nX 3;\n", pp("'a\n'b'\nX __LINE__;\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, ClosedLiteralsStillMerge) {
    EXPECT_EQ("'abcd'\n", pp("'ab'\n'cd'"));
    EXPECT_TRUE(empty());
}
