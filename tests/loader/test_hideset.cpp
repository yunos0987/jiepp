#include "test_helper.hpp"
#include "loader/hideset.hpp"

#include <random>
#include <set>
#include <vector>

// task_slug hideset-linear: HideSet is a persistent bitmap-trie
// representation of Prosser's hide set (see design-hideset.md section 4).
// These are pure data-structure tests; tests/core/test_hideset_chain.cpp
// covers the integration with the preprocessor.

namespace {

// T-HS1 adds raw numeric ids directly (HideSet::with(Id)), never through the
// name-interning API, so the "full contents" check below cannot use
// names()/find_id() (which only know about interned names). Instead it
// checks containment of every id in [0, range) against the reference set --
// equivalent to extracting the full contents, since both sides are bounded
// to that range.
bool full_contents_match(const HideSet& h, const std::set<HideSet::Id>& ref, HideSet::Id range) {
    for (HideSet::Id id = 0; id < range; ++id) {
        if (h.contains(id) != (ref.count(id) != 0))
            return false;
    }
    return true;
}

} // namespace

class HideSetTest : public JieppTest {};

// ---- T-HS1: differential vs std::set<Id> ----
//
// Fixed seed (std::mt19937 12345), id ranges 100 / 5000 / 300000, 32 sets,
// >= 100000 random with/unite/intersect/contains/clear steps per range.
// Emptiness is checked every step; full contents (via names()/find_id)
// every 997 steps and at the end.

TEST_F(HideSetTest, T_HS1_DifferentialVsStdSet) {
    std::mt19937 rng(12345);
    const int kNumSets = 32;
    const int kSteps = 100000;
    const HideSet::Id ranges[] = {100, 5000, 300000};

    for (HideSet::Id range : ranges) {
        std::vector<HideSet> hs(kNumSets);
        std::vector<std::set<HideSet::Id>> ref(kNumSets);

        for (int step = 0; step < kSteps; ++step) {
            int i = static_cast<int>(rng() % kNumSets);
            int j = static_cast<int>(rng() % kNumSets);
            int k = static_cast<int>(rng() % kNumSets);
            int op = static_cast<int>(rng() % 6);

            if (op <= 1) {
                HideSet::Id id = rng() % range;
                hs[k] = hs[i].with(id);
                auto s = ref[i];
                s.insert(id);
                ref[k] = std::move(s);
            } else if (op == 2) {
                hs[k] = HideSet::unite(hs[i], hs[j]);
                auto s = ref[i];
                s.insert(ref[j].begin(), ref[j].end());
                ref[k] = std::move(s);
            } else if (op == 3) {
                hs[k] = HideSet::intersect(hs[i], hs[j]);
                std::set<HideSet::Id> s;
                for (auto x : ref[i])
                    if (ref[j].count(x))
                        s.insert(x);
                ref[k] = std::move(s);
            } else if (op == 4) {
                hs[k] = HideSet();
                ref[k].clear();
            } else {
                HideSet::Id id = rng() % range;
                ASSERT_EQ(hs[i].contains(id), ref[i].count(id) != 0)
                    << "range=" << range << " step=" << step;
            }

            ASSERT_EQ(hs[k].empty(), ref[k].empty())
                << "range=" << range << " step=" << step;
            if (step % 997 == 0) {
                ASSERT_TRUE(full_contents_match(hs[k], ref[k], range))
                    << "range=" << range << " step=" << step;
            }
        }

        for (int k = 0; k < kNumSets; ++k) {
            EXPECT_TRUE(full_contents_match(hs[k], ref[k], range))
                << "range=" << range << " k=" << k;
        }
    }
}

// ---- T-HS2: boundaries ----
//
// ids at every height boundary (0, 63, 64, 1023, 1024, 16383, 16384,
// 262143, 262144, 2^32-1); union/intersection of a high-id set with a
// low-id set in both orders; an intersect result that collapses to a
// lower root height, then with()/contains() on it.

