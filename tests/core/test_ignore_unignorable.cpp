#include "test_helper.hpp"

// PP10-PP14 (file / include failures) cannot be suppressed by {#ignore}
// (SPEC 10): suppressing one would silently drop an input file or hide a
// failed output write. These tests cover the library level (default,
// non-continue mode) and the Issue API; the CLI level (-o / -MF under
// {#ignore PP10}) is in test_smoke.cpp.

class IgnoreUnignorableTest : public JieppTest {};

TEST_F(IgnoreUnignorableTest, AddIgnoringHasNoEffectOnPP10ToPP14) {
    const Issue::Code codes[] = {
        Issue::Code::FILE_ERROR,                  // PP10
        Issue::Code::FILE_NOT_FOUND,              // PP11
        Issue::Code::MAX_INCLUDE_DEPTH_EXCEEDED,  // PP12
        Issue::Code::INVALID_COMMAND,             // PP13
        Issue::Code::INCLUDE_TARGET_IS_DIRECTORY, // PP14
    };
    for (Issue::Code c : codes) {
        EXPECT_FALSE(Issue::is_ignored(c)) << "before, PP" << static_cast<int>(c);
        Issue::add_ignoring(c);
        EXPECT_FALSE(Issue::is_ignored(c)) << "after, PP" << static_cast<int>(c);
    }
}

TEST_F(IgnoreUnignorableTest, AddIgnoringStillWorksForOtherErrorCodes) {
    EXPECT_FALSE(Issue::is_ignored(Issue::Code::UNKNOWN_DIRECTIVE));
    Issue::add_ignoring(Issue::Code::UNKNOWN_DIRECTIVE);
    EXPECT_TRUE(Issue::is_ignored(Issue::Code::UNKNOWN_DIRECTIVE));
}

TEST_F(IgnoreUnignorableTest, HappenStillThrowsForIgnoredPP10AndPP13) {
    // PP10 and PP13 have no library-level trigger; raise them directly.
    Issue::add_ignoring(Issue::Code::FILE_ERROR);
    Issue::add_ignoring(Issue::Code::INVALID_COMMAND);
    EXPECT_THROW(Issue::happen(Issue::Code::FILE_ERROR, "x"), Issue::Exception);
    EXPECT_THROW(Issue::happen(Issue::Code::INVALID_COMMAND, "x"), Issue::Exception);
    EXPECT_EQ(2u, codes().size());
}

#ifndef JIEPP_SANDBOX
TEST_F(IgnoreUnignorableTest, IgnoredPP11MissingIncludeStillFails) {
    fs::current_path(jiepp_root_dir());
    EXPECT_THROW(pp("{#ignore PP11}{#include 'nonexistent_file.iec'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::FILE_NOT_FOUND, code());
}

TEST_F(IgnoreUnignorableTest, IgnoredPP14DirectoryIncludeStillFails) {
    fs::current_path(jiepp_root_dir());
    EXPECT_THROW(pp("{#ignore PP14}{#include 'tests/core/test_include'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INCLUDE_TARGET_IS_DIRECTORY, code());
}

TEST_F(IgnoreUnignorableTest, IgnoredPP12IncludeDepthStillFails) {
    fs::current_path(jiepp_root_dir());
    Env env = setup();
    env.set_max_include_depth(8);
    EXPECT_THROW(pp("{#ignore PP12}{#include 'tests/core/test_include/circular_a.iec'}", env),
                 Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_INCLUDE_DEPTH_EXCEEDED, code());
}

TEST_F(IgnoreUnignorableTest, IgnorePP11ItselfIsAcceptedSilently) {
    EXPECT_NO_THROW(pp("{#ignore PP11}x"));
    EXPECT_TRUE(empty());
}

TEST_F(IgnoreUnignorableTest, IgnoreOfOtherCodesStillSuppresses) {
    // Control: PP41 is still suppressed by {#ignore}.
    EXPECT_NO_THROW(pp("{#ignore PP41}{#line x}"));
    EXPECT_TRUE(empty());
}
#endif
