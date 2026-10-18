#include "test_helper.hpp"
#include "core/expand_helpers.hpp"
#include "util/stack_guard.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace {

// noinline + not_tail_called: without both, an optimizing (Release) build
// can turn this self-recursion into a loop that reuses one stack frame per
// level, defeating the point of DetectsWithinBudget below (each level must
// actually consume its own 4 KiB of stack).
[[gnu::noinline, clang::not_tail_called]]
int stack_guard_recurse(int depth, int cap) {
    // Every element (not just buf[0]) is written through a volatile access,
    // so the whole 4 KiB has to be materialized on the stack -- an
    // optimizer is otherwise free to shrink an array down to only the
    // offsets it can prove are read back.
    volatile char buf[4096];
    for (std::size_t i = 0; i < sizeof(buf); ++i)
        buf[i] = static_cast<char>(depth + static_cast<int>(i));
    if (Util::stack_nearly_exhausted())
        return depth;
    if (depth >= cap)
        return -1;
    return stack_guard_recurse(depth + 1, cap);
}

} // namespace

// Tests for the PP63 stack-exhaustion guard (note_stack_base()/
// stack_nearly_exhausted()). Every test that records a base restores it in
// TearDown, so a failure partway through a test cannot leave thread_local
// state behind for a later test in the same binary.
class StackGuardTest : public JieppTest {
protected:
    void TearDown() override {
        Util::reset_stack_base();
        JieppTest::TearDown();
    }
};

TEST_F(StackGuardTest, NoBaseNeverExhausted) {
    EXPECT_EQ(0u, Util::stack_budget());
    EXPECT_FALSE(Util::stack_nearly_exhausted());
}

TEST_F(StackGuardTest, DetectsWithinBudget) {
    Util::note_stack_base(Util::STACK_HEADROOM + (64u << 10));  // 64 KiB above headroom

    // 4 KiB of stack per level; the budget above the headroom is exhausted
    // well before 100 levels.
    int depth_at_exhaustion = stack_guard_recurse(0, 1000);
    EXPECT_GE(depth_at_exhaustion, 0) << "never detected exhaustion within 1000 levels";
    EXPECT_LE(depth_at_exhaustion, 100);
}

TEST_F(StackGuardTest, ZeroBudgetDisables) {
    Util::note_stack_base(0);
    EXPECT_EQ(0u, Util::stack_budget());
    EXPECT_FALSE(Util::stack_nearly_exhausted());
}

// The guard is checked before the expansion depth counter moves and before
// PP60, so a tiny stack budget reports PP63 (not PP60), and the depth
// counter is left untouched (the throw happens before inc_expansion_depth()).
TEST_F(StackGuardTest, ExpandThrowsPP63BeforePP60AndKeepsDepth) {
    Env env = setup();
    env.set_max_expansion_depth(1);
    Util::note_stack_base(Util::STACK_HEADROOM);  // no room at all above the headroom

    EXPECT_THROW(pp("{#define I(a) (a)}\nI(I(0))", env), Issue::Exception);
    EXPECT_EQ(Issue::Code::STACK_EXHAUSTED, code());
    EXPECT_EQ(0, env.expansion_depth());
}

// Library-mode callers (no note_stack_base() call, e.g. preprocess() used
// directly by tests) never trigger the guard, regardless of nesting depth.
TEST_F(StackGuardTest, LibraryModeUnaffected) {
    constexpr int kDepth = 100;
    std::string src = "{#define I(a) (a)+1}\n";
    for (int i = 0; i < kDepth; ++i)
        src += "I(";
    src += "0";
    for (int i = 0; i < kDepth; ++i)
        src += ")";
    src += ";";

    EXPECT_NO_THROW(pp(src));
}

// ---- F2: nested __VA_OPT__ recursion (subst() -> subst()) ----
//
// subst() substitutes __VA_OPT__ content by calling itself through
// subst_va_opt_content(), without passing through expand() and so without
// ExpansionDepthGuard's PP63 check. Nesting __VA_OPT__ (PP37 ignored or in
// continue mode, also in sandbox builds) used to overflow the stack. The
// fixture name matches the sandbox test presets' filter
// ^(Sandbox|RobustnessTest), so these also run in sandbox builds.
class RobustnessTestStackGuard : public StackGuardTest {};

// Each of the three callers of subst_va_opt_content() (plain __VA_OPT__,
// `@@ __VA_OPT__`, `@__VA_OPT__`) must check the stack before recursing.
// With the base recorded at exactly the headroom, the first check fires, so
// calling subst() directly shows PP63 without a deep (crashing) recursion,
// and expand()'s own guard is never on the path. Before the fix subst()
// returned normally here.
TEST_F(RobustnessTestStackGuard, VaOptContentRecursionReportsPP63) {
    const char* const bodies[] = {
        "__VA_OPT__(x)",
        "a @@ __VA_OPT__(x)",
        "@__VA_OPT__(x)",
    };
    for (const char* body : bodies) {
        SCOPED_TRACE(body);
        Env env = setup();
        pp(std::string("{#define M(...) ") + body + "}", env);
        ASSERT_TRUE(empty());
        const auto* fm = dynamic_cast<const FunctionMacro*>(env.lookup("M"));
        ASSERT_NE(nullptr, fm);
        const std::vector<std::vector<Token>> actuals{{Token::create(Token::ANY, "1")}};

        Util::note_stack_base(Util::STACK_HEADROOM);  // no room above the headroom
        EXPECT_THROW(jiepp::expand_detail::subst(fm->body(), fm->va_opt_spans(), fm->args(),
                                                 actuals, nullptr, {}, env),
                     Issue::Exception);
        Util::reset_stack_base();

        const auto ms = messages();
        EXPECT_EQ(1u, ms.size());
        if (ms.size() != 1)
            continue;  // still check the remaining bodies
        EXPECT_EQ(Issue::Code::STACK_EXHAUSTED, PlainTextMessage::parse_code(ms[0]));
        // Same wording as expand()'s PP63 (one shared reporting function).
        EXPECT_THAT(ms[0], ::testing::HasSubstr(
                               "PP63: Stack nearly exhausted; 'expansion depth 0 with a 256 KiB "
                               "stack; retry with --recursion-limit greater than 32'"));
        EXPECT_EQ(0, env.expansion_depth());
    }
}

// Regression watchpoints around the __VA_OPT__ paths (library mode, no
// stack base): a bracketed argument stays one argument (__VA_ARGC__ tells
// a split apart, which the joined __VA_ARGS__ spelling cannot), and an
// identical redefinition of a variadic macro is silent.
TEST_F(RobustnessTestStackGuard, VaOptWatchpointsUnchanged) {
    EXPECT_EQ("<a[x,y]>", pp("{#define V(...) __VA_OPT__(<__VA_ARGS__>)}V(a[x,y])"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("1", pp("{#define C(...) __VA_ARGC__}C(a[x,y])"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("", pp("{#define V(...) __VA_OPT__(<__VA_ARGS__>)}"
                     "{#define V(...) __VA_OPT__(<__VA_ARGS__>)}"));
    EXPECT_TRUE(empty());
}
