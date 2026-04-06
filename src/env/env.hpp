#pragma once
#include "symtab.hpp"
#include "file_context.hpp"
#include "param.hpp"

// Env composes three mixin classes via multiple inheritance:
//   Symtab      — symbol table (define/exist/lookup/undef/symbols)
//   FileContext  — include stack, syspaths, line number
//   Param        — limits, pragma style, comment removal, token cache
//
// All methods are inherited directly; callers use env.define(), env.push_file(), etc.
class Env : public Symtab, public FileContext, public Param {
public:
    Env() = default;
    ~Env() override = default;
    Env(Env&&) noexcept = default;
    Env& operator=(Env&&) noexcept = default;
};
