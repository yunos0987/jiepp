#include "lexer_helpers.hpp"

namespace jiepp::detail {

namespace {

std::size_t consume_digits(const std::string& s, std::size_t pos) {
    if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos])))
        return std::string::npos;
    ++pos;
    while (pos < s.size()) {
        if (std::isdigit(static_cast<unsigned char>(s[pos]))) {
            ++pos;
            continue;
        }
        if (s[pos] == '_'
            && pos + 1 < s.size()
            && std::isdigit(static_cast<unsigned char>(s[pos + 1]))) {
            pos += 2;
            continue;
        }
        break;
    }
    return pos;
}

std::size_t consume_hhmmss(const std::string& s, std::size_t pos) {
    auto p = consume_digits(s, pos);
    if (p == std::string::npos || p >= s.size() || s[p] != ':')
        return std::string::npos;
    p = consume_digits(s, p + 1);
    if (p == std::string::npos || p >= s.size() || s[p] != ':')
        return std::string::npos;
    p = consume_digits(s, p + 1);
    if (p == std::string::npos)
        return std::string::npos;
    if (p < s.size() && s[p] == '.') {
        auto frac = consume_digits(s, p + 1);
        if (frac == std::string::npos)
            return std::string::npos;
        p = frac;
    }
    return p;
}

std::size_t consume_yymmdd(const std::string& s, std::size_t pos) {
    auto p = consume_digits(s, pos);
    if (p == std::string::npos || p >= s.size() || s[p] != '-')
        return std::string::npos;
    p = consume_digits(s, p + 1);
    if (p == std::string::npos || p >= s.size() || s[p] != '-')
        return std::string::npos;
    return consume_digits(s, p + 1);
}

std::size_t consume_date_value(const std::string& s, std::size_t pos) {
    auto p = consume_yymmdd(s, pos);
    if (p != std::string::npos) {
        if (p < s.size() && s[p] == '-') {
            auto dt = consume_hhmmss(s, p + 1);
            if (dt != std::string::npos)
                return dt;
        }
        return p;
    }
    return consume_hhmmss(s, pos);
}

std::size_t consume_time_unit(const std::string& s, std::size_t pos) {
    auto p = consume_digits(s, pos);
    if (p == std::string::npos)
        return std::string::npos;
    if (p < s.size() && s[p] == '.') {
        auto frac = consume_digits(s, p + 1);
        if (frac == std::string::npos)
            return std::string::npos;
        p = frac;
    }
    if (p >= s.size() || !std::isalpha(static_cast<unsigned char>(s[p])))
        return std::string::npos;
    ++p;
    if (p < s.size() && std::isalpha(static_cast<unsigned char>(s[p])))
        ++p;
    return p;
}

std::size_t consume_time_value(const std::string& s, std::size_t pos) {
    if (pos < s.size() && (s[pos] == '+' || s[pos] == '-'))
        ++pos;
    auto p = consume_time_unit(s, pos);
    if (p == std::string::npos)
        return std::string::npos;
    while (p < s.size()) {
        auto next_start = p;
        while (next_start < s.size() && s[next_start] == '_')
            ++next_start;
        auto next = consume_time_unit(s, next_start);
        if (next == std::string::npos)
            break;
        p = next;
    }
    return p;
}

} // anonymous namespace

bool try_push_date_or_time(std::vector<Token>& result,
                           const std::string& text, std::size_t& pos) {
    if (!is_ident_start(text[pos])) return false;

    std::size_t ident_end = pos + 1;
    while (ident_end < text.size() && is_ident_cont(text[ident_end])) ++ident_end;

    std::size_t ws1_end = ident_end;
    while (ws1_end < text.size() && is_ws_char(text[ws1_end])) ++ws1_end;
    if (ws1_end >= text.size() || text[ws1_end] != '#') return false;

    std::size_t ws2_end = ws1_end + 1;
    while (ws2_end < text.size() && is_ws_char(text[ws2_end]))
        ++ws2_end;

    std::size_t value_end = consume_date_value(text, ws2_end);
    if (value_end == std::string::npos)
        value_end = consume_time_value(text, ws2_end);
    if (value_end == std::string::npos)
        return false;

    result.push_back(Token::create(Token::ANY, text.substr(pos, ident_end - pos)));
    if (ws1_end > ident_end) {
        result.push_back(Token::create(Token::WS, text.substr(ident_end, ws1_end - ident_end)));
    }
    result.push_back(Token::create(Token::ANY, "#"));
    if (ws2_end > ws1_end + 1) {
        result.push_back(Token::create(Token::WS, text.substr(ws1_end + 1, ws2_end - ws1_end - 1)));
    }
    result.push_back(Token::create(Token::ANY, text.substr(ws2_end, value_end - ws2_end)));
    pos = value_end;
    return true;
}

bool try_push_number(std::vector<Token>& result,
                     const std::string& text, std::size_t& pos) {
    if (!std::isdigit(static_cast<unsigned char>(text[pos])))
        return false;

    auto start = pos;
    auto p = consume_digits(text, pos);
    if (p == std::string::npos)
        return false;

    if (p < text.size() && text[p] == '#') {
        auto q = p + 1;
        if (q < text.size() && (text[q] == '+' || text[q] == '-'))
            ++q;
        if (q < text.size() && std::isalnum(static_cast<unsigned char>(text[q]))) {
            ++q;
            while (q < text.size()) {
                if (std::isalnum(static_cast<unsigned char>(text[q]))) {
                    ++q;
                    continue;
                }
                if (text[q] == '_'
                    && q + 1 < text.size()
                    && std::isalnum(static_cast<unsigned char>(text[q + 1]))) {
                    q += 2;
                    continue;
                }
                break;
            }
            p = q;
        }
    } else if (p < text.size() && text[p] == '.') {
        auto frac = consume_digits(text, p + 1);
        if (frac != std::string::npos) {
            p = frac;
            if (p < text.size() && (text[p] == 'e' || text[p] == 'E')) {
                auto exp = p + 1;
                if (exp < text.size() && (text[exp] == '+' || text[exp] == '-'))
                    ++exp;
                auto exp_end = consume_digits(text, exp);
                if (exp_end != std::string::npos)
                    p = exp_end;
            }
        }
    } else if (p < text.size() && (text[p] == 'e' || text[p] == 'E')) {
        auto exp = p + 1;
        if (exp < text.size() && (text[exp] == '+' || text[exp] == '-'))
            ++exp;
        auto exp_end = consume_digits(text, exp);
        if (exp_end != std::string::npos)
            p = exp_end;
    }

    result.push_back(Token::create(Token::ANY, text.substr(start, p - start)));
    pos = p;
    return true;
}

void push_string_token(std::vector<Token>& result, int type, std::string str) {
    std::vector<Token> trailing_ws;
    std::size_t scan = result.size();
    while (scan > 0 && result[scan - 1].type == Token::WS) {
        trailing_ws.push_back(std::move(result[scan - 1]));
        --scan;
    }

    if (scan > 0 && result[scan - 1].type == type) {
        Token merged = std::move(result[scan - 1]);
        result.resize(scan - 1);
        merged.text.pop_back();
        merged.text += str.substr(1);
        result.push_back(std::move(merged));
        for (auto it = trailing_ws.rbegin(); it != trailing_ws.rend(); ++it) {
            result.push_back(std::move(*it));
        }
        return;
    }

    for (auto it = trailing_ws.rbegin(); it != trailing_ws.rend(); ++it) {
        result.push_back(std::move(*it));
    }
    result.push_back(Token::create(type, std::move(str)));
}

} // namespace jiepp::detail
