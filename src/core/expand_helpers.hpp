#pragma once
// expand_helpers.hpp — Internal shared header for expand*.cpp files.
// Mirrors cpp.algo.md (Prosser's algorithm) section structure.

#include "../loader/token.hpp"
#include "../env/env.hpp"
#include "../env/issue.hpp"

#include <optional>
#include <string>
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
bool eval_ifdef(const std::string& raw_arg, bool is_ifndef, Env& env);

// ── Prosser's algorithm support (expand_subst.cpp) ─────────────────
// Corresponds to cpp.algo.md: §hsadd, §glue, §Support functions, §subst.

// §hsadd — add HS to every token in ts
std::vector<Token>& hsadd(const Token::HideSet& hs, std::vector<Token>& ts);

// §glue — paste last of left side with first of right side
void glue_tokens(std::vector<Token>& src, std::vector<Token>& item);

// §Support functions: select
std::vector<Token> select_arg(int idx,
                              const std::vector<std::vector<Token>>& actuals,
                              bool is_va);
int argc_from(int idx, const std::vector<std::vector<Token>>& actuals);

// §Support functions: stringize
Token stringize_tokens(const std::vector<Token>& ts);

// §subst — substitute args, handle stringize and paste
std::vector<Token> subst(
    const std::vector<Token>& body,
    const std::unordered_map<std::string, std::pair<int, bool>>& formal_params,
    const std::vector<std::vector<Token>>& actual_params,
    const Token::HideSet& hs,
    Env& env);

} // namespace jiepp::expand_detail
