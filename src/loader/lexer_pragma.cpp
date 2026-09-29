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
// raw newline like C does, track -- over the *decoded* character stream --
// whether the scan is inside a '//' comment, using the same string/comment
// rules as tokenize() (lexer.cpp). A '{' inside a directive body never
// opens a nested pragma here (a raw '{' always raises PP22 below and stays
// a literal character; only the next '}' closes the directive), so unlike
// tokenize() this has no separate pragma-opener sub-state.
class LineCommentTracker {
public:
    // c: decoded char; off: body offset where its raw spelling starts.
    void feed(char c, std::size_t off) {
        switch (state_) {
        case State::Normal:
            if (prev_ == '/' && c == '/') { state_ = State::Line; start_ = prev_off_; prev_ = 0; return; }
            if ((prev_ == '/' || prev_ == '(') && c == '*') {
                closer_ = (prev_ == '(') ? ')' : '/'; state_ = State::Block; star_ = false; prev_ = 0; return;
            }
            if (c == '\'' || c == '"') { quote_ = c; esc_ = false; state_ = State::Str; prev_ = 0; return; }
            prev_ = c; prev_off_ = off; return;
        case State::Str:                       // mirrors tokenize()'s string loop
            if (esc_) { esc_ = false; return; } // '$'-escaped char, incl. '$'+newline
            if (c == '$') { esc_ = true; return; }
            if (c == quote_ || is_nl_char(c)) state_ = State::Normal;
            return;
        case State::Line:                      // a decoded $n/$r/$0A/$0D ends it
            if (is_nl_char(c)) { state_ = State::Normal; prev_ = 0; }
            return;
        case State::Block:
            if (star_ && c == closer_) { state_ = State::Normal; prev_ = 0; return; }
            star_ = (c == '*');
            return;
        }
    }
    bool in_line_comment() const { return state_ == State::Line; }
    std::size_t line_comment_start() const { return start_; }
    void end_line_comment() { state_ = State::Normal; prev_ = 0; }
private:
    enum class State { Normal, Str, Line, Block };
    State state_ = State::Normal;
    char prev_ = 0, quote_ = 0, closer_ = 0;
    bool esc_ = false, star_ = false;
    std::size_t prev_off_ = 0, start_ = 0;
};

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
    LineCommentTracker lc;
    int pending_hex = -1;
    std::size_t pending_hex_off = 0;

    // A "$X" that turns out not to be a two-digit hex escape is kept
    // literally by decode_directive_text(), so lc must see '$' and X too.
    auto flush_pending_hex = [&] {
        if (pending_hex >= 0) {
            lc.feed('$', pending_hex_off);
            lc.feed(static_cast<char>(pending_hex), pending_hex_off + 1);
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
                    if (is_pragma_hex_digit(nc)) {
                        pending_hex = static_cast<unsigned char>(nc);
                        pending_hex_off = off;
                    } else if (auto decoded = decode_directive_escape(nc)) {
                        lc.feed(*decoded, off);
                    } else {
                        // An invalid escape is kept literally by
                        // decode_directive_text(), so lc sees both chars.
                        lc.feed('$', off);
                        lc.feed(nc, off + 1);
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
                    body.resize(lc.line_comment_start());
                    lc.end_line_comment();
                }
            }
            if (body.empty() || !is_ws_char(body.back())) {
                body += ' ';
                if (is_directive)
                    lc.feed(' ', body.size() - 1); // folded ws must reset "/" lookbehind
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
                lc.feed(decoded, pending_hex_off);
                pending_hex = -1;
            } else {
                flush_pending_hex();
                lc.feed(c, body.size());
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
