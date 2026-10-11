#include "test_helper.hpp"

#include <string>
#include <vector>

// -Wundef (PP54, SPEC 5.3): an identifier in a condition that evaluates to 0
// is reported when the code was switched on with Issue::enable(); the right
// operand of \and\ / \or\ that is not evaluated is not reported (SPEC 6.4).
//
// Messages in the inputs have no apostrophes (they would end the quoted
// context of the diagnostic).

using ::testing::ElementsAre;
using ::testing::HasSubstr;

class WundefTest : public JieppTest {
protected:
    // Default-off state is process wide: leave it clean for other fixtures.
    void TearDown() override {
        Issue::initialize(std::cerr);
        JieppTest::TearDown();
    }

    static void enable() { Issue::enable(Issue::Code::UNDEFINED_IDENTIFIER); }

    // One tag per diagnostic: "PP54 NAME" for -Wundef, "PPnn" otherwise.
    std::vector<std::string> tags() {
        std::vector<std::string> out;
        for (const auto& line : messages()) {
            auto code = PlainTextMessage::parse_code(line);
            std::string tag = "PP" + std::to_string(static_cast<unsigned>(code));
            if (code == Issue::Code::UNDEFINED_IDENTIFIER) {
                auto a = line.find('\'');
                auto b = line.find("' is not defined");
                tag += " " + line.substr(a + 1, b - a - 1);
            }
            out.push_back(tag);
        }
        return out;
    }

    // Names reported by PP54 for the condition `cond` (evaluated true or
    // false, the group body is irrelevant).
    std::vector<std::string> undefs(const std::string& cond, Env& env) {
        pp("{#if " + cond + "}{#endif}", env);
        return tags();
    }
    std::vector<std::string> undefs(const std::string& cond) {
        Env env = setup();
        return undefs(cond, env);
    }
};

