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

// Decode $-escape sequences in directive text.
std::string decode_directive_text(std::string_view t);

// Encode special characters to $-escape sequences.
std::string encode_directive_text(std::string_view t);

// Parse "{#keyword arg}" or "{#keyword: arg}" -> (keyword, arg).
// Returns ("", "") on parse error (also calls Issue::happen).
std::pair<std::string, std::string> parse_directive(std::string_view text);

// Classify a directive key that DirectiveToken::name_to_kind() could not
// resolve (kind == -1): a key that looks like a typo'd identifier (starts
// with an identifier char but contains a character not valid in an
// identifier, e.g. "if;") is INVALID_DIRECTIVE_NAME (PP46, ERROR); anything
// else (e.g. a plain, unrelated pragma body) is UNKNOWN_DIRECTIVE (PP45,
// ERROR). Shared between dispatch_directive() (expand.cpp, src/core/) --
// the sole place that reports the diagnostic -- and directive_token.cpp.
Issue::Code classify_unknown_directive(std::string_view key);
