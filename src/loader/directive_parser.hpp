#pragma once
#include <string>
#include <string_view>
#include <utility>

// Decode $-escape sequences in directive text.
std::string decode_directive_text(std::string_view t);

// Encode special characters to $-escape sequences.
std::string encode_directive_text(std::string_view t);

// Parse "{#keyword arg}" or "{#keyword: arg}" -> (keyword, arg).
// Returns ("", "") on parse error (also calls Issue::happen).
std::pair<std::string, std::string> parse_directive(std::string_view text);
