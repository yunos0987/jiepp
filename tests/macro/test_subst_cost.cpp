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
// after the RED run. The timing guards at the end are Release-only and were
// RED before the fix.
//
// A malformed __VA_OPT__ in a function-like macro is rejected when the
// macro is defined (one PP37, the macro is not defined); the tests for that
// are the definition-time ones below. A __VA_OPT__ outside a variadic macro
// is a warning (PP80) at the definition; the matrix of its forms is in
// test_va_opt_outside_variadic.cpp.

class SubstCostTest : public JieppTest {
protected:
    void expect_malformed_rejected_at_definition();
    void expect_nested_reported_once_at_definition();
};

// Same checks under a name matching the sandbox test presets' filter
// ^(Sandbox|RobustnessTest): they use continue mode only, so they also run
// in sandbox builds.
class RobustnessTestVaOptDefinition : public SubstCostTest {};

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
const char* const kBadStringize =
    "<unknown location>:1.0: error: PP31: Invalid stringizing (@)";

// The PP80 warning for a __VA_OPT__ token in the definition of macro `name`.
std::string vow(const std::string& name) {
    return "<unknown location>:1.0: warning: PP80: __VA_OPT__ outside a variadic macro; '" +
           name + "'";
}

// `n` PP80 warnings for macro `name`.
std::vector<std::string> vows(const std::string& name, int n) {
    return std::vector<std::string>(static_cast<std::size_t>(n), vow(name));
}

