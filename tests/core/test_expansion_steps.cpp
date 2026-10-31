#include "test_helper.hpp"

#include <cstdint>
#include <string>

// =====================================================================
// PP64 (MAX_EXPANSION_STEPS_EXCEEDED): the cap on macro-expansion work.
// See Param::charge_expansion_steps() and SPECIFICATION.md section 11.
// =====================================================================

namespace {

constexpr std::uint64_t kLimit = 10000;

// {#define D0 1} {#define D1 D0 + D0} ... : Dn expands to 2^n tokens.
// Also a valid constant expression, so it can be used inside {#if}.
std::string doubling_defs(int n) {
    std::string s = "{#define D0 1}\n";
    for (int i = 1; i <= n; ++i)
        s += "{#define D" + std::to_string(i) + " D" + std::to_string(i - 1) +
             " + D" + std::to_string(i - 1) + "}\n";
    return s;
}

// F0(x) = x; Fi(x) = F(i-1)(BODY(x)) where BODY is built from `x`.
std::string chain_defs(const std::string& prefix, int n, const std::string& arg_expr) {
    std::string s = "{#define " + prefix + "0(x) x}\n";
    for (int i = 1; i <= n; ++i)
        s += "{#define " + prefix + std::to_string(i) + "(x) " + prefix +
             std::to_string(i - 1) + "(" + arg_expr + ")}\n";
    return s;
}

} // namespace

class ExpansionStepsTest : public JieppTest {
protected:
    // Runs `src` with the given limit; expects PP64 and returns the final
    // step count.
    std::uint64_t expect_pp64(const std::string& src, std::uint64_t limit = kLimit,
                              bool dd_mode = false) {
        Env env = setup();
        env.set_max_expansion_steps(limit);
        env.set_dd_mode(dd_mode);
        EXPECT_THROW(pp(src, env), Issue::Exception);
        EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
        return env.expansion_steps();
    }
};

// ---- C1/C2: code, severity, text, default ----

TEST_F(ExpansionStepsTest, CodeSeverityAndText) {
    EXPECT_TRUE(Issue::is_severe(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED));
    EXPECT_EQ("MAX_EXPANSION_STEPS_EXCEEDED",
              Issue::codename(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED));
    EXPECT_EQ(64, static_cast<int>(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED));
}

TEST_F(ExpansionStepsTest, DefaultIs2To24) {
    Env env;
    EXPECT_EQ(16777216u, env.get_max_expansion_steps());
    EXPECT_EQ(0u, env.expansion_steps());
    EXPECT_EQ(16777216u, DEFAULT_MAX_EXPANSION_STEPS);
}

TEST_F(ExpansionStepsTest, MessageNamesLimitAndOption) {
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    EXPECT_THROW(pp(doubling_defs(20) + "D20\n", env), Issue::Exception);
    const std::string m = message();
    EXPECT_NE(std::string::npos, m.find("PP64: Maximum expansion steps exceeded; "
                                        "'limit 10000; use --max-expansion-steps N to raise it "
                                        "(0 = no limit)'")) << m;
}

// ---- C4: boundary ----

TEST_F(ExpansionStepsTest, BoundaryExactAndOneBelow) {
    const std::string src = doubling_defs(8) + "D8\n{#if 1}x{#endif}\n";

    Env probe = setup();
    const std::string expected = pp(src, probe);
    const std::uint64_t s = probe.expansion_steps();
    ASSERT_GT(s, 0u);

    {
        Env env = setup();
        env.set_max_expansion_steps(s);
        EXPECT_EQ(expected, pp(src, env));
        EXPECT_EQ(s, env.expansion_steps());
    }
    {
        Env env = setup();
        env.set_max_expansion_steps(s - 1);
        EXPECT_THROW(pp(src, env), Issue::Exception);
        EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
    }
}

TEST_F(ExpansionStepsTest, ZeroMeansNoLimit) {
    Env env = setup();
    env.set_max_expansion_steps(0);
    EXPECT_EQ(0u, env.get_max_expansion_steps());
    // 2^17 tokens: far above 10000 steps, still fine with no limit.
    EXPECT_NO_THROW(pp(doubling_defs(17) + "D17\n", env));
    EXPECT_GT(env.expansion_steps(), 100000u);
    EXPECT_TRUE(empty());
}

