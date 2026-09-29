#pragma once
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

// Decode $-escape sequences in directive text. An invalid escape is kept
// literally: "$" + a character that is neither a known escape nor a hex
// digit stays as those two characters ("$q" -> "$q"); "$X" (X a hex digit)
// not followed by a second hex digit stays "$X" and decoding resumes at the
// next character ("$4G" -> "$4G", "$4$n" -> "$4" + LF); a trailing "$" or
// "$X" stays as is. No diagnostic is raised here; *has_invalid (when
// non-null) is set to true if any escape was kept.
std::string decode_directive_text(std::string_view t, bool* has_invalid);

// As above, but raises INVALID_ESCAPE_SEQUENCE (PP21) once, with the raw
// text `t`, when `t` contains an invalid escape; then (if PP21 did not
// throw) returns the decoded text with the escape kept literally.
std::string decode_directive_text(std::string_view t);

// Encode special characters to $-escape sequences.
std::string encode_directive_text(std::string_view t);

// Parse "{#keyword arg}" or "{#keyword: arg}" -> (keyword, arg), decoding
// $-escapes in both with the literal-on-invalid rule (see
// decode_directive_text() above). Raises no diagnostic itself (not even for
// "{...}" that is not a directive at all -- INVALID_PP_SYNTAX is still
// raised for that, since the caller cannot recover a (key, arg) pair for it
// either way). *invalid_escape (when non-null) is set to the raw,
// undecoded text of the key or the arg -- whichever is invalid, key
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