// a followed by b.
std::vector<std::string> cat(std::vector<std::string> a, const std::vector<std::string>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

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

// ---- well-formed __VA_OPT__ forms: same output, no diagnostics ----

TEST_F(SubstCostTest, WellFormedVaOptFormsUnchanged) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("x|", pp("{#define M(...) __VA_OPT__ (* c *) (x)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("'x'|''", pp("{#define M(...) @ (* c *) __VA_OPT__(x)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("'y'|''", pp("{#define M(...) @ __VA_OPT__ (y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("xy|x", pp("{#define M(...) x @@ __VA_OPT__(y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("xy|x", pp("{#define M(...) x @@ __VA_OPT__ (y)}M(1)|M()"));
    EXPECT_TRUE(empty());

    EXPECT_EQ("y|", pp("{#define M(...) __VA_OPT__(@@ y)}M(1)|M()"));
    EXPECT_TRUE(empty());
}

// ---- malformed and nested __VA_OPT__: rejected when the macro is defined ----

namespace {

struct MalformedBody {
    const char* body;
    const char* message;
    // __VA_OPT__ tokens up to and including the one where the problem is
    // found, and after it. clang warns for every __VA_OPT__ token of a
    // non-variadic definition, also for the rest of a rejected line; jiepp
    // does not warn for the body of a variadic macro at all.
    int before;
    int after;
};

// The first problem from left to right decides the message.
const MalformedBody kMalformedBodies[] = {
    // nested __VA_OPT__ (inside the parentheses of another one)
    {"__VA_OPT__(__VA_OPT__(x))", kNested, 2, 0},
    {"__VA_OPT__(__VA_OPT__ x)", kNested, 2, 0},
    {"__VA_OPT__(a __VA_OPT__(b)", kNested, 2, 0},
    {"__VA_OPT__((a) __VA_OPT__", kNested, 2, 0},
    {"x @@ __VA_OPT__(__VA_OPT__(y) z)", kNested, 2, 0},
    {"@__VA_OPT__(__VA_OPT__(y) z)", kNested, 2, 0},
    {"<__VA_OPT__((a)(b) __VA_OPT__(c))>", kNested, 2, 0},
    {"__VA_OPT__(__VA_OPT__(x)) __VA_OPT__(y)", kNested, 2, 1},
    {"__VA_OPT__(a __VA_OPT__(b) c __VA_OPT__(d)", kNested, 2, 1},
    // __VA_OPT__ not followed by '('
    {"__VA_OPT__ x", kNeedsParen, 1, 0},
    {"x @@ __VA_OPT__ y", kNeedsParen, 1, 0},
    {"@__VA_OPT__ y", kNeedsParen, 1, 0},
    {"__VA_OPT__", kNeedsParen, 1, 0},
    {"x __VA_OPT__", kNeedsParen, 1, 0},
    {"__VA_OPT__ __VA_OPT__(a)", kNeedsParen, 2, 0},
    {"__VA_OPT__ x __VA_OPT__(a __VA_OPT__(b))", kNeedsParen, 1, 2},
    {"__VA_OPT__ x __VA_OPT__ y", kNeedsParen, 1, 1},
    {"__VA_OPT__(a) __VA_OPT__ b", kNeedsParen, 2, 0},
    {"__VA_OPT__(a) @@ __VA_OPT__", kNeedsParen, 2, 0},
    // __VA_OPT__( without its ')'
    {"__VA_OPT__(a", kNeedsParen, 1, 0},
    {"x @@ __VA_OPT__(a", kNeedsParen, 1, 0},
    {"@__VA_OPT__(a", kNeedsParen, 1, 0},
    {"__VA_OPT__(a (b)", kNeedsParen, 1, 0},
};

// Parameter lists of the five kinds of function-like macro.
const char* const kParamLists[] = {"(...)", "(args...)", "(x, ...)", "(a)", "()"};

bool is_variadic_list(const std::string& params) {
    return params.find("...") != std::string::npos;
}

} // namespace

void SubstCostTest::expect_malformed_rejected_at_definition() {
    Issue::ContinueMode guard({});
    for (const char* params : kParamLists) {
        for (const auto& c : kMalformedBodies) {
            SCOPED_TRACE(std::string("M") + params + " " + c.body);
            Env env = setup();
            EXPECT_EQ("M(1)", pp(std::string("{#define M") + params + " " + c.body + "}M(1)", env));
            const bool variadic = is_variadic_list(params);
            EXPECT_EQ(cat(cat(vows("M", variadic ? 0 : c.before), {c.message}),
                          vows("M", variadic ? 0 : c.after)),
                      messages());
            EXPECT_EQ(nullptr, env.lookup("M"));
        }
    }
}

TEST_F(SubstCostTest, MalformedVaOptRejectedAtDefinition) {
    expect_malformed_rejected_at_definition();
}

TEST_F(RobustnessTestVaOptDefinition, MalformedVaOptRejectedAtDefinition) {
    expect_malformed_rejected_at_definition();
}

// A parameter named __VA_OPT__ does not change the rule: the body tokens
// spelled __VA_OPT__ are checked whatever the parameters are. The parameter
// is one more __VA_OPT__ token for PP80 (clang warns for it too).
TEST_F(RobustnessTestVaOptDefinition, ParameterNamedVaOptRejected) {
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_EQ("F(1)", pp("{#define F(__VA_OPT__) __VA_OPT__(__VA_OPT__)}F(1)", env));
    EXPECT_EQ(cat(vows("F", 3), {kNested}), messages());
    EXPECT_EQ(nullptr, env.lookup("F"));
    EXPECT_EQ("F(1)", pp("{#define F(__VA_OPT__) __VA_OPT__ x}F(1)", env));
    EXPECT_EQ(cat(vows("F", 2), {kNeedsParen}), messages());
    EXPECT_EQ(nullptr, env.lookup("F"));
}

// A nested __VA_OPT__ is reported once, at the definition, whatever the
// nesting depth and the parameter list; the macro is not defined and the
// call stays as text. The __VA_OPT__ tokens of a non-variadic definition
// warn (PP80): the two up to the problem before the PP37, the rest after it;
// a variadic macro has the PP37 only.
void SubstCostTest::expect_nested_reported_once_at_definition() {
    Issue::ContinueMode guard({});
    for (int k : {2, 3, 60, 5000}) {
        for (const char* params : {"(...)", "(a)", "()"}) {
            SCOPED_TRACE("k=" + std::to_string(k) + " M" + params);
            const std::string def = std::string("{#define M") + params + " " +
                                    repeat("__VA_OPT__(", k) + "x" + repeat(")", k) + "}";
            Env env = setup();
            EXPECT_EQ("M(1)", pp(def + "M(1)", env));
            const bool variadic = is_variadic_list(params);
            EXPECT_EQ(variadic ? std::vector<std::string>{kNested}
                               : cat(cat(vows("M", 2), {kNested}), vows("M", k - 2)),
                      messages());
            EXPECT_EQ(nullptr, env.lookup("M"));
        }
    }
}

TEST_F(SubstCostTest, NestedVaOptReportedOnceAtDefinition) {
    expect_nested_reported_once_at_definition();
}

// The cost of a call does not depend on the depth: before, k levels raised
// k(k-1)/2 charged PP37 per call.
TEST_F(SubstCostTest, NestedVaOptCallCostIndependentOfDepth) {
    Issue::ContinueMode guard({});
    auto call_cost = [&](int k) {
        const std::string def = "{#define M(...) " + repeat("__VA_OPT__(", k) + "x" +
                                repeat(")", k) + "}";
        Env env0 = setup();
        pp(def, env0);
        Env env1 = setup();
        pp(def + "M(1)", env1);
        return env1.expansion_steps() - env0.expansion_steps();
    };
    EXPECT_EQ(call_cost(3), call_cost(60));
}

TEST_F(RobustnessTestVaOptDefinition, NestedVaOptReportedOnceAtDefinition) {
    expect_nested_reported_once_at_definition();
}

// An object macro does not treat __VA_OPT__ specially, so k nested levels
// give k PP80 warnings at the definition and nothing at a use (no PP37, no
// recursion in subst()); the text is kept and the cost of a use is linear in
// the body.
TEST_F(SubstCostTest, ObjectMacroNestedVaOptIsLinear) {
    Issue::ContinueMode guard({});
    constexpr int kK = 5000;
    const std::string body = repeat("__VA_OPT__(", kK) + "x" + repeat(")", kK);
    const std::string def = "{#define O a @@ " + body + "}";

    Env env0 = setup();
    pp(def, env0);
    const std::uint64_t def_steps = env0.expansion_steps();
    EXPECT_EQ(vows("O", kK), messages());

    Env env1 = setup();
    EXPECT_EQ("a" + body, pp(def + "O", env1));
    EXPECT_EQ(vows("O", kK), messages());
    EXPECT_LT(env1.expansion_steps() - def_steps, static_cast<std::uint64_t>(8 * kK));
    EXPECT_EQ(0, Issue::error_count_);
}

// ---- what a rejected definition leaves behind ----

// The earlier definition (user or built-in) stays and no PP35 is raised.
TEST_F(RobustnessTestVaOptDefinition, RejectedDefinitionKeepsExistingOne) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("old", pp("{#define M(...) old}{#define M(...) __VA_OPT__ x}M(1)"));
    EXPECT_EQ((std::vector<std::string>{kNeedsParen}), messages());

    EXPECT_EQ("old", pp("{#define F(a) old}{#define F(a) __VA_OPT__(__VA_OPT__(x))}F(1)"));
    EXPECT_EQ(cat(vows("F", 2), {kNested}), messages());

    EXPECT_EQ("1", pp("{#define __LINE__(...) __VA_OPT__(__VA_OPT__(x))}__LINE__"));
    EXPECT_EQ((std::vector<std::string>{kNested}), messages());
    EXPECT_EQ("y", pp("{#define __LINE__(...) __VA_OPT__ x}{#if __has_builtin(__LINE__)}y{#else}n{#endif}"));
    EXPECT_EQ((std::vector<std::string>{kNeedsParen}), messages());

    pp("{#define __assert(a, b) __VA_OPT__ x}__assert(0, m)");
    const auto ms = messages();
    ASSERT_EQ(3u, ms.size());
    EXPECT_EQ(vow("__assert"), ms[0]);
    EXPECT_EQ(kNeedsParen, ms[1]);
    EXPECT_THAT(ms[2], ::testing::HasSubstr("PP91"));
    EXPECT_THAT(ms[2], ::testing::HasSubstr("'m'"));
}

// A rejected definition is abandoned like PP36: the library without
// continue mode throws at the definition (the PP80 warnings before it are
// reported).
TEST_F(RobustnessTestVaOptDefinition, RejectedDefinitionThrowsWithoutContinueMode) {
    const Issue::Code vow_code = Issue::Code::VA_OPT_OUTSIDE_VARIADIC_MACRO;
    const Issue::Code pp37 = Issue::Code::INVALID_VA_OPT;
    const struct {
        const char* src;
        std::vector<Issue::Code> codes;
    } cases[] = {
        {"{#define M(...) __VA_OPT__ x}", {pp37}},
        {"{#define F(a) __VA_OPT__(__VA_OPT__(x))}", {vow_code, vow_code, pp37}},
        {"{#define F() x @@ __VA_OPT__ y}", {vow_code, pp37}},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.src);
        EXPECT_THROW(pp(c.src), Issue::Exception);
        EXPECT_EQ(c.codes, codes());
    }
}

#ifndef JIEPP_SANDBOX
// {#ignore PP37} only hides the report: the macro is still not defined
// ({#ignore} is restricted in sandbox builds, so this is a normal-build test).
// The PP80 warnings of a non-variadic definition are not PP37 and stay.
TEST_F(SubstCostTest, RejectedDefinitionUnderIgnoreIsNotDefined) {
    Env env = setup();
    EXPECT_EQ("M(1)", pp("{#ignore PP37}{#define M(...) __VA_OPT__(__VA_OPT__(x))}M(1)", env));
    EXPECT_TRUE(empty());
    EXPECT_EQ(nullptr, env.lookup("M"));

    EXPECT_EQ("old", pp("{#define F(a) old}{#define F(a) __VA_OPT__ x}F(1)", env));
    EXPECT_EQ(vows("F", 1), messages());

    EXPECT_EQ("old", pp("{#ignore PP37}{#ignore PP80}{#define F(a) __VA_OPT__ x}F(1)", env));
    EXPECT_TRUE(empty());
}
#endif

// A {#define} inside a macro call's argument list goes through the same check.
TEST_F(RobustnessTestVaOptDefinition, RejectedDefinitionInArgumentList) {
    Issue::ContinueMode guard({});
    Env env = setup();
    pp("{#define G(x) x}G({#define M(...) __VA_OPT__ x} y)", env);
    EXPECT_EQ((std::vector<std::string>{kNeedsParen}), messages());
    EXPECT_EQ(nullptr, env.lookup("M"));
}

// -dD does not echo a rejected {#define}; -dM does not list it.
TEST_F(RobustnessTestVaOptDefinition, RejectedDefinitionNotEchoedNorDumped) {
    Issue::ContinueMode guard({});
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output = pp(
        "{#define M(...) __VA_OPT__ x}{#define F(a) __VA_OPT__(__VA_OPT__(x))}{#define OK 1}", env);
    EXPECT_EQ(std::string::npos, output.find("{#define M(")) << "output:\n" << output;
    EXPECT_EQ(std::string::npos, output.find("{#define F(")) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("{#define OK 1}")) << "output:\n" << output;
    EXPECT_EQ(cat({kNeedsParen}, cat(vows("F", 2), {kNested})), messages());

    std::ostringstream os;
    dump_macros(env, os);
    EXPECT_EQ(std::string::npos, os.str().find("{#define M(")) << os.str();
    EXPECT_EQ(std::string::npos, os.str().find("{#define F(")) << os.str();
    EXPECT_NE(std::string::npos, os.str().find("{#define OK 1}")) << os.str();
}

