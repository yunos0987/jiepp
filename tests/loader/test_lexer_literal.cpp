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

// ---- date/time literal splitting ----

class LexerLiteralTest : public JieppTest {};

TEST_F(LexerLiteralTest, DateLiterals) {
    expect_tokens({make(Token::ANY, "date"), make(Token::ANY, "#"), make(Token::ANY, "1987-6-5")},
                  "date#1987-6-5");
    expect_tokens({make(Token::ANY, "DATE"), make(Token::WS, "   "), make(Token::ANY, "#"),
                   make(Token::WS, "  "), make(Token::ANY, "1987-6-5")},
                  "DATE   #  1987-6-5");
    expect_tokens({make(Token::ANY, "DATE"), make(Token::ANY, "#"),
                   make(Token::ANY, "1987-6-5"), make(Token::ANY, "xyz")},
                  "DATE#1987-6-5xyz");
    expect_tokens({make(Token::ANY, "tod"), make(Token::ANY, "#"), make(Token::ANY, "12:34:56")},
                  "tod#12:34:56");
    expect_tokens({make(Token::ANY, "tod"), make(Token::ANY, "#"), make(Token::ANY, "1:2:3.45")},
                  "tod#1:2:3.45");
    expect_tokens({make(Token::ANY, "TIME_OF_DAY"), make(Token::WS, "   "), make(Token::ANY, "#"),
                   make(Token::WS, "  "), make(Token::ANY, "1:2:3.45")},
                  "TIME_OF_DAY   #  1:2:3.45");
    expect_tokens({make(Token::ANY, "TIME_OF_DAY"), make(Token::ANY, "#"),
                   make(Token::ANY, "1:2:3.45"), make(Token::ANY, "xyz")},
                  "TIME_OF_DAY#1:2:3.45xyz");
    expect_tokens({make(Token::ANY, "dt"), make(Token::ANY, "#"),
                   make(Token::ANY, "1978-9-01-12:34:56.78")},
                  "dt#1978-9-01-12:34:56.78");
    expect_tokens({make(Token::ANY, "DATE_AND_TIME"), make(Token::WS, "   "), make(Token::ANY, "#"),
                   make(Token::WS, "  "), make(Token::ANY, "1978-9-01-12:34:56.78")},
                  "DATE_AND_TIME   #  1978-9-01-12:34:56.78");
    expect_tokens({make(Token::ANY, "DATE_AND_TIME"), make(Token::ANY, "#"),
                   make(Token::ANY, "1978-9-01-12:34:56.78"), make(Token::ANY, "xyz")},
                  "DATE_AND_TIME#1978-9-01-12:34:56.78xyz");
    EXPECT_TRUE(empty());
}

TEST_F(LexerLiteralTest, TimeLiterals) {
    expect_tokens({make(Token::ANY, "time"), make(Token::ANY, "#"), make(Token::ANY, "-1s")},
                  "time#-1s");
    expect_tokens({make(Token::ANY, "LTIME"), make(Token::WS, "   "), make(Token::ANY, "#"),
                   make(Token::WS, "  "), make(Token::ANY, "1.2s3.4ms")},
                  "LTIME   #  1.2s3.4ms");
    expect_tokens({make(Token::ANY, "LTIME"), make(Token::ANY, "#"),
                   make(Token::ANY, "-234sx"), make(Token::ANY, "yz")},
                  "LTIME#-234sxyz");
    EXPECT_TRUE(empty());
}

// ---- numbers ----

TEST_F(LexerLiteralTest, Numbers) {
    expect_tokens({make(Token::ANY, "0")}, "0");
    expect_tokens({make(Token::ANY, "1234567890")}, "1234567890");
    expect_tokens({make(Token::ANY, "1_2_3_4_5_6_7_8_9_0")}, "1_2_3_4_5_6_7_8_9_0");
    expect_tokens({make(Token::ANY, "16#0f")}, "16#0f");
    expect_tokens({make(Token::ANY, "16#f_f")}, "16#f_f");
    // Ed.3: one '_' may directly follow '#', before the first digit -- one
    // token, not "16#" split from a macro-expandable identifier "_ff".
    expect_tokens({make(Token::ANY, "16#_ff")}, "16#_ff");
    // A doubled leading '_' still fails to match: only "16" is one token,
    // and "#" / the identifier "__ff" are re-lexed separately (not one
    // Token::ANY covering the whole literal).
    expect_tokens({make(Token::ANY, "16"), make(Token::ANY, "#"), make(Token::ANY, "__ff")},
                  "16#__ff");
    expect_tokens({make(Token::ANY, "10#01")}, "10#01");
    expect_tokens({make(Token::ANY, "1_2_3")}, "1_2_3");
    expect_tokens({make(Token::ANY, "1_2_3"), make(Token::ANY, "s")}, "1_2_3s");
    expect_tokens({make(Token::ANY, "0.0")}, "0.0");
    expect_tokens({make(Token::ANY, "1.0")}, "1.0");
    expect_tokens({make(Token::ANY, "2.0e+0")}, "2.0e+0");
    expect_tokens({make(Token::ANY, "3.0E-4")}, "3.0E-4");
    expect_tokens({make(Token::ANY, "1e0")}, "1e0");
    EXPECT_TRUE(empty());
}

