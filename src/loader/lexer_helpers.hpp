#pragma once
#include "token.hpp"
#include "../env/issue.hpp"
#include <cctype>
#include <string>
#include <vector>

namespace jiepp::detail {

// Character classification
inline bool is_ws_char(char c) {
    return (c == ' ') || (c == '\t') || (c == '\f') || (c == '\v') || (static_cast<unsigned char>(c) == 0xa0);
}

inline bool is_nl_char(char c) { return (c == '\n') || (c == '\r'); }

inline bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || (c == '_');
}

inline bool is_ident_cont(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || (c == '_');
}

// Consume one newline sequence at pos (\r\n, \r, \n).
// Returns 1 on success, 0 if not a newline.
int consume_one_nl(const std::string& s, std::size_t& pos);

// Execute fn with fallback line context for error reporting.
template <typename F>
void with_fallback_line(int lineno, F&& fn) {
    Issue::LineGuard guard(lineno);
    fn();
}

// Comment readers (lexer_comment.cpp)
Token read_block_comment(const std::string& text, std::size_t& pos,
                         int& lineno, char opener1, int type);
Token read_line_comment(const std::string& text, std::size_t& pos,
                        int& lineno, int type);

// Pragma/directive reader (lexer_pragma.cpp)
Token read_pragma_body(const std::string& text, std::size_t& pos,
                       int& lineno, char opener_sign,
                       std::vector<Token>& extra_nl);

// Literal helpers (lexer_literal.cpp)
bool try_push_date_or_time(std::vector<Token>& result,
                           const std::string& text, std::size_t& pos);
bool try_push_number(std::vector<Token>& result,
                     const std::string& text, std::size_t& pos);
void push_string_token(std::vector<Token>& result, int type, std::string str);

} // namespace jiepp::detail
