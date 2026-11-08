#include "test_helper.hpp"

#include <fstream>
#include <string>
#include <vector>

// =====================================================================
// {#pragma push_macro('NAME')} / {#pragma pop_macro('NAME')}
// (SPECIFICATION.md 3.8): push saves NAME's current definition (or
// "undefined") on a per-name stack; pop restores it exactly, silently. An
// empty stack makes pop a no-op. A malformed operand is PP82 (ERROR) and the
// pragma is ignored; tokens after ')' are PP49 and the pragma still applies.
// =====================================================================

class PragmaPushPopTest : public JieppTest {};

// ---- [C1] 1-5: basic save and restore ----

TEST_F(PragmaPushPopTest, PopRestoresRedefinedMacro) {
    EXPECT_EQ("21", pp("{#define X 1}{#pragma push_macro('X')}{#undef X}{#define X 2}X"
                       "{#pragma pop_macro('X')}X"));
    // [C3]: restoring over another definition is not a redefinition (PP35).
    EXPECT_TRUE(empty());
}

TEST_F(PragmaPushPopTest, PopRestoresUndefinedState) {
    EXPECT_EQ("X", pp("{#pragma push_macro('X')}{#define X 1}{#pragma pop_macro('X')}X"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaPushPopTest, TwoPushesRestoreTwoLevels) {
    EXPECT_EQ("321", pp("{#define W 1}{#pragma push_macro('W')}"
                        "{#undef W}{#define W 2}{#pragma push_macro('W')}"
                        "{#undef W}{#define W 3}W"
                        "{#pragma pop_macro('W')}W"
                        "{#pragma pop_macro('W')}W"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaPushPopTest, PopWithEmptyStackIsSilentNoOp) {
    EXPECT_EQ("", pp("{#pragma pop_macro('X')}"));
    EXPECT_TRUE(empty());
    // The definition is left alone.
    EXPECT_EQ("1", pp("{#define X 1}{#pragma pop_macro('X')}X"));
    EXPECT_TRUE(empty());
    // A pop beyond the pushes does not undo the restored definition.
    EXPECT_EQ("1", pp("{#define X 1}{#pragma push_macro('X')}{#undef X}"
                      "{#pragma pop_macro('X')}{#pragma pop_macro('X')}X"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaPushPopTest, DoubleQuotesBehaveTheSame) {
    EXPECT_EQ("21", pp("{#define X 1}{#pragma push_macro(\"X\")}{#undef X}{#define X 2}X"
                       "{#pragma pop_macro(\"X\")}X"));
    // The two quote forms share one stack.
    EXPECT_EQ("1", pp("{#define X 1}{#pragma push_macro(\"X\")}{#undef X}"
                      "{#pragma pop_macro('X')}X"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaPushPopTest, BlanksAndCommentsAroundOperand) {
    // The three newlines inside the two pragmas are kept as line breaks.
    EXPECT_EQ("\n\n\n1", pp("{#define X 1}{#pragma push_macro (* a *) ( 'X' ) // b\n}{#undef X}"
                            "{#pragma pop_macro(\n'X'\n)}X"));
    EXPECT_TRUE(empty());
}

// ---- [C1] 6: one stack set across files ----

namespace {

class PragmaPushPopIncludeTest : public JieppTest {
protected:
    void SetUp() override {
        JieppTest::SetUp();
        dir_ = fs::temp_directory_path() / "jiepp_pragma_push_pop";
        std::error_code ec;
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
        JieppTest::TearDown();
    }

    static void write(const fs::path& p, const std::string& text) {
        std::ofstream f(p, std::ios::binary);
        f << text;
    }

    fs::path dir_;
};

} // namespace

TEST_F(PragmaPushPopIncludeTest, PushInIncludedFilePopInIncluder) {
    write(dir_ / "h.iec", "{#pragma push_macro('X')}{#undef X}{#define X 2}");
    write(dir_ / "m.iec", "{#define X 1}{#include 'h.iec'}A=X;{#pragma pop_macro('X')}B=X;");
    const std::string out = pp_file(dir_ / "m.iec");
    EXPECT_NE(std::string::npos, out.find("A=2;")) << out;
    EXPECT_NE(std::string::npos, out.find("B=1;")) << out;
    EXPECT_TRUE(empty());
}

// ---- [C1] 7: built-in macros, restored silently ----

TEST_F(PragmaPushPopTest, BuiltinLineRestoredWithOnlyTheUndefWarning) {
    EXPECT_EQ("1", pp("{#pragma push_macro('__LINE__')}{#undef __LINE__}"
                      "{#pragma pop_macro('__LINE__')}__LINE__"));
    // Exactly one PP35, from {#undef}; none from the pop ([C3]).
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, PlainTextMessage::parse_code(msgs[0]));
    EXPECT_NE(std::string::npos, msgs[0].find("__LINE__: undefining built-in macro")) << msgs[0];
}

// Pop puts back the very same object, so __COUNTER__ continues from where
// it was (gcc/clang keep one counter too).
TEST_F(PragmaPushPopTest, BuiltinCounterContinuesAfterRestore) {
    EXPECT_EQ("0 1", pp("__COUNTER__{#pragma push_macro('__COUNTER__')}{#undef __COUNTER__}"
                        "{#pragma pop_macro('__COUNTER__')} __COUNTER__"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, PlainTextMessage::parse_code(msgs[0]));
}

// ---- [C1] 8: malformed operand ----

TEST_F(PragmaPushPopTest, MalformedPushIsPP82AndIgnored) {
    Issue::ContinueMode guard({});
    // Two literals the lexer does not join (different quotes, or a comment
    // between them) are two operands.
    for (const std::string op : {"push_macro", "push_macro(X)", "push_macro('X' \"Y\")",
                                 "push_macro('X' (*c*) 'Y')", "push_macro('X'"}) {
        SCOPED_TRACE(op);
        // If the push happened, the pop would restore 1.
        EXPECT_EQ("2", pp("{#define X 1}{#pragma " + op + "}{#undef X}{#define X 2}"
                          "{#pragma pop_macro('X')}X"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_PUSH_POP_MACRO, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_TRUE(Issue::is_error(Issue::Code::INVALID_PUSH_POP_MACRO));
    }
}

TEST_F(PragmaPushPopTest, MalformedPopIsPP82AndIgnored) {
    Issue::ContinueMode guard({});
    for (const std::string op : {"pop_macro", "pop_macro(X)", "pop_macro('X' \"Y\")",
                                 "pop_macro('X' (*c*) 'Y')", "pop_macro('X'"}) {
        SCOPED_TRACE(op);
        // The malformed pop changes nothing; the stack still holds 1.
        EXPECT_EQ("21", pp("{#define X 1}{#pragma push_macro('X')}{#undef X}{#define X 2}"
                           "{#pragma " + op + "}X{#pragma pop_macro('X')}X"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_PUSH_POP_MACRO, PlainTextMessage::parse_code(msgs[0]));
    }
}

TEST_F(PragmaPushPopTest, AdjacentLiteralsAreJoinedIntoOneName) {
    // The lexer joins adjacent literals of the same quote (SPEC 2), so
    // ('X' 'Y') names XY, like 'XY' (gcc/clang reject two strings).
    EXPECT_EQ("1", pp("{#define XY 1}{#pragma push_macro('X' 'Y')}{#undef XY}"
                      "{#pragma pop_macro('XY')}XY"));
    EXPECT_TRUE(empty());
}

// ---- [C1] 9: a string that is not an identifier ----

TEST_F(PragmaPushPopTest, NonIdentifierStringIsSilentNoOp) {
    // None of the three touches X's stack: the last pop restores 1.
    EXPECT_EQ("1", pp("{#define X 1}{#pragma push_macro('X')}"
                      "{#pragma push_macro('1')}{#pragma pop_macro('X Y')}{#pragma push_macro('')}"
                      "{#undef X}{#pragma pop_macro('X')}X"));
    EXPECT_TRUE(empty());
}

// ---- [C1] 10: extra tokens after ')' ----

TEST_F(PragmaPushPopTest, ExtraTokensWarnAndPushStillApplies) {
    EXPECT_EQ("1", pp("{#define X 1}{#pragma push_macro('X') y}{#undef X}"
                      "{#pragma pop_macro('X')}X"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, PlainTextMessage::parse_code(msgs[0]));
}

// ---- [C1] 11: inside a function-like macro's argument list ----

TEST_F(PragmaPushPopTest, InsideMacroArgumentIsNotPP02) {
    EXPECT_EQ("[]1", pp("{#define F(a) [a]}{#define X 1}F({#pragma push_macro('X')})"
                        "{#undef X}{#pragma pop_macro('X')}X"));
    EXPECT_TRUE(empty());
}

// A pop inside the argument list of the very macro it restores: the call
// already in progress keeps using the definition it looked up (Symtab
// lifetime invariant), the next call uses the restored one.
TEST_F(PragmaPushPopTest, PopInsideArgumentOfTheRestoredMacro) {
    EXPECT_EQ("[x]<y>", pp("{#define F(a) <a>}{#pragma push_macro('F')}{#undef F}"
                           "{#define F(a) [a]}F({#pragma pop_macro('F')}x)F(y)"));
    EXPECT_TRUE(empty());
}

// ---- [C1] 13: 'defined' ----

TEST_F(PragmaPushPopTest, DefinedIsUnaffected) {
    EXPECT_EQ("ok", pp("{#define A 1}{#pragma push_macro('defined')}{#pragma push_macro('A')}"
                       "{#undef A}{#pragma pop_macro('A')}{#pragma pop_macro('defined')}"
                       "{#if defined(A)}ok{#endif}"));
    EXPECT_TRUE(empty());
}

// ---- [C1] 14: a function-like macro keeps its parameters ----

TEST_F(PragmaPushPopTest, FunctionLikeMacroKeepsParameters) {
    EXPECT_EQ("1+2", pp("{#define G(a,b) a+b}{#pragma push_macro('G')}{#undef G}{#define G 0}"
                        "{#pragma pop_macro('G')}G(1,2)"));
    EXPECT_TRUE(empty());
}

// ---- other rules ----

TEST_F(PragmaPushPopTest, InactiveGroupDoesNothing) {
    EXPECT_EQ("5", pp("{#if 0}{#pragma push_macro('Q')}{#endif}{#define Q 5}"
                      "{#pragma pop_macro('Q')}Q"));
    EXPECT_TRUE(empty());
}

// [C2]: an unknown pragma name is still dropped silently.
TEST_F(PragmaPushPopTest, UnknownPragmaStillSilent) {
    EXPECT_EQ("", pp("{#pragma foo}{#pragma push_macroX('X')}"));
    EXPECT_TRUE(empty());
}
