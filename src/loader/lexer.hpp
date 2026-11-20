#pragma once
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "token.hpp"

// Tokenize text string (source text: diagnostics at the text's own lines,
// counted from lineno, 1-based). report_nested_brace: as for iec3_relex()
// below. Callers read the text through Loader::read_file() /
// Loader::read_stream(), which count bytes for --max-input-bytes (PP67) and
// report read errors (PP10).
std::vector<Token> iec3_tokens_from_string(const std::string& input, bool remove_comments, LineNo lineno = 1,
                                           bool report_nested_brace = true);

// Re-lexes text that is not read verbatim from a source file: a decoded
// directive operand, a pragma body, or macro-produced text. Its newlines are
// not source lines, so every lexer diagnostic is reported at the current
// Issue location (Issue::lineno()/filepath()), like the other diagnostics of
// the directive or pragma being processed, moved down by line_map (below)
// when the text's source lines are known; call it only where that location
// is set. Otherwise identical to iec3_tokens_from_string(), except that a
// leading UTF-8 BOM in the text is kept (source text strips it).
// report_nested_brace: false only when re-lexing the body of a pragma that
// was itself produced by re-lexing a pragma body (Token::relexed_pragma):
// a raw '{' inside a nested pragma/directive body is then not reported
// (PP22), since the enclosing pass already reported it. Every other
// diagnostic is reported as usual, except that report_unclosed_comment ==
// false skips PP20 (unclosed comment), for the rescan of a pragma body that
// already reported it. found_unclosed_comment, if not null, is set to true
// when the text has an unclosed comment (reported or not); never set to false.
// line_map, if not null, gives the source lines of the text (offsets into
// it): a diagnostic is then reported that many lines below the current Issue
// location, and the pragmas, directives and unterminated literals lexed from
// the text carry the part of the map that covers them (Token::line_map).
std::vector<Token> iec3_relex(const std::string& text, bool remove_comments,
                              bool report_nested_brace = true,
                              bool report_unclosed_comment = true,
                              bool* found_unclosed_comment = nullptr,
                              const LineMap* line_map = nullptr);

// The -nC filter (remove_comments above): replaces every comment token
// (Token::C) by a newline token of its num_of_lines, or by one space when it
// has none. Document comments (Token::DOCUMENT) are kept.
void iec3_remove_comments(std::vector<Token>& tokens);

// The token-boundary rules of macro expansion (SPEC §3.9, §4.3). These read
// text the way the source lexer does, without any diagnostic, and count
// each IEC 61131-3 operator of two characters (:= => <= >= <> ** ..) as one
// token, although the lexer reads it as two single-character tokens: the
// output is read by an IEC compiler, which takes it as one token.
//
// At a macro-expansion edge: whether `before` (the text that ends at the
// edge, a few output tokens) and `after` (the text that starts there),
// written together, read as other tokens than each does alone -- the edge
// is no longer a token boundary: "a" "b" (one identifier), "1" ".5" (one
// number), "<" "=" (the operator <=), "/" "/" (a comment) -- while one space
// at the edge keeps them apart. False for two literals with the same quote,
// which the lexer merges with or without a space, and for a '#' on either
// side: the lexer splits an IEC typed literal (16#FF, T#1s, INT#5) at its
// '#' into several tokens written together, so none is a boundary to keep.
bool iec3_needs_space(std::string_view before, std::string_view after);

// A cheap necessary condition for iec3_needs_space(): false when the last
// output token `left` (after `before_left`, the output token before it, or
// "" when that is whitespace or there is none) followed by text starting
// with the characters `right` and `right2` ('\0' when unknown or none) can
// never lex differently, so the texts need not be built and lexed (this
// runs at every macro-expansion edge).
bool iec3_may_join(std::string_view before_left, std::string_view left, char right,
                   char right2);

// The type of the one token that `left` and `right` written together read
// as -- with the same spelling, so not two literals that the lexer merges
// into one -- or nullopt when they read as more than one token, as an
// unterminated literal, or as a comment, pragma or directive. A paste on
// either side of the '#' of an IEC typed literal (`16 #`, `# FF`, `T #`,
// `# 1s`; the lexer reads such a literal as several tokens) counts as one
// token (Token::ANY).
std::optional<int> iec3_pasted_token_type(std::string_view left, std::string_view right);

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
std::string iec3_blank_out_comments(std::string_view s);
