#include "directive_parser.hpp"
#include "../env/issue.hpp"

#include <cctype>
#if defined(__cpp_lib_format)
#include <format>
#endif
#include <string>
#include <string_view>
#include <utility>

namespace {

bool is_directive_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\f' || c == '\v';
}

bool is_hex_digit(unsigned char c) {
    return std::isxdigit(c) != 0;
}

unsigned char hex_code(unsigned char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

std::size_t skip_directive_ws(std::string_view text, std::size_t pos) {
    while (pos < text.size() && is_directive_ws(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    return pos;
}

} // namespace

std::optional<char> decode_directive_escape(char nc) {
    switch (nc) {
    case '$': return '$';
    case '\'': return '\'';
    case '"': return '"';
    case 'l': case 'L': return '\x0a';
    case 'n': case 'N': return '\n';
    case 'p': case 'P': return '\x0c';
    case 'r': case 'R': return '\r';
    case 't': case 'T': return '\t';
    case '{': return '{';
    case '}': return '}';
    case ':': return ':';
    case ' ': return ' ';
    default: return std::nullopt;
    }
}

std::string decode_directive_text(std::string_view t, bool* has_invalid) {
    std::string r;
    r.reserve(t.size());
    std::size_t i = 0;
    // Keep the n raw characters of an invalid escape as they are.
    auto keep_invalid = [&](std::size_t n) {
        r.append(t.substr(i, n));
        i += n;
        if (has_invalid)
            *has_invalid = true;
    };
    while (i < t.size()) {
        const char c = t[i];
        if (c != '$') {
            r += c;
            ++i;
            continue;
        }
        if (i + 1 >= t.size()) {   // trailing "$"
            keep_invalid(1);
            continue;
        }
        const unsigned char nc = static_cast<unsigned char>(t[i + 1]);
        if (auto decoded = decode_directive_escape(static_cast<char>(nc))) {
            r += *decoded;
            i += 2;
            continue;
        }
        if (is_hex_digit(nc) && i + 2 < t.size()
            && is_hex_digit(static_cast<unsigned char>(t[i + 2]))) {
            r += static_cast<char>((hex_code(nc) << 4)
                                   | hex_code(static_cast<unsigned char>(t[i + 2])));
            i += 3;
            continue;
        }
        // "$q", or "$X" without a second hex digit: keep both characters;
        // whatever follows (possibly another '$') is decoded afresh.
        keep_invalid(2);
    }
    return r;
}

std::string decode_directive_text(std::string_view t) {
    bool has_invalid = false;
    std::string r = decode_directive_text(t, &has_invalid);
    if (has_invalid)
        ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(t));
    return r;
}

std::string encode_directive_text(std::string_view t) {
    std::string r;
    r.reserve(t.size() * 2);
    for (unsigned char c : t) {
        switch (c) {
        case '$': r += "$$"; break;
        case '\'': r += "$'"; break;
        case '"': r += "$\""; break;
        case '\n': r += "$n"; break;
        case '\x0c': r += "$p"; break;
        case '\r': r += "$r"; break;
        case '\t': r += "$t"; break;
        case '{': r += "${"; break;
        case '}': r += "$}"; break;
        case ':': r += "$:"; break;
        default:   r += static_cast<char>(c); break;
        }
    }
    return r;
}

std::pair<std::string, std::string> parse_directive(std::string_view text) {
    if (text.size() < 3 || text.front() != '{' || text.back() != '}') {
        ISSUE(INVALID_PP_SYNTAX, std::string(text));
        return {"", ""};
    }
    std::string_view inner = text.substr(2, text.size() - 3);
    if (text[1] != '#') {
        ISSUE(INVALID_PP_SYNTAX, std::string(text));
        return {"", ""};
    }

    std::size_t pos = skip_directive_ws(inner, 0);
    std::size_t key_start = pos;
    while (pos < inner.size()) {
        unsigned char c = static_cast<unsigned char>(inner[pos]);
        if (c == '$') {
            if (pos + 1 >= inner.size()) {
                break;
            }
            if (pos + 2 < inner.size()
                && is_hex_digit(static_cast<unsigned char>(inner[pos + 1]))
                && is_hex_digit(static_cast<unsigned char>(inner[pos + 2]))) {
                pos += 3;
            } else {
                pos += 2;
            }
            continue;
        }
        if (c == ':' || c == ';' || c == '\'' || c == '"' || c == '<' || is_directive_ws(c)) {
            break;
        }
        ++pos;
    }
    std::string_view raw_key = inner.substr(key_start, pos - key_start);

    std::string_view raw_rem = inner.substr(pos);
    std::size_t value_pos = skip_directive_ws(raw_rem, 0);
    if (value_pos < raw_rem.size() && raw_rem[value_pos] == ':') {
        ++value_pos;
        value_pos = skip_directive_ws(raw_rem, value_pos);
    }
    std::string_view raw_value = raw_rem.substr(value_pos);

    return {decode_directive_text(raw_key), decode_directive_text(raw_value)};
}
