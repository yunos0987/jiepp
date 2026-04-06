#include "iec_61131-3.hpp"

#include "../env/issue.hpp"

namespace {

inline bool is_hex_digit(unsigned char c)
{
    return std::isxdigit(c) != 0;
}

// e.g. 'a' -> 10, 'f' -> 15, 'A' -> 10, etc.
inline unsigned char hex_code(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

}

// ---------------------------------------------------------------------------
// IEC string encoding/decoding (for directive arguments)
// ---------------------------------------------------------------------------

// e.g. "a$c" -> "'a$$c'", "a\nc" -> "'a$nc'"
std::string Util::encode_iec_string(std::string_view raw, const char quote)
{
    std::string r;
    r.reserve(raw.size() * 2);

    r += quote;
    for (unsigned char c : raw) {
        switch (c) {
        case '$': r += "$$"; break;
        case '\'': r += "$27"; break;
        case '"': r += "$22"; break;
        case '\n': r += "$n"; break;
        case '\r': r += "$r"; break;
        case '\t': r += "$t"; break;
        case '\f': r += "$p"; break;
        default:
            r += static_cast<char>(c);
            break;
        }
    }
    r += quote;
    return r;
}

// e.g. "'a$$c'" -> "a$c", "\"a$nc\"" -> "a\nc"
std::string Util::decode_iec_string(std::string_view iec_string)
{
    if (iec_string.size() < 2) {
        FATAL();
        return "";
    }
    char c0 = iec_string.front(), c1 = iec_string.back();
    if (!((c0 == '\'' && c1 == '\'') || (c0 == '"' && c1 == '"'))) {
        FATAL();
        return "";
    }

    std::string_view body = iec_string.substr(1, iec_string.size() - 2);

    std::string r;
    r.reserve(body.size());

    int mode = 0;
    unsigned char hi = 0;

    for (unsigned char c : body) {
        switch (mode) {
        case 0:
            if (c == '$')
                mode = 1;
            else
                r += static_cast<char>(c);
            break;
        case 1:
            switch (c) {
            case '$': r += '$'; mode = 0; break;
            case '\'': r += '\''; mode = 0; break;
            case '\"': r += '\"'; mode = 0; break;
            case 'l': r += '\n'; mode = 0; break;
            case 'n': r += '\n'; mode = 0; break;
            case 'p': r += '\f'; mode = 0; break;
            case 'r': r += '\r'; mode = 0; break;
            case 't': r += '\t'; mode = 0; break;
            default:
                if (is_hex_digit(c)) {
                    hi = c;
                    mode = 2;
                    break;
                }
                ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(iec_string));
                return "";
            }
            break;
        case 2:
            if (is_hex_digit(c)) {
                r += static_cast<char>((hex_code(hi) << 4) | hex_code(c));
                mode = 0;
                break;
            }
            ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(iec_string));
            return "";
        default:
            FATAL();
            break;
        }
    }

    if (mode != 0) {
        ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(iec_string));
        return "";
    }

    return r;
}
