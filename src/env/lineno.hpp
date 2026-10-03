#pragma once
#include <cstdint>

// item a: the running line-number counter jiepp tracks per file wraps at
// 2^32, matching gcc/clang's unsigned 32-bit line counter (after
// 4294967295 the next line is 0). LineNo stores it in a 64-bit signed
// integer -- rather than std::uint32_t -- so ordinary arithmetic
// (`get_lineno() + delta`, `new_lineno - 1`, comparisons against negative
// intermediate values such as `0 - 1`) never needs separate signed/unsigned
// handling; wrap_lineno() re-normalizes into [0, 4294967295] afterwards.
using LineNo = std::int64_t;

// Wraps `v` into [0, 4294967295] (2^32 - 1), like gcc/clang's unsigned
// 32-bit line counter. Correct for any `v` whose magnitude is well within
// int64_t's range (true for every caller here: single-file line counts and
// small deltas, never astronomically large sums).
constexpr LineNo wrap_lineno(LineNo v) {
    return v & 0xFFFFFFFF;
}
