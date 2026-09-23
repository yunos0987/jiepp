#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class Macro; // forward declaration

class Symtab {
public:
    Symtab() = default;
    virtual ~Symtab() = default;
    Symtab(Symtab&&) noexcept = default;
    Symtab& operator=(Symtab&&) noexcept = default;

    // Lifetime invariant: a Macro* returned by lookup() remains valid for
    // the rest of the Symtab's lifetime, even after that name is undef'd or
    // redefined. Both undef() and define()'s replace-in-place branch move
    // the superseded Macro into retired_ instead of destroying it, because
    // expand()'s function-macro-call handling keeps a raw FunctionMacro*
    // obtained via lookup() alive across the argument-collection loop, and
    // that loop is permitted to dispatch {#define}/{#undef} on the very
    // same name (F3 classifies both as state-only, hence allowed) before
    // the pointer is used again in subst() (see F14).
    bool define(std::string name, std::unique_ptr<Macro> macro);
    bool exist(std::string_view name) const;
    Macro* lookup(std::string_view name) const;
    // Undefines `name`. The superseded Macro (if any) is retired, not
    // destroyed (see the lifetime invariant above), so this returns void:
    // no caller may take ownership of a Macro that must keep living.
    void undef(std::string_view name);
    // Returns (name, Macro*) pairs in insertion order, skipping undefined entries.
    std::vector<std::pair<std::string, Macro*>> symbols() const;

private:
    struct SymEntry {
        std::string            name;
        std::unique_ptr<Macro> macro; // nullptr means the entry was undef'd
    };
    // Transparent hash so exist()/lookup()/undef() can probe the map with a
    // std::string_view directly, without allocating a temporary std::string.
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };
    std::vector<SymEntry> sym_order_; // insertion order
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>>
        sym_index_; // name -> index
    // Macros superseded by undef() or by define()'s replace-in-place branch
    // are moved here instead of being destroyed, to satisfy the lifetime
    // invariant documented above. Never read back; exists purely to extend
    // ownership to the Symtab's own lifetime (F14).
    // This vector only ever grows -- once per undef/redefine -- for as long
    // as this Symtab (i.e. one preprocessing run) lives; that growth is
    // intentional and bounded by the run's own directive count, not an
    // unbounded leak, so do not "fix" it by trying to shrink or reuse it.
    std::vector<std::unique_ptr<Macro>> retired_;
};
