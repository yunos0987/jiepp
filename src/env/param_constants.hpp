#pragma once

#include <cstdint>

// Parameter key names (used for CLI options and directives)
inline constexpr const char* KEY_MAX_INCLUDE_DEPTH     	    = "max-include-depth";
inline constexpr const char* KEY_PP_OUTPUT_PRAGMA_STYLE     = "pp-output-pragma-style";
inline constexpr const char* KEY_REMOVE_COMMENTS            = "remove-comments";

// Pragma style values
inline constexpr const char* VAL_PRAGMA_STANDARD            = "standard";
inline constexpr const char* VAL_PRAGMA_ANNOTATED           = "annotated";

// Default parameter values
inline constexpr int         DEFAULT_MAX_INCLUDE_DEPTH      = 100;
inline constexpr int         DEFAULT_MAX_EXPANSION_DEPTH    = 256;
inline constexpr int         DEFAULT_MAX_IF_NESTING         = 256;
// Longest run of consecutive blank lines emitted verbatim before it is
// replaced by a single line-marker line (gcc/clang-compatible threshold).
// 0 disables blank-line compaction entirely (pre-compaction output).
inline constexpr int         DEFAULT_MAX_BLANK_LINES        = 7;

// Cap on macro-expansion work ("steps", see Param::charge_expansion_steps()),
// used in every build including the sandbox. 0 means no limit. This is a work
// count, not a depth, so MAX_PARAMETER_VALUE below does not apply to it.
inline constexpr std::uint64_t DEFAULT_MAX_EXPANSION_STEPS  = 1ull << 24; // 16,777,216

// Cap on executed {#include}/{#sinclude} directives (PP66), used in every
// build. 0 means no limit. Like DEFAULT_MAX_EXPANSION_STEPS this is a count of
// work, not a depth, so MAX_PARAMETER_VALUE below does not apply to it.
inline constexpr int         DEFAULT_MAX_INCLUDE_COUNT      = 10000;

// Default for --error-limit (PP65): the number of errors counted before the
// next one stops processing. 0 means no limit. The CLI sets this default; the
// library's Issue::error_limit_ stays 0 unless the caller sets it. A sandbox
// build uses a smaller default, since its input is untrusted.
#ifdef JIEPP_SANDBOX
inline constexpr int         DEFAULT_ERROR_LIMIT            = 100;
#else
inline constexpr int         DEFAULT_ERROR_LIMIT            = 1000;
#endif

// Upper bound for all integer parameters
inline constexpr int         MAX_PARAMETER_VALUE            = 1 << 24; // 2^24 = 16,777,216
