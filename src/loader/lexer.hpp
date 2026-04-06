#pragma once
#include <iosfwd>
#include <string>
#include <vector>
#include "token.hpp"

// Tokenize input stream → Token vector
// lineno: starting line number (1-based)
std::vector<Token> iec3_tokens(std::istream& input, bool remove_comments, int lineno = 1);

// Tokenize text string
std::vector<Token> iec3_tokens_from_string(const std::string& input, bool remove_comments, int lineno = 1);
