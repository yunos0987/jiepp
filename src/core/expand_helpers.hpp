#pragma once
// expand_helpers.hpp — Internal shared header for expand*.cpp files.
// Mirrors cpp.algo.md (Prosser's algorithm) section structure.

#include "../constfold/constfold.hpp"
#include "../loader/token.hpp"
#include "../env/env.hpp"
#include "../env/issue.hpp"
#include "../macro/macro.hpp"

#include <optional>
#include <span>
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

// Evaluates the operand tokens of a built-in function macro (__assert's
// expression, __counter_set's value) with the {#if} rules: the tokens are NOT pre-expanded; they
// are expanded during evaluation with the transient 'defined' operator
// installed (or, inside an {#if} condition, re-enabled with the outer
// condition's defined-error state saved and restored), __has_include(...)
// resolved, undefined identifiers = 0, comments = whitespace. Hide sets on
// the tokens are kept (a text round trip would lose them, so
// {#define N __assert(N, m)} would re-expand N until PP60). Charges one
// expansion step per byte of the operand text (PP64), like an {#if} operand.
// failure != SUCCESS when the evaluation itself failed -- PP62 (sandbox
// __has_include), a 'defined' operand error (PP40, or PP52 for a missing
// ')'), or a constfold failure (PP50/PP51/PP52, literal errors included) --
// whether or not that diagnostic was shown (it may have been suppressed by
// {#ignore}). Division/modulo by zero is not a failure (SPEC 6.3).
struct CondValue {
    Issue::Code failure = Issue::Code::SUCCESS; // first failing diagnostic code
    cf::ConstValue value;                       // valid only when failure == SUCCESS
};
CondValue eval_cond_value_tokens(std::vector<Token> ts, Env& env);

// ── Prosser's algorithm support (expand_subst.cpp) ─────────────────
// Corresponds to cpp.algo.md: §hsadd, §glue, §Support functions, §subst.

// §hsadd — add HS to every token in ts
std::vector<Token>& hsadd(const Token::HideSet& hs, std::vector<Token>& ts);

// §glue — paste last of left side with first of right side
void glue_tokens(std::vector<Token>& src, std::vector<Token>& item);

// Leading/trailing whitespace-or-comment presence of one actual argument in
// the macro call's own source, used to reproduce clang's comma spacing when
// stringizing __VA_ARGS__ (R6).
// lead/trail: any whitespace/comment run (newlines included) at that edge --
// clang's Stringify test (hasLeadingSpace() || isAtStartOfLine()), used when
// the variable arguments are stringized (R6). lead_sp/trail_sp: clang's
// Token::LeadingSpace for the token after that run -- true iff the run's last
// token does not end in a newline -- used for plain substitution (C6).
struct ArgWs {
    bool lead = false;
    bool trail = false;
    bool lead_sp = false;
    bool trail_sp = false;
};

// How select_arg() joins the variable arguments (C6).
// plain:     one space where lead_sp/trail_sp (non-stringize use).
// stringize: one space where lead/trail (R6).
// shared:    the stringize spelling, with each space that plain would not
//            emit tagged Token::va_sep (D3; one memoised spelling filtered
//            per consumer by append_expanded()).
enum class VaJoin { plain, stringize, shared };

// §Support functions: select
// join (see VaJoin): VaJoin::shared (task_slug arg-expand-once, D3) tags the
// WS tokens that only the stringize consumer keeps (newline-only edges) with
// Token::va_sep = true, so a later consumer can choose to keep or skip them
// (append_expanded()/subst()) -- used only when building the ONE shared
// spelling of a memoised multi-use variadic parameter's expansion, never for
// a direct (non-memoised) call. ws == nullptr (non-variadic call, or a caller
// that has no per-actual flags) joins with a bare comma.
std::vector<Token> select_arg(int idx,
                              const std::vector<std::vector<Token>>& actuals,
                              bool is_va,
                              const std::vector<ArgWs>* ws = nullptr,
                              VaJoin join = VaJoin::plain);
// select_arg(idx, actuals, is_va).empty(), in O(1) and without copying any
// actual token: it runs the same join as select_arg() (one definition of the
// separator rule) and stops at the first token produced (S-01).
bool va_args_empty(int idx, const std::vector<std::vector<Token>>& actuals, bool is_va);
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
//
// S-01: `stringized` caches, per slot, the encoded STRING text of `@param`
// (or `@__VA_ARGS__` / `@args`), so a parameter is stringized at most once
// per invocation however often `@` names it. The text depends only on the
// slot: `@param` always joins variable arguments with VaJoin::stringize
// (R6), whatever va_sep_ws is. `@__VA_ARGC__` resolves to the same pidx (and
// so the same slot) as the variable arguments, so subst() looks the cache up
// only after it has ruled `__VA_ARGC__` out. The vector stays empty until
// the first stringize of a parameter with a slot.
struct ArgExpansionMemo {
    const std::vector<int>* slots;      // FunctionMacro::arg_slots(), by pidx
    std::vector<int> remaining;         // copy of FunctionMacro::arg_slot_uses()
    std::vector<std::optional<std::vector<Token>>> expanded; // per slot, lazy
    std::vector<std::optional<std::string>> stringized;      // per slot, lazy
};

// §Support functions: stringize
// Text of ts per the stringizing whitespace rule R1-R5: every maximal run of
// MASK_WS (non-line_filler) tokens between two non-WS tokens becomes one
// space; a run at either end is dropped; every other token is appended
// verbatim (no quoting/escaping -- the caller encodes the result).
std::string stringize_text(const std::vector<Token>& ts);
Token stringize_tokens(const std::vector<Token>& ts);

// §subst — substitute args, handle stringize and paste
// actual_ws: per-actual-argument leading/trailing whitespace flags, built by
// expand.cpp for every call of a variadic macro (nullptr otherwise); they let
// select_arg() reproduce the call's spacing around the joining commas.
// va_sep_ws: true only while substituting the content of @__VA_OPT__(...),
// where the variadic commas are joined with the stringize spelling (R6:
// a newline-only edge counts as whitespace) instead of the plain one (C6).
// memo: non-null only when the macro has at least one multi-use parameter
// (see ArgExpansionMemo above); forwarded unchanged through every recursive
// subst() call for __VA_OPT__(...) content, so the single shared expansion
// is visible to occurrences inside and outside that content alike.
// va_opt: the VaOptSpan table of `body` (FunctionMacro / UserDefinedObjectMacro
// ::va_opt_spans(), or the matching subrange for __VA_OPT__ content). An
// empty table is computed locally the first time a __VA_OPT__ token is met,
// so a caller without one (a built-in replacement) stays correct. The
// content of __VA_OPT__(...) is substituted as a subspan of `body`, never
// copied or rescanned.
std::vector<Token> subst(
    std::span<const Token> body,
    std::span<const VaOptSpan> va_opt,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const std::vector<ArgWs>* actual_ws,
    const Token::HideSet& hs,
    Env& env,
    bool va_sep_ws = false,
    ArgExpansionMemo* memo = nullptr);

// ── Stack-exhaustion report (expand.cpp) ──────────────────────────

// PP63 (SEVERE). Shared by ExpansionDepthGuard and subst_va_opt_content()
// so the message cannot drift. Out of line: its std::string temporaries
// must not enlarge the frames of the recursive functions.
[[noreturn, gnu::noinline, gnu::cold]] void raise_stack_exhausted(const Env& env);

} // namespace jiepp::expand_detail
