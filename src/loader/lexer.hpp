#pragma once
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>
#include "token.hpp"

// Tokenize input stream → Token vector
// lineno: starting line number (1-based)
std::vector<Token> iec3_tokens(std::istream& input, bool remove_comments, int lineno = 1);

// Tokenize text string
std::vector<Token> iec3_tokens_from_string(const std::string& input, bool remove_comments, int lineno = 1);

// True iff `s` is exactly one identifier as tokenize() lexes it:
// [A-Za-z_][A-Za-z0-9_]* (ASCII only, like IEC 61131-3 identifiers).
bool iec3_is_identifier(std::string_view s);
