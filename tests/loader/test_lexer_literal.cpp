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