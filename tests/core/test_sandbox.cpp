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

TEST_F(SandboxDirectiveTest, MaxExpansionDepthBlocked) {
    EXPECT_THROW(pp("{#max_expansion_depth 100}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxIfNestingBlocked) {
    EXPECT_THROW(pp("{#max_if_nesting 100}"), Issue::Exception);
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
