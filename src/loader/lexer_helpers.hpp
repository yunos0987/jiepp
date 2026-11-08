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
// report_unclosed_comment: whether an unclosed comment is reported (PP20).
//   false only for the rescan of a pragma body whose first pass already
//   reported it (Token::pragma_unterminated_reported).
// found_unclosed_comment: if not null, set to true when an unclosed comment
//   is met (whether or not it is reported); never set to false.
// line_map: re-lex only; the source lines of the text (LineMap, offsets
//   into the text being lexed), or null when it is on one line or its lines
//   are not known. A diagnostic is then reported line_of(pos) lines below
//   the current Issue location, and the PRAGMA/DIRECTIVE tokens and
//   unterminated literals lexed from the text get the part of the map that
//   covers them (Token::line_map).
struct LexOptions {
    bool report_nested_brace = true;
    bool at_current_location = false;
    bool report_unclosed_comment = true;
    bool* found_unclosed_comment = nullptr;
    const LineMap* line_map = nullptr;
};

// Runs fn, which raises a lexer diagnostic about the character at offset
// `pos` of the text: at the lexer's line `lineno` of the current file
// (source lexing), or at the current Issue location (re-lexing,
// LexOptions::at_current_location), moved down by the line map's lines
// before `pos` when the text has one.
template <typename F>
void report_lex_issue(LineNo lineno, LexOptions o, std::size_t pos, F&& fn) {
    if (o.at_current_location) {
        const std::size_t below = o.line_map ? o.line_map->line_of(pos) : 0;
        if (below == 0) {
            fn();
            return;
        }
        Issue::LineGuard guard(wrap_lineno(Issue::lineno() + static_cast<LineNo>(below)));
        fn();
        return;
    }
    // The counter is not wrapped while lexing; wrap it here like the other
    // line counters (LineNo wraps at 2^32).
    Issue::LineGuard guard(wrap_lineno(lineno));
    fn();
}

// Comment readers (lexer_comment.cpp)
Token read_block_comment(const std::string& text, std::size_t& pos,
                         LineNo& lineno, char opener1, int type,
                         LexOptions opts);
Token read_line_comment(const std::string& text, std::size_t& pos,
                        LineNo& lineno, int type);

// Pragma/directive reader (lexer_pragma.cpp)
Token read_pragma_body(const std::string& text, std::size_t& pos,
                       LineNo& lineno, char opener_sign,
                       std::vector<Token>& extra_nl,
                       LexOptions opts);

// Literal helpers (lexer_literal.cpp)
bool try_push_date_or_time(std::vector<Token>& result,
                           const std::string& text, std::size_t& pos);
bool try_push_number(std::vector<Token>& result,
                     const std::string& text, std::size_t& pos);
// State of push_string_token() for one tokenize() pass: tokens
// [0, scanned) of the result have been examined, and ws_run_begin is the
// index just after the last non-Token::WS token among them (0 if none).
// Valid only while tokens before `scanned` are not removed or retyped;
// push_string_token() asserts this in Debug builds and does not recover in
// Release builds.
struct StringMergeState {
    std::size_t ws_run_begin = 0;
    std::size_t scanned = 0;
};
// line_map: for an unterminated literal lexed from a multi-line re-lexed
// text, the line its quote is on (Token::line_map); also given to the
// literal it is merged into, which is then reported at this quote's line.
void push_string_token(std::vector<Token>& result, StringMergeState& state,
                       int type, std::string str,
                       int num_of_lines = 0, bool unterminated = false,
                       LineMapRef line_map = {});

} // namespace jiepp::detail
