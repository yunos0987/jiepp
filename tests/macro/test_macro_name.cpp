#include "test_helper.hpp"

#include "core/preprocessor.hpp"

// R4/D5: macro NAMEs (in {#define}/{#undef}/-D/-U/{#ifdef}/{#ifndef}/
// `defined`) must be identifiers, like gcc/clang. See
// scratchpad/design-r4.md for the full design; only "Part A" (C1..C6,
// settled by gcc/clang) is covered here.

class MacroNameTest : public JieppTest {};

// ---- C1: {#define}/{#undef} name validation --------------------------

TEST_F(MacroNameTest, DefineRejectsNonIdentifierName) {
    for (const std::string& def : {
             std::string("{#define 1 x}"),
             std::string("{#define + y}"),
             std::string("{#define %IX0 z}"),
             std::string("{#define 16#FF 1}"),
             std::string("{#define \xE5\xA4\x89\xE6\x95\xB0 1}"), // "変数"
             std::string("{#define 'a' 1}"),
             std::string("{#define (x) 1}"),
             std::string("{#define}"),
             std::string("{#define (* c *)}"),
         }) {
        SCOPED_TRACE(def);
        EXPECT_THROW(pp(def), Issue::Exception);
        EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    }
}

TEST_F(MacroNameTest, DefineRejectedNamesReportedOnceEachInContinueMode) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";1 + %IX0;", pp("{#define 1 x}{#define + y}{#define %IX0 z};1 + %IX0;"));
    auto msgs = messages();
    ASSERT_EQ(3u, msgs.size());
    for (const auto& m : msgs)
        EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, PlainTextMessage::parse_code(m)) << m;
    EXPECT_NE(std::string::npos, msgs[0].find("macro name must be an identifier: 1 x")) << msgs[0];
}

// IEC keywords, __VA_ARGS__, and a name preceded by a comment (comments are
// whitespace) all remain valid macro names -- this is a regression check,
// not new behavior.
TEST_F(MacroNameTest, ValidNamesUnaffected) {
    EXPECT_EQ(";1;", pp("{#define if 1};if;"));
    EXPECT_EQ(";1;", pp("{#define __VA_ARGS__ 1};__VA_ARGS__;"));
    EXPECT_EQ(";1;", pp("{#define (* c *) X 1};X;"));
    EXPECT_EQ(";2;", pp("{#define _x1 2};_x1;"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroNameTest, UndefRejectsNonIdentifierName) {
    for (const std::string& def : {
             std::string("{#undef 1}"),
             std::string("{#undef}"),
             std::string("{#undef %IX0}"),
             std::string("{#undef \xE5\xA4\x89\xE6\x95\xB0}"),
         }) {
        SCOPED_TRACE(def);
        EXPECT_THROW(pp(def), Issue::Exception);
        EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    }
}

// A trailing comment after the name is whitespace (tokenized away), so
// {#undef A (* c *)} undefines A cleanly -- pre-fix, the untokenized string
// "A (* c *)" matched nothing, so A silently stayed defined.
TEST_F(MacroNameTest, UndefStripsTrailingComment) {
    EXPECT_EQ(";A;", pp("{#define A 1}{#undef A (* c *)};A;"));
}

// A rejected {#undef} must not appear in -dD output (matches {#define}'s
// existing rule for a rejected directive), while an accepted one still does.
TEST_F(MacroNameTest, RejectedUndefNotEchoedUnderDD) {
    Issue::ContinueMode guard({});
    Env env = setup();
    env.set_dd_mode(true);
    std::string out = pp("{#undef 1}{#undef A}", env);
    EXPECT_EQ(std::string::npos, out.find("{#undef 1}"));
    EXPECT_NE(std::string::npos, out.find("{#undef A}"));
}

// ---- C2: {#ifdef}/{#ifndef} name validation ---------------------------

TEST_F(MacroNameTest, IfdefIfndefRejectNonIdentifierName) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("n", pp("{#ifdef 1}y{#else}n{#endif}"));
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_EQ("n", pp("{#ifndef 1}y{#else}n{#endif}"));
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    // {#ifndef}'s branch is false on an error too, so a later {#elif} can
    // still run.
    EXPECT_EQ("e", pp("{#ifdef 1}y{#elif 1}e{#else}n{#endif}"));
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_EQ("n", pp("{#ifdef}y{#else}n{#endif}"));
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_EQ("n", pp("{#ifndef}y{#else}n{#endif}"));
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
}

TEST_F(MacroNameTest, IfdefRejectsNonIdentifierNameLibraryMode) {
    EXPECT_THROW(pp("{#ifdef %IX0}y{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
}

TEST_F(MacroNameTest, IfdefIfndefRegressionCasesUnaffected) {
    // A trailing comment is whitespace.
    EXPECT_EQ("y", pp("{#define A}{#ifdef A (* c *)}y{#endif}"));
    // 'defined' is not itself a macro outside of a {#if}/{#elif} condition.
    EXPECT_EQ("n", pp("{#ifdef defined}y{#else}n{#endif}"));
    EXPECT_EQ("y", pp("{#ifdef __LINE__}y{#endif}"));
    // Not checked inside an inactive block.
    EXPECT_EQ("ok", pp("{#if 0}{#ifdef 1 2 3}{#endif}{#endif}ok"));
    EXPECT_TRUE(empty());
}

TEST_F(MacroNameTest, IfdefIfndefDoNotDisturbLineNumbering) {
    std::string out = pp("{#ifdef\nA\n}y{#endif}\n__LINE__\n{#ifndef $nA$n}y{#endif}\n__LINE__");
    EXPECT_NE(std::string::npos, out.find("4"));
    EXPECT_NE(std::string::npos, out.find("6"));
}

// ---- C3: 'defined' operand validation ----------------------------------

TEST_F(MacroNameTest, DefinedConditionsRejectNonIdentifierOperand) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("n", pp("{#if 1 \\or\\ defined 1}y{#else}n{#endif}"));
    {
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[0]);
    }
    EXPECT_EQ("n", pp("{#if defined()}y{#else}n{#endif}"));
    {
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[0]);
    }
    EXPECT_EQ("n", pp("{#if defined(A B)}y{#else}n{#endif}"));
    {
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("missing ')' after 'defined'")) << msgs[0];
    }
    EXPECT_EQ("n", pp("{#if 0}{#elif defined 1}y{#else}n{#endif}"));
    {
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[0]);
    }
    // Only the first 'defined' error in the condition is reported.
    EXPECT_EQ("n", pp("{#if defined 1 \\or\\ defined 2}y{#else}n{#endif}"));
    {
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[0]);
    }
}

