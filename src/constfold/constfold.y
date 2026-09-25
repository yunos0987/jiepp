%skeleton "lalr1.cc"
%require "3.2"
%defines
%define api.namespace {cf}
%define api.parser.class {CfParser}
%define api.value.type variant
%parse-param {int64_t& result}

%code requires {
#include "constfold/constfold_internal.hpp"
}

%code top {
#include "env/issue.hpp"
#include <cmath>
#include <cstdint>
#include <stdexcept>
}

%code {
CfValue cflval;  // global semantic value shared with flex scanner
bool cf_expr_complete = false;  // set true when a complete expr is reduced

extern int cflex(void);

static int yylex(cf::CfParser::semantic_type* yylval) {
    int tok = cflex();
    switch (tok) {
    case cf::CfParser::token::CF_INT:
    case cf::CfParser::token::CF_FLOAT:
    case cf::CfParser::token::CF_TYPE_KW:
    case cf::CfParser::token::CF_IDENT:
    // H: CF_INT_MIN_MAG carries cflval.text (the original spelling) for the
    // "integer literal overflow" diagnostic in unary_expr; without this
    // case the variant's CfValue slot is never constructed and every rule
    // that reads $1/$2 for this token (including $1.text) is undefined
    // behavior.
    case cf::CfParser::token::CF_INT_MIN_MAG:
        yylval->emplace<CfValue>(cflval);
        break;
    default:
        break;
    }
    return tok;
}

static CfValue make_bitstring(std::uint64_t raw, BitKind kind) {
    return CfValue::bitstring_value(raw & bit_mask(kind), kind);
}

static CfValue cast_int_literal_to_dword(const CfValue& v) {
    if (v.kind == ValueKind::Int)
        return make_bitstring(static_cast<std::uint32_t>(v.ival), BitKind::DWORD);
    return v;
}

[[noreturn]] static void type_error() {
    ISSUE(EXPR_TYPE_ERROR);
    // Reached when EXPR_TYPE_ERROR (PP50) is suppressed via {#ignore}, in
    // which case Issue::happen() above returns instead of throwing.
    // CfTypeError (not Issue::Exception) lets eval_const_expr() recognize
    // this specific case and recover instead of an unrelated exception type
    // escaping uncaught.
    throw CfTypeError();
}

// A3: perform +, -, *, and unary negation via uint64_t so that wraparound on
// signed overflow (e.g. INT64_MIN - 1) is well-defined two's-complement
// arithmetic instead of undefined behavior.
static int64_t wrap_add(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b));
}

static int64_t wrap_sub(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b));
}

static int64_t wrap_mul(int64_t a, int64_t b) {
    return static_cast<int64_t>(static_cast<std::uint64_t>(a) * static_cast<std::uint64_t>(b));
}

static int64_t wrap_neg(int64_t a) {
    return static_cast<int64_t>(static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(a));
}

// D: shift count n and left operand v/L for '<<'/'>>' in {#if}. "Out of
// range" means n < 0 or n >= 64 (checked without shifting by an
// out-of-range or negative amount, which is undefined behavior). Neither
// helper below can trigger undefined behavior for any int64_t input: every
// left shift is an unsigned shift by a count in [0,63], and the only right
// shift is of a plain (possibly negative) int64_t by a count in [0,63],
// which C++20 defines as arithmetic (floor division by 2^n).
//
// Int follows clang's PPExpressionEvaluator (SPEC §6.3, rev2/rev3
// decision): out of range, '<<' gives 0 and '>>' behaves like a shift by
// 63 (-1 for a negative value, 0 otherwise). In range, both are the
// ordinary C shift.
static int64_t shift_int(int64_t v, int64_t n, bool left) {
    if (n < 0 || n >= 64)
        return left ? 0 : (v < 0 ? -1 : 0);
    if (left)
        return static_cast<int64_t>(static_cast<std::uint64_t>(v) << n);
    return v >> n;
}

// Bitstring is not a C type: out of range gives 0 for every width and both
// directions (coordinator decision, rev3) -- unlike Int's '>>', which does
// not clamp. In range, both directions are the ordinary logical shift
// (masked to width by the caller via make_bitstring()).
static std::uint64_t shift_bits(std::uint64_t v, int64_t n, bool left) {
    if (n < 0 || n >= 64)
        return 0;
    return left ? (v << n) : (v >> n);
}

