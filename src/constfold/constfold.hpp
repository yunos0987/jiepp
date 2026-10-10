#pragma once
#include <cstdint>
#include <string>

#include "constfold_internal.hpp"
#include "../env/issue.hpp"

// Evaluate an IEC 61131-3 constant expression string for {#if}/{#elif}.
// Returns 1 if the expression is true (non-zero), else 0; an expression that
// fails to parse, an empty one and a type error give 0.
// On error: may throw Issue::Exception (e.g. INVALID_EXPRESSION, EXPR_TYPE_ERROR).
// On empty expression: calls Issue::happen(MISSING_EXPRESSION) and returns 0.
int64_t eval_const_expr(const std::string& expr);

namespace cf {

// The typed result of a constant expression (mirrors CfValue without its
// scanner-only text field).
struct ConstValue {
    ValueKind kind = ValueKind::Int;
    std::int64_t i = 0;
    std::uint64_t bits = 0;
    BitKind bit_kind = BitKind::None;
    double f = 0.0;
    bool b = false;

    // Identical to CfValue::truthy().
    bool truthy() const {
        switch (kind) {
        case ValueKind::Int:       return i != 0;
        case ValueKind::Float:     return f != 0.0;
        case ValueKind::Bool:      return b;
        case ValueKind::Bitstring: return bits != 0;
        }
        return false;
    }
};

struct EvalResult {
    // The code of the first diagnostic that made the evaluation fail
    // (PP50/PP51/PP52), whether that diagnostic was shown or suppressed by
    // {#ignore}; SUCCESS otherwise. Division and modulo by zero (PP52) are
    // not failures: evaluation continues with 0 for that operation.
    Issue::Code failure = Issue::Code::SUCCESS;
    ConstValue value; // meaningful only when failure == SUCCESS
};

} // namespace cf

// Like eval_const_expr(), but returns the typed value and the failure code
// instead of a 0/1 truth value. Reports the same diagnostics. A failure is
// also recorded for literal errors raised inside scanner/grammar actions
// (invalid base or digit, integer/float literal overflow, float value out of
// integer range), after which eval_const_expr() still returns the truth value
// of the parsed expression.
cf::EvalResult eval_const_value(const std::string& expr);
