#include "test_helper.hpp"
#include "core/expand_helpers.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// task_slug security-impl (A3 / S-01): subst() must not do O(n) work per
// __VA_OPT__ or per repeated `@` that is never charged to the expansion-step
// budget. Before the fix, every __VA_OPT__ copied the variadic arguments just
// to test them for emptiness and copied its content even when it was
// skipped, every `@x` rebuilt and rescanned the argument, every subst() call
// rescanned the whole body for `@@` adjacency, and every use of an object
// macro copied its body.
//
// The output/diagnostic tests below pin the output and diagnostics of the
// pre-fix build (e644f7d) and pass both before and after the fix.
// VaArgsEmptyAgreesWithSelectArg calls the new va_args_empty() helper, so it
// (and this file) does not build against the pre-fix code; it was added
// after the RED run. NestedVaOptPP37IsCharged guards the step charge for
// nested-__VA_OPT__ PP37s that the fix added (RED with that charge
// removed). The timing guards at the end are Release-only and were RED
// before the fix.

class SubstCostTest : public JieppTest {};

namespace {

std::string repeat(const std::string& s, int n) {
    std::string r;
    r.reserve(s.size() * static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        r += s;
    return r;
}

const char* const kNested =
    "<unknown location>:1.0: error: PP37: Invalid __VA_OPT__; 'nested __VA_OPT__ is not allowed'";
const char* const kNeedsParen =
    "<unknown location>:1.0: error: PP37: Invalid __VA_OPT__; '__VA_OPT__ requires parenthesized argument'";
const char* const kOutside =
    "<unknown location>:1.0: error: PP37: Invalid __VA_OPT__; '__VA_OPT__ used outside variadic macro'";
const char* const kBadStringize =
    "<unknown location>:1.0: error: PP31: Invalid stringizing (@)";

} // namespace

// ---- __VA_OPT__ emptiness (O(1) va_args_empty() must agree with select_arg()) ----

TEST_F(SubstCostTest, VaOptEmptinessMatrix) {
    EXPECT_EQ("<>|<>|<>|<+>|<+>|<+>|<+>",
              pp("{#define V(...) <__VA_OPT__(+)>}V()|V( )|V((* c *))|V(,)|V(a)|V(,a)|V(a,)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("<>|<>|<>|<+>|<+>|<+>|<+>",
              pp("{#define N(args...) <__VA_OPT__(+)>}"
                 "N()|N( )|N((* c *))|N(,)|N(a)|N(,a)|N(a,)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("<>|<>|<+>|<+>",
              pp("{#define F(x, ...) <__VA_OPT__(+)>}F(1)|F(1,)|F(1,2)|F(1,,)"));
    EXPECT_TRUE(empty());
}

// va_args_empty() stops the shared join at its first token; it must agree
// with select_arg(...).empty() for every shape of actuals, separator flags
// and join kind (guard: the helper is new in S-01).
TEST_F(SubstCostTest, VaArgsEmptyAgreesWithSelectArg) {
    using namespace jiepp::expand_detail;
    for (int n = 0; n <= 3; ++n) {
        for (int mask = 0; mask < (1 << n); ++mask) {
            std::vector<std::vector<Token>> actuals(static_cast<std::size_t>(n));
            for (int k = 0; k < n; ++k)
                if (mask & (1 << k))
                    actuals[static_cast<std::size_t>(k)].push_back(
                        Token::create(Token::ANY, "a" + std::to_string(k)));
            const std::vector<ArgWs> all_ws(static_cast<std::size_t>(n),
                                            ArgWs{true, true, true, true});
            for (int idx = 0; idx <= n; ++idx) {
                for (bool is_va : {false, true}) {
                    const bool empty_now = va_args_empty(idx, actuals, is_va);
                    const std::vector<ArgWs>* no_ws = nullptr;
                    for (const std::vector<ArgWs>* ws : {no_ws, &all_ws}) {
                        for (VaJoin join : {VaJoin::plain, VaJoin::stringize, VaJoin::shared}) {
                            EXPECT_EQ(select_arg(idx, actuals, is_va, ws, join).empty(), empty_now)
                                << "n=" << n << " mask=" << mask << " idx=" << idx
                                << " is_va=" << is_va << " ws=" << (ws != nullptr)
                                << " join=" << static_cast<int>(join);
                        }
                    }
                }
            }
        }
    }
}

// ---- malformed and nested __VA_OPT__: same output, same PP37 count and order ----

TEST_F(SubstCostTest, VaOptMalformedAndNestedDiagnosticsUnchanged) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("x|x", pp("{#define M(...) __VA_OPT__(__VA_OPT__(x))}M(1)|M(1)"));
    EXPECT_EQ((std::vector<std::string>{kNested, kNested}), messages());

    EXPECT_EQ("__VA_OPT__ x", pp("{#define M(...) __VA_OPT__ x}M(1)"));
    EXPECT_EQ((std::vector<std::string>{kNeedsParen}), messages());

    EXPECT_EQ("__VA_OPT__(a b", pp("{#define M(...) __VA_OPT__(a __VA_OPT__(b)}M(1)"));
    EXPECT_EQ((std::vector<std::string>{kNested, kNeedsParen}), messages());

    EXPECT_EQ("__VA_OPT__(a (b", pp("{#define M(...) __VA_OPT__(a (b}M(1)"));
    EXPECT_EQ((std::vector<std::string>{kNeedsParen}), messages());

    EXPECT_EQ("__VA_OPT__", pp("{#define M(...) __VA_OPT__}M(1)"));
    EXPECT_EQ((std::vector<std::string>{kNeedsParen}), messages());

    EXPECT_EQ("x|", pp("{#define M(...) __VA_OPT__ (* c *) (x)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("'x'|''", pp("{#define M(...) @ (* c *) __VA_OPT__(x)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("'y'|''", pp("{#define M(...) @ __VA_OPT__ (y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("@__VA_OPT__", pp("{#define M(...) @__VA_OPT__}M(1)"));
    EXPECT_EQ((std::vector<std::string>{kBadStringize, kNeedsParen}), messages());

    EXPECT_EQ("xy|x", pp("{#define M(...) x @@ __VA_OPT__(y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("xy|x", pp("{#define M(...) x @@ __VA_OPT__ (y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("x__VA_OPT__", pp("{#define M(...) x @@ __VA_OPT__}M(1)"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("y|", pp("{#define M(...) __VA_OPT__(@@ y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    // Nested __VA_OPT__ under `@@` and `@`: one PP37 per invocation, raised
    // even when the variable arguments are empty and the content is skipped.
    EXPECT_EQ("xy z|x", pp("{#define M(...) x @@ __VA_OPT__(__VA_OPT__(y) z)}M(1)|M()"));
    EXPECT_EQ((std::vector<std::string>{kNested, kNested}), messages());

    EXPECT_EQ("'y z'|''", pp("{#define M(...) @__VA_OPT__(__VA_OPT__(y) z)}M(1)|M()"));
    EXPECT_EQ((std::vector<std::string>{kNested, kNested}), messages());

    EXPECT_EQ("<(a)(b) c>|<>",
              pp("{#define M(...) <__VA_OPT__((a)(b) __VA_OPT__(c))>}M(1)|M()"));
    EXPECT_EQ((std::vector<std::string>{kNested, kNested}), messages());
}

// Each nested-__VA_OPT__ PP37 is charged one expansion step (S-01): k nested
// levels raise (k-1) + (k-2) + ... + 0 = k(k-1)/2 PP37 per invocation, so a
// lowered --max-expansion-steps stops it. Small k so it also runs in Debug
// (deep __VA_OPT__ nesting recurses in subst()).
TEST_F(SubstCostTest, NestedVaOptPP37IsCharged) {
    Issue::ContinueMode guard({});
    constexpr int kK = 60;
    constexpr std::uint64_t kReports = kK * (kK - 1) / 2;
    const std::string def = "{#define M(...) " + repeat("__VA_OPT__(", kK) + "x" +
                            repeat(")", kK) + "}";

    Env env0 = setup();
    pp(def, env0);
    const std::uint64_t def_steps = env0.expansion_steps();
    EXPECT_TRUE(empty());

    Env env1 = setup();
    EXPECT_EQ("x", pp(def + "M(1)", env1));
    EXPECT_EQ(kReports, messages().size());
    EXPECT_GE(env1.expansion_steps() - def_steps, kReports);

    Env env2 = setup();
    env2.set_max_expansion_steps(def_steps + kReports / 2);
    EXPECT_THROW(pp(def + "M(1)", env2), Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, codes().back());
}

// ---- @@ adjacency inside and around __VA_OPT__ content ----

TEST_F(SubstCostTest, GlueAdjacencyAroundVaOptUnchanged) {
    EXPECT_EQ("'a1, 2 b'|''", pp("{#define M(...) @__VA_OPT__(a @@ __VA_ARGS__ b)}M(1, 2)|M()"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("ppp|pp", pp("{#define M(x, ...) x @@ __VA_OPT__(x) @@ x}M(p, 1)|M(p)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("pp|p||p", pp("{#define M(x, ...) __VA_OPT__(x @@ x)|x}M(p, 1)|M(p)"));
    EXPECT_TRUE(empty());
}

// ---- object macros take the same @@/@ __VA_OPT__ paths ----

TEST_F(SubstCostTest, ObjectMacroVaOptFormsUnchanged) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("a", pp("{#define O a @@ __VA_OPT__(b)}O"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("''", pp("{#define O2 @__VA_OPT__(b)}O2"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("__VA_OPT__(b)", pp("{#define O3 __VA_OPT__(b)}O3"));
    EXPECT_EQ((std::vector<std::string>{kOutside}), messages());

    EXPECT_EQ("__VA_OPT__ (* c *) (b)", pp("{#define O __VA_OPT__ (* c *) (b)}O"));
    EXPECT_EQ((std::vector<std::string>{kOutside}), messages());

    EXPECT_EQ("x__VA_OPT__ y", pp("{#define O x @@ __VA_OPT__ y}O"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("@__VA_OPT__ (b", pp("{#define O @__VA_OPT__ (b}O"));
    EXPECT_EQ((std::vector<std::string>{kBadStringize, kOutside}), messages());

    EXPECT_EQ("@ x", pp("{#define O @ x}O"));
    EXPECT_EQ((std::vector<std::string>{kBadStringize}), messages());
}

// ---- a repeated @ gives exactly what a single @ gives ----

TEST_F(SubstCostTest, RepeatedStringizeMatchesSingle) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("'a $27q r$27 b'|'a $27q r$27 b'|a (* c *) 'q r'  b\n",
              pp("{#define S(x) @x|@x|x}S(a (* c *) 'q r'\n b)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'a b'|'a b'|a  b", pp("{#define S(x) @x|@x|x}S( a  b )"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'1' '2' '1' '2'|'' '' '' ''", pp("{#define S(x,y) @x @y @x @y}S(1,2)|S(,)"));
    EXPECT_TRUE(empty());

    // __VA_ARGS__: lead/trail space and newline edges (R6 for @, C6 for plain).
    EXPECT_EQ("'a , b , c'|'a , b , c'|a ,b , c\n",
              pp("{#define S(...) @__VA_ARGS__|@__VA_ARGS__|__VA_ARGS__}S( a ,\nb , c )"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'a ,b'|'a ,b'|a,b\n",
              pp("{#define S(...) @__VA_ARGS__|@__VA_ARGS__|__VA_ARGS__}S(a\n,b)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'a , b'|'a , b'|a , b\n\n",
              pp("{#define S(...) @__VA_ARGS__|@__VA_ARGS__|__VA_ARGS__}S(\n a , b \n)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("''|''|','|','", pp("{#define S(...) @__VA_ARGS__|@__VA_ARGS__}S()|S(,)"));
    EXPECT_TRUE(empty());

    // @x inside and outside __VA_OPT__(@x).
    EXPECT_EQ("'p q' 'p q' 'p q'|'p q'  'p q'",
              pp("{#define S(x, ...) @x __VA_OPT__(@x) @x}S(p q, 1)|S(p q)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'a , b' 'a , b' a , b\n| ",
              pp("{#define M(...) __VA_OPT__(@__VA_ARGS__ @__VA_ARGS__) __VA_ARGS__}"
                 "M( a ,\n b )|M()"));
    EXPECT_TRUE(empty());

    // Review M1: __VA_ARGC__ resolves to the same formal index (and so the
    // same memo slot) as the variable arguments; @__VA_ARGC__ must never be
    // answered from the cached stringized arguments.
    EXPECT_EQ("'a, b'|'a, b'|'2'",
              pp("{#define S(...) @__VA_ARGS__|@__VA_ARGS__|@__VA_ARGC__}S(a, b)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'a, b'|'a, b'|'2'|__VA_ARGS__",
              pp("{#define S(args...) @args|@args|@__VA_ARGC__|__VA_ARGS__}S(a, b)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("'2'|'a, b'|'a, b'|'2'",
              pp("{#define S(...) @__VA_ARGC__|@__VA_ARGS__|@__VA_ARGS__|@__VA_ARGC__}S(a, b)"));
    EXPECT_TRUE(empty());
}

// ---- watchpoints (see ArgExpandOnceTest T22/T26) ----

TEST_F(SubstCostTest, VaOptBodyIdenticalRedefinitionSilent_Guard) {
    EXPECT_EQ("", pp("{#define M(...) __VA_OPT__(x) @__VA_ARGS__}"
                     "{#define M(...) __VA_OPT__(x) @__VA_ARGS__}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("", pp("{#define O a @@ __VA_OPT__(b)}{#define O a  @@  __VA_OPT__(b)}"));
    EXPECT_TRUE(empty());
}

TEST_F(SubstCostTest, VariadicBracketedArgumentIsOneArgument_Guard) {
    EXPECT_EQ("1|y", pp("{#define V(...) __VA_ARGC__|__VA_OPT__(y)}V(a[x,y])"));
    EXPECT_TRUE(empty());
}

// ---- Release-only timing guards (A3) ----
//
// Release-only, like HideSetChainTest.H7_TimingGuard: a Debug build is not
// representative of real performance. Pre-fix times are for the in-process
// pp() on the e644f7d Release build; each was RED (> 3 s).
#ifdef NDEBUG
namespace {

// "z z z ... z" (n tokens separated by one space). Defined here, with its
// only users, so a Debug build does not hit -Wunused-function.
std::string spaced_z(int n) {
    std::string r = "z";
    for (int i = 1; i < n; ++i)
        r += " z";
    return r;
}

std::pair<std::string, double> timed_pp(const std::string& src) {
    auto t0 = std::chrono::steady_clock::now();
    std::string out = pp(src);
    auto t1 = std::chrono::steady_clock::now();
    return {out, std::chrono::duration<double>(t1 - t0).count()};
}

} // namespace

// k=1600 __VA_OPT__ against n=40000 variable arguments.
// Pre-fix: 8.9 s.
TEST_F(SubstCostTest, VaOptEmptiness_TimingGuard) {
    constexpr int kN = 40000;
    constexpr int kK = 1600;
    const std::string src = "{#define M(...) " + repeat("__VA_OPT__(y)", kK) + "}M(" +
                            repeat("a,", kN - 1) + "a);";
    auto [out, secs] = timed_pp(src);
    EXPECT_EQ(std::string(kK, 'y') + ";", out);
    EXPECT_LT(secs, 3.0) << "k=" << kK << " n=" << kN << " took " << secs << "s";
}

// 20000-token __VA_OPT__ content skipped by 4000 calls with no variable
// arguments, in the standalone, `a @@` and `@` forms.
// Pre-fix: 8.9 / 8.8 / 8.6 s.
TEST_F(SubstCostTest, VaOptSkippedContent_TimingGuard) {
    constexpr int kCalls = 4000;
    const std::string c = spaced_z(20000);
    const std::string calls = repeat("M();", kCalls);
    const std::pair<std::string, std::string> forms[] = {
        {"__VA_OPT__(" + c + ")", repeat(";", kCalls)},
        {"a @@ __VA_OPT__(" + c + ")", repeat("a;", kCalls)},
        {"@__VA_OPT__(" + c + ")", repeat("'';", kCalls)},
    };
    for (const auto& [body, expected] : forms) {
        auto [out, secs] = timed_pp("{#define M(...) " + body + "}" + calls);
        EXPECT_EQ(expected, out);
        EXPECT_LT(secs, 3.0) << body.substr(0, 16) << "... x" << kCalls << " took " << secs
                             << "s";
    }
}

// Object macro bodies with a 20000-token __VA_OPT__ content, used 4000 times.
// Pre-fix: 12.2 / 13.1 s.
TEST_F(SubstCostTest, ObjectMacroVaOpt_TimingGuard) {
    constexpr int kUses = 4000;
    const std::string c = spaced_z(20000);
    const std::string uses = repeat("O;", kUses);
    const std::pair<std::string, std::string> forms[] = {
        {"a @@ __VA_OPT__(" + c + ")", repeat("a;", kUses)},
        {"@__VA_OPT__(" + c + ")", repeat("'';", kUses)},
    };
    for (const auto& [body, expected] : forms) {
        auto [out, secs] = timed_pp("{#define O " + body + "}" + uses);
        EXPECT_EQ(expected, out);
        EXPECT_LT(secs, 3.0) << body.substr(0, 16) << "... x" << kUses << " took " << secs
                             << "s";
    }
}

// @x x8000 and @__VA_ARGS__ x4000 with the argument `a (**)x40000 b`.
// Pre-fix: 7.6 / 7.9 s.
TEST_F(SubstCostTest, RepeatedStringize_TimingGuard) {
    const std::string arg = "a " + repeat("(**)", 40000) + " b";
    {
        constexpr int kK = 8000;
        auto [out, secs] = timed_pp("{#define S(x) " + repeat("@x ", kK) + "}S(" + arg + ");");
        EXPECT_EQ(repeat("'a b' ", kK - 1) + "'a b';", out);
        EXPECT_LT(secs, 3.0) << "@x x" << kK << " took " << secs << "s";
    }
    {
        constexpr int kK = 4000;
        auto [out, secs] =
            timed_pp("{#define S(...) " + repeat("@__VA_ARGS__ ", kK) + "}S(" + arg + ");");
        EXPECT_EQ(repeat("'a b' ", kK - 1) + "'a b';", out);
        EXPECT_LT(secs, 3.0) << "@__VA_ARGS__ x" << kK << " took " << secs << "s";
    }
}
#endif