static CfValue cast_to_bool(const CfValue& v) {
    switch (v.kind) {
    case ValueKind::Bool:      return v;
    case ValueKind::Int:       return CfValue::bool_value(v.ival != 0);
    case ValueKind::Bitstring: return CfValue::bool_value(v.bits != 0);
    case ValueKind::Float:     type_error();
    }
    type_error();
}

static int64_t cast_scalar_to_int(const CfValue& v) {
    switch (v.kind) {
    case ValueKind::Bool:      return v.bval ? 1 : 0;
    case ValueKind::Int:       return v.ival;
    case ValueKind::Float: {
        constexpr auto max_i64 = static_cast<double>(INT64_MAX);
        constexpr auto min_i64 = static_cast<double>(INT64_MIN);
        if (v.fval > max_i64 || v.fval < min_i64 || std::isnan(v.fval)) {
            ISSUE(INVALID_EXPRESSION, "float value out of integer range");
            return 0;
        }
        return static_cast<int64_t>(v.fval);
    }
    case ValueKind::Bitstring: return static_cast<int64_t>(v.bits);
    }
    return 0;
}

static CfValue compare_eq(const CfValue& lhs, const CfValue& rhs, bool equal) {
    if (lhs.kind == ValueKind::Bool && rhs.kind == ValueKind::Bool)
        return CfValue::bool_value(equal ? (lhs.bval == rhs.bval) : (lhs.bval != rhs.bval));
    if (lhs.kind == ValueKind::Int && rhs.kind == ValueKind::Int)
        return CfValue::bool_value(equal ? (lhs.ival == rhs.ival) : (lhs.ival != rhs.ival));
    if (lhs.kind == ValueKind::Float && rhs.kind == ValueKind::Float)
        return CfValue::bool_value(equal ? (lhs.fval == rhs.fval) : (lhs.fval != rhs.fval));
    if (lhs.kind == ValueKind::Bitstring && rhs.kind == ValueKind::Bitstring && lhs.bit_kind == rhs.bit_kind)
        return CfValue::bool_value(equal ? (lhs.bits == rhs.bits) : (lhs.bits != rhs.bits));
    type_error();
}

enum class CompareOp { LT, LE, GT, GE };

static CfValue compare_ord(const CfValue& lhs, const CfValue& rhs, CompareOp op) {
    if (lhs.kind == ValueKind::Int && rhs.kind == ValueKind::Int) {
        bool r = false;
        switch (op) {
        case CompareOp::LT: r = lhs.ival < rhs.ival; break;
        case CompareOp::LE: r = lhs.ival <= rhs.ival; break;
        case CompareOp::GT: r = lhs.ival > rhs.ival; break;
        case CompareOp::GE: r = lhs.ival >= rhs.ival; break;
        }
        return CfValue::bool_value(r);
    }
    if (lhs.kind == ValueKind::Float && rhs.kind == ValueKind::Float) {
        bool r = false;
        switch (op) {
        case CompareOp::LT: r = lhs.fval < rhs.fval; break;
        case CompareOp::LE: r = lhs.fval <= rhs.fval; break;
        case CompareOp::GT: r = lhs.fval > rhs.fval; break;
        case CompareOp::GE: r = lhs.fval >= rhs.fval; break;
        }
        return CfValue::bool_value(r);
    }
    type_error();
}

void cf::CfParser::error(const std::string& msg) {
    if (cf_expr_complete)
        ISSUE(INVALID_EXPRESSION, msg);
    else
        ISSUE(MISSING_EXPRESSION, msg);
}
}

%token CF_LNOT CF_LAND CF_LOR CF_LXOR
%token CF_TRUE CF_FALSE
%token CF_NOT CF_AND CF_OR CF_XOR CF_MOD
%token <CfValue> CF_TYPE_KW
%token <CfValue> CF_INT CF_FLOAT CF_INT_MIN_MAG
%token <CfValue> CF_IDENT
%token CF_LPAREN CF_RPAREN
%token CF_PLUS CF_MINUS CF_STAR CF_SLASH
%token CF_AMP CF_EQ CF_NE CF_LT CF_LE CF_GT CF_GE
%token CF_SHL CF_SHR
%token CF_HASH
%token CF_UNKNOWN
%token CF_END 0
// H: fails the build on any shift/reduce or reduce/reduce conflict,
// including in unary_expr/unary_core below (prototyped against bison 3.8.2
// in scratchpad/followups/judge/cf_proto.y: 0 conflicts for this split
// form; the naive "CF_MINUS unary_expr" + "CF_MINUS CF_INT_MIN_MAG" form
// gives 22 reduce/reduce conflicts).
%expect 0

