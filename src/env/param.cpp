#include "param.hpp"
#include "issue.hpp"
#include "../loader/token.hpp"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool validate_parameter(int n, std::string_view name) {
    if (n <= 0) {
        ISSUE(INVALID_PARAMETER_VALUE, std::string(name));
        return false;
    }
    if (n > MAX_PARAMETER_VALUE) {
        ISSUE(PARAMETER_VALUE_OVERFLOW, std::string(name));
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// CacheImpl (pimpl for token cache)
// ---------------------------------------------------------------------------

struct Param::CacheImpl {
    std::unordered_map<std::string, std::vector<Token>> data;
};

Param::Param() : cache_(std::make_unique<CacheImpl>()) {}

Param::~Param() = default;
Param::Param(Param&&) noexcept = default;
Param& Param::operator=(Param&&) noexcept = default;

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

int Param::get_max_include_depth() const { return max_include_depth_; }

bool Param::set_max_include_depth(int depth) {
    if (max_include_depth_fixed_)
        return true; // no-op when locked
    if (!validate_parameter(depth, "max_include_depth"))
        return false;
    max_include_depth_ = depth;
    return true;
}

bool Param::fix_max_include_depth(int depth) {
    if (!validate_parameter(depth, "max_include_depth"))
        return false;
    max_include_depth_       = depth;
    max_include_depth_fixed_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// Expansion depth (recursion limit)
// ---------------------------------------------------------------------------

int Param::get_max_expansion_depth() const { return max_expansion_depth_; }

bool Param::set_max_expansion_depth(int depth) {
    if (max_expansion_depth_fixed_)
        return true; // no-op when locked
    if (!validate_parameter(depth, "max_expansion_depth"))
        return false;
    max_expansion_depth_ = depth;
    return true;
}

bool Param::fix_max_expansion_depth(int depth) {
    if (!validate_parameter(depth, "max_expansion_depth"))
        return false;
    max_expansion_depth_       = depth;
    max_expansion_depth_fixed_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// Conditional nesting limit
// ---------------------------------------------------------------------------

bool Param::set_max_if_nesting(int n) {
    if (max_if_nesting_fixed_)
        return true; // no-op when locked
    if (!validate_parameter(n, "max_if_nesting"))
        return false;
    max_if_nesting_ = n;
    return true;
}

bool Param::fix_max_if_nesting(int n) {
    if (!validate_parameter(n, "max_if_nesting"))
        return false;
    max_if_nesting_       = n;
    max_if_nesting_fixed_ = true;
    return true;
}

void Param::set_pragma_style(std::string style) {
    if (pragma_style_fixed_)
        return;
    pragma_style_ = std::move(style);
}

void Param::fix_pragma_style(std::string style) {
    pragma_style_       = std::move(style);
    pragma_style_fixed_ = true;
}

bool Param::is_standard_pragma_style() const {
    return pragma_style_ == VAL_PRAGMA_STANDARD;
}

void Param::set_remove_comments(bool b) {
    if (remove_comments_fixed_)
        return;
    remove_comments_ = b;
}

void Param::fix_remove_comments(bool b) {
    remove_comments_       = b;
    remove_comments_fixed_ = true;
}

// ---------------------------------------------------------------------------
// Token cache
// ---------------------------------------------------------------------------

const std::vector<Token>* Param::get_cache(const std::string& key) const {
    auto it = cache_->data.find(key);
    if (it != cache_->data.end())
        return &it->second;
    return nullptr;
}

void Param::set_cache(std::string key, std::vector<Token> tokens) {
    cache_->data.insert_or_assign(std::move(key), std::move(tokens));
}
