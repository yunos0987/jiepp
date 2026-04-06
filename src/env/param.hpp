#pragma once
#include "param_constants.hpp"
#include <memory>
#include <string>
#include <vector>

struct Token; // forward declaration (defined in loader/token.hpp)

class Param {
public:
    Param();
    virtual ~Param();
    Param(Param&&) noexcept;
    Param& operator=(Param&&) noexcept;
    Param(const Param&) = delete;
    Param& operator=(const Param&) = delete;

    // ---- Max include depth ----
    int  get_max_include_depth() const;
    bool set_max_include_depth(int depth);
    bool fix_max_include_depth(int depth); // set and lock

    // ---- Expansion depth (recursion limit) ----
    int  expansion_depth() const { return expansion_depth_; }
    void inc_expansion_depth() { ++expansion_depth_; }
    void dec_expansion_depth() { if (expansion_depth_ > 0) --expansion_depth_; }
    int  get_max_expansion_depth() const;
    bool set_max_expansion_depth(int depth);
    bool fix_max_expansion_depth(int depth);

    // ---- Conditional nesting limit ----
    int  get_max_if_nesting() const { return max_if_nesting_; }
    bool set_max_if_nesting(int n);
    bool fix_max_if_nesting(int n); // set and lock

    // ---- Pragma style ----
    std::string get_pragma_style() const { return pragma_style_; }
    void        set_pragma_style(std::string style);
    void        fix_pragma_style(std::string style);
    bool        is_standard_pragma_style() const;

    // ---- Comment removal ----
    bool get_remove_comments() const { return remove_comments_; }
    void set_remove_comments(bool b);
    void fix_remove_comments(bool b);

    // ---- Token cache (filepath -> tokens) ----
    const std::vector<Token>* get_cache(const std::string& key) const;
    void set_cache(std::string key, std::vector<Token> tokens);

private:
    int         max_include_depth_       = DEFAULT_MAX_INCLUDE_DEPTH;
    bool        max_include_depth_fixed_ = false;

    int         expansion_depth_         = 0;
    int         max_expansion_depth_     = DEFAULT_MAX_EXPANSION_DEPTH;
    bool        max_expansion_depth_fixed_ = false;

    int         max_if_nesting_          = DEFAULT_MAX_IF_NESTING;
    bool        max_if_nesting_fixed_    = false;

    std::string pragma_style_            = VAL_PRAGMA_ANNOTATED;
    bool        pragma_style_fixed_      = false;

    bool        remove_comments_         = false;
    bool        remove_comments_fixed_   = false;

    struct CacheImpl;
    std::unique_ptr<CacheImpl> cache_;
};