%type <CfValue> expr or_expr and_expr xor_expr not_expr cmp_expr
%type <CfValue> shift_expr add_expr bor_expr bxor_expr band_expr mul_expr unary_expr unary_core primary
%type <CfValue> cast_value

%%

start: expr { result = $1.truthy() ? 1 : 0; }
     ;

expr: or_expr { cf_expr_complete = true; $$ = $1; }
    ;

or_expr:
    and_expr                          { $$ = $1; }
  | or_expr CF_LOR and_expr           {
        CfValue lb = cast_to_bool($1), rb = cast_to_bool($3);
        $$ = CfValue::bool_value(lb.bval || rb.bval);
    }
  ;

and_expr:
    xor_expr                          { $$ = $1; }
  | and_expr CF_LAND xor_expr         {
        CfValue lb = cast_to_bool($1), rb = cast_to_bool($3);
        $$ = CfValue::bool_value(lb.bval && rb.bval);
    }
  ;

xor_expr:
    not_expr                          { $$ = $1; }
  | xor_expr CF_LXOR not_expr         {
        CfValue lb = cast_to_bool($1), rb = cast_to_bool($3);
        $$ = CfValue::bool_value(lb.bval != rb.bval);
    }
  ;

not_expr:
    cmp_expr                          { $$ = $1; }
  | CF_LNOT not_expr                  {
        CfValue b = cast_to_bool($2);
        $$ = CfValue::bool_value(!b.bval);
    }
  ;

cmp_expr:
    shift_expr                        { $$ = $1; }
  | cmp_expr CF_EQ  shift_expr        { $$ = compare_eq($1, $3, true); }
  | cmp_expr CF_NE  shift_expr        { $$ = compare_eq($1, $3, false); }
  | cmp_expr CF_LT  shift_expr        { $$ = compare_ord($1, $3, CompareOp::LT); }
  | cmp_expr CF_LE  shift_expr        { $$ = compare_ord($1, $3, CompareOp::LE); }
  | cmp_expr CF_GT  shift_expr        { $$ = compare_ord($1, $3, CompareOp::GT); }
  | cmp_expr CF_GE  shift_expr        { $$ = compare_ord($1, $3, CompareOp::GE); }
  ;

shift_expr:
    add_expr                          { $$ = $1; }
  | shift_expr CF_SHL add_expr        {
        CfValue l = $1;
        int64_t n = cast_scalar_to_int($3);
        if (l.kind == ValueKind::Int) {
            $$ = CfValue::int_value(shift_int(l.ival, n, true));
        } else if (l.kind == ValueKind::Bitstring) {
            $$ = make_bitstring(shift_bits(l.bits, n, true), l.bit_kind);
        } else {
            CfValue bit = cast_int_literal_to_dword(l);
            if (bit.kind == ValueKind::Bitstring)
                $$ = make_bitstring(shift_bits(bit.bits, n, true), bit.bit_kind);
            else type_error();
        }
    }
  | shift_expr CF_SHR add_expr        {
        CfValue l = $1;
        int64_t n = cast_scalar_to_int($3);
        if (l.kind == ValueKind::Int) {
            $$ = CfValue::int_value(shift_int(l.ival, n, false));
        } else if (l.kind == ValueKind::Bitstring) {
            $$ = make_bitstring(shift_bits(l.bits, n, false), l.bit_kind);
        } else {
            CfValue bit = cast_int_literal_to_dword(l);
            if (bit.kind == ValueKind::Bitstring)
                $$ = make_bitstring(shift_bits(bit.bits, n, false), bit.bit_kind);
            else type_error();
        }
    }
  ;

add_expr:
    bor_expr                          { $$ = $1; }
  | add_expr CF_PLUS  bor_expr        {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int)
            $$ = CfValue::int_value(wrap_add($1.ival, $3.ival));
        else type_error();
    }
  | add_expr CF_MINUS bor_expr        {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int)
            $$ = CfValue::int_value(wrap_sub($1.ival, $3.ival));
        else type_error();
    }
  ;

