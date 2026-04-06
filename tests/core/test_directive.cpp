#include "test_helper.hpp"

class DirectiveTest : public JieppTest {};

TEST_F(DirectiveTest, Basic) {
    EXPECT_EQ("", pp("{#define: N 3}"));
    EXPECT_EQ("", pp("{#define N 3}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NoColon) {
    EXPECT_EQ(";3", pp("{#define N 3};N"));
    EXPECT_EQ(";", pp("{#define N};N"));
    EXPECT_EQ(";xyzw", pp("{#define F(a,b) a@@b};F(xy,zw)"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineComment) {
    EXPECT_EQ("\n3", pp("//{#define: N 3}\nN"));
    EXPECT_EQ("\n3", pp("//{#define N 3}\nN"));
    EXPECT_EQ("\n3", pp("// {#\tdefine:\tN\t3}\nN"));
    EXPECT_EQ("\n3", pp("// {#\tdefine\tN\t3}\nN"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, Unknown) {
    const std::string input = "{# /**/};{# (**)};{# //};{# */};{# /*};";
    EXPECT_EQ(";;;;;", pp(input));
    auto cs = codes();
    ASSERT_EQ(5u, cs.size());
    for (auto c : cs)
        EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, c);
}

TEST_F(DirectiveTest, InvalidDirectiveName) {
    // Identifier-like key with non-identifier chars → ERROR (not WARNING)
    // parse_directive keeps +, (, ) in the key since they're not separator chars
    EXPECT_THROW(pp("{#endif.}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, code());
    EXPECT_THROW(pp("{#if+}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, code());
}

TEST_F(DirectiveTest, SpecialInDirective) {
    EXPECT_EQ(";'0';'1'", pp("{#define L ${## __COUNTER__$}};L;L"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, Ignore) {
    EXPECT_THROW(pp("{#line x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_NO_THROW(pp("{#ignore PP41}{#line x}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, IgnoreMultipleCodes) {
    // Each {#ignore} suppresses a different code independently
    EXPECT_THROW(pp("{#line x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    EXPECT_NO_THROW(pp("{#ignore PP41}{#line x}"));
    EXPECT_TRUE(empty());
    
    // Verify a second ignore also works
    EXPECT_NO_THROW(pp("{#ignore PP42}{#ignore abc}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, IgnoreInvalidOperand) {
    EXPECT_THROW(pp("{#ignore 41}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore foo}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PPxx}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PP}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
}

TEST_F(DirectiveTest, OperationNotAllowedDefine) {
    EXPECT_THROW(pp("{#define defined}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

TEST_F(DirectiveTest, OperationNotAllowedUndef) {
    EXPECT_THROW(pp("{#undef defined}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

TEST_F(DirectiveTest, Undef) {
    // Define, undef, redefine
    EXPECT_EQ(";2;;N;;3", pp("{#define N 2};N;{#undef N};N;{#define N 3};N"));
    // Undef of undefined macro is a no-op
    EXPECT_EQ(";N;;2;;N", pp("{#undef N};N;{#define N 2};N;{#undef N};N"));
    // Python parity: trailing spaces remain part of the operand, so this undef is a no-op
    EXPECT_EQ(";2;;N", pp("{#define N 2};N;{#undef   N   };N"));
    EXPECT_TRUE(empty());
}
