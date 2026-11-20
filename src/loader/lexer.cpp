#include "lexer.hpp"
#include "lexer_helpers.hpp"

#include <algorithm>
#include <optional>
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
                      std::size_t n, LineNo& lineno, std::vector<Token>& result,
                      LexOptions opts) {
    if (pos + 2 < n && text[pos + 2] == '!') {
        // Doc comment: (*! or /*!
        pos += 3;
        result.push_back(read_block_comment(text, pos, lineno, opener, Token::DOCUMENT, opts));
    } else {
        std::size_t look = pos + 2;
        while (look < n && is_ws_char(text[look])) ++look;
        if (look < n && text[look] == '{') {
            // Pragma: (*{ or /*{
            pos = look + 1;
            std::vector<Token> extra_nl;
            auto tok = read_pragma_body(text, pos, lineno, opener, extra_nl, opts);
            result.push_back(std::move(tok));
            for (const auto& et : extra_nl)
                result.push_back(et);
        } else {
            // Block comment: (* or /*
            pos += 2;
            result.push_back(read_block_comment(text, pos, lineno, opener, Token::C, opts));
        }
    }
}

// Lexes text from offset `start` to its end.
std::vector<Token> tokenize(const std::string& text, std::size_t start, LineNo lineno,
                            LexOptions opts) {
    const std::size_t n = text.size();
    std::size_t p = start;

    std::vector<Token> r;
    r.reserve((n - p) / 8);

    StringMergeState merge_state;

    // A re-lexed text with a line map (a multi-line directive operand or
    // pragma body): every token lexed on a line below the text's first line
    // keeps that line (Token::src_line), so a __LINE__ there -- written, or
    // made by a macro whose name or ')' is there -- is that line. Applied
    // to the tokens each step pushes, when the step ends (also through its
    // `continue`).
    const LineMap* const src_lines = opts.at_current_location ? opts.line_map : nullptr;
    struct MarkSrcLines {
        const LineMap* map;
        std::vector<Token>& r;
        std::size_t first;
        std::size_t pos;
        ~MarkSrcLines() {
            if (!map)
                return;
            const std::size_t below = map->line_of(pos);
            if (below == 0)
                return;
            const auto line = static_cast<std::uint32_t>(
                wrap_lineno(Issue::lineno() + static_cast<LineNo>(below)));
            for (std::size_t i = first; i < r.size(); ++i) {
                r[i].has_src_line = true;
                r[i].src_line = line;
            }
        }
    };

    while (p < n) {
        MarkSrcLines mark{src_lines, r, r.size(), p};
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
            lex_block_opener(c, text, p, n, lineno, r, opts);
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
                auto tok = read_pragma_body(text, p, lineno, '{', extra_nl, opts);
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
            auto tok = read_pragma_body(text, p, lineno, '{', extra_nl, opts);
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
            int dollar_nl = 0;
            while (p < n && text[p] != quote && !is_nl_char(text[p])) {
                if (text[p] == '$' && p + 1 < n && is_nl_char(text[p + 1])) {
                    // $-escaped newline (line continuation): consume the full
                    // newline sequence (\r\n, \r or \n) and count it so the
                    // running line counter and the token's num_of_lines stay
                    // in sync with the physical lines this literal spans.
                    ++p; // skip '$'
                    dollar_nl += consume_one_nl(text, p);
                    continue;
                }
                if (text[p] == '$') ++p;
                if (p < n) ++p;
            }
            bool closed = false;
            if (p < n && text[p] == quote) {
                ++p;
                closed = true;
            }
            lineno += dollar_nl;
            // No closing quote (ended by a newline or the end of the input):
            // never merged with the next literal; core/ reports PP29, at the
            // line of the quote when the re-lexed text has a line map.
            LineMapRef line_map;
            if (!closed && opts.at_current_location && opts.line_map) {
                if (const std::size_t below = opts.line_map->line_of(start))
                    line_map = LineMapRef(LineMap{below, {}});
            }
            push_string_token(r, merge_state, tok_type, text.substr(start, p - start),
                              dollar_nl, !closed, std::move(line_map));
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

namespace {

// Shared by iec3_tokens_from_string() and iec3_relex(), so the two cannot
// drift apart (BOM stripping, comment removal). strip_bom: source text strips
// a leading UTF-8 BOM; re-lexed text (iec3_relex()) keeps it, since its bytes
// are data of the directive/pragma/macro text, not a file's encoding mark.
std::vector<Token> lex_text(const std::string& input, bool remove_comments, LineNo lineno,
                            LexOptions opts, bool strip_bom) {
    // A leading UTF-8 BOM is skipped, not erased: the text is lexed in place
    // from the byte after it. Only a re-lex uses offsets into the text
    // (LexOptions::line_map), and it never skips a BOM.
    const bool has_bom = strip_bom && (input.size() >= 3) &&
        (static_cast<unsigned char>(input[0]) == 0xEF) &&
        (static_cast<unsigned char>(input[1]) == 0xBB) &&
        (static_cast<unsigned char>(input[2]) == 0xBF);
    auto tokens = tokenize(input, has_bom ? 3 : 0, lineno, opts);
    if (remove_comments)
        iec3_remove_comments(tokens);
    return tokens;
}

} // anonymous namespace

void iec3_remove_comments(std::vector<Token>& tokens) {
    for (auto& t : tokens) {
        if (t.type == Token::C) // Token::DOCUMENT (/*!  //!  (*!) is preserved
            t = (t.num_of_lines > 0) ? Token::newline(t.num_of_lines) : Token::create(Token::WS, " ");
    }
}

std::vector<Token> iec3_tokens_from_string(const std::string& input, bool remove_comments, LineNo lineno,
                                           bool report_nested_brace) {
    return lex_text(input, remove_comments, lineno,
                    LexOptions{.report_nested_brace = report_nested_brace},
                    /*strip_bom=*/true);
}

std::vector<Token> iec3_tokens_from_source(const std::string& input, bool remove_comments) {
    auto tokens = iec3_tokens_from_string(input, remove_comments, 1);
    const std::size_t bom = input.starts_with("\xEF\xBB\xBF") ? 3 : 0;
    if (input.size() > bom && !is_nl_char(input.back()))
        tokens.push_back(Token::newline(1));
    return tokens;
}

std::vector<Token> iec3_relex(const std::string& text, bool remove_comments,
                              bool report_nested_brace, bool report_unclosed_comment,
                              bool* found_unclosed_comment, const LineMap* line_map) {
    // The line counter only feeds diagnostics, which at_current_location
    // reports at the current Issue location instead; 1 is never shown.
    return lex_text(text, remove_comments, 1,
                    LexOptions{.report_nested_brace = report_nested_brace,
                               .at_current_location = true,
                               .report_unclosed_comment = report_unclosed_comment,
                               .found_unclosed_comment = found_unclosed_comment,
                               .line_map = line_map},
                    /*strip_bom=*/false);
}

std::string iec3_blank_out_comments(std::string_view s) {
    std::string r;
    r.reserve(s.size());
    const std::size_t n = s.size();
    std::size_t p = 0;
    while (p < n) {
        const char c = s[p];
        const char d = (p + 1 < n) ? s[p + 1] : '\0';
        if (c == '\'' || c == '"') {
            std::size_t q = p + 1;
            while (q < n && s[q] != c && !is_nl_char(s[q])) {
                if (s[q] == '$' && q + 1 < n && is_nl_char(s[q + 1])) {
                    ++q;
                    q += (s[q] == '\r' && q + 1 < n && s[q + 1] == '\n') ? 2 : 1;
                    continue;
                }
                if (s[q] == '$') ++q;
                if (q < n) ++q;
            }
            if (q < n && s[q] == c) ++q;
            r.append(s.substr(p, q - p));
            p = q;
            continue;
        }
        const bool block = (c == '(' || c == '/') && d == '*';
        const bool line = (c == '/' && d == '/');
        if (block || line) {
            std::size_t look = p + 2;
            while (look < n && is_ws_char(s[look])) ++look;
            const bool pragma = look < n && s[look] == '{';
            const bool doc = p + 2 < n && s[p + 2] == '!';
            std::size_t end = p + 2;
            if (line) {
                while (end < n && !is_nl_char(s[end])) ++end;
            } else {
                const char closer = (c == '(') ? ')' : '/';
                while (end + 1 < n && !(s[end] == '*' && s[end + 1] == closer)) ++end;
                end = (end + 1 < n) ? end + 2 : n;
            }
            if (pragma) { r.append(s.substr(p, 2)); p += 2; continue; }
            if (doc) { r.append(s.substr(p, end - p)); p = end; continue; }
            r += ' ';
            p = end;
            continue;
        }
        r += c;
        ++p;
    }
    return r;
}

namespace {

// The IEC 61131-3 operators of two characters. tokenize() reads each of
// them as two single-character tokens, but an IEC compiler reads the output
// and takes them as one token.
bool is_iec_operator(char a, char b) {
    switch (a) {
    case ':': return b == '=';
    case '=': return b == '>';
    case '<': return b == '=' || b == '>';
    case '>': return b == '=';
    case '*': return b == '*';
    case '.': return b == '.';
    default: return false;
    }
}

// The tokens of `text` for the token-boundary rules: tokenize() without any
// diagnostic, then each IEC operator above, written as two adjacent
// single-character tokens, joined into one token (left to right, so "<=>"
// is "<=" ">" and "..." is ".." ".").
std::vector<Token> lex_for_boundary(const std::string& text) {
    auto ts = tokenize(text, 0, 1,
                       LexOptions{.report_nested_brace = false,
                                  .report_unclosed_comment = false,
                                  .quiet = true});
    std::vector<Token> r;
    r.reserve(ts.size());
    for (auto& t : ts) {
        if (!r.empty() && t.type == Token::ANY && t.text.size() == 1 &&
            r.back().type == Token::ANY && r.back().text.size() == 1 &&
            is_iec_operator(r.back().text[0], t.text[0])) {
            r.back().text += t.text;
            continue;
        }
        r.push_back(std::move(t));
    }
    return r;
}

bool is_all_ident_cont(std::string_view s) {
    return std::all_of(s.begin(), s.end(), is_ident_cont);
}

} // anonymous namespace

bool iec3_may_join(std::string_view before_left, std::string_view left, char right,
                   char right2) {
    if (left.empty())
        return false;
    const char l = left.back();
    const char r = right;
    // The '#' of an IEC typed literal (16#FF, T#1s, INT#5) is where the
    // lexer splits such a literal into several tokens: no space is put next
    // to it (iec3_needs_space()).
    if (l == '#' || r == '#')
        return false;
    const auto is_digit = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; };
    if (left.front() == '%')
        return is_ident_cont(r) || r == '.' || r == '*'; // %IX0.1, %I*
    if (is_ident_cont(l)) {
        if (is_ident_cont(r))
            return true; // one identifier or number
        // Only a number goes on with '.' and a digit ("1" ".5") or, after
        // its exponent letter, with a sign ("1e" "+5"); an identifier never
        // does.
        if (!is_digit(left.front()))
            return false;
        return (r == '.' && is_digit(right2)) ||
               ((r == '+' || r == '-') && (l == 'e' || l == 'E'));
    }
    switch (l) {
    case '(': return r == '*';             // (*
    case '/': return r == '/' || r == '*'; // // /*
    case '@': return r == '@';             // @@
    case '.':
        // A number continued ("1" "." "5", "%IX0" "." "1"), or `..`.
        if (r == '.')
            return true;
        // A number goes on with a digit after its '.', a direct
        // representation with any letter or digit.
        return !before_left.empty() &&
               ((is_digit(before_left.front()) && is_digit(r)) ||
                (before_left.front() == '%' && is_ident_cont(r)));
    case '+':
    case '-':
        // An exponent continued: "1e" "+" "5".
        return is_digit(r) && !before_left.empty() && is_digit(before_left.front()) &&
               (before_left.back() == 'e' || before_left.back() == 'E');
    default:
        break;
    }
    return is_iec_operator(l, r);
}

bool iec3_needs_space(std::string_view before, std::string_view after) {
    if (before.empty() || after.empty() || before.back() == '#' || after.front() == '#')
        return false;
    const auto b = lex_for_boundary(std::string(before));
    const auto a = lex_for_boundary(std::string(after));
    // Whether ts is b's tokens, then (with_space) one whitespace token, then
    // a's tokens.
    auto is_concat = [&](const std::vector<Token>& ts, bool with_space) {
        if (ts.size() != b.size() + a.size() + (with_space ? 1 : 0))
            return false;
        std::size_t k = 0;
        for (const auto& u : b)
            if (ts[k++].text != u.text)
                return false;
        if (with_space && ts[k++].type != Token::WS)
            return false;
        for (const auto& u : a)
            if (ts[k++].text != u.text)
                return false;
        return true;
    };
    std::string text;
    text.reserve(before.size() + after.size() + 1);
    text.append(before).append(after);
    if (is_concat(lex_for_boundary(text), false))
        return false;
    text.insert(before.size(), 1, ' ');
    return is_concat(lex_for_boundary(text), true);
}

std::optional<int> iec3_pasted_token_type(std::string_view left, std::string_view right) {
    if (left.empty() || right.empty())
        return std::nullopt;
    // Identifiers and numbers: one identifier, or one number ("1" "x" is
    // the number token "1x").
    if (is_all_ident_cont(left) && is_all_ident_cont(right))
        return Token::ANY;
    // The '#' of an IEC typed literal (16#FF, T#1s, INT#5, COLOR#RED): the
    // lexer reads the prefix, the '#' and the value as separate tokens
    // (except a based number such as 16#FF), so a paste on either side of
    // the '#' is one literal written together.
    if ((left.back() == '#' && is_ident_cont(right.front())) ||
        (right.front() == '#' && is_ident_cont(left.back())))
        return Token::ANY;
    std::string text;
    text.reserve(left.size() + right.size());
    text.append(left).append(right);
    const auto ts = lex_for_boundary(text);
    if (ts.size() != 1 || ts[0].text != text || ts[0].unterminated)
        return std::nullopt;
    switch (ts[0].type) {
    case Token::C:
    case Token::DOCUMENT:
    case Token::PRAGMA:
    case Token::DIRECTIVE:
        return std::nullopt;
    default:
        return ts[0].type;
    }
}

bool iec3_is_identifier(std::string_view s) {
    if (s.empty() || !is_ident_start(s.front()))
        return false;
    for (char c : s.substr(1)) {
        if (!is_ident_cont(c))
            return false;
    }
    return true;
}
