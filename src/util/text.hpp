#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace Util {

std::string_view ltrim_view(std::string_view s);
std::string_view rtrim_view(std::string_view s);
std::string_view trim_view(std::string_view s);

// Re-escapes a real newline/carriage-return (e.g. one decoded from a $n/$r
// source escape) back to the literal two-character "$n"/"$r" sequence, so
// text that may contain either can still be embedded in a single-line
// diagnostic message without splitting it. A NUL byte (e.g. decoded from
// $00) is likewise written as the three characters "$00", so a diagnostic
// never carries a raw 0x00.
std::string escape_line_breaks(std::string_view s);

// Longest prefix of s that is at most max_bytes long and does not end inside
// a UTF-8 multibyte character (a cut that would leave a lead byte without all
// of its continuation bytes is moved back to the character boundary). ASCII
// text is cut at exactly max_bytes.
std::string_view utf8_head(std::string_view s, std::size_t max_bytes);

// Longest suffix of s that is at most max_bytes long and does not start with a
// UTF-8 continuation byte (0x80-0xBF); the cut is moved forward to the next
// character boundary. ASCII text is cut at exactly max_bytes.
std::string_view utf8_tail(std::string_view s, std::size_t max_bytes);

// Log-injection strip used for diagnostics in JIEPP_SANDBOX builds (SPEC 14,
// called from Issue::happen()). Removes C0 controls (bytes 0x00-0x1F) except
// '\n', '\r' and '\t', DEL (0x7F), and the C1 controls U+0080-U+009F in
// their UTF-8 form (the byte pair C2 80 - C2 9F, e.g. C2 9B = U+009B CSI).
// The pair test is made on the resulting text, not on the input: a byte
// 0x80-0x9F whose last *kept* byte is C2 removes that C2 and is dropped
// itself, so removing a byte in between (C2 01 9B, C2 7F 9B, C2 C2 80 9B)
// cannot re-form a pair. Keeping a byte never creates a pair except in that
// case, and dropping the C2 restores an earlier, already pair-free state, so
// the result never contains a forbidden byte or pair. A byte 0x80-0x9F not
// preceded by a kept C2 is kept: it is a continuation byte of another UTF-8
// character (or part of an invalid sequence). Invalid UTF-8 is not
// validated or repaired; it is passed through unchanged except for the
// removals above. Defined in every build; only sandbox builds call it.
//
// c1_pairs == C1Pairs::Keep skips the C2 80 - C2 9F rule and removes only
// the C0 controls and DEL above. The C1 rule assumes UTF-8 text; a file path
// on Windows is in the ANSI code page (argv), where C2 followed by 0x80-0x9F
// is ordinary text (CP932: U+FF82 then the lead byte of a double-byte
// character, e.g. C2 82 A0 = U+FF82 U+3042), so Issue::happen() passes Keep
// for paths there. The "no forbidden byte" result for C0/DEL still holds.
enum class C1Pairs { Remove, Keep };
std::string strip_control_chars(std::string_view s, C1Pairs c1_pairs = C1Pairs::Remove);

// Replaces each '\n' and each '\r' in s with one space, byte for byte, so a
// CR LF pair becomes two spaces; every other byte is unchanged. Used after
// strip_control_chars() for diagnostics in JIEPP_SANDBOX builds (SPEC 14,
// called from Issue::happen()): a space is neither C2 nor 0x80-0x9F and no
// byte is removed, so the strip's "no forbidden byte or pair" result is
// preserved. Defined in every build; only sandbox builds call it.
std::string line_breaks_to_spaces(std::string_view s);

} // namespace Util
