#include "test_helper.hpp"
#include "core/expand_helpers.hpp"
#include "loader/lexer.hpp"

#include <algorithm>
#include <string>
#include <vector>

// __VA_OPT__ outside a variadic macro (non-variadic function-like macros and
// object-like macros) is a warning, PP80, reported once per __VA_OPT__ token
// when the macro is defined, and the macro expands like clang's:
//  - with at least one parameter, __VA_OPT__(...) acts as if the variable
//    arguments were empty (the content is dropped; `x @@ __VA_OPT__(y)` is
//    `x`, `@__VA_OPT__(y)` is an empty string);
//  - F() and an object-like macro have no variable arguments to ask about,
//    so __VA_OPT__ is an ordinary identifier there (it keeps its text, `@@`
//    pastes it, `@` before it is kept as written without PP31).
// The expected PP80 counts for definitions that are rejected for another
// reason follow what clang 22 prints for the same line (measured with
// `clang -E`, `@@` as `##` and `@` as `#`).

class VaOptOutsideVariadicTest : public JieppTest {
protected:
    void expect_form_matrix();
    void expect_rejected_for_another_reason();
};

// The tests that use continue mode only, so they also run in sandbox builds
// (the sandbox test presets run ^(Sandbox|RobustnessTest)).
class RobustnessTestVaOptOutsideVariadic : public VaOptOutsideVariadicTest {};

namespace {

const char* const kNested =
    "<unknown location>:1.0: error: PP37: Invalid __VA_OPT__; 'nested __VA_OPT__ is not allowed'";
const char* const kNeedsParen =
    "<unknown location>:1.0: error: PP37: Invalid __VA_OPT__; '__VA_OPT__ requires parenthesized argument'";

std::string vow(const std::string& name) {
    return "<unknown location>:1.0: warning: PP80: __VA_OPT__ outside a variadic macro; '" +
           name + "'";
}

std::vector<std::string> vows(const std::string& name, int n) {
    return std::vector<std::string>(static_cast<std::size_t>(n), vow(name));
}

// "PPnn" or, for PP80, "PP80(NAME)": the shape of one diagnostic line.
std::string shape(const std::string& message) {
    const auto p = message.find(": PP");
    if (p == std::string::npos)
        return message;
    std::string s = message.substr(p + 2, 4);
    if (s == "PP80") {
        const auto q = message.rfind("; '");
        if (q != std::string::npos)
            s += "(" + message.substr(q + 3, message.size() - q - 4) + ")";
    }
    return s;
}

std::vector<std::string> shapes(const std::vector<std::string>& messages) {
    std::vector<std::string> r;
    for (const auto& m : messages)
        r.push_back(shape(m));
    return r;
}

struct Kind {
    const char* head; // what follows `{#define `, before the body
    const char* call;
    const char* name;
    bool has_params;
};

const Kind kKinds[] = {
    {"F(a)", "F(1)", "F", true},
    {"F(a, b)", "F(1, 2)", "F", true},
    {"F()", "F()", "F", false},
    {"O", "O", "O", false},
};

struct Form {
    const char* body;
    const char* with_params;    // expansion in a macro with parameters
    const char* without_params; // expansion in F() and in an object macro
    int tokens;                 // __VA_OPT__ tokens in the body
};

const Form kForms[] = {
    {"__VA_OPT__(x)", "", "__VA_OPT__(x)", 1},
    {"x @@ __VA_OPT__(y)", "x", "x__VA_OPT__(y)", 1},
    {"@__VA_OPT__(y)", "''", "@__VA_OPT__(y)", 1},
    {"<__VA_OPT__(a)>", "<>", "<__VA_OPT__(a)>", 1},
    {"__VA_OPT__(x) __VA_OPT__(y)", " ", "__VA_OPT__(x) __VA_OPT__(y)", 2},
    {"x @@ __VA_OPT__(y) @@ z", "xz", nullptr, 1},
    {"__VA_OPT__(y) @@ z", "z", nullptr, 1},
    {"x __VA_OPT__(y) @@ z", "x z", nullptr, 1},
};

} // namespace

// ---- the forms x kinds matrix ----

