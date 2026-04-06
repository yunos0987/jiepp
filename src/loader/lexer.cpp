#include "lexer.hpp"
#include "lexer_helpers.hpp"

#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace jiepp::detail;

// ---------------------------------------------------------------------------
// Main tokenizer
// ---------------------------------------------------------------------------

namespace {

// Unified handler for (* ... *) and /* ... */ families.
// opener is '(' or '/'; pos points to the opener char.
void lex_block_opener(char opener, const std::string& text, std::size_t& pos,
                      std::size_t n, int& lineno, std::vector<Token>& result) {
    if (pos + 2 < n && text[pos + 2] == '!') {
        // Doc comment: (*! or /*!
        pos += 3;
        auto tok = read_block_comment(text, pos, lineno, opener, Token::DOCUMENT);
        tok.text.insert(2, 1, '!');
        result.push_back(std::move(tok));
    } else {
        std::size_t look = pos + 2;
        while (look < n && is_ws_char(text[look])) ++look;
        if (look < n && text[look] == '{') {
            // Pragma: (*{ or /*{
            pos = look + 1;
            std::vector<Token> extra_nl;
            auto tok = read_pragma_body(text, pos, lineno, opener, extra_nl);
            result.push_back(std::move(tok));
            for (const auto& et : extra_nl)
                result.push_back(et);
        } else {
            // Block comment: (* or /*
            pos += 2;
            result.push_back(read_block_comment(text, pos, lineno, opener, Token::C));
        }
    }
}

std::vector<Token> tokenize(const std::string& text, int lineno) {
    std::vector<Token> r;
    r.reserve(text.size() / 8);

    const std::size_t n = text.size();
    std::size_t p = 0;

    while (p < n) {
        char c = text[p];

        // ---- Whitespace (non-newline) ----
        if (is_ws_char(c)) {
            std::size_t start = p;
            while (p < n && is_ws_char(text[p]))
                ++p;
            r.push_back(Token::create(Token::WS, text.substr(start, p - start)));
            continue;
        }

        // ---- Newlines ----
        if (is_nl_char(c)) {
            int count = 0;
            while (p < n && is_nl_char(text[p])) {
                consume_one_nl(text, p);
                ++count;
            }
            lineno += count;
            r.push_back(Token::newline(count));
            continue;
        }

        // ---- (* ... *) and /* ... */ families (unified) ----
        if ((c == '(' || c == '/') && p + 1 < n && text[p + 1] == '*') {
            lex_block_opener(c, text, p, n, lineno, r);
            continue;
        }

        // ---- Line comments: // ----
        if (c == '/' && p + 1 < n && text[p + 1] == '/') {
            std::size_t look = p + 2;
            while (look < n && is_ws_char(text[look]))
                ++look;

            if (look < n && text[look] == '{') {
                p = look + 1;
                std::vector<Token> extra_nl;
                auto tok = read_pragma_body(text, p, lineno, '{', extra_nl);
                while (p < n && is_ws_char(text[p]))
                    ++p;
                if (p < n && is_nl_char(text[p])) {
                    consume_one_nl(text, p);
                    ++lineno;
                    r.push_back(std::move(tok));
                    r.push_back(Token::newline(1));
                    for (const auto& et : extra_nl)
                        r.push_back(et);
                } else {
                    r.push_back(std::move(tok));
                    for (const auto& et : extra_nl)
                        r.push_back(et);
                }
            } else if (p + 2 < n && text[p + 2] == '!') {
                p += 2;
                r.push_back(read_line_comment(text, p, lineno, Token::DOCUMENT));
            } else {
                p += 2;
                r.push_back(read_line_comment(text, p, lineno, Token::C));
            }
            continue;
        }

        // ---- Plain pragma/directive { ----
        if (c == '{') {
            ++p;
            std::vector<Token> extra_nl;
            auto tok = read_pragma_body(text, p, lineno, '{', extra_nl);
            r.push_back(std::move(tok));
            for (const auto& et : extra_nl)
                r.push_back(et);
            continue;
        }

        // ---- String literals ----
        if (c == '\'' || c == '"') {
            int tok_type = (c == '\'') ? Token::STRING : Token::WSTRING;
            char quote = c;
            std::size_t start = p++;
            while (p < n && text[p] != quote && !is_nl_char(text[p])) {
                if (text[p] == '$') ++p;
                if (p < n) ++p;
            }
            if (p < n && text[p] == quote)
                ++p;
            push_string_token(r, tok_type, text.substr(start, p - start));
            continue;
        }

        // ---- @@ and @ ----
        if (c == '@') {
            if (p + 1 < n && text[p + 1] == '@') {
                r.push_back(Token::create(Token::GLUE, "@@"));
                p += 2;
            } else {
                r.push_back(Token::create(Token::STRINGIZE, "@"));
                ++p;
            }
            continue;
        }

        // ---- Typed single-char tokens ----
        switch (c) {
        case '(': r.push_back(Token::create(Token::LP, "(")); ++p; continue;
        case ')': r.push_back(Token::create(Token::RP, ")")); ++p; continue;
        case '[': r.push_back(Token::create(Token::LB, "[")); ++p; continue;
        case ']': r.push_back(Token::create(Token::RB, "]")); ++p; continue;
        case ',': r.push_back(Token::create(Token::SEP, ",")); ++p; continue;
        }

        // ---- Numbers ----
        if (std::isdigit(static_cast<unsigned char>(c))) {
            if (try_push_number(r, text, p)) continue;
            r.push_back(Token::create(Token::ANY, std::string(1, text[p++])));
            continue;
        }

        // ---- Identifiers (and typed date/time forms) ----
        if (is_ident_start(c)) {
            if (try_push_date_or_time(r, text, p)) continue;
            std::size_t start = p;
            while (p < n && is_ident_cont(text[p])) ++p;
            r.push_back(Token::create(Token::ANY, text.substr(start, p - start)));
            continue;
        }

        // ---- % direct representation ----
        if (c == '%') {
            std::size_t start = p++;
            while (p < n && (std::isalnum(static_cast<unsigned char>(text[p]))
                                || text[p] == '.' || text[p] == '_' || text[p] == '*'))
                ++p;
            r.push_back(Token::create(Token::ANY, text.substr(start, p - start)));
            continue;
        }

        // ---- Other single chars ----
        r.push_back(Token::create(Token::ANY, std::string(1, text[p++])));
    }

    return r;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<Token> iec3_tokens(std::istream& input, bool remove_comments, int lineno) {
    // kludge
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return iec3_tokens_from_string(text, remove_comments, lineno);
}

std::vector<Token> iec3_tokens_from_string(const std::string& input, bool remove_comments, int lineno) {
    std::string text = input;
    // Strip UTF-8 BOM if present
    if ((text.size() >= 3) &&
        (static_cast<unsigned char>(text[0]) == 0xEF) &&
        (static_cast<unsigned char>(text[1]) == 0xBB) &&
        (static_cast<unsigned char>(text[2]) == 0xBF))
        text.erase(0, 3);
    auto tokens = tokenize(text, lineno);
    if (remove_comments) {
        for (auto& t : tokens) {
            if (t.type == Token::C) { // Token::DOCUMENT (/*!  //!  (*!) is preserved
                if (t.num_of_lines > 0) {
                    t = Token::newline(t.num_of_lines);
                } else {
                    t = Token::create(Token::WS, " ");
                }
            }
        }
    }
    return tokens;
}
