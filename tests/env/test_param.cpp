#include "test_helper.hpp"
#include "env/env.hpp"

class ParamTest : public JieppTest {};

TEST_F(ParamTest, DefaultConstruct) {
    Env env;
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
    EXPECT_FALSE(env.is_standard_pragma_style());
    EXPECT_FALSE(env.get_remove_comments());
    EXPECT_EQ(nullptr, env.get_cache("key"));
}

TEST_F(ParamTest, MaxIncludeDepth) {
    Env env;
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
    EXPECT_TRUE(env.set_max_include_depth(10));
    EXPECT_EQ(10, env.get_max_include_depth());
}

TEST_F(ParamTest, FixMaxIncludeDepth) {
    Env env;
    EXPECT_TRUE(env.fix_max_include_depth(5));
    EXPECT_EQ(5, env.get_max_include_depth());
    // set_max_include_depth is a no-op after fix (returns true, but value unchanged)
    EXPECT_TRUE(env.set_max_include_depth(999));
    EXPECT_EQ(5, env.get_max_include_depth());
}

TEST_F(ParamTest, SetMaxIncludeDepthRejectsZero) {
    Env env;
    EXPECT_THROW(env.set_max_include_depth(0), Issue::Exception);
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
}

TEST_F(ParamTest, SetMaxIncludeDepthRejectsNegative) {
    Env env;
    EXPECT_THROW(env.set_max_include_depth(-1), Issue::Exception);
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
}

TEST_F(ParamTest, SetMaxIncludeDepthRejectsOverflow) {
    Env env;
    EXPECT_THROW(env.set_max_include_depth(MAX_PARAMETER_VALUE + 1),
                 Issue::Exception);
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
}

TEST_F(ParamTest, FixMaxIncludeDepthRejectsZero) {
    Env env;
    EXPECT_THROW(env.fix_max_include_depth(0), Issue::Exception);
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
}

TEST_F(ParamTest, FixMaxIncludeDepthRejectsOverflow) {
    Env env;
    EXPECT_THROW(env.fix_max_include_depth(MAX_PARAMETER_VALUE + 1),
                 Issue::Exception);
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_DEPTH, env.get_max_include_depth());
}

TEST_F(ParamTest, SetMaxExpansionDepthValidation) {
    Env env;
    EXPECT_TRUE(env.set_max_expansion_depth(100));
    EXPECT_EQ(100, env.get_max_expansion_depth());
    EXPECT_THROW(env.set_max_expansion_depth(0), Issue::Exception);
    EXPECT_EQ(100, env.get_max_expansion_depth());
    EXPECT_THROW(env.set_max_expansion_depth(MAX_PARAMETER_VALUE + 1),
                 Issue::Exception);
}

TEST_F(ParamTest, SetMaxIfNestingValidation) {
    Env env;
    EXPECT_TRUE(env.set_max_if_nesting(128));
    EXPECT_EQ(128, env.get_max_if_nesting());
    EXPECT_THROW(env.set_max_if_nesting(0), Issue::Exception);
    EXPECT_EQ(128, env.get_max_if_nesting());
    EXPECT_THROW(env.set_max_if_nesting(MAX_PARAMETER_VALUE + 1),
                 Issue::Exception);
}

TEST_F(ParamTest, MaxParameterValueBoundary) {
    Env env;
    EXPECT_TRUE(env.set_max_include_depth(MAX_PARAMETER_VALUE));
    EXPECT_EQ(MAX_PARAMETER_VALUE, env.get_max_include_depth());
}

TEST_F(ParamTest, PragmaStyle) {
    Env env;
    EXPECT_EQ(VAL_PRAGMA_ANNOTATED, env.get_pragma_style());
    EXPECT_FALSE(env.is_standard_pragma_style());
    env.set_pragma_style(VAL_PRAGMA_STANDARD);
    EXPECT_EQ(VAL_PRAGMA_STANDARD, env.get_pragma_style());
    EXPECT_TRUE(env.is_standard_pragma_style());
}

TEST_F(ParamTest, FixPragmaStyle) {
    Env env;
    env.fix_pragma_style(VAL_PRAGMA_STANDARD);
    EXPECT_TRUE(env.is_standard_pragma_style());
    // set_pragma_style is a no-op after fix
    env.set_pragma_style(VAL_PRAGMA_ANNOTATED);
    EXPECT_TRUE(env.is_standard_pragma_style());
}

TEST_F(ParamTest, RemoveComments) {
    Env env;
    EXPECT_FALSE(env.get_remove_comments());
    env.set_remove_comments(true);
    EXPECT_TRUE(env.get_remove_comments());
    env.set_remove_comments(false);
    EXPECT_FALSE(env.get_remove_comments());
}

TEST_F(ParamTest, FixRemoveComments) {
    Env env;
    env.fix_remove_comments(true);
    EXPECT_TRUE(env.get_remove_comments());
    // set_remove_comments is a no-op after fix
    env.set_remove_comments(false);
    EXPECT_TRUE(env.get_remove_comments());
}

TEST_F(ParamTest, Cache) {
    Env env;
    EXPECT_EQ(nullptr, env.get_cache("key"));
    std::vector<Token> tokens;
    env.set_cache("key", tokens);
    const auto* cached = env.get_cache("key");
    ASSERT_NE(nullptr, cached);
    EXPECT_TRUE(cached->empty());
}