void VaOptOutsideVariadicTest::expect_form_matrix() {
    Issue::ContinueMode guard({});
    for (const auto& k : kKinds) {
        for (const auto& f : kForms) {
            // `@@` after the closing ')' of an identifier-like __VA_OPT__ is a
            // failed paste (PP32 at use) where __VA_OPT__ is an identifier.
            if (!k.has_params && !f.without_params)
                continue;
            SCOPED_TRACE(std::string(k.head) + " " + f.body);
            const std::string def = std::string("{#define ") + k.head + " " + f.body + "}";
            // Two calls: no diagnostic at a use, whatever their number.
            EXPECT_EQ(std::string(k.has_params ? f.with_params : f.without_params) + "|" +
                          (k.has_params ? f.with_params : f.without_params),
                      pp(def + k.call + "|" + k.call));
            EXPECT_EQ(vows(k.name, f.tokens), messages());
            EXPECT_EQ(0, Issue::error_count_);
        }
    }
}

TEST_F(VaOptOutsideVariadicTest, FormsByKind) {
    expect_form_matrix();
}

TEST_F(RobustnessTestVaOptOutsideVariadic, FormsByKind) {
    expect_form_matrix();
}

// `__VA_OPT__(y) @@ z` where __VA_OPT__ is an identifier pastes ')' and 'z':
// the existing failed-paste PP32 at use, both operands kept (clang: error
// too).
TEST_F(RobustnessTestVaOptOutsideVariadic, IdentifierFollowedByPasteIsAFailedPaste) {
    Issue::ContinueMode guard({});
    for (const char* head : {"F()", "O"}) {
        SCOPED_TRACE(head);
        const std::string call = std::string(head) == "O" ? "O" : "F()";
        EXPECT_EQ("__VA_OPT__(y)z", pp(std::string("{#define ") + head + " __VA_OPT__(y) @@ z}" + call));
        const auto ms = messages();
        ASSERT_EQ(2u, ms.size());
        EXPECT_EQ(vow(std::string(head).substr(0, 1)), ms[0]);
        EXPECT_THAT(ms[1], ::testing::HasSubstr("PP32"));
    }
}

// ---- object-only forms: nothing is checked, the text stays ----

TEST_F(RobustnessTestVaOptOutsideVariadic, ObjectMacroOnlyForms) {
    Issue::ContinueMode guard({});
    const struct {
        const char* body;
        const char* out;
        int tokens;
    } cases[] = {
        {"__VA_OPT__(__VA_OPT__(b))", "__VA_OPT__(__VA_OPT__(b))", 2},
        {"a @@ __VA_OPT__(__VA_OPT__(b))", "a__VA_OPT__(__VA_OPT__(b))", 2},
        {"x @@ __VA_OPT__ y", "x__VA_OPT__ y", 1},
        {"__VA_OPT__(a", "__VA_OPT__(a", 1},
        {"__VA_OPT__", "__VA_OPT__", 1},
        {"a @@ __VA_OPT__", "a__VA_OPT__", 1},
        {"__VA_OPT__ x", "__VA_OPT__ x", 1},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.body);
        EXPECT_EQ(c.out, pp(std::string("{#define O ") + c.body + "}O"));
        EXPECT_EQ(vows("O", c.tokens), messages());
        EXPECT_EQ(0, Issue::error_count_);
    }
}

// ---- variadic macros: no PP80, today's output (control) ----

TEST_F(RobustnessTestVaOptOutsideVariadic, VariadicMacrosAreUnchanged_Guard) {
    Issue::ContinueMode guard({});
    for (const char* head : {"M(...)", "M(args...)", "M(p, ...)"}) {
        SCOPED_TRACE(head);
        const std::string call1 = std::string(head) == "M(p, ...)" ? "M(0, 1)" : "M(1)";
        const std::string call0 = std::string(head) == "M(p, ...)" ? "M(0)" : "M()";
        const std::string def = std::string("{#define ") + head +
                                " __VA_OPT__(y) @@ z|__VA_OPT__(x)|a @@ __VA_OPT__(y)|"
                                "@__VA_OPT__(y)}";
        EXPECT_EQ("yz|x|ay|'y'", pp(def + call1));
        EXPECT_TRUE(empty());
        EXPECT_EQ("z||a|''", pp(def + call0));
        EXPECT_TRUE(empty());
    }
}