TEST_F(WundefTest, OffByDefault) {
    EXPECT_EQ("n", pp("{#if FOO}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(WundefTest, WarnsUndefinedIdentifier) {
    enable();
    EXPECT_EQ("n", pp("{#if FOO}y{#else}n{#endif}"));
    EXPECT_EQ("<unknown location>:1.0: warning: PP54: 'FOO' is not defined, evaluates to 0",
              message());
}

TEST_F(WundefTest, WarnsAfterExpansion) {
    enable();
    Env env = setup();
    EXPECT_THAT(undefs("A", env), ElementsAre("PP54 A"));  // not defined yet
    pp("{#define A B}", env);
    EXPECT_THAT(undefs("A", env), ElementsAre("PP54 B"));
    pp("{#define F(x) x}", env);
    EXPECT_THAT(undefs("F", env), ElementsAre("PP54 F"));
    pp("{#define N N}", env);
    EXPECT_THAT(undefs("N", env), ElementsAre("PP54 N"));
}

TEST_F(WundefTest, NoWarningForOperatorsAndLiterals) {
    enable();
    Env env = setup();
    for (const char* cond : {
             "defined(FOO)", "defined FOO", "TRUE", "true", "FALSE", "false",
             "not 1 = DWORD#16#FFFFFFFE", "5 mod 2 = 1", "BYTE#16#1 = BYTE#1",
             "__has_builtin(FOO)",
#ifndef JIEPP_SANDBOX
             "__has_include('nofile.iec')",
#endif
         }) {
        SCOPED_TRACE(cond);
        EXPECT_THAT(undefs(cond, env), ElementsAre());
    }
}

TEST_F(WundefTest, BareTypeKeyword) {
    enable();
    EXPECT_THAT(undefs("byte"), ElementsAre("PP54 byte"));
    EXPECT_THAT(undefs("BOOL"), ElementsAre("PP54 BOOL"));
    EXPECT_THAT(undefs("DWord"), ElementsAre("PP54 DWord"));
}

TEST_F(WundefTest, MixedCaseTrue) {
    enable();
    EXPECT_THAT(undefs("True"), ElementsAre("PP54 True"));
}

TEST_F(WundefTest, ElifAndSkippedGroups) {
    enable();
    Env env = setup();
    pp("{#if 1}{#elif FOO}{#endif}", env);
    EXPECT_TRUE(empty());
    pp("{#if 0}{#if FOO}{#endif}{#endif}", env);
    EXPECT_TRUE(empty());
    pp("{#if 0}{#elif FOO}{#endif}", env);
    EXPECT_THAT(tags(), ElementsAre("PP54 FOO"));
}

TEST_F(WundefTest, AndRightUnevaluated) {
    enable();
    Env env = setup();
    for (const char* cond : {"FALSE \\and\\ FOO", "0 \\and\\ FOO", "defined(BAR) \\and\\ BAR"}) {
        SCOPED_TRACE(cond);
        EXPECT_THAT(undefs(cond, env), ElementsAre());
    }
    for (const char* cond : {"TRUE \\and\\ FOO", "1 \\and\\ FOO"}) {
        SCOPED_TRACE(cond);
        EXPECT_THAT(undefs(cond, env), ElementsAre("PP54 FOO"));
    }
}

TEST_F(WundefTest, OrRightUnevaluated) {
    enable();
    Env env = setup();
    for (const char* cond : {"TRUE \\or\\ FOO", "1 \\or\\ FOO"}) {
        SCOPED_TRACE(cond);
        EXPECT_THAT(undefs(cond, env), ElementsAre());
    }
    EXPECT_THAT(undefs("FALSE \\or\\ FOO", env), ElementsAre("PP54 FOO"));
}

TEST_F(WundefTest, LeftAlwaysEvaluated) {
    enable();
    Env env = setup();
    EXPECT_THAT(undefs("FOO \\and\\ BAR", env), ElementsAre("PP54 FOO"));
    EXPECT_THAT(undefs("FOO \\or\\ BAR", env), ElementsAre("PP54 FOO", "PP54 BAR"));
}

TEST_F(WundefTest, NestedShortCircuit) {
    enable();
    Env env = setup();
    EXPECT_THAT(undefs("FALSE \\and\\ (X \\or\\ Y)", env), ElementsAre());
    EXPECT_THAT(undefs("TRUE \\or\\ (X \\and\\ Y)", env), ElementsAre());
    EXPECT_THAT(undefs("TRUE \\and\\ (X \\or\\ Y)", env), ElementsAre("PP54 X", "PP54 Y"));
    EXPECT_THAT(undefs("X \\or\\ (FALSE \\and\\ Y)", env), ElementsAre("PP54 X"));
    EXPECT_THAT(undefs("(FALSE \\and\\ X) \\or\\ Y", env), ElementsAre("PP54 Y"));
    EXPECT_THAT(undefs("TRUE \\and\\ (FALSE \\and\\ X)", env), ElementsAre());
    // Chains: the depth is lowered again after each dead operand.
    EXPECT_THAT(undefs("FALSE \\and\\ X \\and\\ Y", env), ElementsAre());
    EXPECT_THAT(undefs("(FALSE \\and\\ X) \\or\\ (TRUE \\or\\ Y) \\or\\ Z", env), ElementsAre());
    EXPECT_THAT(undefs("(FALSE \\and\\ X) \\or\\ Y", env), ElementsAre("PP54 Y"));
}

TEST_F(WundefTest, NonShortCircuitOperators) {
    enable();
    Env env = setup();
    EXPECT_THAT(undefs("FALSE \\xor\\ (FOO = 0)", env), ElementsAre("PP54 FOO"));
    EXPECT_THAT(undefs("FALSE and (FOO = 0)", env), ElementsAre("PP54 FOO"));
    EXPECT_THAT(undefs("\\not\\ FOO", env), ElementsAre("PP54 FOO"));
}

TEST_F(WundefTest, FloatLeftIsLive) {
    enable();
    Issue::ContinueMode guard({});
    EXPECT_THAT(undefs("1.5 \\and\\ FOO"), ElementsAre("PP54 FOO", "PP50"));
}

TEST_F(WundefTest, AssertAndCounterSet) {
    enable();
    Issue::ContinueMode guard({});
    Env env = setup();
    pp("__assert(FOO, m)", env);
    EXPECT_THAT(tags(), ElementsAre("PP54 FOO", "PP91"));
    pp("__assert(TRUE \\or\\ FOO, m)", env);
    EXPECT_TRUE(empty());
    EXPECT_EQ("1", pp("__counter_set(c, FOO + 1)__counter_peek(c)", env));
    EXPECT_THAT(tags(), ElementsAre("PP54 FOO"));
}

#ifndef JIEPP_SANDBOX
TEST_F(WundefTest, IgnoreSuppresses) {
    enable();
    EXPECT_EQ("n", pp("{#ignore PP54}{#if FOO}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}
#endif

TEST_F(WundefTest, SuppressionAndPromotion) {
    enable();
    Issue::ContinueMode guard({});

    Issue::suppress_warnings_ = true;
    pp("{#if FOO}{#endif}");
    EXPECT_TRUE(empty());
    EXPECT_EQ(0, Issue::error_count_);
    Issue::suppress_warnings_ = false;

    // -Werror alone does not switch the warning on.
    Issue::initialize(output());
    Issue::werror_ = true;
    pp("{#if FOO}{#endif}");
    EXPECT_TRUE(empty());
    EXPECT_EQ(0, Issue::error_count_);

    // Switched on, -Werror promotes it.
    enable();
    Issue::ContinueMode guard2({});
    pp("{#if FOO}{#endif}");
    EXPECT_THAT(message(), HasSubstr("error: PP54: 'FOO' is not defined, evaluates to 0"));
    EXPECT_EQ(1, Issue::error_count_);
}

TEST_F(WundefTest, SilentHides) {
    enable();
    Issue::silent_ = true;
    pp("{#if FOO}{#endif}");
    EXPECT_TRUE(empty());
}

TEST_F(WundefTest, SyntaxErrorAfterIdentifier) {
    enable();
    Issue::ContinueMode guard({});
    EXPECT_THAT(undefs("FOO 1"), ElementsAre("PP54 FOO", "PP52"));
}

TEST_F(WundefTest, DefinedOperandErrorSkipsEvaluation) {
    enable();
    Issue::ContinueMode guard({});
    EXPECT_THAT(undefs("defined 1 \\or\\ FOO"), ElementsAre("PP40"));
}

// A parse error inside a dead operand aborts before the lowering action runs;
// the next condition must not be treated as unevaluated.
TEST_F(WundefTest, ParseErrorInDeadOperandLeavesNoStaleDepth) {
    enable();
    Issue::ContinueMode guard({});
    Env env = setup();
    EXPECT_THAT(undefs("FALSE \\and\\ BAR 1", env), ElementsAre("PP52"));
    EXPECT_THAT(undefs("X1", env), ElementsAre("PP54 X1"));
}

TEST_F(WundefTest, ReportsDirectiveLine) {
    enable();
    Issue::ContinueMode guard({});
    pp("\n\n{#if FOO}{#endif}");
    EXPECT_EQ("<unknown location>:3.0: warning: PP54: 'FOO' is not defined, evaluates to 0",
              message());

    // A multi-line __assert call reports at the line of its closing paren.
    Env env = setup();
    pp("__assert(\nFOO,\nm\n)", env);
    auto msgs = messages();
    ASSERT_EQ(2u, msgs.size());
    EXPECT_THAT(msgs[0], HasSubstr(":4.0: warning: PP54: 'FOO'"));
    EXPECT_THAT(msgs[1], HasSubstr(":4.0: error: PP91"));
}

// Typed literals and casts keep working with the spelling kept by the lexer.
TEST_F(WundefTest, CastsAcceptAnyCase) {
    enable();
    Env env = setup();
    EXPECT_EQ("y", pp("{#if byte#1 = BYTE#1}y{#endif}", env));
    EXPECT_EQ("y", pp("{#if Dword#16#FF = DWORD#255}y{#endif}", env));
    EXPECT_EQ("y", pp("{#if bool#1}y{#endif}", env));
    EXPECT_EQ("y", pp("{#if wOrD#2 = WORD#2}y{#endif}", env));
    EXPECT_EQ("y", pp("{#if lword#3 = LWORD#3}y{#endif}", env));
    EXPECT_TRUE(empty());
}
