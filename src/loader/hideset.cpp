#include "hideset.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------------
// §Node layout
//
// Leaf (height 0) and Inner (height >= 1) are DIFFERENT allocation sizes: a
// Leaf stores one 64-bit bitmask for ids [base, base+64); an Inner stores 16
// child pointers, each covering a 16x-larger range than its own children.
// Both share a {rc, height} header so the destructor (and every recursive
// walk) can dispatch on `height` alone without a vtable.
// ---------------------------------------------------------------------------

struct HideSet::Node {
    std::atomic<std::uint32_t> rc{1};
    std::uint8_t height;
    explicit Node(std::uint8_t h) noexcept : height(h) {}
};

namespace {

constexpr int kLeafBits = 6;              // a Leaf covers 2^6 = 64 ids
constexpr int kFanBits  = 4;              // an Inner has 2^4 = 16 children
constexpr int kFan      = 1 << kFanBits;
constexpr int kFanMask  = kFan - 1;

using Node = HideSet::Node;
using Id   = HideSet::Id;

struct Leaf : Node {
    std::uint64_t bits;
    Leaf(std::uint64_t b) noexcept : Node(0), bits(b) {}
};

struct Inner : Node {
    Node* child[kFan];
    explicit Inner(std::uint8_t h) noexcept : Node(h) {
        for (auto& c : child) c = nullptr;
    }
};

inline Leaf* as_leaf(Node* n) noexcept { return static_cast<Leaf*>(n); }
inline const Leaf* as_leaf(const Node* n) noexcept { return static_cast<const Leaf*>(n); }
inline Inner* as_inner(Node* n) noexcept { return static_cast<Inner*>(n); }
inline const Inner* as_inner(const Node* n) noexcept { return static_cast<const Inner*>(n); }

// Shift to apply to an id to get the child index at height h (h >= 1).
inline int shift_of(int h) noexcept { return kLeafBits + kFanBits * (h - 1); }
// Number of ids covered by a node of height h: 2^(kLeafBits + kFanBits*h).
inline std::uint64_t capacity_of(int h) noexcept {
    return std::uint64_t{1} << (kLeafBits + kFanBits * h);
}

void retain(Node* n) noexcept {
    if (n) n->rc.fetch_add(1, std::memory_order_relaxed);
}

void release(Node* n) noexcept {
    if (!n) return;
    if (n->rc.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
    if (n->height == 0) {
        delete as_leaf(n);
    } else {
        Inner* in = as_inner(n);
        for (Node* c : in->child) release(c);
        delete in;
    }
}

Node* make_leaf(std::uint64_t bits) { return new Leaf(bits); }
Node* make_inner(int h) { return new Inner(static_cast<std::uint8_t>(h)); }

Inner* copy_inner(const Node* s) {
    const Inner* src = as_inner(s);
    Inner* n = as_inner(make_inner(src->height));
    for (int i = 0; i < kFan; ++i) {
        n->child[i] = src->child[i];
        retain(n->child[i]);
    }
    return n;
}

// Returns a new reference to `b` wrapped in child[0]-only Inner nodes up to
// height h (b->height <= h required).
Node* lift(Node* b, int h) {
    retain(b);
    for (int k = b->height + 1; k <= h; ++k) {
        Inner* up = as_inner(make_inner(k));
        up->child[0] = b;
        b = up;
    }
    return b;
}

// Inserts `id` into the (possibly null) subtree `n` of height `h`,
// path-copying every node on the way down. Consumes one reference to `n`
// (as if the caller had passed ownership) and returns a new owning
// reference to the resulting subtree.
Node* insert(Node* n, int h, Id id) {
    if (h == 0) {
        std::uint64_t bits = (n ? as_leaf(n)->bits : 0) | (std::uint64_t{1} << (id & 63));
        release(n);
        return make_leaf(bits);
    }
    Inner* m = n ? copy_inner(n) : as_inner(make_inner(h));
    release(n);
    int i = static_cast<int>((id >> shift_of(h)) & kFanMask);
    m->child[i] = insert(m->child[i], h - 1, id);
    return m;
}

Node* unite_(Node* a, Node* b) {
    if (a == b || !b) { retain(a); return a; }
    if (!a) { retain(b); return b; }
    if (a->height < b->height) std::swap(a, b);
    if (a->height > b->height) {
        // b is shorter: recurse into a's child[0] when present, otherwise
        // lift b itself up to height a->height-1 before storing it there --
        // a node of the wrong height in a child slot would corrupt the
        // trie's height invariant (prototype bug 1).
        Inner* ai = as_inner(a);
        Node* c0 = ai->child[0] ? unite_(ai->child[0], b) : lift(b, a->height - 1);
        if (c0 == ai->child[0]) { release(c0); retain(a); return a; }
        Inner* m = copy_inner(a);
        release(m->child[0]);
        m->child[0] = c0;
        return m;
    }
    // Equal height.
    if (a->height == 0) {
        std::uint64_t u = as_leaf(a)->bits | as_leaf(b)->bits;
        if (u == as_leaf(a)->bits) { retain(a); return a; }
        if (u == as_leaf(b)->bits) { retain(b); return b; }
        return make_leaf(u);
    }
    const Inner* ai = as_inner(a);
    const Inner* bi = as_inner(b);
    Node* c[kFan];
    bool eq_a = true, eq_b = true;
    for (int i = 0; i < kFan; ++i) {
        c[i] = unite_(ai->child[i], bi->child[i]);
        eq_a = eq_a && c[i] == ai->child[i];
        eq_b = eq_b && c[i] == bi->child[i];
    }
    if (eq_a || eq_b) {
        for (Node* x : c) release(x);
        Node* r = eq_a ? a : b;
        retain(r);
        return r;
    }
    Inner* m = as_inner(make_inner(a->height));
    for (int i = 0; i < kFan; ++i) m->child[i] = c[i];
    return m;
}

Node* isect_(Node* a, Node* b) {
    if (!a || !b) return nullptr;
    // Descend the taller operand through child[0] to equal height; never
    // normalise here (a wrong-height child would be left behind -- that is
    // only safe at the root, after recursion returns, see intersect()).
    while (a->height > b->height) {
        a = as_inner(a)->child[0];
        if (!a) return nullptr;
    }
    while (b->height > a->height) {
        b = as_inner(b)->child[0];
        if (!b) return nullptr;
    }
    if (a == b) { retain(a); return a; }
    if (a->height == 0) {
        std::uint64_t x = as_leaf(a)->bits & as_leaf(b)->bits;
        if (!x) return nullptr;
        if (x == as_leaf(a)->bits) { retain(a); return a; }
        if (x == as_leaf(b)->bits) { retain(b); return b; }
        return make_leaf(x);
    }
    const Inner* ai = as_inner(a);
    const Inner* bi = as_inner(b);
    Node* c[kFan];
    bool eq_a = true, eq_b = true, any = false;
    for (int i = 0; i < kFan; ++i) {
        c[i] = isect_(ai->child[i], bi->child[i]);
        eq_a = eq_a && c[i] == ai->child[i];
        eq_b = eq_b && c[i] == bi->child[i];
        any = any || c[i] != nullptr;
    }
    if (!any) return nullptr;
    if (eq_a || eq_b) {
        for (Node* x : c) release(x);
        Node* r = eq_a ? a : b;
        retain(r);
        return r;
    }
    Inner* m = as_inner(make_inner(a->height));
    for (int i = 0; i < kFan; ++i) m->child[i] = c[i];
    return m;
}

template <class F>
void walk(const Node* n, Id base, F&& f) {
    if (!n) return;
    if (n->height == 0) {
        const Leaf* l = as_leaf(n);
        for (int i = 0; i < 64; ++i)
            if ((l->bits >> i) & 1u)
                f(base + static_cast<Id>(i));
        return;
    }
    const Inner* in = as_inner(n);
    for (int i = 0; i < kFan; ++i)
        walk(in->child[i], base + (Id(i) << shift_of(n->height)), f);
}

// ---------------------------------------------------------------------------
// §Name table (process-global; see hideset.hpp for the thread-safety note)
// ---------------------------------------------------------------------------

std::deque<std::string>& name_table() {
    static std::deque<std::string> t;
    return t;
}

std::unordered_map<std::string_view, Id>& name_index() {
    static std::unordered_map<std::string_view, Id> idx;
    return idx;
}

} // namespace

// ---------------------------------------------------------------------------
// §HideSet member functions
// ---------------------------------------------------------------------------

HideSet::HideSet(const HideSet& o) noexcept : n_(o.n_) { retain(n_); }
HideSet::HideSet(HideSet&& o) noexcept : n_(o.n_) { o.n_ = nullptr; }

HideSet& HideSet::operator=(const HideSet& o) noexcept {
    retain(o.n_);
    release(n_);
    n_ = o.n_;
    return *this;
}

HideSet& HideSet::operator=(HideSet&& o) noexcept {
    if (this != &o) {
        release(n_);
        n_ = o.n_;
        o.n_ = nullptr;
    }
    return *this;
}

HideSet::~HideSet() { release(n_); }

bool HideSet::contains(Id id) const noexcept {
    const Node* n = n_;
    if (!n || id >= capacity_of(n->height)) return false;
    for (int h = n->height; h > 0; --h) {
        n = as_inner(n)->child[(id >> shift_of(h)) & kFanMask];
        if (!n) return false;
    }
    return (as_leaf(n)->bits >> (id & 63)) & 1u;
}

bool HideSet::contains(std::string_view name) const {
    Id id;
    if (!find_id(name, id)) return false; // never interned -> in no hide set
    return contains(id);
}

HideSet HideSet::with(Id id) const {
    if (contains(id)) return *this;
    Node* root = n_;
    retain(root);
    int h = root ? root->height : 0;
    while (id >= capacity_of(h)) {
        if (root) {
            Inner* up = as_inner(make_inner(h + 1));
            up->child[0] = root;
            root = up;
        }
        ++h;
    }
    return HideSet(insert(root, h, id));
}

HideSet HideSet::with(std::string_view name) const {
    return with(intern(name));
}

HideSet HideSet::unite(const HideSet& a, const HideSet& b) {
    return HideSet(unite_(a.n_, b.n_));
}

HideSet HideSet::intersect(const HideSet& a, const HideSet& b) {
    Node* r = isect_(a.n_, b.n_);
    // Root-only normalisation to the minimal height: an Inner with only
    // child[0] populated is replaced by that child, repeatedly. Doing this
    // inside the recursion would leave a wrong-height child in some other
    // node's slot (prototype bug 2); it is only safe once, at the root.
    while (r && r->height > 0) {
        const Inner* in = as_inner(r);
        bool only0 = true;
        for (int i = 1; i < kFan; ++i) only0 = only0 && in->child[i] == nullptr;
        if (!only0) break;
        Node* c0 = in->child[0];
        retain(c0);
        release(r);
        r = c0;
    }
    return HideSet(r);
}

std::size_t HideSet::size() const {
    std::size_t n = 0;
    walk(n_, 0, [&](Id) { ++n; });
    return n;
}

std::vector<std::string> HideSet::names() const {
    std::vector<std::string> result;
    walk(n_, 0, [&](Id id) { result.push_back(name_of(id)); });
    std::sort(result.begin(), result.end());
    return result;
}

HideSet::Id HideSet::intern(std::string_view name) {
    Id found;
    if (find_id(name, found)) return found;
    auto& table = name_table();
    auto& index = name_index();
    table.emplace_back(name);
    Id id = static_cast<Id>(table.size() - 1);
    // The view keys into the deque's stable storage (deque never invalidates
    // references/addresses to existing elements on push_back/emplace_back).
    index.emplace(std::string_view(table.back()), id);
    return id;
}

bool HideSet::find_id(std::string_view name, Id& out) noexcept {
    auto& index = name_index();
    auto it = index.find(name);
    if (it == index.end()) return false;
    out = it->second;
    return true;
}

const std::string& HideSet::name_of(Id id) {
    return name_table()[id];
}
