#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "../env/issue.hpp"

// Character a two-character "$<nc>" directive escape decodes to, or
// std::nullopt when nc is a hex digit (first half of "$XX") or invalid.
// Shared between decode_directive_text() (this file) and read_pragma_body()
// (lexer_pragma.cpp), which needs the decoded character -- without
// appending it to the body, since that decoding still happens later here --
// to track '//' comments over the decoded character stream.
std::optional<char> decode_directive_escape(char nc);

// Character a "$<nc>" escape that exists only for directives -- "${",
// "$}", "$:", "$ " (not IEC 61131-3 string escapes) -- decodes to, or
// std::nullopt. These are the only escapes decoded inside a string literal
// written with raw quotes in a directive (see decode_directive_text()): a
// raw '}' would close the directive and a raw '{' is PP22 even inside a
// string, so "$}" / "${" are the only way to write them there.
std::optional<char> decode_directive_only_escape(char nc);

// Lexical state of a directive operand, fed one character at a time as
// decode_directive_text() outputs it. Follows tokenize()'s (lexer.cpp)
// rules for where a string literal ('...' / "...", '$' escapes the next
// character, ended by its quote or a newline) and a comment ((* *), /* */,
// // up to a newline) start and end, so it sees strings and comments where
// the handlers' later re-lexing of the decoded operand will. `raw` is true
// for a character written as is and false for one produced by a '$'
// escape; it matters only for a quote that opens a string: a raw quote
// whose literal also closes within the operand opens a "raw string",
// inside which decode_directive_text() keeps IEC 61131-3 string escapes as
// written. A quote produced by "$'" / "$27" -- so text written with
// escaped quotes, like -dM's "$'it$$$'s$'", reads back unchanged -- and a
// raw quote that never closes (the apostrophe in {#error don't$nstop})
// are fed with raw == false: they open a string for the lexer, decoded as
// before. A '{' never opens a pragma here (a raw '{' is PP22 inside a
// directive). `off` is only recorded for read_pragma_body()
// (lexer_pragma.cpp), which uses line_comment_start() to cut a '//'
// comment at a raw newline.
class DirectiveLexState {
public:
    void feed(char c, bool raw, std::size_t off = 0);
    bool in_raw_string() const { return state_ == State::Str && raw_str_; }
    // Whether feeding c now would open a string literal.
    bool opens_string(char c) const {
        return state_ == State::Normal && (c == '\'' || c == '"');
    }
    bool in_line_comment() const { return state_ == State::Line; }
    std::size_t line_comment_start() const { return start_; }
    void end_line_comment() { state_ = State::Normal; prev_ = 0; }
private:
    enum class State { Normal, Str, Line, Block };
    State state_ = State::Normal;
    char prev_ = 0, quote_ = 0, closer_ = 0;
    bool esc_ = false, star_ = false, raw_str_ = false;
    std::size_t prev_off_ = 0, start_ = 0;
};

// Decode $-escape sequences in directive text (a directive's name or its
// operand). Outside string literals -- and inside comments -- every escape
// in the table (SPEC §2) is decoded. Inside a string literal written with
// raw quotes ('...' / "...", see DirectiveLexState) only the
// directive-only escapes of decode_directive_only_escape() are decoded;
// every other "$X" there is an IEC 61131-3 string escape and is kept as
// written ("'it$'s'" and "'a$nb'" stay as they are, like gcc/clang keep
// "\'" and "\n" inside a string literal in a macro body), and is never
// invalid here (validating IEC string escapes is the compiler's job; a
// trailing '$' in such a string is kept too). Outside raw strings, an
// invalid escape is kept literally: "$" + a character that is neither a
// known escape nor a hex digit stays as those two characters ("$q" ->
// "$q"); "$X" (X a hex digit) not followed by a second hex digit stays
// "$X" and decoding resumes at the next character ("$4G" -> "$4G", "$4$n"
// -> "$4" + LF); a trailing "$" or "$X" stays as is. No diagnostic is
// raised here; *has_invalid (when non-null) is set to true if any such
// invalid escape was kept.
std::string decode_directive_text(std::string_view t, bool* has_invalid);

// As above, but raises INVALID_ESCAPE_SEQUENCE (PP21) once, with the raw
// text `t`, when `t` contains an invalid escape; then (if PP21 did not
// throw) returns the decoded text with the escape kept literally.
std::string decode_directive_text(std::string_view t);

// Index just past the IEC 61131-3 string literal that starts at s[pos]
// (a '\'' or '"'): its closing quote, skipping "$X" escape pairs, as
// tokenize() does. std::string_view::npos if the literal is not closed
// before a newline or the end of s. For path operands, whose string
// literals keep their IEC escapes as written ("'it$'s.iec'").
std::size_t string_literal_end(std::string_view s, std::size_t pos);

// The file path written as the string literal `lit` ('...' or "...", as
// delimited by string_literal_end()): its contents with '$' escapes
// decoded by the same table as outside strings (so {#include 'a$$b.iec'}
// still names the file a$b.iec). An invalid escape is kept literally and
// raises INVALID_ESCAPE_SEQUENCE (PP21) with `lit` as the message.
std::string decode_path_literal(std::string_view lit);

// Encode special characters to $-escape sequences.
std::string encode_directive_text(std::string_view t);

// Parse "{#keyword arg}" or "{#keyword: arg}" -> (keyword, arg), decoding
// $-escapes in both with the literal-on-invalid rule (see
// decode_directive_text() above). Raises no diagnostic itself, except
// INVALID_PP_SYNTAX for a "{...}" that is not a directive at all (no
// key/arg pair can be recovered). *invalid_escape (when non-null) is set to
// the raw, undecoded text of the key or the arg -- whichever is invalid, key
// preferred if both are -- so the caller can raise INVALID_ESCAPE_SEQUENCE
// (PP21) itself, once per directive, only when/if the directive turns out
// to matter (e.g. not inside a skipped {#if 0} group).
std::pair<std::string, std::string> parse_directive(std::string_view text,
                                                      std::optional<std::string>& invalid_escape);

// As above, but raises INVALID_ESCAPE_SEQUENCE (PP21) once, unconditionally,
// when *invalid_escape would have been set. Returns ("", "") on parse error
// (also calls Issue::happen).
std::pair<std::string, std::string> parse_directive(std::string_view text);

// Classify a directive key that DirectiveToken::name_to_kind() could not
// resolve (kind == -1): a key that looks like a typo'd identifier (starts
// with an identifier char but contains a character not valid in an
// identifier, e.g. "if;") is INVALID_DIRECTIVE_NAME (PP46, ERROR); anything
// else (e.g. a plain, unrelated pragma body) is UNKNOWN_DIRECTIVE (PP45,
// ERROR). Shared between dispatch_directive() (expand.cpp, src/core/) --
// the sole place that reports the diagnostic -- and directive_token.cpp.
Issue::Code classify_unknown_directive(std::string_view key);
