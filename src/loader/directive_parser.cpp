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

// Skips whitespace and comments -- (* *), /* */, and // (which runs to the
// end: a raw newline already ended it in read_pragma_body()) -- the way
// gcc/clang accept "#/*c*/define". A document comment ((*!, /*!, //!) is a
// token in jiepp and is not skipped; an unclosed block comment is not a
// comment here, so "{# /*}" keeps "/*" as the (unknown) name.
std::size_t skip_directive_ws_and_comments(std::string_view text, std::size_t pos) {
    for (;;) {
        pos = skip_directive_ws(text, pos);
        if (pos + 1 >= text.size())
            return pos;
        const char c = text[pos], d = text[pos + 1];
        const bool doc = pos + 2 < text.size() && text[pos + 2] == '!';
        if ((c == '(' || c == '/') && d == '*' && !doc) {
            const std::size_t e = text.find(c == '(' ? "*)" : "*/", pos + 2);
            if (e == std::string_view::npos)
                return pos;
            pos = e + 2;
            continue;
        }
        if (c == '/' && d == '/' && !doc)
            return text.size();
        return pos;
    }
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

std::optional<char> decode_directive_only_escape(char nc) {
    switch (nc) {
    case '{': return '{';
    case '}': return '}';
    case ':': return ':';
    case ' ': return ' ';
    default: return std::nullopt;
    }
}

void DirectiveLexState::feed(char c, bool raw, std::size_t off) {
    const bool nl = (c == '\n' || c == '\r');
    switch (state_) {
    case State::Normal:
        if (prev_ == '/' && c == '/') { state_ = State::Line; start_ = prev_off_; prev_ = 0; return; }
        if ((prev_ == '/' || prev_ == '(') && c == '*') {
            closer_ = (prev_ == '(') ? ')' : '/'; state_ = State::Block; star_ = false; prev_ = 0; return;
        }
        if (c == '\'' || c == '"') {
            quote_ = c; raw_str_ = raw; esc_ = false; state_ = State::Str; prev_ = 0; return;
        }
        prev_ = c; prev_off_ = off; return;
    case State::Str:                       // mirrors tokenize()'s string loop
        if (esc_) { esc_ = false; return; } // '$'-escaped char
        if (c == '$') { esc_ = true; return; }
        if (c == quote_ || nl) state_ = State::Normal;
        return;
    case State::Line:                      // a decoded $n/$r/$0A/$0D ends it
        if (nl) { state_ = State::Normal; prev_ = 0; }
        return;
    case State::Block:
        if (star_ && c == closer_) { state_ = State::Normal; prev_ = 0; return; }
        star_ = (c == '*');
        return;
    }
}

std::string decode_directive_text(std::string_view t, bool* has_invalid) {
    std::string r;
    r.reserve(t.size());
    DirectiveLexState lex;
    std::size_t i = 0;
    auto put = [&](char c, bool raw) {
        r += c;
        lex.feed(c, raw);
    };
    // Keep the n raw characters of an invalid escape as they are.
    auto keep_invalid = [&](std::size_t n) {
        for (std::size_t k = 0; k < n; ++k)
            put(t[i + k], true);
        i += n;
        if (has_invalid)
            *has_invalid = true;
    };
    while (i < t.size()) {
        const char c = t[i];
        if (c != '$') {
            // A raw quote opens a raw string only if its literal closes
            // within t; an unclosed one is decoded like the text around it.
            put(c, !lex.opens_string(c) || string_literal_end(t, i) != std::string_view::npos);
            ++i;
            continue;
        }
        if (lex.in_raw_string()) {
            // An IEC 61131-3 string escape ("$'", "$n", "$$", "$41", ...)
            // stays as written for the compiler; only "${" "$}" "$:" "$ "
            // are decoded, since they cannot be written otherwise.
            if (i + 1 < t.size()) {
                if (auto decoded = decode_directive_only_escape(t[i + 1])) {
                    put(*decoded, false);
                } else {
                    put('$', true);
                    put(t[i + 1], true);
                }
                i += 2;
            } else {
                put('$', true);            // unterminated string; not ours to report
                ++i;
            }
            continue;
        }
        if (i + 1 >= t.size()) {   // trailing "$"
            keep_invalid(1);
            continue;
        }
        const unsigned char nc = static_cast<unsigned char>(t[i + 1]);
        if (auto decoded = decode_directive_escape(static_cast<char>(nc))) {
            put(*decoded, false);
            i += 2;
            continue;
        }
        if (is_hex_digit(nc) && i + 2 < t.size()
            && is_hex_digit(static_cast<unsigned char>(t[i + 2]))) {
            put(static_cast<char>((hex_code(nc) << 4)
                                  | hex_code(static_cast<unsigned char>(t[i + 2]))), false);
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

std::size_t string_literal_end(std::string_view s, std::size_t pos) {
    const char quote = s[pos];
    for (std::size_t i = pos + 1; i < s.size(); ++i) {
        if (s[i] == '\n' || s[i] == '\r')
            break;
        if (s[i] == '$') {
            ++i;                       // skip the escaped character
            if (i < s.size() && (s[i] == '\n' || s[i] == '\r'))
                break;
            continue;
        }
        if (s[i] == quote)
            return i + 1;
    }
    return std::string_view::npos;
}

std::string decode_path_literal(std::string_view lit) {
    bool has_invalid = false;
    std::string r = decode_directive_text(lit.substr(1, lit.size() - 2), &has_invalid);
    if (has_invalid)
        ISSUE(INVALID_ESCAPE_SEQUENCE, std::string(lit));
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

std::pair<std::string, std::string> parse_directive(std::string_view text,
                                                      std::optional<std::string>& invalid_escape) {
    invalid_escape.reset();
    if (text.size() < 3 || text.front() != '{' || text.back() != '}') {
        ISSUE(INVALID_PP_SYNTAX, std::string(text));
        return {"", ""};
    }
    std::string_view inner = text.substr(2, text.size() - 3);
    if (text[1] != '#') {
        ISSUE(INVALID_PP_SYNTAX, std::string(text));
        return {"", ""};
    }

    std::size_t pos = skip_directive_ws_and_comments(inner, 0);
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
        // A comment ends the name, like whitespace ("{#define(*c*)X 1}"):
        // reuse the same recognizer as the leading skip, so a document
        // comment (a token, kept as part of an unrecognized name, e.g.
        // "{#(*! d *)define X 1}" stays PP45) and an unclosed block comment
        // (not a comment at all, e.g. "{# /*}") are treated the same way
        // here as there.
        if (skip_directive_ws_and_comments(inner, pos) != pos)
            break;
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

    // Decode key and value without raising PP21 (the flag overload); report
    // at most one invalid escape for the whole directive to the caller, key
    // preferred over value, so a caller that ends up raising PP21 does so
    // exactly once no matter how many "$q"-style escapes the directive has.
    bool key_invalid = false, value_invalid = false;
    std::string key = decode_directive_text(raw_key, &key_invalid);
    std::string value = decode_directive_text(raw_value, &value_invalid);
    if (key_invalid)
        invalid_escape = std::string(raw_key);
    else if (value_invalid)
        invalid_escape = std::string(raw_value);

    return {std::move(key), std::move(value)};
}

std::pair<std::string, std::string> parse_directive(std::string_view text) {
    std::optional<std::string> invalid_escape;
    auto r = parse_directive(text, invalid_escape);
    if (invalid_escape)
        ISSUE(INVALID_ESCAPE_SEQUENCE, *invalid_escape);
    return r;
}