// ---- string literal passthrough ----

TEST_F(LexerLiteralTest, String1) {
    EXPECT_EQ("'abc'",                    pp("'abc'"));
    EXPECT_EQ("'abc','$0041$$$n$'$\"$t'", pp("'abc','$0041$$$n$'$\"$t'"));
    EXPECT_TRUE(empty());
}

TEST_F(LexerLiteralTest, String2) {
    EXPECT_EQ("\"abc\"",                       pp("\"abc\""));
    EXPECT_EQ("\"abc\",\"$0041$$$n$'$\"\"$t\"", pp("\"abc\",\"$0041$$$n$'$\"\"$t\""));
    // The unescaped quote right after "$\"" (a $-escaped quote) closes this
    // literal early; the trailing $t" then opens a third literal that is
    // never closed, so this input genuinely triggers PP29 -- output is
    // unaffected (it is still copied through verbatim).
    EXPECT_EQ("<unknown location>:1.0: warning: PP29: Missing terminating quote character; '\"'",
              message());
}

// ---- $-newline continuation inside a string literal (B13) ----

TEST_F(LexerLiteralTest, DollarNewlineContinuationCountsLines) {
    // One embedded $-escaped newline: the token must carry num_of_lines = 1
    // so downstream line-number bookkeeping (advance_lineno) is not silently
    // dropped, even though the raw text is passed through unchanged.
    expect_tokens({make(Token::STRING, "'line1 $\nline2'", 1)}, "'line1 $\nline2'");
    // Two embedded $-escaped newlines in one literal.
    expect_tokens({make(Token::WSTRING, "\"a$\nb$\nc\"", 2)}, "\"a$\nb$\nc\"");
    // A plain (non-escaped) newline inside a string is not reachable (the
    // scanner stops at the quote or bare newline), so only $-escaped ones
    // contribute to num_of_lines here.
    EXPECT_TRUE(empty());
}

// ---- adjacent string literals: Python-compatible merging across whitespace ----

TEST_F(LexerLiteralTest, StringSequence1) {
    EXPECT_EQ("'abcd'   ", pp("'ab'   'cd'"));
    EXPECT_EQ("'\xE5\x88\xB6\xE5\xBE\xA1\xF0\x9F\x91\x8D\xEF\xB8\x8F'   ",
              pp("'\xE5\x88\xB6\xE5\xBE\xA1'   '\xF0\x9F\x91\x8D\xEF\xB8\x8F'"));
    EXPECT_EQ("'abcdef'    ", pp("'ab'   'cd' 'ef'"));
    EXPECT_TRUE(empty());
}

TEST_F(LexerLiteralTest, StringSequence2) {
    EXPECT_EQ("\"abcd\"   ", pp("\"ab\"   \"cd\""));
    EXPECT_EQ("\"\xE5\x88\xB6\xE5\xBE\xA1\xF0\x9F\x91\x8D\xEF\xB8\x8F\"   ",
              pp("\"\xE5\x88\xB6\xE5\xBE\xA1\"   \"\xF0\x9F\x91\x8D\xEF\xB8\x8F\""));
    EXPECT_EQ("\"abcdef\"    ", pp("\"ab\"   \"cd\" \"ef\""));
    EXPECT_TRUE(empty());
}

// ---- zombie WS/newline token regression (F1) ----