// ---- rejected non-variadic definitions: PP80 up to the problem, then PP37 ----

TEST_F(RobustnessTestVaOptOutsideVariadic, RejectedNonVariadicDefinitionOrder) {
    Issue::ContinueMode guard({});

    EXPECT_EQ("F(1)", pp("{#define F(a) __VA_OPT__(__VA_OPT__(x))}F(1)"));
    EXPECT_EQ((std::vector<std::string>{vow("F"), vow("F"), kNested}), messages());

    EXPECT_EQ("F(1)", pp("{#define F(a) __VA_OPT__ x}F(1)"));
    EXPECT_EQ((std::vector<std::string>{vow("F"), kNeedsParen}), messages());

    EXPECT_EQ("F()", pp("{#define F() __VA_OPT__}F()"));
    EXPECT_EQ((std::vector<std::string>{vow("F"), kNeedsParen}), messages());
}

// ---- PP80 against PP35 (redefinition) ----

TEST_F(RobustnessTestVaOptOutsideVariadic, WarningComesBeforeRedefinitionWarning) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("[]", pp("{#define F(a) x}{#define F(a) [__VA_OPT__(y)]}F(1)"));
    EXPECT_EQ((std::vector<std::string>{"PP80(F)", "PP35"}), shapes(messages()));
}

// An identical redefinition is still silent as far as PP35 goes, but each
// {#define} warns about its __VA_OPT__ again (clang warns at every #define).
TEST_F(RobustnessTestVaOptOutsideVariadic, IdenticalRedefinitionWarnsAgainWithoutPP35) {
    Issue::ContinueMode guard({});
    for (const char* def : {"{#define O a @@ __VA_OPT__(b)}", "{#define F(a) [__VA_OPT__(b)]}",
                            "{#define F() __VA_OPT__(b)}"}) {
        SCOPED_TRACE(def);
        const std::string name = std::string(def).find("O ") != std::string::npos ? "O" : "F";
        EXPECT_EQ("", pp(std::string(def) + def));
        EXPECT_EQ(vows(name, 2), messages());
    }
    // A different body is a redefinition: PP80 then PP35 each time.
    EXPECT_EQ("", pp("{#define O a @@ __VA_OPT__(b)}{#define O a @@ __VA_OPT__(c)}"));
    EXPECT_EQ((std::vector<std::string>{"PP80(O)", "PP80(O)", "PP35"}), shapes(messages()));
}

// A parameter named __VA_OPT__ is one more __VA_OPT__ token (clang: two
// warnings); in the body the token keeps its meaning.
TEST_F(RobustnessTestVaOptOutsideVariadic, ParameterNamedVaOptCountsAsAToken) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("[[]]", pp("{#define F(__VA_OPT__) [__VA_OPT__(x)]}[F(1)]"));
    EXPECT_EQ(vows("F", 2), messages());
    // A variadic macro with such a parameter: the parameter list still warns.
    EXPECT_EQ("[x]", pp("{#define F(__VA_OPT__, ...) [__VA_OPT__(x)]}F(1, 2)"));
    EXPECT_EQ(vows("F", 1), messages());
}

// ---- definitions rejected for another reason: what clang prints ----
//
// clang lexes the whole line with the warning enabled and keeps warning for
// the rest of a line it discards, so the tokens after the error warn too.
// (clang 22.1.3, `clang -E`; the order of the lines is clang's.) The body of
// a variadic macro never warns here, where clang warns for a GNU named
// variadic and for the rest of a rejected variadic body.

