#include "text.hpp"

#include <algorithm>
#include <cctype>

namespace {

bool _not_space(unsigned char ch) {
	return !std::isspace(ch);
}

}

std::string_view Util::ltrim_view(std::string_view s) {
    auto itr = std::find_if(s.begin(), s.end(), _not_space);
    return std::string_view(itr, s.end());
}

std::string_view Util::rtrim_view(std::string_view s) {
    auto itr = std::find_if(s.rbegin(), s.rend(), _not_space);
    return std::string_view(s.begin(), itr.base());
}

std::string_view Util::trim_view(std::string_view s) {
    return ltrim_view(rtrim_view(s));
}

namespace {

bool _is_utf8_continuation(char c) {
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

}

std::string_view Util::utf8_head(std::string_view s, std::size_t max_bytes) {
    if (s.size() <= max_bytes)
        return s;
    std::size_t cut = max_bytes;
    // s[cut] is the first dropped byte; a continuation byte there means the
    // cut is inside a character, so back up to that character's lead byte.
    while (cut > 0 && _is_utf8_continuation(s[cut]))
        --cut;
    return s.substr(0, cut);
}

std::string_view Util::utf8_tail(std::string_view s, std::size_t max_bytes) {
    if (s.size() <= max_bytes)
        return s;
    std::size_t start = s.size() - max_bytes;
    while (start < s.size() && _is_utf8_continuation(s[start]))
        ++start;
    return s.substr(start);
}

std::string Util::escape_line_breaks(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        if (c == '\n')
            r += "$n";
        else if (c == '\r')
            r += "$r";
        else
            r += c;
    }
    return r;
}
