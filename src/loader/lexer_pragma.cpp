#include "lexer_helpers.hpp"

#include <regex>

namespace jiepp::detail {

Token read_pragma_body(const std::string& text,
                       std::size_t& pos,
                       int& lineno,
                       char opener_sign,
                       std::vector<Token>& extra_nl)
{
    const std::size_t start_pos = pos;
    size_t len = text.size();
    while (pos < len && is_ws_char(text[pos]))
        ++pos;

    bool is_directive = (pos < len) && (text[pos] == '#');

    std::string body;
    bool closed = false;

    // Closer validation: '(' expects *), '/' expects */, '{' expects bare }
    const bool needs_star = (opener_sign == '(' || opener_sign == '/');
    const char expected_end = (opener_sign == '(') ? ')' : '/';

    while (pos < len) {
        char c = text[pos];

        // Dollar-escape
        if ((c == '$') && (pos + 1 < len)) {
            char nc = text[pos + 1];
            switch (nc) {
            case '\n':
                pos += 2;
                ++lineno;
                extra_nl.push_back(Token::newline(1));
                continue;
            case '\r':
                pos += 2;
                if ((pos < len) && (text[pos] == '\n'))
                    ++pos;
                ++lineno;
                extra_nl.push_back(Token::newline(1));
                continue;
            default:
                body += c;
                body += nc;
                pos += 2;
                continue;
            }
        }

        // Actual newline
        if (is_nl_char(c)) {
            if (!is_directive && (body.empty() || !is_ws_char(body.back()))) {
                body += ' ';
            }
            consume_one_nl(text, pos);
            ++lineno;
            extra_nl.push_back(Token::newline(1));
            continue;
        }

        if (c == '{') {
            Issue::with_lineno(lineno, [&] {
                ISSUE(INVALID_PRAGMA_SYNTAX, std::string(1, c));
            });
        }

        // Closing '}'
        if (c == '}') {
            std::size_t cl = pos + 1;
            while (cl < len && is_ws_char(text[cl]))
                ++cl;
            bool has_star = (cl + 1 < len) && (text[cl] == '*') && (text[cl + 1] == ')' || text[cl + 1] == '/');

            if (needs_star) {
                if (has_star && text[cl + 1] == expected_end) {
                    pos = cl + 2;
                } else {
                    Issue::with_lineno(lineno, [&] {
                        ISSUE(INVALID_PRAGMA_SYNTAX, std::string(1, c));
                    });
                    pos = has_star ? cl + 2 : pos + 1;
                }
            } else {
                if (has_star) {
                    Issue::with_lineno(lineno, [&] {
                        ISSUE(INVALID_PRAGMA_SYNTAX, std::string(1, c));
                    });
                    pos = cl + 2;
                } else {
                    ++pos;
                }
            }
            closed = true;
            break;
        }

        body += c;
        ++pos;
    }

    if (!closed) {
        if (is_directive) {
            Issue::with_lineno(lineno, [&] {
                std::string s = text.substr(start_pos - 1, pos - start_pos + 1);
                ISSUE(INVALID_PP_SYNTAX, std::regex_replace(s, std::regex("\n"), "$n"));
            });
        } else {
            Issue::with_lineno(lineno, [&] {
                std::string s = text.substr(start_pos - 1, pos - start_pos + 1);
                ISSUE(INVALID_PRAGMA_SYNTAX, std::regex_replace(s, std::regex("\n"), "$n"));
            });
        }
    }

    std::string full_text = "{" + body + "}";

    if (is_directive) {
        DirectiveToken dt = DirectiveToken::create_empty(full_text);
        dt.ready();
        Token t;
        t.type = Token::DIRECTIVE;
        t.text = std::move(full_text);
        t.num_of_lines = 0;
        return t;
    }

    return Token::create(Token::PRAGMA, std::move(full_text));
}

} // namespace jiepp::detail
