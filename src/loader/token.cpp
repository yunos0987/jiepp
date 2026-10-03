#include "token.hpp"
#include "directive_parser.hpp"
#include "../util/iec_61131-3.hpp"

#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Token factories
// ---------------------------------------------------------------------------

Token Token::create(int type, std::string text, int num_of_lines) {
    Token t;
    t.type = type;
    t.text = std::move(text);
    t.num_of_lines = num_of_lines;
    return t;
}

Token Token::newline(int num_of_lines) {
    Token t;
    t.type = Token::WS;
    t.text = std::string(num_of_lines, '\n');
    t.num_of_lines = num_of_lines;
    return t;
}

Token Token::pragma(std::string_view key,
                    std::optional<std::string_view> value,
                    bool standard,
                    int num_of_lines)
{
    std::string body;
    if (value.has_value()) {
        body = std::string(key) + ":" + std::string(*value);
    } else {
        body = std::string(key);
    }

    std::string text;
    if (standard) {
        text = "{" + body + "}";
    } else {
        text = "(*{" + body + "}*)";
    }

    Token t;
    t.type = Token::PRAGMA;
    t.text = std::move(text);
    t.num_of_lines = num_of_lines;
    return t;
}

Token Token::line_pragma(LineNo lineno,
                         std::optional<std::string_view> filepath,
                         bool standard,
                         int num_of_lines)
{
    if (filepath.has_value() && !filepath->empty()) {
        std::string fp = Util::encode_iec_string(*filepath, '\'');
        std::string value = std::to_string(lineno) + " " + fp;
        return pragma("#", value, standard, num_of_lines);
    } else {
        return pragma("#", std::to_string(lineno), standard, num_of_lines);
    }
}

Token Token::clone() const {
    Token t = *this;  // hs is a shared handle copy (O(1))
    return t;
}

Token& Token::flatten() {
    // Keep the newline that ends a `//` comment (like clang -CC); flattening it
    // to a space would comment out the rest of the line. `//!` (DOCUMENT) is
    // not in MASK_WS and is therefore already kept as is.
    if (type == Token::C && text.starts_with("//"))
        return *this;
    if ((type & Token::MASK_WS || type == Token::ANY) && num_of_lines != 0) {
        std::replace(text.begin(), text.end(), '\n', ' ');
        num_of_lines = 0;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Token list helpers
// ---------------------------------------------------------------------------

std::vector<Token> ts_ltrim(std::vector<Token> ts) {
    auto itr = ts.begin();
    while (itr != ts.end() && (itr->type & Token::MASK_WS))
        ++itr;
    ts.erase(ts.begin(), itr);
    return ts;
}

std::vector<Token> ts_rtrim(std::vector<Token> ts) {
    while (!ts.empty() && (ts.back().type & Token::MASK_WS))
        ts.pop_back();
    return ts;
}

std::vector<Token> ts_trim(std::vector<Token> ts) {
    return ts_ltrim(ts_rtrim(std::move(ts)));
}

std::vector<Token> ts_flatten(std::vector<Token> ts) {
    ts = ts_trim(std::move(ts));
    for (auto& t : ts)
        t.flatten();
    return ts;
}

void ts_mark_output_only(std::vector<Token>& ts) noexcept {
    for (auto& t : ts)
        t.mark_output_only();
}