// U5: an exception raised while evaluating a {#if defined ...} condition
// must not leave the temporary 'defined' operator installed in `env`.
TEST_F(MacroNameTest, DefinedOperatorNotLeakedAfterException) {
    Env env = setup();
    EXPECT_THROW(pp("{#if defined 1}{#endif}", env), Issue::Exception);
    EXPECT_NE(std::string::npos, pp("{#define X}{#if defined(X)}y{#endif}", env).find("y"));
}

// ---- C4: -D/-U processed like {#define}/{#undef} ----------------------

TEST_F(MacroNameTest, SetupProcessesPredefineMacrosLikeDefine) {
    {
        Env env = setup({{"F(x)", "x+1"}});
        EXPECT_EQ("3+1;", pp("F(3);", env));
    }
    {
        Env env = setup({{"F(x)", "1"}});
        EXPECT_EQ("1;F;", pp("F(3);F;", env));
    }
    {
        Env env = setup({{"A B", "2"}});
        EXPECT_EQ("[B 2];", pp("[A];", env));
    }
    {
        Env env = setup({{" A ", "1"}});
        EXPECT_EQ("[1];", pp("[A];", env));
    }
    {
        // Regression: an empty value still defines an empty object macro.
        Env env = setup({{"F", ""}});
        EXPECT_EQ("[];", pp("[F];", env));
    }
    for (const auto& kv : std::vector<std::pair<std::string, std::string>>{
             {"1", "2"},
             {"\xE5\xA4\x89\xE6\x95\xB0", "2"},
             {"F(a a)", "a"},
         }) {
        SCOPED_TRACE(kv.first);
        EXPECT_THROW(setup({kv}), Issue::Exception);
        EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    }
    {
        EXPECT_THROW(setup({{"defined", "1"}}), Issue::Exception);
        EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
    }
    {
        Env env = setup({{"A", "1"}, {"A", "2"}});
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::MACRO_REDEFINED, cs[0]);
        EXPECT_EQ("[2];", pp("[A];", env));
    }
}

// ---- C4/C5: jiepp_command()-level checks live in test_jiepp_command.cpp ---