TEST_F(LexerLiteralTest, StringLiteralNotFirstTokenLeavesNoZombie) {
    // push_string_token() pops the trailing WS/newline run off `result`
    // into a local buffer and re-appends it; before the fix, the popped
    // slots were never actually removed from `result` (no resize/erase),
    // so a moved-from-but-still-WS-typed "zombie" token (empty text, but
    // a real, nonzero num_of_lines carried over from Token's defaulted
    // move) survived in the vector ahead of the freshly re-appended copy.
    // This only manifests when the string literal is NOT the first token
    // in the stream (i.e. there is a real trailing-WS run to pop), which
    // is why the pre-existing DollarNewlineContinuationCountsLines test
    // (string literal as the sole/first input token) never caught it.
    const auto toks = ts("A;\n'x';");
    int ws_tokens_before_string = 0;
    int total_num_of_lines = 0;
    bool seen_string = false;
    for (const auto& t : toks) {
        if (!seen_string && t.type == Token::WS)
            ++ws_tokens_before_string;
        if (t.type == Token::STRING)
            seen_string = true;
        total_num_of_lines += t.num_of_lines;
    }
    EXPECT_TRUE(seen_string);
    // Exactly one newline token (from "A;\n") precedes the string literal;
    // a zombie duplicate would make this 2.
    EXPECT_EQ(1, ws_tokens_before_string);
    // The stream contains exactly one physical newline; a zombie would
    // double-count it.
    EXPECT_EQ(1, total_num_of_lines);
    EXPECT_TRUE(empty());
}

// ---- extended sequence cases ----

TEST_F(LexerLiteralTest, StringSequence1Extended) {
    EXPECT_EQ("'a'   ", pp("'a'   ''"));
    EXPECT_EQ("'a'   ", pp("''   'a'"));
    EXPECT_EQ("''   ", pp("''   ''"));
    EXPECT_EQ("'$'$''   ", pp("'$''   '$''"));
    EXPECT_EQ("'\"$\"'   ", pp("'\"'   '$\"'"));
    EXPECT_EQ("'$n$n'   ", pp("'$n'   '$n'"));
    EXPECT_EQ("'abcd'  \n ", pp("'ab'  \n 'cd'"));
    EXPECT_EQ("'abcdef' \n     \n    ", pp("'ab' \n  'cd'   \n    'ef'"));
    EXPECT_EQ("'abcdef'    \n  ", pp("'ab'   'cd' \n  'ef'"));
    EXPECT_EQ("'abcdef' \n     ", pp("'ab' \n  'cd'   'ef'"));
        EXPECT_EQ("'ab' /*x*/ 'cd' /*x*/ 'ef'", pp("'ab' /*x*/ 'cd' /*x*/ 'ef'"));
    EXPECT_EQ("'ab' (*x*) 'cd' (*x*) 'ef'", pp("'ab' (*x*) 'cd' (*x*) 'ef'"));
    EXPECT_EQ("'abcd'    (*x*) 'ef'", pp("'ab'   'cd' (*x*) 'ef'"));
    EXPECT_EQ("'ab' (*x*) 'cdef'   ", pp("'ab' (*x*) 'cd'   'ef'"));
    EXPECT_EQ("\"ab\" 'cd'", pp("\"ab\" 'cd'"));
    EXPECT_TRUE(empty());
}

TEST_F(LexerLiteralTest, StringSequence2Extended) {
    EXPECT_EQ("\"a\"   ", pp("\"a\"   \"\""));
    EXPECT_EQ("\"a\"   ", pp("\"\"   \"a\""));
    EXPECT_EQ("\"\"   ", pp("\"\"   \"\""));
    EXPECT_EQ("\"$\"$\"\"   ", pp("\"$\"\"   \"$\"\""));
    EXPECT_EQ("\"'$'\"   ", pp("\"'\"   \"$'\""));
    EXPECT_EQ("\"$n$n\"   ", pp("\"$n\"   \"$n\""));
    EXPECT_EQ("\"abcd\"  \n ", pp("\"ab\"  \n \"cd\""));
    EXPECT_EQ("\"abcdef\" \n     \n    ", pp("\"ab\" \n  \"cd\"   \n    \"ef\""));
    EXPECT_EQ("\"abcdef\"    \n  ", pp("\"ab\"   \"cd\" \n  \"ef\""));
    EXPECT_EQ("\"abcdef\" \n     ", pp("\"ab\" \n  \"cd\"   \"ef\""));
    EXPECT_EQ("\"ab\" /*x*/ \"cd\" /*x*/ \"ef\"", pp("\"ab\" /*x*/ \"cd\" /*x*/ \"ef\""));
    EXPECT_EQ("\"ab\" (*x*) \"cd\" (*x*) \"ef\"", pp("\"ab\" (*x*) \"cd\" (*x*) \"ef\""));
    EXPECT_EQ("\"abcd\"    (*x*) \"ef\"", pp("\"ab\"   \"cd\" (*x*) \"ef\""));
    EXPECT_EQ("\"ab\" (*x*) \"cdef\"   ", pp("\"ab\" (*x*) \"cd\"   \"ef\""));
    EXPECT_EQ("'ab' \"cd\"", pp("'ab' \"cd\""));
    EXPECT_TRUE(empty());
}