void VaOptOutsideVariadicTest::expect_rejected_for_another_reason() {
    Issue::ContinueMode guard({});
    const struct {
        const char* def; // what follows `{#define `
        std::vector<std::string> expected;
    } cases[] = {
        // duplicate parameter (PP36): the error, then the body
        {"F(a, a) __VA_OPT__(x)", {"PP36", "PP80(F)"}},
        {"F(a, a) __VA_OPT__(x) __VA_OPT__(y)", {"PP36", "PP80(F)", "PP80(F)"}},
        {"F(__VA_OPT__, __VA_OPT__) x", {"PP80(F)", "PP80(F)", "PP36"}},
        {"F(__VA_OPT__, a, __VA_OPT__) x", {"PP80(F)", "PP80(F)", "PP36"}},
        // a bad parameter list (PP30): tokens up to the bad one, the error, the rest
        {"F(a,) __VA_OPT__(x)", {"PP30", "PP80(F)"}},
        {"F(a __VA_OPT__(x)", {"PP80(F)", "PP30"}},
        {"F(a b) __VA_OPT__(x)", {"PP30", "PP80(F)"}},
        {"F(1) __VA_OPT__(x)", {"PP30", "PP80(F)"}},
        {"F(a, 1) __VA_OPT__", {"PP30", "PP80(F)"}},
        // '...' not last (PP33): variadic is not settled yet, the body warns
        {"F(a, ..., b) __VA_OPT__(x)", {"PP33", "PP80(F)"}},
        {"F(..., a) __VA_OPT__(x)", {"PP33", "PP80(F)"}},
        {"F(a..., b) __VA_OPT__(x)", {"PP33", "PP80(F)"}},
        // '@@' at either end (PP32): the body is read first
        {"F(a) @@ __VA_OPT__(x)", {"PP80(F)", "PP32"}},
        {"F(a) __VA_OPT__(x) @@", {"PP80(F)", "PP32"}},
        {"O @@ __VA_OPT__(x)", {"PP80(O)", "PP32"}},
        {"O __VA_OPT__(x) @@", {"PP80(O)", "PP32"}},
        {"F(...) __VA_OPT__(x) @@", {"PP32"}}, // variadic body: no warning
        {"F(__VA_OPT__, ...) __VA_OPT__(x) @@", {"PP80(F)", "PP32"}}, // the parameter does
        // a bad macro name: the error, then the rest of the line
        {"1 __VA_OPT__(x)", {"PP30", "PP80(1)"}},
        // a parameter that collides with __VA_ARGS__ (jiepp rejects it with PP36,
        // clang accepts the variadic macro): only the parameter list warns
        {"F(__VA_OPT__, __VA_ARGS__, ...) __VA_OPT__(x)", {"PP80(F)", "PP36"}},
        {"F(__VA_ARGS__, ...) __VA_OPT__(x)", {"PP36"}},
        // __VA_OPT__ as the macro name warns for the name token
        {"__VA_OPT__ x", {"PP80(__VA_OPT__)"}},
        {"__VA_OPT__(...) x", {"PP80(__VA_OPT__)"}},
        {"__VA_OPT__ __VA_OPT__(x)", {"PP80(__VA_OPT__)", "PP80(__VA_OPT__)"}},
        // a variadic body that is rejected for PP37: clang also warns for the
        // __VA_OPT__ after the problem, jiepp does not (the macro is variadic)
        {"F(__VA_OPT__, ...) __VA_OPT__(__VA_OPT__(x)) __VA_OPT__(y)", {"PP80(F)", "PP37"}},
        {"F(args...) __VA_OPT__(x)", {}},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.def);
        Env env = setup();
        pp(std::string("{#define ") + c.def + "}", env);
        EXPECT_EQ(c.expected, shapes(messages()));
    }
}

TEST_F(VaOptOutsideVariadicTest, RejectedForAnotherReasonFollowsClang) {
    expect_rejected_for_another_reason();
}

TEST_F(RobustnessTestVaOptOutsideVariadic, RejectedForAnotherReasonFollowsClang) {
    expect_rejected_for_another_reason();
}

// ---- modes ----

