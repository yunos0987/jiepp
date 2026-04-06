#pragma once

#include <cstdint>
#include <string>

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