TEST_F(HideSetTest, T_HS2_Boundaries) {
    const HideSet::Id ids[] = {
        0, 63, 64, 1023, 1024, 16383, 16384, 262143, 262144,
        static_cast<HideSet::Id>(0xFFFFFFFFu)};

    // Every boundary id can be added, and contains() agrees for all others.
    for (std::size_t n = 0; n < std::size(ids); ++n) {
        HideSet h;
        std::set<HideSet::Id> ref;
        for (std::size_t m = 0; m <= n; ++m) {
            h = h.with(ids[m]);
            ref.insert(ids[m]);
        }
        for (std::size_t m = 0; m < std::size(ids); ++m) {
            EXPECT_EQ(h.contains(ids[m]), ref.count(ids[m]) != 0)
                << "n=" << n << " id=" << ids[m];
        }
    }

    // Union/intersection of a high-id set with a low-id set, both orders.
    HideSet lo = HideSet().with(static_cast<HideSet::Id>(5));
    HideSet hi = HideSet().with(static_cast<HideSet::Id>(0xFFFFFFFFu));

    HideSet u1 = HideSet::unite(lo, hi);
    HideSet u2 = HideSet::unite(hi, lo);
    EXPECT_TRUE(u1.contains(5u));
    EXPECT_TRUE(u1.contains(0xFFFFFFFFu));
    EXPECT_TRUE(u2.contains(5u));
    EXPECT_TRUE(u2.contains(0xFFFFFFFFu));

    HideSet i1 = HideSet::intersect(lo, hi);
    HideSet i2 = HideSet::intersect(hi, lo);
    EXPECT_TRUE(i1.empty());
    EXPECT_TRUE(i2.empty());

    // An intersect result that collapses to a lower root height, then
    // with()/contains() on it.
    HideSet wide = HideSet().with(static_cast<HideSet::Id>(5))
                             .with(static_cast<HideSet::Id>(0xFFFFFFFFu));
    HideSet narrow = HideSet().with(static_cast<HideSet::Id>(5))
                               .with(static_cast<HideSet::Id>(9));
    HideSet collapsed = HideSet::intersect(wide, narrow);
    EXPECT_TRUE(collapsed.contains(5u));
    EXPECT_FALSE(collapsed.contains(9u));
    EXPECT_FALSE(collapsed.contains(0xFFFFFFFFu));
    HideSet collapsed2 = collapsed.with(static_cast<HideSet::Id>(7));
    EXPECT_TRUE(collapsed2.contains(5u));
    EXPECT_TRUE(collapsed2.contains(7u));
    EXPECT_FALSE(collapsed2.contains(0xFFFFFFFFu));
}

// ---- T-HS3: aliasing (pointer-identity shortcuts) ----

TEST_F(HideSetTest, T_HS3_Aliasing) {
    HideSet a = HideSet().with("M").with("N").with("O");
    HideSet subset = HideSet().with("M").with("N");

    EXPECT_TRUE(HideSet::unite(a, subset).same(a));
    EXPECT_TRUE(HideSet::unite(subset, a).same(a));

    HideSet superset = a.with("P");
    EXPECT_TRUE(HideSet::intersect(a, superset).same(a));

    EXPECT_TRUE(a.with("M").same(a));

    EXPECT_TRUE(HideSet::unite(a, HideSet()).same(a));
    EXPECT_TRUE(HideSet::intersect(a, a).same(a));
}

// ---- T-HS4: names() / interning ----

TEST_F(HideSetTest, T_HS4_NamesAndInterning) {
    HideSet h = HideSet().with("M").with("N");
    EXPECT_EQ((std::vector<std::string>{"M", "N"}), h.names());

    EXPECT_FALSE(h.contains("never_interned_xyz"));
    HideSet::Id ignored;
    EXPECT_FALSE(HideSet::find_id("never_interned_xyz", ignored));

    // Case-sensitive.
    HideSet abc = HideSet().with("abc");
    EXPECT_FALSE(abc.contains("ABC"));
}
