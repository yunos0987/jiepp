#include "test_helper.hpp"

// A directive operand ({#if}/{#elif} condition, {#string}/{#wstring},
// message text, ...) and an ordinary pragma body are macro-expanded without
// blank-line compaction. Compaction is a post-pass over the final output;
// running it on an operand holding 8 or more consecutive newlines spliced a
// line marker into the operand text itself.

namespace {
class OperandExpansionTest : public JieppTest {};
const std::string kTenNewlines = "$n$n$n$n$n$n$n$n$n$n";
} // namespace

TEST_F(OperandExpansionTest, IfConditionIsNotCompacted) {
    // Before the fix: PP52 + PP51 "Missing expression; '1$n(*{#:9}*)$n+0'".
    Issue::ContinueMode guard({});
    const std::string out = pp("{#if 1" + kTenNewlines + "+0}\nyes;\n{#endif}");
    EXPECT_NE(std::string::npos, out.find("yes;")) << out;
    EXPECT_TRUE(empty());
}

TEST_F(OperandExpansionTest, MessageTextIsNotCompacted) {
    pp("{#info a" + kTenNewlines + "b}");
    std::string all;
    for (const auto& m : messages())
        all += m + "\n";
    EXPECT_EQ(std::string::npos, all.find("{#:")) << all;
    EXPECT_NE(std::string::npos, all.find("'a" + std::string(10, '\n') + "b'")) << all;
}

TEST_F(OperandExpansionTest, StringizeResultIsNotCompacted) {
    // Before the fix: 'a$n(*{#:9}*)$nb'.
    EXPECT_EQ("'a" + kTenNewlines + "b'", pp("{## a" + kTenNewlines + "b}"));
    EXPECT_EQ("\"a" + kTenNewlines + "b\"", pp("{#wstring a" + kTenNewlines + "b}"));
    EXPECT_TRUE(empty());
}

TEST_F(OperandExpansionTest, PragmaBodyIsNotCompacted) {
    // Before the fix: (*{foo a\n(*{#:9}*)\nb}*) -- a nested comment.
    EXPECT_EQ("(*{foo a" + std::string(10, '\n') + "b}*)",
              pp("{#define Z a" + kTenNewlines + "b}{foo Z}"));
    EXPECT_TRUE(empty());
}
