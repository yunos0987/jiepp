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

std::string Util::strip_control_chars(std::string_view s, C1Pairs c1_pairs) {
    const bool remove_c1 = c1_pairs == C1Pairs::Remove;
    std::string out;
    out.reserve(s.size());
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x20 && c != '\n' && c != '\r' && c != '\t')
            continue;
        if (c == 0x7F)
            continue;
        if (remove_c1 && c >= 0x80 && c <= 0x9F && !out.empty() &&
            static_cast<unsigned char>(out.back()) == 0xC2) {
            out.pop_back();
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

std::string Util::line_breaks_to_spaces(std::string_view s) {
    std::string out(s);
    for (char& ch : out) {
        if (ch == '\n' || ch == '\r')
            ch = ' ';
    }
    return out;
}

std::string Util::escape_control_chars(std::string_view s, C1Pairs c1_pairs, bool keep_line_breaks) {
    static const char HEX[] = "0123456789ABCDEF";
    const bool escape_c1 = c1_pairs == C1Pairs::Remove;
    auto append_hex = [](std::string& out, unsigned char c) {
        out.push_back('$');
        out.push_back(HEX[c >> 4]);
        out.push_back(HEX[c & 0x0F]);
    };
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == '\n' || c == '\r') {
            if (keep_line_breaks)
                out.push_back(static_cast<char>(c));
            else
                out += (c == '\n') ? "$n" : "$r";
        } else if ((c < 0x20 && c != '\t') || c == 0x7F) {
            append_hex(out, c);
        } else if (escape_c1 && c == 0xC2 && i + 1 < s.size() &&
                   static_cast<unsigned char>(s[i + 1]) >= 0x80 &&
                   static_cast<unsigned char>(s[i + 1]) <= 0x9F) {
            append_hex(out, c);
            append_hex(out, static_cast<unsigned char>(s[i + 1]));
            ++i;
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
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
        else if (c == '\0')
            r += "$00";
        else
            r += c;
    }
    return r;
}