// ---- C5: each bypass is capped ----

TEST_F(ExpansionStepsTest, DoublingBombInOutput) {
    auto steps = expect_pp64(doubling_defs(24) + "D24\n");
    EXPECT_LE(steps, kLimit + 64);
}

TEST_F(ExpansionStepsTest, BombInsideIf) {
    auto steps = expect_pp64(doubling_defs(24) + "{#if D24 == 0}\n{#endif}\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, BombInsideInfoMessage) {
    auto steps = expect_pp64(doubling_defs(24) + "{#info D24}\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, BombInsidePragma) {
    auto steps = expect_pp64(doubling_defs(24) + "{D24}\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, BombInsideSkippedGroupCostsSteps) {
    // A huge skipped group is still tokens popped off the work stack.
    std::string big = "{#if 0}\n";
    for (int i = 0; i < 20000; ++i)
        big += "a b c\n";
    big += "{#endif}\n";
    expect_pp64(big);
}

TEST_F(ExpansionStepsTest, GlueGrowthOfOneToken) {
    // x @@ x doubles the single token's text at each of 24 nested levels.
    auto steps = expect_pp64(chain_defs("G", 24, "x @@ x") + "G24(a)\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, StringizeGrowth) {
    // Stringizing escapes the quote and "$", so the text at least doubles.
    auto steps = expect_pp64(chain_defs("S", 24, "@x") + "S24('a$b')\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, PragmaGlueGrowth) {
    auto steps = expect_pp64(chain_defs("Q", 20, "x @@ x") + "{Q20(a)}\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, WideBodyNestedFourDeep) {
    // 64 copies of the previous level, 4 levels: 64^4 tokens.
    std::string defs = "{#define W0 x}\n";
    for (int i = 1; i <= 4; ++i) {
        defs += "{#define W" + std::to_string(i) + " ";
        for (int k = 0; k < 64; ++k)
            defs += "W" + std::to_string(i - 1) + " ";
        defs += "}\n";
    }
    auto steps = expect_pp64(defs + "W4\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, VaArgsDoubling) {
    std::string s = "{#define V0(...) __VA_ARGS__}\n";
    for (int i = 1; i <= 24; ++i)
        s += "{#define V" + std::to_string(i) + "(...) V" + std::to_string(i - 1) +
             "(__VA_ARGS__, __VA_ARGS__)}\n";
    auto steps = expect_pp64(s + "V24(a)\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, VaOptDoubling) {
    std::string s = "{#define V0(...) __VA_OPT__(__VA_ARGS__)}\n";
    for (int i = 1; i <= 24; ++i)
        s += "{#define V" + std::to_string(i) + "(...) V" + std::to_string(i - 1) +
             "(__VA_OPT__(__VA_ARGS__, __VA_ARGS__))}\n";
    auto steps = expect_pp64(s + "V24(a)\n");
    EXPECT_LE(steps, kLimit + 2 * kLimit);
}

TEST_F(ExpansionStepsTest, NewlineEscapeBodies) {
    std::string s = "{#define N0 a}\n";
    for (int i = 1; i <= 24; ++i)
        s += "{#define N" + std::to_string(i) + " N" + std::to_string(i - 1) + "$nN" +
             std::to_string(i - 1) + "}\n";
    auto steps = expect_pp64(s + "N24\n");
    EXPECT_LE(steps, kLimit + 64);
}

TEST_F(ExpansionStepsTest, BombUnderDdMode) {
    auto steps = expect_pp64("{#define Z 1}\n" + doubling_defs(24) + "D24\n", kLimit,
                             /*dd_mode=*/true);
    EXPECT_LE(steps, kLimit + 64);
}

#ifndef JIEPP_SANDBOX
TEST_F(ExpansionStepsTest, IncludeSelfRecursionGuardedByIncludeLevel) {
    fs::path dir = fs::temp_directory_path() / "jiepp_expansion_steps_test";
    fs::create_directories(dir);
    fs::path file = dir / "self.iec";
    {
        std::ofstream f(file, std::ios::binary);
        f << "x\n{#if __INCLUDE_LEVEL__ < 40}\n{#include '" << file.generic_string()
          << "'}\n{#include '" << file.generic_string() << "'}\n{#endif}\n";
    }
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    EXPECT_THROW(pp_file(file, env), Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
    EXPECT_LE(env.expansion_steps(), kLimit + 2 * kLimit);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

TEST_F(ExpansionStepsTest, IgnoreDirectiveCannotSuppressPP64) {
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    EXPECT_THROW(pp("{#ignore PP64}\n" + doubling_defs(24) + "D24\n", env), Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
}

TEST_F(ExpansionStepsTest, NoDirectiveToRaiseTheLimit) {
    // Input must not be able to raise its own limit: there is no directive,
    // so this is an unknown directive (PP45), and the limit is untouched.
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    EXPECT_THROW(pp("{#max_expansion_steps 0}\n", env), Issue::Exception);
    EXPECT_EQ(kLimit, env.get_max_expansion_steps());
}
#endif

// ---- C7: library behaviour ----

TEST_F(ExpansionStepsTest, LibraryThrowsAndEnvIsReusable) {
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    EXPECT_THROW(preprocess_text(doubling_defs(24) + "D24\n", env), Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());

    // A second call on the same Env gets a fresh budget.
    std::string r;
    EXPECT_NO_THROW(r = preprocess_text("{#define Y 7}\nY\n", env));
    EXPECT_NE(std::string::npos, r.find('7'));
    EXPECT_LT(env.expansion_steps(), kLimit);
}

// ---- @@: the right operand is charged once ----
//
// subst() charges every token merged by @@ before merging it, and charges the
// tokens pushed to the result at the next element. A right operand token that
// is pushed (the rest of a successful paste's operand, and all of a failed
// paste's operand) must be counted only once. These tests pin the exact step
// count and the PP64 boundary of expansions with a multi-token right operand.
namespace {

// Runs `src` in continue mode (PP32 is counted, not thrown) with `limit`
// (0 = unlimited) and returns the step count; *threw tells whether PP64 stopped it.
std::uint64_t steps_of(const std::string& src, std::uint64_t limit, bool* threw) {
    Issue::ContinueMode guard({});
    Env env = setup();
    env.set_max_expansion_steps(limit);
    try {
        pp(src, env);
        *threw = false;
    } catch (const Issue::Exception&) {
        *threw = true;
    }
    return env.expansion_steps();
}

} // namespace

TEST_F(ExpansionStepsTest, FailedPasteChargesRightOperandOnce) {
    // x @@ (y z): ANY @@ LP is PP32; the five tokens ( y ' ' z ) are kept.
    const std::string src = "{#define F(a, b) a @@ b}F(x, (y z))\n";
    // 25 steps; counting the pushed operand tokens twice gave 30.
    constexpr std::uint64_t kSteps = 25;
    bool threw = true;
    EXPECT_EQ(kSteps, steps_of(src, 0, &threw));
    EXPECT_FALSE(threw);
    (void)steps_of(src, kSteps, &threw);
    EXPECT_FALSE(threw) << "limit == steps must pass";
    (void)steps_of(src, kSteps - 1, &threw);
    EXPECT_TRUE(threw) << "limit == steps - 1 must stop with PP64";
}

TEST_F(ExpansionStepsTest, ValidPasteChargesRestOfRightOperandOnce) {
    // x @@ y z: the paste merges y into x; the blank and z are pushed.
    const std::string src = "{#define F(a, b) a @@ b}F(x, y z)\n";
    // 18 steps; counting the pushed operand tokens twice gave 20.
    constexpr std::uint64_t kSteps = 18;
    bool threw = true;
    EXPECT_EQ(kSteps, steps_of(src, 0, &threw));
    EXPECT_FALSE(threw);
    (void)steps_of(src, kSteps, &threw);
    EXPECT_FALSE(threw) << "limit == steps must pass";
    (void)steps_of(src, kSteps - 1, &threw);
    EXPECT_TRUE(threw) << "limit == steps - 1 must stop with PP64";
}

// ---- __VA_OPT__ content is charged once ----
//
// The tokens of a __VA_OPT__ content are charged by the substitution that
// produces them; the enclosing substitution must not charge them again. So an
// expansion costs the same with the tokens written inside __VA_OPT__(...) as
// with the same tokens written without it. The cost of a call is measured as
// the steps of definition + call minus the steps of the definition alone, so
// the length of the definition does not matter.
namespace {

std::uint64_t call_cost(const std::string& def, const std::string& call) {
    bool threw = true;
    const std::uint64_t with_call = steps_of(def + call, 0, &threw);
    EXPECT_FALSE(threw);
    return with_call - steps_of(def, 0, &threw);
}

// `src` takes exactly its own step count: limit == steps passes, one less stops.
void expect_exact_budget(const std::string& src) {
    bool threw = true;
    const std::uint64_t steps = steps_of(src, 0, &threw);
    ASSERT_FALSE(threw);
    (void)steps_of(src, steps, &threw);
    EXPECT_FALSE(threw) << "limit == steps must pass: " << src;
    (void)steps_of(src, steps - 1, &threw);
    EXPECT_TRUE(threw) << "limit == steps - 1 must stop with PP64: " << src;
}

// `with_va_opt` and `plain` (the same tokens without __VA_OPT__) must cost
// `expected` steps per call, and `with_va_opt` must have an exact budget.
void expect_same_cost(const std::string& with_va_opt, const std::string& plain,
                      const std::string& call, std::uint64_t expected) {
    EXPECT_EQ(expected, call_cost(plain, call)) << plain;
    EXPECT_EQ(expected, call_cost(with_va_opt, call)) << with_va_opt;
    expect_exact_budget(with_va_opt + call);
}

} // namespace

TEST_F(ExpansionStepsTest, VaOptContentChargedOnce) {
    expect_same_cost("{#define M(...) __VA_OPT__(a b c d e f g h)}\n",
                     "{#define M(...) a b c d e f g h}\n", "M(1)\n", 35);
}

TEST_F(ExpansionStepsTest, GlueVaOptContentChargedOnce) {
    // Right operand of @@ (the first token is merged, the rest pushed).
    expect_same_cost("{#define M(...) p @@ __VA_OPT__(a b c d e f g h)}\n",
                     "{#define M(...) p @@ a b c d e f g h}\n", "M(1)\n", 36);
    // Left operand of @@.
    expect_same_cost("{#define M(...) __VA_OPT__(a b c d e f g x) @@ y}\n",
                     "{#define M(...) a b c d e f g x @@ y}\n", "M(1)\n", 36);
    // A failed paste (PP32, continue mode) pushes the whole operand.
    expect_same_cost("{#define M(...) ( @@ __VA_OPT__(a b c d e f g h)}\n",
                     "{#define M(...) ( @@ a b c d e f g h}\n", "M(1)\n", 37);
}

TEST_F(ExpansionStepsTest, GluePlacemarkerVaOptContentChargedOnce) {
    // An empty left operand (placemarker): the operand is pushed as it is.
    expect_same_cost("{#define M(x, ...) x @@ __VA_OPT__(a b c d e f g h)}\n",
                     "{#define M(x, ...) x @@ a b c d e f g h}\n", "M(, 1)\n", 37);
    // Chained pastes.
    expect_same_cost("{#define M(...) p @@ __VA_OPT__(a b c) @@ q}\n",
                     "{#define M(...) p @@ a b c @@ q}\n", "M(1)\n", 17);
}

TEST_F(ExpansionStepsTest, VaOptArgsChargedOnce) {
    expect_same_cost("{#define M(...) __VA_OPT__(__VA_ARGS__)}\n",
                     "{#define M(...) __VA_ARGS__}\n", "M(a b c d e f g h)\n", 64);
}

// Guard (passes before and after the change): the stringize form and skipped
// content were already charged once.
TEST_F(ExpansionStepsTest, StringizeAndSkippedVaOptCostUnchanged) {
    EXPECT_EQ(22u, call_cost("{#define M(...) @__VA_OPT__(a b c d e f g h)}\n", "M(1)\n"));
    EXPECT_EQ(4u, call_cost("{#define M(...) __VA_OPT__(a b c d e f g h)}\n", "M()\n"));
    EXPECT_EQ(6u, call_cost("{#define M(...) p @@ __VA_OPT__(a b c d e f g h)}\n", "M()\n"));
    EXPECT_EQ(6u, call_cost("{#define M(...) @__VA_OPT__(a b c d e f g h)}\n", "M()\n"));
}

// An empty __VA_OPT__ before @@ is a placemarker, so the tokens around it are
// rescanned as separate tokens: the call costs what the body with the
// __VA_OPT__ replaced by an empty parameter (or by its content) costs.
TEST_F(ExpansionStepsTest, EmptyVaOptPlacemarkerCostsLikeTheWrittenOutBody) {
    const auto same = [](const std::string& head, const std::string& with_va_opt,
                         const std::string& written_out, const std::string& call,
                         std::uint64_t expected) {
        SCOPED_TRACE(head + " " + with_va_opt);
        expect_same_cost("{#define M" + head + " " + with_va_opt + "}\n",
                         "{#define M" + head + " " + written_out + "}\n", call + "\n", expected);
    };
    same("(a, e, ...)", "a __VA_OPT__(z) @@ y", "a e @@ y", "M(x, )", 14);
    same("(a, e)", "a __VA_OPT__(z) @@ y", "a e @@ y", "M(x, )", 14);
    same("(a, e, ...)", "( __VA_OPT__(z) @@ y", "( e @@ y", "M(x, )", 13);
    same("(a, e, ...)", "'s' __VA_OPT__(z) @@ y", "'s' e @@ y", "M(x, )", 13);
    same("(a, e, ...)", "a __VA_OPT__(z) @@ )", "a e @@ )", "M(x, )", 14);
    same("(a, e, b, c, ...)", "a __VA_OPT__(b c) @@ y", "a b c @@ y", "M(x, , , , 1)", 23);
    same("(a, e, ...)", "p @@ a __VA_OPT__(b) @@ c", "p @@ a e @@ c", "M(x, )", 14);
    same("(a, e, ...)", "a __VA_OPT__(b) @@ __VA_OPT__(c) @@ d", "a e @@ e @@ d", "M(x, )", 14);
    same("(a, e, b, ...)", "a __VA_OPT__(z) @@ b", "a e @@ b", "M(x, , q r s t u v w)", 53);
    same("(a, e, b, ...)", "a __VA_OPT__(b) @@ y", "a b @@ y", "M(x, , , 1)", 19);
    same("(a, b, ...)", "a __VA_OPT__(b) @@ __VA_OPT__(w) y", "a b @@ w y", "M(x, , 1)", 21);
    same("(a, e, ...)", "a __VA_OPT__(b) @@ __VA_OPT__(c) @@ __VA_OPT__(d) @@ y",
         "a e @@ e @@ e @@ y", "M(x, )", 14);
    same("(a, e, args...)", "a __VA_OPT__(z) @@ y", "a e @@ y", "M(x, )", 14);
    // Skipped content is not charged, however long it is.
    std::string skipped = "a __VA_OPT__(c0";
    for (int i = 1; i < 40; ++i)
        skipped += " c" + std::to_string(i);
    same("(a, e, ...)", skipped + ") @@ y", "a e @@ y", "M(x, )", 14);
}

// Guard (passes before and after the change): the right-operand form and a
// non-empty content were already charged like the written-out body.
TEST_F(ExpansionStepsTest, VaOptNextToPasteCostUnchanged_Guard) {
    expect_same_cost("{#define M(a, e, ...) a @@ __VA_OPT__(z) y}\n",
                     "{#define M(a, e, ...) a @@ e y}\n", "M(x, )\n", 13);
    expect_same_cost("{#define M(a, e, ...) a __VA_OPT__(z) @@ y}\n",
                     "{#define M(a, e, ...) a z @@ y}\n", "M(x, , 1)\n", 18);
}

TEST_F(ExpansionStepsTest, LibraryWritesNothingOnStop) {
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    std::istringstream in(doubling_defs(24) + "D24\n");
    std::ostringstream out;
    EXPECT_THROW(preprocess(in, out, env), Issue::Exception);
    EXPECT_TRUE(out.str().empty());
}