// ---- forms that are not rejected at definition ----

// A well-formed, non-nested __VA_OPT__ in a non-variadic function-like macro
// is accepted with a PP80 warning at the definition and nothing at a use.
TEST_F(RobustnessTestVaOptDefinition, NonVariadicWellFormedVaOptIsAWarning) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("", pp("{#define F(a) __VA_OPT__(x)}F(1)"));
    EXPECT_EQ(vows("F", 1), messages());

    EXPECT_EQ("x", pp("{#define F(a) x @@ __VA_OPT__(y)}F(1)"));
    EXPECT_EQ(vows("F", 1), messages());

    EXPECT_EQ("''", pp("{#define F(a) @__VA_OPT__(y)}F(1)"));
    EXPECT_EQ(vows("F", 1), messages());

    EXPECT_EQ("__VA_OPT__(X)", pp("{#define BAD() __VA_OPT__(X)}BAD()"));
    EXPECT_EQ(vows("BAD", 1), messages());
    EXPECT_EQ(0, Issue::error_count_);
}

// Object-like macros are not checked at definition (no PP37), only warned
// about once per __VA_OPT__ token.
TEST_F(RobustnessTestVaOptDefinition, ObjectMacroMalformedVaOptIsOnlyWarned) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("__VA_OPT__(__VA_OPT__(y))", pp("{#define O __VA_OPT__(__VA_OPT__(y))}O"));
    EXPECT_EQ(vows("O", 2), messages());

    EXPECT_EQ("x__VA_OPT__ y", pp("{#define O x @@ __VA_OPT__ y}O"));
    EXPECT_EQ(vows("O", 1), messages());

    EXPECT_EQ("__VA_OPT__(a", pp("{#define O __VA_OPT__(a}O"));
    EXPECT_EQ(vows("O", 1), messages());
    EXPECT_EQ(0, Issue::error_count_);
}

