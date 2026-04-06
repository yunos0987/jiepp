#include "macro.hpp"
#include "../env/env.hpp"
#include "../env/issue.hpp"

#include <algorithm>
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
    if (ts_.size() != p->ts_.size())
        return false;
    for (std::size_t i = 0; i < ts_.size(); ++i) {
        if (!(ts_[i] == p->ts_[i]))
            return false;
    }
    return true;
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

FunctionMacro::FunctionMacro(std::vector<std::string> args_list, std::vector<Token> body)
    : args_list_(args_list) {
    bool has_va = !args_list.empty() && args_list.back() == VA_SYM;

    int regular_count = static_cast<int>(args_list.size()) - (has_va ? 1 : 0);

    for (int i = 0; i < regular_count; ++i) {
        args_[args_list[i]] = {i, false};
    }

    if (has_va) {
        int va_idx = regular_count;
        args_[VA_ARGS] = {va_idx, true};
        args_[VA_ARGC] = {va_idx, true};
        num_params_min_ = va_idx;
        num_params_max_ = NUM_OF_MAX_ARGS;
    } else {
        num_params_min_ = regular_count;
        num_params_max_ = regular_count;
    }

    body_ = normalize(std::move(body), args_);
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
    if (args_ != p->args_)
        return false;
    if (body_.size() != p->body_.size())
        return false;
    for (std::size_t i = 0; i < body_.size(); ++i)
        if (!(body_[i] == p->body_[i]))
            return false;
    return true;
}

std::string FunctionMacro::str() const {
    std::string s = "(";
    for (std::size_t i = 0; i < args_list_.size(); ++i) {
        if (i > 0)
            s += ",";
        s += args_list_[i];
    }
    s += ") ";
    for (const auto& t : body_)
        s += t.text;
    return s;
}
