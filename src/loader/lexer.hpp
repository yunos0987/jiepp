#pragma once
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>
#include "token.hpp"

// Tokenize input stream → Token vector
// lineno: starting line number (1-based)
std::vector<Token> iec3_tokens(std::istream& input, bool remove_comments, int lineno = 1);

// Tokenize text string
// report_nested_brace: false only when re-lexing the body of a pragma that
// was itself produced by re-lexing a pragma body (Token::relexed_pragma):
// a raw '{' inside a nested pragma/directive body is then not reported (PP22), since
// the enclosing pass already reported it. Every other diagnostic is
// reported as usual.
std::vector<Token> iec3_tokens_from_string(const std::string& input, bool remove_comments, int lineno = 1,
                                           bool report_nested_brace = true);

// True iff `s` is exactly one identifier as tokenize() lexes it:
// [A-Za-z_][A-Za-z0-9_]* (ASCII only, like IEC 61131-3 identifiers).
bool iec3_is_identifier(std::string_view s);

// Returns s with every comment -- (* *), /* */, and // up to the end of the
// line -- replaced by one space, as C treats a comment (translation phase 3).
// For directive operands that are parsed as text after macro expansion,
// where comments survive unless -nC. Follows tokenize(): a comment opener
// inside a '...'/"..." literal ($-escapes, ended by a newline) is not a
// comment; a document comment ((*! *), /*! */, //!) is a token and is kept
// verbatim; an opener followed by optional whitespace and '{' is a pragma
// opener and is kept; an unclosed block comment runs to the end of s (the
// lexer has already reported PP20).
//
// When header_name is true, a '<' that is the first character other than
// whitespace and comments starts a <...> path that is copied verbatim up to
// the first '>', so {#include <dir//a.iec>} keeps its path.
std::string iec3_blank_out_comments(std::string_view s, bool header_name = false);
