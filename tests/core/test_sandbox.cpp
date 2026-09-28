#include "test_helper.hpp"

// =====================================================================
// Sandbox mode tests — only active when JIEPP_SANDBOX is defined.
// In a non-sandbox build these tests compile but are empty.
// =====================================================================

#ifdef JIEPP_SANDBOX

class SandboxDirectiveTest : public JieppTest {};

// ---- Blocked directives produce SANDBOX_RESTRICTED_DIRECTIVE ----

TEST_F(SandboxDirectiveTest, IncludeBlocked) {
    EXPECT_THROW(pp("{#include 'dummy.iec'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, SincludeBlocked) {
    EXPECT_THROW(pp("{#sinclude 'dummy.iec'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, SyspathBlocked) {
    EXPECT_THROW(pp("{#syspath '/tmp'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxIncludeDepthBlocked) {
    EXPECT_THROW(pp("{#max_include_depth 10}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, IgnoreBlocked) {
    EXPECT_THROW(pp("{#ignore PP41}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, HasIncludeBlocked) {
    EXPECT_THROW(pp("{#if __has_include('dummy.iec')}YES{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

// ---- R4 follow-up U8: PP62 only for an actual __has_include(...) use ----

// X1: __has_include reads as undefined in sandbox mode, so {#ifdef}/
// {#ifndef} do not need the filesystem probe and are not PP62.
TEST_F(SandboxDirectiveTest, HasIncludeUndefinedForIfdefIfndef) {
    {
        auto r = pp("{#ifdef __has_include}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#ifndef __has_include}y{#else}n{#endif}");
        EXPECT_EQ("y", r);
        EXPECT_TRUE(empty());
    }
}

// X2: likewise 'defined(__has_include)'/'defined __has_include' are feature
// tests, not uses, so they are not PP62 either.
TEST_F(SandboxDirectiveTest, HasIncludeUndefinedForDefined) {
    {
        auto r = pp("{#if defined(__has_include)}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#if defined __has_include}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
}

// X3: a user macro whose name happens to end in "__has_include" is not the
// operator and must not be blocked.
TEST_F(SandboxDirectiveTest, KeywordSuffixMacroNotBlocked) {
    auto r = pp("{#define weird__has_include(x) 99}"
                "{#if weird__has_include('foo') = 99}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_EQ(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

// X4: regression -- an actual __has_include(...) use is still PP62.
TEST_F(SandboxDirectiveTest, HasIncludeCallStillBlocked) {
    EXPECT_THROW(pp("{#if __has_include ('dummy.iec')}YES{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxExpansionDepthBlocked) {
    EXPECT_THROW(pp("{#max_expansion_depth 100}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxIfNestingBlocked) {
    EXPECT_THROW(pp("{#max_if_nesting 100}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxBlankLinesBlocked) {
    EXPECT_THROW(pp("{#max_blank_lines 2}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

// ---- Allowed directives still work in sandbox ----

TEST_F(SandboxDirectiveTest, DefineAllowed) {
    auto result = pp("{#define N 42};N");
    EXPECT_EQ(";42", result);
    EXPECT_TRUE(empty());
}

TEST_F(SandboxDirectiveTest, UndefAllowed) {
    auto result = pp("{#define N 42}{#undef N};N");
    EXPECT_EQ(";N", result);
    EXPECT_TRUE(empty());
}

TEST_F(SandboxDirectiveTest, IfElseAllowed) {
    auto result = pp("{#if 1}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, result.find("YES"));
    EXPECT_EQ(std::string::npos, result.find("NO"));
    EXPECT_TRUE(empty());
}

TEST_F(SandboxDirectiveTest, ErrorAllowed) {
    EXPECT_THROW(pp("{#error test message}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

TEST_F(SandboxDirectiveTest, WarningAllowed) {
    pp("{#warning test message}");
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, code());
}

// ---- Info disclosure prevention ----

class SandboxInfoLeakTest : public JieppTest {};

TEST_F(SandboxInfoLeakTest, TimestampEmpty) {
    auto result = pp("__TIMESTAMP__");
    EXPECT_NE(std::string::npos, result.find("''"));
    EXPECT_TRUE(empty());
}

TEST_F(SandboxInfoLeakTest, SetlineIgnoresFilepath) {
    // {#setline} with a filepath arg should not change the filename in errors
    EXPECT_THROW(pp("{#line 10 'secret/path.iec'}{#error test}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
    // The error output should NOT contain the secret path
    for (const auto& msg : messages()) {
        EXPECT_EQ(std::string::npos, msg.find("secret/path.iec"))
            << "filepath should be ignored in sandbox: " << msg;
    }
}

// ---- Allowed macros still work in sandbox ----

TEST_F(SandboxInfoLeakTest, CounterWorks) {
    auto result = pp("__COUNTER__;__COUNTER__");
    // __COUNTER__ produces Token::ANY "0", "1", ...
    EXPECT_NE(std::string::npos, result.find("0"));
    EXPECT_NE(std::string::npos, result.find("1"));
    EXPECT_TRUE(empty());
}

TEST_F(SandboxInfoLeakTest, DateTimeWork) {
    // __DATE__ and __TIME__ should still produce non-empty strings
    auto result_d = pp("__DATE__");
    auto result_t = pp("__TIME__");
    EXPECT_FALSE(result_d.empty());
    EXPECT_FALSE(result_t.empty());
}

#endif  // JIEPP_SANDBOX
