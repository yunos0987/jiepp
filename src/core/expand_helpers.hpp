#pragma once
// expand_helpers.hpp — Internal shared header for expand*.cpp files.
// Mirrors cpp.algo.md (Prosser's algorithm) section structure.

#include "../loader/token.hpp"
#include "../env/env.hpp"
#include "../env/issue.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jiepp::expand_detail {

// ── Conditional compilation control (expand_ctrl.cpp) ──────────────
// Not part of Prosser's algorithm; jiepp extension for #if/#elif/#else/#endif.

struct CtrlState {
    bool has_entered = false;
    std::optional<bool> condition;
    bool seen_else = false;
};

bool ctrl_is_active(const std::vector<CtrlState>& stack);
bool ctrl_parent_active(const std::vector<CtrlState>& stack);

// operand_error, when non-null, receives whether the condition's evaluation
// hit a 'defined' operand/paren error (C3): eval_cond() uses this to treat
// the whole condition as false without ever calling eval_const_expr on it.
std::string eval_cond_str(const std::string& raw_cond, Env& env, bool* operand_error = nullptr);
bool eval_cond(const std::string& raw_cond, Env& env);

// C2: {#ifdef NAME}/{#ifndef NAME}, like clang's HandleIfdefDirective. NAME
// is not macro-expanded; comments in raw_arg are whitespace. A missing or
// non-identifier NAME is INVALID_DEFINED_OPERAND and the group is false
// (even for {#ifndef}), so a later {#elif}/{#else} may still be taken.
// Tokens after NAME are PP49 (warning) and ignored.
bool eval_ifdef(const std::string& raw_arg, bool is_ifndef, Env& env);

// Whether NAME counts as defined for {#ifdef}/{#ifndef}/`defined`. See the
// definition in expand_ctrl.cpp for the __has_include special case.
bool macro_name_is_defined(std::string_view name, Env& env);

// ── Prosser's algorithm support (expand_subst.cpp) ─────────────────
// Corresponds to cpp.algo.md: §hsadd, §glue, §Support functions, §subst.

// §hsadd — add HS to every token in ts
std::vector<Token>& hsadd(const Token::HideSet& hs, std::vector<Token>& ts);

// §glue — paste last of left side with first of right side
void glue_tokens(std::vector<Token>& src, std::vector<Token>& item);

// Leading/trailing whitespace-or-comment presence of one actual argument in
// the macro call's own source, used to reproduce clang's comma spacing when
// stringizing __VA_ARGS__ (R6).
struct ArgWs { bool lead = false; bool trail = false; };

// §Support functions: select
// mark_sep (task_slug arg-expand-once, D3): when true, every R6 comma-joining
// WS token this call inserts is tagged Token::va_sep = true, so a later
// consumer can choose to keep or skip it (append_expanded()/subst()) --
// used only when building the ONE shared spelling of a memoised multi-use
// variadic parameter's expansion, never for a direct (non-memoised) call.
std::vector<Token> select_arg(int idx,
                              const std::vector<std::vector<Token>>& actuals,
                              bool is_va,
                              const std::vector<ArgWs>* ws = nullptr,
                              bool mark_sep = false);
int argc_from(int idx, const std::vector<std::vector<Token>>& actuals);

// ── §subst — argument-expansion memoisation (task_slug arg-expand-once, D1/D2) ──
//
// One ArgExpansionMemo is created per function-macro invocation in
// expand.cpp, but only when FunctionMacro::num_arg_slots() > 0 (i.e. at
// least one formal parameter is spelled >= 2 times in the body); otherwise
// nullptr is passed through and subst() behaves exactly as it did before
// this feature (direct expand(actual, result, env) per occurrence, no
// allocation). Object macros always pass nullptr.
//
// For a parameter with a slot, the first occurrence that actually needs
// the expanded form (not glue-adjacent, not a `@` operand, and reached
// only when it is substituted at all -- e.g. inside __VA_OPT__(...) only
// when the variable arguments are non-empty) expands the raw actual once
// into `expanded[slot]` and every occurrence (including this first one)
// appends a copy; the occurrence that observes `remaining[slot]` reach 0
// (computed from FunctionMacro::arg_slot_uses(), which over-counts, so
// remaining can reach 0 only at the true last need) moves the vector out
// instead of copying it.
struct ArgExpansionMemo {
    const std::vector<int>* slots;      // FunctionMacro::arg_slots(), by pidx
    std::vector<int> remaining;         // copy of FunctionMacro::arg_slot_uses()
    std::vector<std::optional<std::vector<Token>>> expanded; // per slot, lazy
};

// §Support functions: stringize
// Text of ts per the stringizing whitespace rule R1-R5: every maximal run of
// MASK_WS (non-line_filler) tokens between two non-WS tokens becomes one
// space; a run at either end is dropped; every other token is appended
// verbatim (no quoting/escaping -- the caller encodes the result).
std::string stringize_text(const std::vector<Token>& ts);
Token stringize_tokens(const std::vector<Token>& ts);

// §subst — substitute args, handle stringize and paste
// actual_ws: per-actual-argument leading/trailing whitespace flags (R6),
// non-null only when va_sep_ws is true somewhere in the call chain (i.e.
// only for a macro whose body stringizes its variable arguments --
// FunctionMacro::stringizes_va()); nullptr for every other call.
// va_sep_ws: true only while substituting the content of @__VA_OPT__(...),
// so select_arg() inserts ArgWs-derived spacing around the variadic commas
// there too, without ever doing so for ordinary (non-stringize) expansion.
// memo: non-null only when the macro has at least one multi-use parameter
// (see ArgExpansionMemo above); forwarded unchanged through every recursive
// subst() call for __VA_OPT__(...) content, so the single shared expansion
// is visible to occurrences inside and outside that content alike.
std::vector<Token> subst(
    const std::vector<Token>& body,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    const Token::HideSet& hs,
    Env& env,
    bool va_sep_ws = false,
    ArgExpansionMemo* memo = nullptr);

} // namespace jiepp::expand_detail
