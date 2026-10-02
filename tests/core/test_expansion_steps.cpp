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

TEST_F(ExpansionStepsTest, LibraryWritesNothingOnStop) {
    Env env = setup();
    env.set_max_expansion_steps(kLimit);
    std::istringstream in(doubling_defs(24) + "D24\n");
    std::ostringstream out;
    EXPECT_THROW(preprocess(in, out, env), Issue::Exception);
    EXPECT_TRUE(out.str().empty());
}
