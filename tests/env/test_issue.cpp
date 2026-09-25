#include "test_helper.hpp"
#include "env/issue.hpp"
#include <sstream>

class IssueTest : public JieppTest {};

TEST_F(IssueTest, IsSevere) {
    EXPECT_TRUE(Issue::is_severe(Issue::Code::FATAL));
    EXPECT_TRUE(Issue::is_severe(Issue::Code::OPERATION_NOT_ALLOWED));
    EXPECT_FALSE(Issue::is_severe(Issue::Code::FILE_NOT_FOUND));
    EXPECT_FALSE(Issue::is_severe(Issue::Code::WARNING_MESSAGE));
    EXPECT_FALSE(Issue::is_severe(Issue::Code::INFO_MESSAGE));
}

TEST_F(IssueTest, IsError) {
    EXPECT_TRUE(Issue::is_error(Issue::Code::FILE_NOT_FOUND));
    EXPECT_TRUE(Issue::is_error(Issue::Code::EXPR_TYPE_ERROR));
    EXPECT_FALSE(Issue::is_error(Issue::Code::FATAL));
    EXPECT_FALSE(Issue::is_error(Issue::Code::WARNING_MESSAGE));
    EXPECT_FALSE(Issue::is_error(Issue::Code::INFO_MESSAGE));
}

TEST_F(IssueTest, IsWarning) {
    EXPECT_TRUE(Issue::is_warning(Issue::Code::WARNING_MESSAGE));
    EXPECT_TRUE(Issue::is_warning(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND));
    EXPECT_FALSE(Issue::is_warning(Issue::Code::FILE_NOT_FOUND));
    EXPECT_FALSE(Issue::is_warning(Issue::Code::FATAL));
    EXPECT_FALSE(Issue::is_warning(Issue::Code::INFO_MESSAGE));
}

TEST_F(IssueTest, IsInfo) {
    EXPECT_TRUE(Issue::is_info(Issue::Code::INFO_MESSAGE));
    EXPECT_FALSE(Issue::is_info(Issue::Code::WARNING_MESSAGE));
    EXPECT_FALSE(Issue::is_info(Issue::Code::FILE_NOT_FOUND));
    EXPECT_FALSE(Issue::is_info(Issue::Code::FATAL));
}

TEST_F(IssueTest, IsFatal) {
    EXPECT_TRUE(Issue::is_fatal(Issue::Code::FATAL));
    EXPECT_FALSE(Issue::is_fatal(Issue::Code::FILE_NOT_FOUND));
}

TEST_F(IssueTest, Codename) {
    EXPECT_EQ("FILE_NOT_FOUND", Issue::codename(Issue::Code::FILE_NOT_FOUND));
    EXPECT_EQ("EXPR_TYPE_ERROR", Issue::codename(Issue::Code::EXPR_TYPE_ERROR));
    EXPECT_EQ("FATAL", Issue::codename(Issue::Code::FATAL));
}

TEST_F(IssueTest, RetiredCode3Unassigned) {
    // PP03 was retired (U3); the enum value is no longer assigned to any
    // code, so codename() falls through to the empty default.
    EXPECT_EQ("", Issue::codename(Issue::Code(3)));
}

TEST_F(IssueTest, Initialize) {
    std::ostringstream local_stream;
    Issue::initialize(local_stream);
    Issue::push({1, "test.iec"});
    EXPECT_THROW(ISSUE(FILE_NOT_FOUND), Issue::Exception);
    Issue::pop();
    EXPECT_FALSE(local_stream.str().empty());
}

TEST_F(IssueTest, SetOutput) {
    std::ostringstream local_stream;
    Issue::set_output(local_stream);
    Issue::push({1, "test.iec"});
    EXPECT_THROW(ISSUE(FILE_NOT_FOUND), Issue::Exception);
    Issue::pop();
    EXPECT_FALSE(local_stream.str().empty());
}

TEST_F(IssueTest, PushPopTop) {
    Issue::LocationEntry loc{42, "foo.iec"};
    Issue::push(loc);
    EXPECT_EQ(42, Issue::top().first);
    EXPECT_EQ("foo.iec", Issue::top().second);
    auto popped = Issue::pop();
    EXPECT_EQ(42, popped.first);
    EXPECT_EQ("foo.iec", popped.second);
    //
    auto empty = Issue::pop();
    EXPECT_EQ(1, empty.first);
    EXPECT_EQ("<unknown location>", empty.second);
}