// Regression watchpoint: an identical redefinition stays silent (no PP35),
// also after a rejected one. The rejected definition leaves the first one in
// place, so the third definition is identical to it. (The bracketed-argument
// watchpoint is VariadicBracketedArgumentIsOneArgument_Guard.)
TEST_F(RobustnessTestVaOptDefinition, IdenticalRedefinitionAfterRejectedStaysSilent) {
    Issue::ContinueMode guard({});
    const std::string v = "{#define V(...) __VA_OPT__(<__VA_ARGS__>)}";
    EXPECT_EQ("<a[x,y]>",
              pp(v + "{#define V(...) __VA_OPT__(<__VA_ARGS__> __VA_OPT__(x))}" + v + "V(a[x,y])"));
    EXPECT_EQ((std::vector<std::string>{kNested}), messages());
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

// ---- in an object macro __VA_OPT__ is an ordinary identifier ----

TEST_F(SubstCostTest, ObjectMacroVaOptIsAnIdentifier) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("a__VA_OPT__(b)", pp("{#define O a @@ __VA_OPT__(b)}O"));
    EXPECT_EQ(vows("O", 1), messages());

    EXPECT_EQ("@__VA_OPT__(b)", pp("{#define O2 @__VA_OPT__(b)}O2"));
    EXPECT_EQ(vows("O2", 1), messages());

    EXPECT_EQ("__VA_OPT__(b)", pp("{#define O3 __VA_OPT__(b)}O3"));
    EXPECT_EQ(vows("O3", 1), messages());

    EXPECT_EQ("__VA_OPT__ (* c *) (b)", pp("{#define O __VA_OPT__ (* c *) (b)}O"));
    EXPECT_EQ(vows("O", 1), messages());

    EXPECT_EQ("x__VA_OPT__ y", pp("{#define O x @@ __VA_OPT__ y}O"));
    EXPECT_EQ(vows("O", 1), messages());

    EXPECT_EQ("@__VA_OPT__ (b", pp("{#define O @__VA_OPT__ (b}O"));
    EXPECT_EQ(vows("O", 1), messages());

    // Only @__VA_OPT__ keeps the '@' silently; `@` before an ordinary
    // identifier is still PP31.
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
    // An object macro: still no PP35, but its __VA_OPT__ warns (PP80) at each
    // {#define}, like clang, so exactly 2 PP80 for the two definitions.
    EXPECT_EQ("", pp("{#define O a @@ __VA_OPT__(b)}{#define O a  @@  __VA_OPT__(b)}"));
    EXPECT_EQ(vows("O", 2), messages());
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

// The same skipped content in a non-variadic function-like macro with
// parameters, where __VA_OPT__ acts as if the variable arguments were empty
// and the content is dropped. (An object macro keeps the text as written, so
// the skipped-content path is no longer reached through one.)
// Pre-fix: 12.2 / 13.1 s through an object macro.
TEST_F(SubstCostTest, NonVariadicVaOptSkippedContent_TimingGuard) {
    constexpr int kCalls = 4000;
    const std::string c = spaced_z(20000);
    const std::string calls = repeat("F(1);", kCalls);
    const std::pair<std::string, std::string> forms[] = {
        {"__VA_OPT__(" + c + ")", repeat(";", kCalls)},
        {"x @@ __VA_OPT__(" + c + ")", repeat("x;", kCalls)},
        {"@__VA_OPT__(" + c + ")", repeat("'';", kCalls)},
    };
    for (const auto& [body, expected] : forms) {
        auto [out, secs] = timed_pp("{#define F(a) " + body + "}" + calls);
        EXPECT_EQ(expected, out);
        EXPECT_LT(secs, 3.0) << body.substr(0, 16) << "... x" << kCalls << " took " << secs
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
