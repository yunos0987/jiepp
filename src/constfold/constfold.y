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
    throw std::logic_error("unreachable");
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
%token <CfValue> CF_INT CF_FLOAT
%token <CfValue> CF_IDENT
%token CF_LPAREN CF_RPAREN
%token CF_PLUS CF_MINUS CF_STAR CF_SLASH
%token CF_AMP CF_EQ CF_NE CF_LT CF_LE CF_GT CF_GE
%token CF_SHL CF_SHR
%token CF_HASH
%token CF_UNKNOWN
%token CF_END 0

%type <CfValue> expr or_expr and_expr xor_expr not_expr cmp_expr
%type <CfValue> shift_expr add_expr bor_expr bxor_expr band_expr mul_expr unary_expr primary
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
            $$ = CfValue::int_value(n < 0 || n >= 64 ? 0 : l.ival << n);
        } else if (l.kind == ValueKind::Bitstring) {
            $$ = make_bitstring(n < 0 || n >= 64 ? 0 : l.bits << n, l.bit_kind);
        } else {
            CfValue bit = cast_int_literal_to_dword(l);
            if (bit.kind == ValueKind::Bitstring)
                $$ = make_bitstring(n < 0 || n >= 64 ? 0 : bit.bits << n, bit.bit_kind);
            else type_error();
        }
    }
  | shift_expr CF_SHR add_expr        {
        CfValue l = $1;
        int64_t n = cast_scalar_to_int($3);
        if (l.kind == ValueKind::Int) {
            $$ = CfValue::int_value(n < 0 || n >= 64 ? 0 : l.ival >> n);
        } else if (l.kind == ValueKind::Bitstring) {
            $$ = make_bitstring(n < 0 || n >= 64 ? 0 : l.bits >> n, l.bit_kind);
        } else {
            CfValue bit = cast_int_literal_to_dword(l);
            if (bit.kind == ValueKind::Bitstring)
                $$ = make_bitstring(n < 0 || n >= 64 ? 0 : bit.bits >> n, bit.bit_kind);
            else type_error();
        }
    }
  ;

add_expr:
    bor_expr                          { $$ = $1; }
  | add_expr CF_PLUS  bor_expr        {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int)
            $$ = CfValue::int_value($1.ival + $3.ival);
        else type_error();
    }
  | add_expr CF_MINUS bor_expr        {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int)
            $$ = CfValue::int_value($1.ival - $3.ival);
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
            $$ = CfValue::int_value($1.ival * $3.ival);
        else type_error();
    }
  | mul_expr CF_SLASH unary_expr      {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int) {
            if ($3.ival == 0) { ISSUE(INVALID_EXPRESSION, "division by zero"); $$ = CfValue::int_value(0); }
            else $$ = CfValue::int_value($1.ival / $3.ival);
        } else type_error();
    }
  | mul_expr CF_MOD   unary_expr      {
        if ($1.kind == ValueKind::Int && $3.kind == ValueKind::Int) {
            if ($3.ival == 0) { ISSUE(INVALID_EXPRESSION, "modulo by zero"); $$ = CfValue::int_value(0); }
            else $$ = CfValue::int_value($1.ival % $3.ival);
        } else type_error();
    }
  ;

unary_expr:
    primary                           { $$ = $1; }
  | CF_PLUS  unary_expr               {
        if ($2.kind == ValueKind::Int || $2.kind == ValueKind::Float) $$ = $2;
        else type_error();
    }
  | CF_MINUS unary_expr               {
        if ($2.kind == ValueKind::Int) $$ = CfValue::int_value(-$2.ival);
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
  | CF_FLOAT                          { $$ = $1; }
  | CF_MINUS cast_value               {
        if ($2.kind == ValueKind::Int) $$ = CfValue::int_value(-$2.ival);
        else if ($2.kind == ValueKind::Float) $$ = CfValue::float_value(-$2.fval);
        else $$ = CfValue::int_value(-cast_scalar_to_int($2));
    }
  | CF_PLUS  cast_value               { $$ = $2; }
  ;

%%