bor_expr:
    bxor_expr                         { $$ = $1; }
  | bor_expr CF_OR bxor_expr          {
        if ($1.kind == ValueKind::Bool && $3.kind == ValueKind::Bool) {
            $$ = CfValue::bool_value($1.bval || $3.bval);
        } else {
            CfValue l = cast_int_literal_to_dword($1);
            CfValue r = cast_int_literal_to_dword($3);
            if (l.kind == ValueKind::Bitstring && r.kind == ValueKind::Bitstring && l.bit_kind == r.bit_kind)
                $$ = make_bitstring(l.bits | r.bits, l.bit_kind);
            else type_error();
        }
    }
  ;

bxor_expr:
    band_expr                         { $$ = $1; }
  | bxor_expr CF_XOR band_expr        {
        if ($1.kind == ValueKind::Bool && $3.kind == ValueKind::Bool) {
            $$ = CfValue::bool_value($1.bval != $3.bval);
        } else {
            CfValue l = cast_int_literal_to_dword($1);
            CfValue r = cast_int_literal_to_dword($3);
            if (l.kind == ValueKind::Bitstring && r.kind == ValueKind::Bitstring && l.bit_kind == r.bit_kind)
                $$ = make_bitstring(l.bits ^ r.bits, l.bit_kind);
            else type_error();
        }
    }
  ;

band_expr:
    mul_expr                          { $$ = $1; }
  | band_expr CF_AND mul_expr         {
        if ($1.kind == ValueKind::Bool && $3.kind == ValueKind::Bool) {
            $$ = CfValue::bool_value($1.bval && $3.bval);
        } else {
            CfValue l = cast_int_literal_to_dword($1);
            CfValue r = cast_int_literal_to_dword($3);
            if (l.kind == ValueKind::Bitstring && r.kind == ValueKind::Bitstring && l.bit_kind == r.bit_kind)
                $$ = make_bitstring(l.bits & r.bits, l.bit_kind);
            else type_error();
        }
    }
  | band_expr CF_AMP mul_expr         {
        if ($1.kind == ValueKind::Bool && $3.kind == ValueKind::Bool) {
            $$ = CfValue::bool_value($1.bval && $3.bval);
        } else {
            CfValue l = cast_int_literal_to_dword($1);
            CfValue r = cast_int_literal_to_dword($3);
            if (l.kind == ValueKind::Bitstring && r.kind == ValueKind::Bitstring && l.bit_kind == r.bit_kind)
                $$ = make_bitstring(l.bits & r.bits, l.bit_kind);
            else type_error();
        }
    }
  ;

mul_expr:
    unary_expr                        { $$ = $1; }
  | mul_expr CF_STAR  unary_expr      {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int)
            $$ = CfValue::int_value(wrap_mul($1.ival, $3.ival));
        else type_error();
    }
  | mul_expr CF_SLASH unary_expr      {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int) {
            if ($3.ival == 0) { ISSUE(INVALID_EXPRESSION, "division by zero"); $$ = CfValue::int_value(0); }
            // A3: INT64_MIN / -1 overflows the quotient, which traps in
            // hardware (SIGFPE-style crash) even though two's-complement
            // wraparound defines the result as INT64_MIN itself.
            else if ($3.ival == -1) $$ = CfValue::int_value(wrap_neg($1.ival));
            else $$ = CfValue::int_value($1.ival / $3.ival);
        } else type_error();
    }
  | mul_expr CF_MOD   unary_expr      {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int) {
            if ($3.ival == 0) { ISSUE(INVALID_EXPRESSION, "modulo by zero"); $$ = CfValue::int_value(0); }
            // A3: same overflow trap as division by -1; the remainder is
            // mathematically 0 but the hardware idiv instruction used for
            // '%' still traps before producing it.
            else if ($3.ival == -1) $$ = CfValue::int_value(0);
            else $$ = CfValue::int_value($1.ival % $3.ival);
        } else type_error();
    }
  ;

