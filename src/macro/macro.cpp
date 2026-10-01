#include "macro.hpp"
#include "../env/env.hpp"
#include "../env/issue.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Remove whitespace tokens adjacent to each Token::GLUE (@@) token.
// Shared by UserDefinedObjectMacro::normalize and FunctionMacro::normalize.
std::vector<Token> normalize_glue(std::vector<Token> ts) {
    std::vector<bool> keep(ts.size(), true);
    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (ts[i].type != Token::GLUE)
            continue;
        // Remove whitespace immediately before @@
        for (std::size_t j = i; j-- > 0;) {
            if (!(ts[j].type & Token::MASK_WS))
                break;
            keep[j] = false;
        }
        // Remove whitespace immediately after @@
        for (std::size_t j = i + 1; j < ts.size(); ++j) {
            if (!(ts[j].type & Token::MASK_WS))
                break;
            keep[j] = false;
        }
    }
    std::vector<Token> result;
    result.reserve(ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i)
        if (keep[i])
            result.push_back(std::move(ts[i]));
    return result;
}

// Compare two replacement-list token sequences the way C17 6.10.3p2 requires
// for macro redefinition: "identical" ignores the AMOUNT of white-space
// between tokens, only whether a separation is present or absent. Each
// maximal run of whitespace-type tokens on either side collapses to a single
// "whitespace present" marker before comparison; everything else (including
// hide-sets via Token::operator==) is compared exactly as before.
// Token::operator== itself is left untouched (tests/loader/test_token.cpp
// depends on its current exact-text semantics).
bool token_lists_equal_ignoring_ws_amount(const std::vector<Token>& a,
                                          const std::vector<Token>& b) {
    std::size_t ia = 0, ib = 0;
    while (ia < a.size() && ib < b.size()) {
        bool a_ws = (a[ia].type & Token::MASK_WS) != 0;
        bool b_ws = (b[ib].type & Token::MASK_WS) != 0;
        if (a_ws || b_ws) {
            if (!a_ws || !b_ws)
                return false; // whitespace present on only one side
            while (ia < a.size() && (a[ia].type & Token::MASK_WS)) ++ia;
            while (ib < b.size() && (b[ib].type & Token::MASK_WS)) ++ib;
            continue;
        }
        if (!(a[ia] == b[ib]))
            return false;
        ++ia;
        ++ib;
    }
    return ia == a.size() && ib == b.size();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// UserDefinedObjectMacro
// ---------------------------------------------------------------------------

std::vector<Token> UserDefinedObjectMacro::normalize(std::vector<Token> ts) {
    ts = ts_trim(std::move(ts));
    ts = normalize_glue(std::move(ts));
    return ts;
}

UserDefinedObjectMacro::UserDefinedObjectMacro(std::vector<Token> ts)
    : ts_(normalize(std::move(ts))) {}

std::vector<Token> UserDefinedObjectMacro::replacement(Env& /*env*/) const {
    return ts_;
}

bool UserDefinedObjectMacro::equal(const Macro& other) const {
    auto* p = dynamic_cast<const UserDefinedObjectMacro*>(&other);
    if (!p)
        return false;
    return token_lists_equal_ignoring_ws_amount(ts_, p->ts_);
}

std::string UserDefinedObjectMacro::str() const {
    std::string s;
    for (const auto& t : ts_)
        s += t.text;
    return s;
}

// ---------------------------------------------------------------------------
// FunctionMacro
// ---------------------------------------------------------------------------

FunctionMacro::FunctionMacro(std::vector<std::string> args_list, std::vector<Token> body,
                             bool named_variadic)
    : args_list_(args_list), named_variadic_(named_variadic) {
    if (named_variadic && args_list.empty())
        throw std::logic_error("FunctionMacro: named variadic without a parameter name");
    bool has_va = named_variadic || (!args_list.empty() && args_list.back() == VA_SYM);

    int regular_count = static_cast<int>(args_list.size()) - (has_va ? 1 : 0);

    for (int i = 0; i < regular_count; ++i) {
        args_[args_list[i]] = {i, false};
    }

    if (has_va) {
        int va_idx = regular_count;
        args_[VA_SYM] = {va_idx, true};
        // '...' names the variable arguments __VA_ARGS__; `args...` names
        // them `args` instead (gcc/clang).
        args_[named_variadic ? args_list.back() : std::string(VA_ARGS)] = {va_idx, true};
        // __VA_ARGC__ (jiepp extension) counts them in both forms.
        args_[VA_ARGC] = {va_idx, true};
        num_params_min_ = va_idx;
        num_params_max_ = NUM_OF_MAX_ARGS;
    } else {
        num_params_min_ = regular_count;
        num_params_max_ = regular_count;
    }

    body_ = normalize(std::move(body), args_);
    stringizes_va_ = compute_stringizes_va(body_, args_);
}

bool FunctionMacro::compute_stringizes_va(
    const std::vector<Token>& body,
    const std::unordered_map<std::string, std::pair<int, bool>>& args) {
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (body[i].type != Token::STRINGIZE)
            continue;
        std::size_t j = i + 1;
        while (j < body.size() && (body[j].type & Token::MASK_WS))
            ++j;
        if (j >= body.size() || body[j].type != Token::ANY)
            continue;
        const std::string& text = body[j].text;
        if (text == VA_OPT)
            return true;
        auto it = args.find(text);
        if (it != args.end() && it->second.second && text != VA_ARGC)
            return true;
    }
    return false;
}

