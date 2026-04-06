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
        || c == '\f' || c == '\v' || c == 0xa0;
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

std::string decode_directive_text(std::string_view t) {
    std::string r;
    r.reserve(t.size());

    int mode = 0;
    unsigned char hi = 0;

    for (unsigned char c : t) {
        if (mode == 0) {
            if (c == '$')
                mode = 1;
            else
                r += static_cast<char>(c);
            continue;
        }

        if (mode == 1) {
            switch (c) {
            case '$': r += '$'; mode = 0; continue;
            case '\'': r += '\''; mode = 0; continue;
            case '"': r += '"'; mode = 0; continue;
            case 'l': case 'L': r += '\x0a'; mode = 0; continue;
            case 'n': case 'N': r += '\n'; mode = 0; continue;
            case 'p': case 'P': r += '\x0c'; mode = 0; continue;
            case 'r': case 'R': r += '\r'; mode = 0; continue;
            case 't': case 'T': r += '\t'; mode = 0; continue;
            case '{': r += '{'; mode = 0; continue;
            case '}': r += '}'; mode = 0; continue;
            case ':': r += ':'; mode = 0; continue;
            case ' ': r += ' '; mode = 0; continue;
            default:
                if (is_hex_digit(c)) {
                    hi = c;
                    mode = 2;
                    continue;
                }
                ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(t));
                return "";
            }
        }

        if (!is_hex_digit(c)) {
            ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(t));
            return "";
        }
        r += static_cast<char>((hex_code(hi) << 4) | hex_code(c));
        mode = 0;
    }

    if (mode != 0) {
        ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(t));
        return "";
    }

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
