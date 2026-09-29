#include "line_compaction.hpp"

#include <charconv>
#include <string>
#include <utility>
#include <vector>

namespace jiepp {

namespace {

// True if there is any character after the last '\n' in `text` (i.e. the
// token still carries content on its own line). A token with no embedded
// newline trivially satisfies this (there is nothing to check "after").
bool has_trailing_content(const std::string& text) {
    auto pos = text.find_last_of('\n');
    if (pos == std::string::npos) return true;
    return pos + 1 < text.size();
}

} // namespace

bool is_line_marker(const Token& t) {
    if (t.type != Token::PRAGMA) return false;
    const auto& s = t.text;
    if (s.size() > 5 && s.compare(0, 5, "(*{#:") == 0) return true;  // annotated
    if (s.size() > 3 && s.compare(0, 3, "{#:") == 0) return true;    // standard
    return false;
}

std::optional<LineMarker> parse_line_marker(const Token& t) {
    if (!is_line_marker(t)) return std::nullopt;
    const std::string& s = t.text;

    bool standard;
    std::string_view body;
    if (s.compare(0, 5, "(*{#:") == 0) {
        standard = false;
        // Strip "(*{#:" prefix and "}*)" suffix.
        body = std::string_view(s).substr(5, s.size() - 5 - 3);
    } else {
        standard = true;
        // Strip "{#:" prefix and "}" suffix.
        body = std::string_view(s).substr(3, s.size() - 3 - 1);
    }

    // body is "N" or "N 'path'"
    std::size_t sp = body.find(' ');
    std::string_view num_part = (sp == std::string_view::npos) ? body : body.substr(0, sp);
    std::string_view enc_file = (sp == std::string_view::npos) ? std::string_view{} : body.substr(sp + 1);

    LineNo lineno = 0;
    auto result = std::from_chars(num_part.data(), num_part.data() + num_part.size(), lineno);
    if (result.ec != std::errc()) return std::nullopt;

    return LineMarker{lineno, enc_file, standard};
}

Token make_line_marker(LineNo lineno, std::string_view enc_file, bool standard) {
    std::string body = "#:" + std::to_string(lineno);
    if (!enc_file.empty()) {
        body += " ";
        body += enc_file;
    }
    std::string text = standard ? ("{" + body + "}") : ("(*{" + body + "}*)");
    return Token::create(Token::PRAGMA, std::move(text));
}

bool compact_blank_lines(std::vector<Token>& ots, int max_blank_lines,
                          BlankLineMode mode, bool default_standard_style,
                          LineNo first_lineno) {
    if (ots.empty())
        return false;

    std::vector<Token> out;
    out.reserve(ots.size());

    LineNo      cur             = wrap_lineno(first_lineno); // item a: wrapped after every advance, see wrap_lineno()
    bool        line_has_content = false;
    std::string enc_file;
    bool        standard        = default_standard_style;
    bool        modified        = false;

    std::vector<Token> run;

    // Flushes the buffered run. `next_is_marker_or_eof` tells the flush
    // whether the token immediately following the run (if any) is itself a
    // line marker, or whether the run is the last thing in the stream — in
    // either case a synthetic marker must not be inserted (D4).
    auto flush = [&](bool next_is_marker_or_eof) {
        if (run.empty())
            return;

        int nl = 0;
        for (const auto& t : run)
            nl += t.num_of_lines;
        cur = wrap_lineno(cur + nl);

        int blank_lines = nl - (line_has_content ? 1 : 0);

        // Pure inline whitespace (no newline in the run at all) is never
        // touched, in either mode — it is not a blank *line*, just spacing
        // between two tokens on the same line.
        bool verbatim = (max_blank_lines == 0) || (nl == 0) ||
                         (mode == BlankLineMode::Markers && blank_lines <= max_blank_lines);

        if (verbatim) {
            for (auto& t : run)
                out.push_back(std::move(t));
        } else {
            modified = true;
            if (line_has_content)
                out.push_back(Token::newline(1));
            if (mode == BlankLineMode::Markers && !next_is_marker_or_eof) {
                out.push_back(make_line_marker(wrap_lineno(cur - 1), enc_file, standard));
                out.push_back(Token::newline(1));
            }
            // Trailing inline whitespace (indentation of the next content
            // line), if the run ends with one or more non-newline tokens.
            std::size_t idx = run.size();
            std::string trailing;
            while (idx > 0 && run[idx - 1].num_of_lines == 0) {
                trailing = run[idx - 1].text + trailing;
                --idx;
            }
            if (!trailing.empty())
                out.push_back(Token::create(Token::WS, std::move(trailing)));
            line_has_content = false;
        }
        run.clear();
    };

    for (auto& t : ots) {
        if (t.type == Token::WS) {
            run.push_back(std::move(t));
            continue;
        }

        auto marker = parse_line_marker(t);
        flush(marker.has_value());

        if (marker) {
            cur      = marker->lineno;
            standard = marker->standard;
            if (!marker->enc_file.empty())
                enc_file = std::string(marker->enc_file);
        } else {
            cur = wrap_lineno(cur + t.num_of_lines);
        }
        // A marker token (num_of_lines == 0, always) is content-bearing:
        // it occupies its own printed line, so the run immediately
        // following it starts from a non-blank line, exactly like any
        // other content token.
        line_has_content = (t.num_of_lines == 0) || has_trailing_content(t.text);

        out.push_back(std::move(t));
    }
    flush(true); // end of stream (D4b)

    ots = std::move(out);
    return modified;
}

} // namespace jiepp