TEST_F(IssueTest, FilepathAndLineno) {
    Issue::push({10, "bar.iec"});
    EXPECT_EQ("bar.iec", Issue::filepath());
    EXPECT_EQ(10, Issue::lineno());
    Issue::pop();
    //
    EXPECT_EQ("<unknown location>", Issue::filepath());
    EXPECT_EQ(1, Issue::lineno());


}

TEST_F(IssueTest, AddIgnoring) {
    EXPECT_FALSE(Issue::is_ignored(Issue::Code::FILE_NOT_FOUND));
    Issue::add_ignoring(Issue::Code::FILE_NOT_FOUND);
    EXPECT_TRUE(Issue::is_ignored(Issue::Code::FILE_NOT_FOUND));
}

TEST_F(IssueTest, DefaultBlockings) {
    // ERROR codes are blocked by default
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FILE_NOT_FOUND));
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::EXPR_TYPE_ERROR));
    // UNKNOWN_DIRECTIVE (PP45) is ERROR, so it is blocked by default too.
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::UNKNOWN_DIRECTIVE));
    // SEVERE codes are blocked by default
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FATAL));
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::OPERATION_NOT_ALLOWED));
    // WARNING codes are NOT blocked by default
    EXPECT_FALSE(Issue::is_blocked(Issue::Code::WARNING_MESSAGE));
    // INFO codes are NOT blocked by default
    EXPECT_FALSE(Issue::is_blocked(Issue::Code::INFO_MESSAGE));
}

TEST_F(IssueTest, AddBlocking) {
    EXPECT_FALSE(Issue::is_blocked(Issue::Code::WARNING_MESSAGE));
    Issue::add_blocking(Issue::Code::WARNING_MESSAGE);
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::WARNING_MESSAGE));
}

TEST_F(IssueTest, RemoveBlocking) {
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FILE_NOT_FOUND));
    Issue::remove_blocking(Issue::Code::FILE_NOT_FOUND);
    EXPECT_FALSE(Issue::is_blocked(Issue::Code::FILE_NOT_FOUND));
}

TEST_F(IssueTest, RemoveBlockingSevereRefused) {
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FATAL));
    Issue::remove_blocking(Issue::Code::FATAL);
    // SEVERE codes cannot be removed
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FATAL));
}

