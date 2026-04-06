#pragma once
#include <cstdint>
#include <string>

// Evaluate an IEC 61131-3 constant expression string.
// Returns the integer value (true=1, false=0 for booleans).
// On error: may throw Issue::Exception (e.g. INVALID_EXPRESSION, EXPR_TYPE_ERROR).
// On empty expression: calls Issue::happen(MISSING_EXPRESSION) and returns 0.
int64_t eval_const_expr(const std::string& expr);
