#pragma once
#include "token.hpp"
#include "../env/issue.hpp"
#include <cctype>
#include <string>
#include <vector>

namespace jiepp::detail {

// Character classification
inline bool is_ws_char(char c) {
    return (c == ' ') || (c == '\t') || (c == '\f') || (c == '\v');
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

// Options of one lexing pass, passed down by value from tokenize().
// report_nested_brace: whether a raw '{' inside a pragma/directive body is
//   reported as PP22 (see iec3_relex()). Nothing else depends
//   on it.
// at_current_location: true for a re-lex (iec3_relex()): the text's
//   newlines are not source lines, so a diagnostic is reported at the
//   current Issue location instead of at the lexer's own line counter.
struct LexOptions {
    bool report_nested_brace = true;
    bool at_current_location = false;
};

// Runs fn, which raises a lexer diagnostic: at the lexer's line `lineno` of
// the current file (source lexing), or at the current Issue location
// unchanged (re-lexing, LexOptions::at_current_location).
template <typename F>
void report_lex_issue(int lineno, LexOptions o, F&& fn) {
    if (o.at_current_location) {
        fn();
        return;
    }
    Issue::LineGuard guard(lineno);
    fn();
}

// Comment readers (lexer_comment.cpp)
Token read_block_comment(const std::string& text, std::size_t& pos,
                         int& lineno, char opener1, int type,
                         LexOptions opts);
Token read_line_comment(const std::string& text, std::size_t& pos,
                        int& lineno, int type);

// Pragma/directive reader (lexer_pragma.cpp)
Token read_pragma_body(const std::string& text, std::size_t& pos,
                       int& lineno, char opener_sign,
                       std::vector<Token>& extra_nl,
                       LexOptions opts);

// Literal helpers (lexer_literal.cpp)
bool try_push_date_or_time(std::vector<Token>& result,
                           const std::string& text, std::size_t& pos);
bool try_push_number(std::vector<Token>& result,
                     const std::string& text, std::size_t& pos);
void push_string_token(std::vector<Token>& result, int type, std::string str,
                       int num_of_lines = 0, bool unterminated = false);

} // namespace jiepp::detail
