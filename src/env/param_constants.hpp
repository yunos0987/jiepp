#pragma once

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

// Upper bound for all integer parameters
inline constexpr int         MAX_PARAMETER_VALUE            = 1 << 24; // 2^24 = 16,777,216