#ifndef JIEPP_SANDBOX
// {#ignore PP80} hides the warning and changes nothing else ({#ignore} is
// restricted in sandbox builds, so this is a normal-build test).
TEST_F(VaOptOutsideVariadicTest, IgnoreHidesTheWarning) {
    EXPECT_EQ("[[]]", pp("{#ignore PP80}{#define F(a) [__VA_OPT__(x)]}[F(1)]"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("[@__VA_OPT__(b)]", pp("{#ignore PP80}{#define O [@__VA_OPT__(b)]}O"));
    EXPECT_TRUE(empty());
}
#endif

// -w / --silent (library: suppress_warnings_ / silent_) hide it too.
TEST_F(RobustnessTestVaOptOutsideVariadic, SuppressedByWarningSwitches) {
    IssueStateReset reset;
    Issue::initialize(output());
    Issue::suppress_warnings_ = true;
    EXPECT_EQ("[[]]", pp("{#define F(a) [__VA_OPT__(x)]}[F(1)]"));
    EXPECT_TRUE(empty());
    Issue::suppress_warnings_ = false;
    Issue::silent_ = true;
    EXPECT_EQ("[[]]", pp("{#define F(a) [__VA_OPT__(x)]}[F(1)]"));
    EXPECT_TRUE(empty());
}

// -Werror promotes it to an error: counted, and in the library without
// continue mode thrown at the definition.
TEST_F(RobustnessTestVaOptOutsideVariadic, WerrorPromotesTheWarning) {
    IssueStateReset reset;
    Issue::initialize(output());
    Issue::werror_ = true;
    EXPECT_THROW(pp("{#define F(a) [__VA_OPT__(x)]}"), Issue::Exception);
    EXPECT_EQ((std::vector<Issue::Code>{Issue::Code::VA_OPT_OUTSIDE_VARIADIC_MACRO}), codes());
    {
        Issue::ContinueMode guard({});
        pp("{#define O __VA_OPT__(x)}");
        const auto ms = messages();
        ASSERT_EQ(1u, ms.size());
        EXPECT_THAT(ms[0], ::testing::HasSubstr("error: PP80:"));
        EXPECT_EQ(1, Issue::error_count_);
    }
}

TEST_F(RobustnessTestVaOptOutsideVariadic, SingleCodePromotionAndSeverity) {
    IssueStateReset reset;
    EXPECT_TRUE(Issue::is_warning(Issue::Code::VA_OPT_OUTSIDE_VARIADIC_MACRO));
    EXPECT_EQ("VA_OPT_OUTSIDE_VARIADIC_MACRO",
              Issue::codename(Issue::Code::VA_OPT_OUTSIDE_VARIADIC_MACRO));
    Issue::initialize(output());
    Issue::add_werror(Issue::Code::VA_OPT_OUTSIDE_VARIADIC_MACRO);
    Issue::ContinueMode guard({});
    pp("{#define F(a) [__VA_OPT__(x)]}{#define G(a) [a]}");
    const auto ms = messages();
    ASSERT_EQ(1u, ms.size());
    EXPECT_THAT(ms[0], ::testing::HasSubstr("error: PP80:"));
    EXPECT_EQ(1, Issue::error_count_);
}

// -dD echoes the definition of an accepted macro (once) and -dM lists it.
TEST_F(RobustnessTestVaOptOutsideVariadic, EchoedAndDumpedLikeAnyDefinition) {
    Issue::ContinueMode guard({});
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output = pp("{#define F(a) __VA_OPT__(x)}", env);
    EXPECT_NE(std::string::npos, output.find("{#define F(a) __VA_OPT__(x)}")) << output;
    EXPECT_EQ(vows("F", 1), messages());
    std::ostringstream os;
    dump_macros(env, os);
    EXPECT_NE(std::string::npos, os.str().find("{#define F(a) __VA_OPT__(x)}")) << os.str();
}

// A {#define} inside a macro call's argument list warns the same way.
TEST_F(RobustnessTestVaOptOutsideVariadic, DefinitionInArgumentList) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("[[]]", pp("{#define G(x) x}G({#define F(a) [__VA_OPT__(x)]})[F(1)]"));
    EXPECT_EQ(vows("F", 1), messages());
}

// ---- the defensive path in subst() ----

// A FunctionMacro built directly in C++ (not through {#define}) can still have
// a nested __VA_OPT__; subst() keeps its use-time report for it.
TEST_F(RobustnessTestVaOptOutsideVariadic, DirectlyBuiltNestedVaOptStillReportsAtUse_Guard) {
    Issue::ContinueMode guard({});
    Env env = setup();
    const FunctionMacro fm({"..."}, iec3_tokens_from_string("__VA_OPT__(__VA_OPT__(x))", false));
    const std::vector<std::vector<Token>> actuals{{Token::create(Token::ANY, "1")}};
    [[maybe_unused]] auto result = jiepp::expand_detail::subst(
        fm.body(), fm.va_opt_spans(), fm.args(), actuals, nullptr, {}, env);
    EXPECT_EQ((std::vector<std::string>{kNested}), messages());
}
