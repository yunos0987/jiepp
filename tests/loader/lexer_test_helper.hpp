#pragma once
// lexer_test_helper.hpp — Shared token helpers for the lexer tests
// (test_lexer*.cpp) and other tests that compare token sequences.

#include "test_helper.hpp"
#include "loader/lexer.hpp"

// The tokens of `text`.
inline std::vector<Token> ts(const std::string& text, bool remove_comments = false) {
    return iec3_tokens_from_string(text, remove_comments);
}

inline Token make(int type, const std::string& text, int line = 0) {
    return Token::create(type, text, line);
}

inline void expect_tokens(const std::vector<Token>& expected,
                          const std::string& text,
                          bool remove_comments = false) {
    EXPECT_EQ(expected, ts(text, remove_comments));
}
