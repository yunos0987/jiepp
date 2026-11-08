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
    // {#pragma push_macro} / {#pragma pop_macro} (SPEC 3.8). push_macro()
    // saves the current definition of `name` (or "undefined") on that
    // name's own stack without changing it. pop_macro() takes the top of the
    // stack and makes it the definition again -- the very same Macro object,
    // not a copy, since a Macro may carry state (__COUNTER__'s counter) --
    // and returns true; with an empty stack it changes nothing and returns
    // false. A definition it replaces is retired like undef()'s (lifetime
    // invariant above). The caller filters out names that {#define}/{#undef}
    // reject ('defined').
    void push_macro(std::string_view name);
    bool pop_macro(std::string_view name);
    // Returns (name, Macro*) pairs in insertion order, skipping undefined entries.
    std::vector<std::pair<std::string, Macro*>> symbols() const;

private:
    // Ownership is shared (not unique) only so that a push_macro() stack
    // entry can keep a definition alive and pop_macro() can put that same
    // object back into its SymEntry.
    struct SymEntry {
        std::string            name;
        std::shared_ptr<Macro> macro; // nullptr means the entry was undef'd
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
    // Macros superseded by undef(), by define()'s replace-in-place branch or
    // by pop_macro() are moved here instead of being destroyed, to satisfy
    // the lifetime invariant documented above. Never read back; exists
    // purely to extend ownership to the Symtab's own lifetime (F14). (A
    // definition popped back from push_stacks_ may also still be listed
    // here; that only adds a reference.)
    // This vector only ever grows -- once per undef/redefine/pop -- for as
    // long as this Symtab (i.e. one preprocessing run) lives; that growth is
    // intentional and bounded by the run's own directive count, not an
    // unbounded leak, so do not "fix" it by trying to shrink or reuse it.
    std::vector<std::shared_ptr<Macro>> retired_;
    // Per-name {#pragma push_macro} stacks; a nullptr element saves
    // "undefined".
    std::unordered_map<std::string, std::vector<std::shared_ptr<Macro>>, StringHash,
                       std::equal_to<>>
        push_stacks_;
};
