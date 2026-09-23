#pragma once
// line_compaction — post-pass that collapses long runs of blank lines in a
// fully materialised token stream, gcc/clang-compatible:
//   - runs of `max_blank_lines` (default 7) or fewer blank lines are emitted
//     byte-identical to today;
//   - runs of more than `max_blank_lines` blank lines are replaced by a
//     single line-marker line so downstream line numbering stays correct
//     (Markers mode), or removed entirely with no marker (CollapseAll mode,
//     used under -P where line markers are already suppressed);
//   - `max_blank_lines == 0` disables compaction entirely (verbatim
//     passthrough, i.e. pre-compaction behavior).
//
// A "blank line" is a physical line all of whose characters are whitespace
// (Token::WS tokens only — NOT Token::C, which also carries MASK_WS but is a
// comment, not blank content).
#include "../loader/token.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jiepp {

enum class BlankLineMode { Markers, CollapseAll };

// Collapses runs of blank lines in `ots` in place. `default_standard_style`
// is the pragma style to use for the very first synthetic marker, before any
// real marker in the stream has been observed (subsequent synthetic markers
// use the style of the most recently observed real marker, per D2 in the
// design plan). Returns true if the stream was modified.
bool compact_blank_lines(std::vector<Token>& ots, int max_blank_lines,
                          BlankLineMode mode, bool default_standard_style);

// True for a preprocessor-injected line marker token: (*{#:N 'file'}*)
// (annotated) or {#:N 'file'} (standard). User IEC pragmas with '#' are
// lexed as DIRECTIVE tokens, so PRAGMA tokens whose body begins with "#:"
// are exclusively factory-created line markers.
bool is_line_marker(const Token& t);

struct LineMarker {
    int              lineno;
    std::string_view enc_file; // already IEC-encoded (quotes included), or empty when the marker carries no path — a view into the source token's text; copy it out before that token is modified or moved
    bool             standard;
};

// Parses a line-marker token. Returns std::nullopt if `t` is not a line
// marker.
std::optional<LineMarker> parse_line_marker(const Token& t);

// Builds a line-marker token. `enc_file` is spliced back verbatim (never
// decode/re-encode the path).
Token make_line_marker(int lineno, std::string_view enc_file, bool standard);

} // namespace jiepp
