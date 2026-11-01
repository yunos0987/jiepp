#include "iec_61131-3.hpp"

// ---------------------------------------------------------------------------
// IEC string encoding/decoding (for directive arguments)
// ---------------------------------------------------------------------------

// e.g. "a$c" -> "'a$$c'", "a\nc" -> "'a$nc'"
std::string Util::encode_iec_string(std::string_view raw, const char quote)
{
    std::string r;
    r.reserve(raw.size() * 2);

    // A quote is written as a hex escape. In a double-byte string ("...",
    // WSTRING) IEC 61131-3 hex escapes have four digits ($hhhh), so a
    // two-digit "$22" there would take the next two characters as digits
    // too ("$22ab" is U+22AB).
    r += quote;
    for (unsigned char c : raw) {
        switch (c) {
        case '$': r += "$$"; break;
        case '\'': r += (quote == '"') ? "$0027" : "$27"; break;
        case '"': r += (quote == '"') ? "$0022" : "$22"; break;
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

// e.g. "a}b$c" -> "'a$7Db$24c'"
std::string Util::encode_line_marker_path(std::string_view raw)
{
    std::string r;
    r.reserve(raw.size() * 3 + 2);

    r += '\'';
    for (unsigned char c : raw) {
        switch (c) {
        case '$': r += "$24"; break;
        case '{': r += "$7B"; break;
        case '}': r += "$7D"; break;
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
    r += '\'';
    return r;
}