TEST_F(IssueTest, HappenError) {
    Issue::push({1, "test.iec"});
    EXPECT_THROW(ISSUE(FILE_NOT_FOUND), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP11: No such file or directory", message());
    Issue::pop();
}

TEST_F(IssueTest, HappenWarning) {
    Issue::push({1, "test.iec"});
    EXPECT_NO_THROW(ISSUE(WARNING_MESSAGE));
    EXPECT_EQ("test.iec:1.0: warning: PP92: ''", message());
    Issue::pop();
}

TEST_F(IssueTest, HappenInfo) {
    Issue::push({1, "test.iec"});
    EXPECT_NO_THROW(ISSUE(INFO_MESSAGE));
    EXPECT_EQ("test.iec:1.0: info: PP93: ''", message());
    Issue::pop();
}

TEST_F(IssueTest, HappenSevere) {
    Issue::push({1, "test.iec"});
    EXPECT_THROW(ISSUE(OPERATION_NOT_ALLOWED), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP02: Operation not allowed", message());
    Issue::pop();
}

TEST_F(IssueTest, SevereBypassesIgnore) {
    Issue::push({1, "test.iec"});
    Issue::add_ignoring(Issue::Code::OPERATION_NOT_ALLOWED);
    EXPECT_THROW(ISSUE(OPERATION_NOT_ALLOWED), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP02: Operation not allowed", message());
    Issue::pop();
}

TEST_F(IssueTest, HappenBlockedWarning) {
    Issue::push({1, "test.iec"});
    Issue::add_blocking(Issue::Code::WARNING_MESSAGE);
    EXPECT_THROW(ISSUE(WARNING_MESSAGE), Issue::Exception);
    Issue::pop();
}

TEST_F(IssueTest, HappenContinuableError) {
    // Remove an ERROR code from blockings to make it continuable
    Issue::push({1, "test.iec"});
    Issue::remove_blocking(Issue::Code::FILE_NOT_FOUND);
    EXPECT_NO_THROW(ISSUE(FILE_NOT_FOUND));
    EXPECT_EQ("test.iec:1.0: error: PP11: No such file or directory", message());
    Issue::pop();
}

TEST_F(IssueTest, Fatal) {
    Issue::push({1, "test.iec"});
    EXPECT_THROW(Issue::fatal(), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP01: A fatal error occurred.", message());
    Issue::pop();
}

TEST_F(IssueTest, LineGuard) {
    EXPECT_EQ("<unknown location>", Issue::filepath());
    {
        Issue::LineGuard g(5, std::string("guarded.iec"));
        EXPECT_EQ(5, Issue::lineno());
        EXPECT_EQ("guarded.iec", Issue::filepath());
    }
    EXPECT_EQ("<unknown location>", Issue::filepath());
    EXPECT_EQ(1, Issue::lineno());
}

TEST_F(IssueTest, BlockingRAII) {
    Issue::push({1, "test.iec"});
    // Default: FILE_NOT_FOUND is blocked (throws)
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FILE_NOT_FOUND));
    {
        // Scope: only WARNING_MESSAGE is blocking (plus SEVERE auto-added)
        Issue::Blocking guard({Issue::Code::WARNING_MESSAGE});
        EXPECT_FALSE(Issue::is_blocked(Issue::Code::FILE_NOT_FOUND));
        EXPECT_TRUE(Issue::is_blocked(Issue::Code::WARNING_MESSAGE));
        // SEVERE codes remain blocked even if not in the provided set
        EXPECT_TRUE(Issue::is_blocked(Issue::Code::FATAL));
        // ERROR without blocking: continues
        EXPECT_NO_THROW(ISSUE(FILE_NOT_FOUND));
        EXPECT_EQ("test.iec:1.0: error: PP11: No such file or directory", message());
    }
    // Restored: FILE_NOT_FOUND is blocked again
    EXPECT_TRUE(Issue::is_blocked(Issue::Code::FILE_NOT_FOUND));
    Issue::pop();
}

TEST_F(IssueTest, IgnoringRAII) {
    EXPECT_FALSE(Issue::is_ignored(Issue::Code::FILE_NOT_FOUND));
    Issue::pop();
}

// ---- -w / -Werror tests ----

TEST_F(IssueTest, SuppressWarningsKeepsErrors) {
    Issue::push({1, "test.iec"});
    Issue::suppress_warnings_ = true;
    // WARNING suppressed
    EXPECT_NO_THROW(ISSUE(WARNING_MESSAGE));
    EXPECT_TRUE(empty());
    // ERROR not suppressed
    EXPECT_THROW(ISSUE(FILE_NOT_FOUND), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP11: No such file or directory", message());
    Issue::pop();
}

TEST_F(IssueTest, WerrorPromotion) {
    Issue::push({1, "test.iec"});
    Issue::werror_ = true;
    // WARNING with -Werror: promoted to error
    // Default blockings include ERROR_MESSAGE → promoted warning throws
    EXPECT_THROW(ISSUE(WARNING_MESSAGE), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP92: ''", message());
}

TEST_F(IssueTest, WerrorDoesNotAffectErrors) {
    Issue::push({1, "test.iec"});
    Issue::werror_ = true;
    // ERROR code: should behave exactly as before
    EXPECT_THROW(ISSUE(FILE_NOT_FOUND), Issue::Exception);
    EXPECT_EQ("test.iec:1.0: error: PP11: No such file or directory", message());
}

// ---- E0/E7: continue-after-error mode ----
//
// continue_mode_ defaults to false, and every test above ran with it off --
// they are unaffected by anything below, which is the point: the library's
// own default (every ERROR/SEVERE throws, -Werror-promoted or not) must
// stay exactly as-is unless a ContinueMode guard is explicitly in scope.

TEST_F(IssueTest, ContinueModeCountsNonAbortErrorInsteadOfThrowing) {
    Issue::push({1, "test.iec"});
    EXPECT_EQ(0, Issue::error_count_);
    {
        // EXPR_TYPE_ERROR is ERROR-severity but not SEVERE and not in the
        // (empty) abort set: counted instead of thrown.
        Issue::ContinueMode guard({});
        EXPECT_NO_THROW(ISSUE(EXPR_TYPE_ERROR));
    }
    EXPECT_EQ(1, Issue::error_count_);
    Issue::pop();
}

TEST_F(IssueTest, ContinueModeAbortSetCodeStillThrows) {
    Issue::push({1, "test.iec"});
    {
        Issue::ContinueMode guard({Issue::Code::FILE_NOT_FOUND});
        EXPECT_THROW(ISSUE(FILE_NOT_FOUND), Issue::Exception);
    }
    // The aborting code itself is not counted: only swallowed codes are.
    EXPECT_EQ(0, Issue::error_count_);
    Issue::pop();
}

TEST_F(IssueTest, ContinueModeSevereAlwaysThrowsEvenIfNotInAbortSet) {
    Issue::push({1, "test.iec"});
    {
        // Empty abort set: FATAL (SEVERE) still throws. ContinueMode forces
        // every SEVERE code into continue_abort_codes_ regardless of what
        // is passed in, mirroring Blocking's own severe-forcing.
        Issue::ContinueMode guard({});
        EXPECT_THROW(Issue::fatal(), Issue::Exception);
    }
    EXPECT_EQ(0, Issue::error_count_);
    Issue::pop();
}

TEST_F(IssueTest, ContinueModeWerrorPromotedWarningCountedNotThrown) {
    // E0: a -Werror-promoted WARNING is judged by its own original code,
    // not by the fact that it was promoted -- WARNING_MESSAGE is not in the
    // abort set, so (unlike WerrorPromotion above, with continue_mode_
    // off) it is counted here rather than thrown.
    Issue::push({1, "test.iec"});
    Issue::werror_ = true;
    {
        Issue::ContinueMode guard({});
        EXPECT_NO_THROW(ISSUE(WARNING_MESSAGE));
    }
    EXPECT_EQ(1, Issue::error_count_);
    Issue::pop();
}

TEST_F(IssueTest, ContinueModeIgnoredNotCounted) {
    // E2: a {#ignore}'d diagnostic is neither displayed nor counted.
    Issue::push({1, "test.iec"});
    Issue::add_ignoring(Issue::Code::EXPR_TYPE_ERROR);
    {
        Issue::ContinueMode guard({});
        EXPECT_NO_THROW(ISSUE(EXPR_TYPE_ERROR));
    }
    EXPECT_EQ(0, Issue::error_count_);
    Issue::pop();
}

TEST_F(IssueTest, ContinueModeSilentStillCounts) {
    // E2: --silent suppresses the printed diagnostic but not the count.
    Issue::push({1, "test.iec"});
    Issue::silent_ = true;
    {
        Issue::ContinueMode guard({});
        EXPECT_NO_THROW(ISSUE(EXPR_TYPE_ERROR));
    }
    EXPECT_EQ(1, Issue::error_count_);
    EXPECT_TRUE(empty());
    Issue::pop();
}

TEST_F(IssueTest, ContinueModeGuardRestoresOnScopeExit) {
    Issue::push({1, "test.iec"});
    EXPECT_FALSE(Issue::continue_mode_);
    {
        Issue::ContinueMode guard({});
        EXPECT_TRUE(Issue::continue_mode_);
        EXPECT_NO_THROW(ISSUE(EXPR_TYPE_ERROR)); // counted, not thrown
    }
    // Guard exited: continue_mode_ is back off, and the very same code now
    // throws again, unaffected by having run under a guard earlier.
    EXPECT_FALSE(Issue::continue_mode_);
    EXPECT_THROW(ISSUE(EXPR_TYPE_ERROR), Issue::Exception);
    Issue::pop();
}

TEST_F(IssueTest, InitializeResetsContinueModeState) {
    Issue::push({1, "test.iec"});
    {
        Issue::ContinueMode guard({});
        EXPECT_NO_THROW(ISSUE(EXPR_TYPE_ERROR));
    }
    EXPECT_EQ(1, Issue::error_count_);
    std::ostringstream local_stream;
    Issue::initialize(local_stream);
    EXPECT_EQ(0, Issue::error_count_);
    EXPECT_FALSE(Issue::continue_mode_);
}
