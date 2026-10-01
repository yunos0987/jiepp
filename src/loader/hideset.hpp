#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Prosser hide set (cpp.algo.md's HS): the set of macro names currently
// "being expanded" that must not be expanded again, carried per-token.
//
// Representation: an immutable, structurally shared bitmap radix trie over
// interned macro-name ids (task_slug hideset-linear). The previous
// representation (a sorted string set, deep-copied per level) made an
// n-level macro-expansion chain O(n^2); this handle is one pointer (8
// bytes), copying is an atomic increment, and with()/unite()/intersect()
// are O(h) (h = trie height, typically <= 3) instead of O(set size).
//
// Value semantics: default-constructed is the empty set; copy/move/assign
// behave like a reference-counted handle (no deep copy). `same()` is a
// pointer-identity shortcut only, never a semantic equality test -- two
// handles can denote equal sets without being `same()`.
//
// The name<->id table (intern/find_id/name_of) is process-global, never
// shrinks, and is NOT thread-safe, like Issue's process-global state (see
// ARCHITECTURE.md, loader/ section): a library host driving concurrent
// preprocessing from multiple threads was already unsupported before this
// change. Ids are global (not per Env/Symtab) so a token carrying a hide
// set stays correct across different Env instances in the same process.
class HideSet {
public:
    using Id = std::uint32_t;

    HideSet() noexcept = default;
    HideSet(const HideSet& o) noexcept;
    HideSet(HideSet&& o) noexcept;
    HideSet& operator=(const HideSet& o) noexcept;
    HideSet& operator=(HideSet&& o) noexcept;
    ~HideSet();

    bool empty() const noexcept { return n_ == nullptr; }
    // Pointer-identity shortcut only (e.g. the hsadd() memo key); NOT a
    // semantic equality test. Do not use operator== in its place -- there
    // is deliberately no operator== on this type.
    bool same(const HideSet& o) const noexcept { return n_ == o.n_; }

    bool contains(Id id) const noexcept;
    // Find-only lookup: a name that was never interned (never added to any
    // hide set) is in no hide set, so this never calls intern() and never
    // grows the name table.
    bool contains(std::string_view name) const;

    // *this u {id} / *this u {name} (interns name if needed). Returns
    // *this (no allocation) when the id is already present.
    HideSet with(Id id) const;
    HideSet with(std::string_view name) const;

    static HideSet unite(const HideSet& a, const HideSet& b);
    static HideSet intersect(const HideSet& a, const HideSet& b);

    std::size_t size() const;                 // tests/debug only
    std::vector<std::string> names() const;    // sorted; tests/debug only

    // ---- Process-global name table ----
    static Id intern(std::string_view name);
    static bool find_id(std::string_view name, Id& out) noexcept;
    static const std::string& name_of(Id id);

    // Opaque node type (defined in hideset.cpp): intentionally public so the
    // .cpp's free functions can name/define it, but it is an incomplete
    // type here -- there is nothing a caller outside hideset.cpp can do
    // with a `Node*` besides pass it back into this class.
    struct Node;

private:
    Node* n_ = nullptr;
    explicit HideSet(Node* adopted) noexcept : n_(adopted) {}
};
