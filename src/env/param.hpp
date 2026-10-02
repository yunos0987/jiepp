#pragma once
#include "param_constants.hpp"
#include <cstdint>
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

    // ---- Expansion work budget (steps; 0 = no limit) ----
    // The count is per preprocess()/preprocess_text() call (reset on entry);
    // jiepp_command() uses a fresh Env, so -include files and the main input
    // share one budget. There is deliberately no in-source directive: input
    // must not be able to raise its own limit.
    void          set_max_expansion_steps(std::uint64_t n) {
        max_expansion_steps_ = n;
        step_limit_          = (n == 0) ? UINT64_MAX : n;
    }
    std::uint64_t get_max_expansion_steps() const { return max_expansion_steps_; }
    std::uint64_t expansion_steps() const { return expansion_steps_; }
    void          reset_expansion_steps() { expansion_steps_ = 0; }
    // Hot path: one add and one compare against the precomputed limit (0 is
    // mapped to UINT64_MAX, so "no limit" needs no extra branch). The count
    // stays above the limit after a hit, so a swallowed exception cannot let
    // expansion carry on for free.
    void charge_expansion_steps(std::uint64_t n) {
        expansion_steps_ += n;
        if (expansion_steps_ > step_limit_) [[unlikely]]
            raise_expansion_steps_exceeded();
    }

    // ---- Conditional nesting limit ----
    int  get_max_if_nesting() const { return max_if_nesting_; }
    bool set_max_if_nesting(int n);
    bool fix_max_if_nesting(int n); // set and lock

    // ---- Max consecutive blank lines before compaction (0 = disabled) ----
    int  get_max_blank_lines() const;
    bool set_max_blank_lines(int n);
    bool fix_max_blank_lines(int n); // set and lock

    // ---- Pragma style ----
    std::string get_pragma_style() const { return pragma_style_; }
    void        set_pragma_style(std::string style);
    void        fix_pragma_style(std::string style);
    bool        is_standard_pragma_style() const;

    // ---- Comment removal ----
    bool get_remove_comments() const { return remove_comments_; }
    void set_remove_comments(bool b);
    void fix_remove_comments(bool b);

    // ---- -dD mode: emit {#define}/{#undef} lines inline ----
    bool is_dd_mode() const { return dd_mode_; }
    void set_dd_mode(bool b) { dd_mode_ = b; }

    // ---- Token cache (filepath -> tokens) ----
    // Returns a shared, immutable view of the cached RAW tokens (comment
    // removal is applied by the caller, not baked into the cache) so a cache
    // hit avoids a deep copy of the file's token vector.
    std::shared_ptr<const std::vector<Token>> get_cache(const std::string& key) const;
    void set_cache(std::string key, std::vector<Token> tokens);

private:
    [[noreturn]] void raise_expansion_steps_exceeded() const;

    std::uint64_t expansion_steps_       = 0;
    std::uint64_t max_expansion_steps_   = DEFAULT_MAX_EXPANSION_STEPS;
    std::uint64_t step_limit_            = DEFAULT_MAX_EXPANSION_STEPS;

    int         max_include_depth_      = DEFAULT_MAX_INCLUDE_DEPTH;
    bool        max_include_depth_fixed_ = false;

    int         expansion_depth_         = 0;
    int         max_expansion_depth_     = DEFAULT_MAX_EXPANSION_DEPTH;
    bool        max_expansion_depth_fixed_ = false;

    int         max_if_nesting_          = DEFAULT_MAX_IF_NESTING;
    bool        max_if_nesting_fixed_    = false;

    int         max_blank_lines_         = DEFAULT_MAX_BLANK_LINES;
    bool        max_blank_lines_fixed_   = false;

    std::string pragma_style_            = VAL_PRAGMA_ANNOTATED;
    bool        pragma_style_fixed_      = false;

    bool        remove_comments_         = false;
    bool        remove_comments_fixed_   = false;

    bool        dd_mode_                 = false;

    struct CacheImpl;
    std::unique_ptr<CacheImpl> cache_;
};