std::vector<Token> FunctionMacro::normalize(
    std::vector<Token> ts,
    const std::unordered_map<std::string, std::pair<int, bool>>& args) {

    ts = ts_trim(std::move(ts));
    ts = normalize_glue(std::move(ts));

    // For Token::STRINGIZE (@): remove whitespace between @ and its argument.
    std::vector<bool> keep(ts.size(), true);
    for (std::size_t i = 0; i < ts.size(); ++i) {
        if (ts[i].type != Token::STRINGIZE)
            continue;
        // Find the next non-WS token
        std::size_t j = i + 1;
        while (j < ts.size() && (ts[j].type & Token::MASK_WS))
            ++j;
        if (j < ts.size() && args.count(ts[j].text))
            // Remove whitespace between @ and the argument token.
            for (std::size_t k = i + 1; k < j; ++k)
                keep[k] = false;
    }
    std::vector<Token> result;
    result.reserve(ts.size());
    for (std::size_t i = 0; i < ts.size(); ++i)
        if (keep[i])
            result.push_back(std::move(ts[i]));
    return result;
}

std::vector<Token> FunctionMacro::replacement(Env& /*env*/) const {
    // Actual expansion with argument substitution is handled by the preprocessor core.
    return body_;
}

bool FunctionMacro::equal(const Macro& other) const {
    auto* p = dynamic_cast<const FunctionMacro*>(&other);
    if (!p)
        return false;
    if (num_params_min_ != p->num_params_min_)
        return false;
    if (num_params_max_ != p->num_params_max_)
        return false;
    // A GNU named variadic and a C99 '...' are different definitions even when
    // args_ coincides (F(__VA_ARGS__...) vs F(...)), as in clang.
    if (named_variadic_ != p->named_variadic_)
        return false;
    if (args_ != p->args_)
        return false;
    return token_lists_equal_ignoring_ws_amount(body_, p->body_);
}

std::string FunctionMacro::str() const {
    std::string s = "(";
    for (std::size_t i = 0; i < args_list_.size(); ++i) {
        if (i > 0)
            s += ",";
        s += args_list_[i];
    }
    if (named_variadic_)
        s += VA_SYM; // "args..." -- gcc/clang -dM print F(a,args...)
    s += ") ";
    for (const auto& t : body_)
        s += t.text;
    return s;
}