// H: split so the grammar (not the lexer) decides which occurrences of '-'
// are unary and adjacent to CF_INT_MIN_MAG (the 2^63 magnitude,
// "9223372036854775808"). unary_core's closure after CF_MINUS does not
// contain "unary_expr -> . CF_INT_MIN_MAG", so "CF_MINUS CF_INT_MIN_MAG" is
// the only place that magnitude becomes INT64_MIN with no diagnostic;
// everywhere else (bare, after '(', after another '+'/'not', as a binary
// operand) reduces through unary_expr's own CF_INT_MIN_MAG rule below,
// which reports the same PP52 "integer literal overflow" as today.
unary_expr:
    unary_core                        { $$ = $1; }
  | CF_INT_MIN_MAG                    {
        ISSUE(INVALID_EXPRESSION, "integer literal overflow: " + $1.text);
        $$ = CfValue::int_value(0);
    }
  ;

unary_core:
    primary                           { $$ = $1; }
  | CF_MINUS CF_INT_MIN_MAG           { $$ = CfValue::int_value(INT64_MIN); }
  | CF_PLUS  unary_expr               {
        if ($2.kind == ValueKind::Int || $2.kind == ValueKind::Float) $$ = $2;
        else type_error();
    }
  | CF_MINUS unary_core               {
        if ($2.kind == ValueKind::Int) $$ = CfValue::int_value(wrap_neg($2.ival));
        else if ($2.kind == ValueKind::Float) $$ = CfValue::float_value(-$2.fval);
        else type_error();
    }
  | CF_NOT   unary_expr               {
        if ($2.kind == ValueKind::Bool) {
            $$ = CfValue::bool_value(!$2.bval);
        } else if ($2.kind == ValueKind::Bitstring) {
            $$ = make_bitstring(~$2.bits, $2.bit_kind);
        } else if ($2.kind == ValueKind::Int) {
            CfValue bit = cast_int_literal_to_dword($2);
            $$ = make_bitstring(~bit.bits, bit.bit_kind);
        } else {
            type_error();
        }
    }
  ;

primary:
    CF_INT                            { $$ = $1; }
  | CF_FLOAT                          { $$ = $1; }
  | CF_TRUE                           { $$ = CfValue::bool_value(true); }
  | CF_FALSE                          { $$ = CfValue::bool_value(false); }
  | CF_IDENT                          { $$ = CfValue::int_value(0); }
  | CF_LPAREN expr CF_RPAREN          { $$ = $2; }
  | CF_TYPE_KW CF_HASH cast_value     {
        const std::string& kw = $1.text;
        if (kw == "BOOL")        $$ = cast_to_bool($3);
        else if (kw == "BYTE")   $$ = make_bitstring(static_cast<uint64_t>(cast_scalar_to_int($3)), BitKind::BYTE);
        else if (kw == "WORD")   $$ = make_bitstring(static_cast<uint64_t>(cast_scalar_to_int($3)), BitKind::WORD);
        else if (kw == "DWORD")  $$ = make_bitstring(static_cast<uint64_t>(cast_scalar_to_int($3)), BitKind::DWORD);
        else if (kw == "LWORD")  $$ = make_bitstring(static_cast<uint64_t>(cast_scalar_to_int($3)), BitKind::LWORD);
        else                     $$ = CfValue::int_value(cast_scalar_to_int($3));
    }
  | CF_TYPE_KW                        { $$ = CfValue::int_value(0); }
  ;

cast_value:
    CF_TRUE                           { $$ = CfValue::bool_value(true); }
  | CF_FALSE                          { $$ = CfValue::bool_value(false); }
  | CF_INT                            { $$ = $1; }
  // H: a typed literal accepts the 2^63 magnitude directly (no diagnostic):
  // LWORD#9223372036854775808 = LWORD#16#8000000000000000, since the bit
  // pattern fits; narrower widths mask it like any other large value
  // (BYTE#9223372036854775808 = BYTE#16#00). LWORD#-9223372036854775808
  // goes through CF_MINUS cast_value below (wrap_neg of INT64_MIN).
  | CF_INT_MIN_MAG                    { $$ = CfValue::int_value(INT64_MIN); }
  | CF_FLOAT                          { $$ = $1; }
  | CF_MINUS cast_value               {
        if ($2.kind == ValueKind::Int) $$ = CfValue::int_value(wrap_neg($2.ival));
        else if ($2.kind == ValueKind::Float) $$ = CfValue::float_value(-$2.fval);
        else $$ = CfValue::int_value(wrap_neg(cast_scalar_to_int($2)));
    }
  | CF_PLUS  cast_value               { $$ = $2; }
  ;

%%
