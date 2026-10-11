#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "../env/issue.hpp"

// Code of the first failing diagnostic of the current evaluation (see
// eval_const_value(), constfold.hpp); reset to SUCCESS at the start of each
// evaluation. Defined in constfold.y. cf_note_failure() is called right
// BEFORE the ISSUE() of every failure site (the ISSUE may throw, and a
// suppressed one returns), except division/modulo by zero.
extern Issue::Code cf_failure_code;
inline void cf_note_failure(Issue::Code code) {
    if (cf_failure_code == Issue::Code::SUCCESS)
        cf_failure_code = code;
}

// Depth of dead (unevaluated) operands the parser is currently inside: the
// right operand of \and\ whose left side is false/0, or of \or\ whose left
// side is true/non-zero (SPEC 6.4). Raised and lowered by actions in
// constfold.y's and_expr/or_expr, reset by evaluate() (constfold.cpp) because
// a parse aborted by an exception never runs the lowering action. Defined in
// constfold.y.
extern int cf_unevaluated_depth;

// Report a value-dependent diagnostic of the grammar: nothing is reported
// (and no failure is recorded) inside an unevaluated operand. `failure` says
// whether it counts as an evaluation failure (cf_note_failure() is called
// right before the report, as at every other failure site). `loc` is the
// caller's source location, so the debug "@file:line" suffix names the
// grammar action and not this helper.
inline void cf_value_diag(Issue::Code code, std::string ctx, bool failure,
                          std::source_location loc = std::source_location::current()) {
    if (cf_unevaluated_depth > 0)
        return;
    if (failure)
        cf_note_failure(code);
    Issue::happen(code, std::move(ctx), loc);
}

enum class ValueKind {
    Int,
    Float,
    Bool,
    Bitstring,
};

enum class BitKind {
    None,
    BYTE,
    WORD,
    DWORD,
    LWORD,
};

struct CfValue {
    ValueKind kind = ValueKind::Int;
    int64_t ival = 0;
    double fval = 0.0;
    bool bval = false;
    std::uint64_t bits = 0;
    BitKind bit_kind = BitKind::None;
    std::string text;  // Used for CF_IDENT and CF_TYPE_KW tokens

    static CfValue int_value(int64_t v) {
        CfValue out;
        out.kind = ValueKind::Int;
        out.ival = v;
        return out;
    }

    static CfValue float_value(double v) {
        CfValue out;
        out.kind = ValueKind::Float;
        out.fval = v;
        return out;
    }

    static CfValue bool_value(bool v) {
        CfValue out;
        out.kind = ValueKind::Bool;
        out.bval = v;
        return out;
    }

    static CfValue bitstring_value(std::uint64_t raw, BitKind kind) {
        CfValue out;
        out.kind = ValueKind::Bitstring;
        out.bits = raw;
        out.bit_kind = kind;
        return out;
    }

    bool truthy() const {
        switch (kind) {
        case ValueKind::Int:       return ival != 0;
        case ValueKind::Float:     return fval != 0.0;
        case ValueKind::Bool:      return bval;
        case ValueKind::Bitstring: return bits != 0;
        }
        return false;
    }
};

// Internal-only signal thrown by type_error() (constfold.y) after the
// EXPR_TYPE_ERROR diagnostic has been issued (or suppressed via
// {#ignore PP50}). Distinct from Issue::Exception so eval_const_expr()
// (constfold.cpp) can always unwind the parser and return a value instead of
// letting an unrelated exception type escape uncaught when the diagnostic
// was ignored and Issue::happen() returned normally.
struct CfTypeError {};

inline std::uint64_t bit_mask(BitKind kind) {
    switch (kind) {
    case BitKind::BYTE:  return 0xffull;
    case BitKind::WORD:  return 0xffffull;
    case BitKind::DWORD: return 0xffff'ffffull;
    case BitKind::LWORD: return 0xffff'ffff'ffff'ffffull;
    case BitKind::None:  return 0;
    }
    return 0;
}
