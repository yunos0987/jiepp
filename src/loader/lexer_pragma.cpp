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

// True for a character that read_pragma_body() only appends to the body and
// feeds to the directive lexical state as is: not the start of an escape, a
// newline, a brace, or a quote (whether a quote opens a string depends on the
// text after it).
bool is_plain_pragma_char(char c) {
    switch (c) {
    case '$': case '\n': case '\r': case '{': case '}': case '\'': case '"':
        return false;
    default:
        return true;
    }
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
                       LineNo& lineno,
                       char opener_sign,
                       std::vector<Token>& extra_nl,
                       LexOptions opts)
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

    // The body's source lines (Token::line_map, task d1), as offsets into
    // body. Source lexing records each raw newline and "$"+newline where it
    // is folded or dropped. A re-lex never does (a newline in re-lexed text
    // is a decoded escape, not a source line); with a line map of the text
    // (opts.line_map) it carries over the breaks that fall inside this
    // body instead, one cursor step per break.
    const LineMap* outer = opts.at_current_location ? opts.line_map : nullptr;
    std::vector<std::size_t> breaks;
    std::size_t lines_before = 0;
    std::size_t next_break = 0;
    if (outer) {
        lines_before = outer->line_of(start_pos);
        next_break = static_cast<std::size_t>(
            std::upper_bound(outer->breaks.begin(), outer->breaks.end(), start_pos) -
            outer->breaks.begin());
    }
    // Breaks of the text at or before `upto` start a line at the next body
    // character.
    auto carry_breaks = [&](std::size_t upto) {
        for (; outer && next_break < outer->breaks.size() && outer->breaks[next_break] <= upto;
             ++next_break)
            breaks.push_back(body.size());
    };

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
        carry_breaks(pos);
        // Copy a run of characters that the rules below only append, in one
        // step; lc still sees each of them.
        if (pending_hex < 0) {
            std::size_t run = pos;
            while (run < len && is_plain_pragma_char(text[run]))
                ++run;
            if (run > pos) {
                for (; outer && next_break < outer->breaks.size() &&
                       outer->breaks[next_break] < run;
                     ++next_break)
                    breaks.push_back(body.size() + (outer->breaks[next_break] - pos));
                body.append(text, pos, run - pos);
                if (is_directive)
                    for (std::size_t i = pos; i < run; ++i)
                        lc.feed(text[i], true, body.size() - (run - i));
                pos = run;
                continue;
            }
        }

        char c = text[pos];

        // Dollar-escape
        if ((c == '$') && (pos + 1 < len)) {
            char nc = text[pos + 1];
            switch (nc) {
            case '\n':
                pos += 2;
                ++lineno;
                if (!opts.at_current_location)
                    breaks.push_back(body.size());
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
                if (!opts.at_current_location)
                    breaks.push_back(body.size());
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
                        // Breaks inside the dropped comment (its "$"+newline
                        // continuations) now start a line where it was.
                        for (auto b = breaks.rbegin(); b != breaks.rend() && *b > cs; ++b)
                            *b = cs;
                    }
                    lc.end_line_comment();
                }
            }
            if (!opts.at_current_location)
                breaks.push_back(body.size());
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

        // A raw '{' inside the body (A2 / S-02). opts.report_nested_brace
        // is false only while re-lexing the body of a Token::relexed_pragma
        // token, i.e. a pragma the lexer itself produced, with its text
        // unchanged, while re-lexing the body B of an enclosing pragma.
        // The pass that read B called this function over the same
        // characters with the same "$" pairing: the body of a nested pragma
        // starts right after its opening '{', and that '{' ended a "$"
        // pair or was a single character in that pass too, and the body
        // ends no later than B does. So this same '{' was already met
        // there, and reported with the same code (PP22) unless that pass
        // was itself quiet, in which case the argument repeats outwards up
        // to a pass that reported it: the source lex or the first re-lex
        // of a pragma body that was not produced by a re-lex. Only this
        // one diagnostic is skipped; the closer mismatches, the unclosed
        // PP22 and PP23 below are always reported. Since the {#ignore} set
        // only grows during a run, the earlier report counted whenever this
        // one would have, so the error count stays >= 1 and the exit status
        // cannot change.
        if (c == '{' && opts.report_nested_brace) {
            report_lex_issue(lineno, opts, pos, [&] {
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
                    report_lex_issue(lineno, opts, pos, [&] {
                        ISSUE(INVALID_PRAGMA_SYNTAX, std::string(1, c));
                    });
                    pos = has_star ? cl + 2 : pos + 1;
                }
            } else {
                if (has_star) {
                    report_lex_issue(lineno, opts, pos, [&] {
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
            report_lex_issue(lineno, opts, pos, [&] {
                std::string s = text.substr(start_pos - 1, pos - start_pos + 1);
                ISSUE(INVALID_PP_SYNTAX, std::regex_replace(s, std::regex("\n"), "$n"));
            });
        } else {
            report_lex_issue(lineno, opts, pos, [&] {
                std::string s = text.substr(start_pos - 1, pos - start_pos + 1);
                ISSUE(INVALID_PRAGMA_SYNTAX, std::regex_replace(s, std::regex("\n"), "$n"));
            });
        }
    }

    std::string full_text = "{" + body + "}";
    LineMapRef line_map;
    if (lines_before != 0 || !breaks.empty())
        line_map = LineMapRef(LineMap{lines_before, std::move(breaks)});

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
        t.line_map = std::move(line_map);
        return t;
    }

    Token t = Token::create(Token::PRAGMA, std::move(full_text));
    t.line_map = std::move(line_map);
    return t;
}

} // namespace jiepp::detail
