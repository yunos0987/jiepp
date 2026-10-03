#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Util {

std::string_view ltrim_view(std::string_view s);
std::string_view rtrim_view(std::string_view s);
std::string_view trim_view(std::string_view s);

// Re-escapes a real newline/carriage-return (e.g. one decoded from a $n/$r
// source escape) back to the literal two-character "$n"/"$r" sequence, so
// text that may contain either can still be embedded in a single-line
// diagnostic message without splitting it.
std::string escape_line_breaks(std::string_view s);

// Longest prefix of s that is at most max_bytes long and does not end inside
// a UTF-8 multibyte character (a cut that would leave a lead byte without all
// of its continuation bytes is moved back to the character boundary). ASCII
// text is cut at exactly max_bytes.
std::string_view utf8_head(std::string_view s, std::size_t max_bytes);

// Longest suffix of s that is at most max_bytes long and does not start with a
// UTF-8 continuation byte (0x80-0xBF); the cut is moved forward to the next
// character boundary. ASCII text is cut at exactly max_bytes.
std::string_view utf8_tail(std::string_view s, std::size_t max_bytes);

} // namespace Util
