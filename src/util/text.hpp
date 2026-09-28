#pragma once

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

} // namespace Util
