#include "lexer_helpers.hpp"

namespace jiepp::detail {

Token read_block_comment(const std::string& text, std::size_t& pos,
                         int& lineno, char opener1, int type) {
    char closer_end = (opener1 == '(') ? ')' : '/';
    std::string body;
    body.reserve(128);
    body += opener1;
    body += '*';
    int lines_in_comment = 0;

    while (pos < text.size()) {
        char c = text[pos];

        if (c == '*' && pos + 1 < text.size() && text[pos + 1] == closer_end) {
            body += '*';
            body += closer_end;
            pos += 2;

            if (pos < text.size() && is_nl_char(text[pos])) {
                int nl = consume_one_nl(text, pos);
                if (nl) {
                    body += '\n';
                    lineno += nl;
                    lines_in_comment += nl;
                }
            }

            return Token::create(type, std::move(body), lines_in_comment);
        }

        if (is_nl_char(c)) {
            int cnt = consume_one_nl(text, pos);
            body += '\n';
            lineno += cnt;
            lines_in_comment += cnt;
            continue;
        }

        body += c;
        ++pos;
    }

    with_fallback_line(lineno, [&] {
        ISSUE(UNCLOSED_COMMENT, body);
    });
    return Token::create(type, std::move(body), lines_in_comment);
}

Token read_line_comment(const std::string& text, std::size_t& pos,
                        int& lineno, int type) {
    std::string body = "//";
    while (pos < text.size() && !is_nl_char(text[pos])) {
        body += text[pos++];
    }
    if (pos < text.size() && is_nl_char(text[pos])) {
        body += '\n';
        consume_one_nl(text, pos);
        ++lineno;
        return Token::create(type, std::move(body), 1);
    }
    return Token::create(type, std::move(body), 0);
}

} // namespace jiepp::detail
