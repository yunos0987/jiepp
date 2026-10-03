#include "lexer_helpers.hpp"
#include "directive_parser.hpp"

#include <cctype>
#include <cstddef>
#include <optional>
#include <regex>

namespace jiepp::detail {

namespace {

bool is_pragma_hex_digit(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

unsigned char pragma_hex_value(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u >= '0' && u <= '9') return static_cast<unsigned char>(u - '0');
    if (u >= 'a' && u <= 'f') return static_cast<unsigned char>(u - 'a' + 10);
    return static_cast<unsigned char>(u - 'A' + 10);
}

// Raw newlines are folded to spaces here, before parse_directive() decodes
// '$' escapes and the handlers re-lex the text. To end a '//' comment at a
// raw newline like C does, read_pragma_body() tracks -- over the *decoded*
// character stream, with DirectiveLexState (directive_parser.hpp), the same
// state decode_directive_text() uses -- whether the scan is inside a '//'
// comment.

// Whether the string literal opened by the raw quote text[pos] in a
// directive body closes before the directive does -- what
// string_literal_end() (directive_parser.hpp) will find in the operand,
// where raw newlines are spaces and a "$"+newline is gone. Decides whether
// the literal is a raw string (see DirectiveLexState).
bool directive_string_closes(const std::string& text, std::size_t pos) {
    const char quote = text[pos];
    for (std::size_t i = pos + 1; i < text.size(); ++i) {
        if (text[i] == '$') {
            ++i;                       // an escaped character, even '}'
            continue;
        }
        if (text[i] == '}')
            return false;
        if (text[i] == quote)
            return true;
    }
    return false;
}

} // namespace

Token read_pragma_body(const std::string& text,
                       std::size_t& pos,
                       int& lineno,
                       char opener_sign,
                       std::vector<Token>& extra_nl)
{
    const std::size_t start_pos = pos;
    size_t len = text.size();

    // Look ahead across any run of whitespace/newlines. If it is immediately
    // followed by '#', this is a directive-shaped body with intervening
    // whitespace/newlines: treat it as an ordinary pragma (PP28, raised later
    // at expansion) and rewind so the run is preserved verbatim in the body.
    // Otherwise fall back to the legacy behaviour of only skipping plain
    // whitespace (not newlines) before deciding.
    std::size_t look = pos;
    while (look < len && (is_ws_char(text[look]) || is_nl_char(text[look])))
        ++look;

    bool is_directive;
    if (look > pos && look < len && text[look] == '#') {
        is_directive = false; // ordinary pragma; pos stays at start_pos
    } else {
        while (pos < len && is_ws_char(text[pos]))
            ++pos;
        is_directive = (pos < len) && (text[pos] == '#');
    }

    std::string body;
    bool closed = false;

    // Closer validation: '(' expects *), '/' expects */, '{' expects bare }
    const bool needs_star = (opener_sign == '(' || opener_sign == '/');
    const char expected_end = (opener_sign == '(') ? ')' : '/';

    // Directive-body-only '//' comment tracking (D6): lc scans the decoded
    // character stream so a raw newline inside a '//' comment can truncate
    // the comment out of body before it is folded to whitespace below,
    // ending it at the newline the way C ends a '//' comment at end of
    // line. pending_hex/pending_hex_off hold the first hex digit of a
    // "$XX" escape (mirrors decode_directive_text()'s own lookahead for a
    // second hex digit) between the two ordinary-char iterations that see
    // its two digits, so the decoded byte can be fed to lc once both are
    // known -- and flush_pending_hex() feeds the raw "$X" to lc instead
    // when the second digit never comes, since decode_directive_text() then
    // keeps "$X" literally rather than decoding it. Neither is a second
    // copy of body's content: they never suppress or reorder the
    // unconditional raw appends below, only decide what (if anything) lc
    // sees.
    DirectiveLexState lc;
    int pending_hex = -1;
    std::size_t pending_hex_off = 0;

    // A "$X" that turns out not to be a two-digit hex escape is kept
    // literally by decode_directive_text(), so lc must see '$' and X too.
    auto flush_pending_hex = [&] {
        if (pending_hex >= 0) {
            lc.feed('$', true, pending_hex_off);
            lc.feed(static_cast<char>(pending_hex), true, pending_hex_off + 1);
            pending_hex = -1;
        }
    };

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
                // '$'+newline (line continuation) still joins with nothing;
                // inside a '//' comment it extends the comment to the next
                // raw newline (clang: '\'+newline continues a '//'
                // comment), so lc sees nothing here. A pending "$X" (the
                // first digit of a "$XX" hex escape) is left as is, not
                // flushed: the continuation adds nothing to body, so "$X"
                // still pairs with whatever character follows it, exactly
                // as if the continuation were not there.
                continue;
            case '\r':
                pos += 2;
                if ((pos < len) && (text[pos] == '\n'))
                    ++pos;
                ++lineno;
                extra_nl.push_back(Token::newline(1));
                // Same as the '\n' case above: a pending "$X" survives.
                continue;
            default:
                if (is_directive) {
                    std::size_t off = body.size();
                    flush_pending_hex();                 // "$X$..": "$X" was invalid
                    if (lc.in_raw_string()) {
                        // Inside a string written with raw quotes,
                        // decode_directive_text() decodes only "${" "$}"
                        // "$:" "$ " and keeps any other "$X" (an IEC
                        // string escape) as written, so lc sees both
                        // characters, as tokenize() will.
                        if (auto decoded = decode_directive_only_escape(nc)) {
                            lc.feed(*decoded, false, off);
                        } else {
                            lc.feed('$', true, off);
                            lc.feed(nc, true, off + 1);
                        }
                    } else if (is_pragma_hex_digit(nc)) {
                        pending_hex = static_cast<unsigned char>(nc);
                        pending_hex_off = off;
                    } else if (auto decoded = decode_directive_escape(nc)) {
                        lc.feed(*decoded, false, off);
                    } else {
                        // An invalid escape is kept literally by
                        // decode_directive_text(), so lc sees both chars.
                        lc.feed('$', true, off);
                        lc.feed(nc, true, off + 1);
                    }
                }
                body += c;
                body += nc;
                pos += 2;
                continue;
            }
        }

        // Actual newline
        if (is_nl_char(c)) {
            if (is_directive) {
                flush_pending_hex();
                if (lc.in_line_comment()) {
                    // Like C, a '//' comment ends at the raw newline; drop it
                    // so it cannot swallow the rest of the directive once the
                    // newline has been folded into whitespace below.
                    // A "//!" document comment is kept, as the block comment
                    // the single-line form is rewritten to (see
                    // line_comments_to_block_comments() in
                    // core/directive_handlers.cpp), with "*/" inside the text
                    // written "*|" so it stays one comment.
                    const std::size_t cs = lc.line_comment_start();
                    if (body.compare(cs, 3, "//!") == 0) {
                        std::string doc = body.substr(cs + 2);
                        for (std::size_t q = doc.find("*/"); q != std::string::npos;
                             q = doc.find("*/", q + 2))
                            doc[q + 1] = '|';
                        body.resize(cs);
                        body += "/*" + doc + "*/";
                    } else {
                        body.resize(cs);
                    }
                    lc.end_line_comment();
                }
            }
            if (body.empty() || !is_ws_char(body.back())) {
                body += ' ';
                if (is_directive)
                    lc.feed(' ', true, body.size() - 1); // folded ws must reset "/" lookbehind
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

        if (is_directive) {
            if (pending_hex >= 0 && is_pragma_hex_digit(c)) {
                char decoded = static_cast<char>(
                    (pragma_hex_value(static_cast<char>(pending_hex)) << 4) | pragma_hex_value(c));
                lc.feed(decoded, false, pending_hex_off);
                pending_hex = -1;
            } else {
                flush_pending_hex();
                lc.feed(c, !lc.opens_string(c) || directive_string_closes(text, pos), body.size());
            }
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
        // B1: classifying/diagnosing this directive's key (PP45/PP46 for an
        // unrecognized name) now happens at dispatch time in
        // dispatch_directive() (expand.cpp), not here at lex time, so
        // constructing a DirectiveToken merely to call ready() for its
        // former diagnostic side effect is no longer needed.
        Token t;
        t.type = Token::DIRECTIVE;
        t.text = std::move(full_text);
        t.num_of_lines = 0;
        return t;
    }

    return Token::create(Token::PRAGMA, std::move(full_text));
}

} // namespace jiepp::detail
