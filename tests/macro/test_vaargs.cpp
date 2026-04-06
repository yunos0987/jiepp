#include "test_helper.hpp"

class VaArgsTest : public JieppTest {};

// NOTE: Leading/trailing Token::WS is trimmed from each VA_ARG (Python-compatible).
TEST_F(VaArgsTest, Basic) {
    EXPECT_EQ(";1",             pp("{#define F(...) __VA_ARGS__};F(1)"));
    EXPECT_EQ(";1,2",           pp("{#define F(...) __VA_ARGS__};F(  1  , 2  )"));
    EXPECT_EQ(";1,2,3",         pp("{#define F(...) __VA_ARGS__};F(1,2,3)"));
    EXPECT_EQ(";",              pp("{#define F(...) __VA_ARGS__};F()"));
    EXPECT_EQ(";",              pp("{#define F(...) __VA_ARGS__};F(   )"));
    EXPECT_EQ(";,",             pp("{#define F(...) __VA_ARGS__};F(,)"));
    EXPECT_EQ(";,,,",           pp("{#define F(...) __VA_ARGS__};F(,,,)"));
    EXPECT_EQ(";1,",            pp("{#define F(...) __VA_ARGS__};F(1,)"));
    EXPECT_EQ(";,2",            pp("{#define F(...) __VA_ARGS__};F(,2)"));
    EXPECT_EQ(";,2,",           pp("{#define F(...) __VA_ARGS__};F(,2,)"));
    EXPECT_EQ(";1,2,3:1,2,3",   pp("{#define F(...) __VA_ARGS__:__VA_ARGS__};F(1,2,3)"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, MixedParams) {
    // Leading whitespace after comma is trimmed from each arg (Python-compatible)
    EXPECT_EQ(";2|1",     pp("{#define F(x,...) __VA_ARGS__|x};F(1, 2)"));
    EXPECT_EQ(";2,3|1",  pp("{#define F(x,...) __VA_ARGS__|x};F(1, 2, 3)"));
    EXPECT_EQ(";|1",       pp("{#define F(x,...) __VA_ARGS__|x};F(1)"));
    EXPECT_EQ(";|1",       pp("{#define F(x,...) __VA_ARGS__|x};F(1,)"));
    EXPECT_EQ(";,,|1",     pp("{#define F(x,...) __VA_ARGS__|x};F(1,,,)"));
    EXPECT_EQ(";3|1, 2",  pp("{#define F(x,y,...) __VA_ARGS__|x, y};F( 1,  2,  3)"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, Argc) {
    EXPECT_EQ(";1",  pp("{#define F(...) __VA_ARGC__};F(1)"));
    EXPECT_EQ(";2",  pp("{#define F(...) __VA_ARGC__};F(  1  , 2  )"));
    EXPECT_EQ(";3",  pp("{#define F(...) __VA_ARGC__};F(1,2,3)"));
    EXPECT_EQ(";0",  pp("{#define F(...) __VA_ARGC__};F()"));
    EXPECT_EQ(";0",  pp("{#define F(...) __VA_ARGC__};F(   )"));
    EXPECT_EQ(";2",  pp("{#define F(...) __VA_ARGC__};F(,)"));   // note
    EXPECT_EQ(";4",  pp("{#define F(...) __VA_ARGC__};F(,,,)")); // note
    EXPECT_EQ(";2",  pp("{#define F(...) __VA_ARGC__};F(1,)"));
    EXPECT_EQ(";4",  pp("{#define F(...) __VA_ARGC__};F(1,,,)"));
    EXPECT_EQ(";2",  pp("{#define F(...) __VA_ARGC__};F(,2)"));
    EXPECT_EQ(";3",  pp("{#define F(...) __VA_ARGC__};F(,2,)"));
    EXPECT_EQ(";3:3", pp("{#define F(...) __VA_ARGC__:__VA_ARGC__};F(1,2,3)"));
    EXPECT_EQ(";1",  pp("{#define F(x, ...) __VA_ARGC__};F(1, 2)"));
    EXPECT_EQ(";2",  pp("{#define F(x, ...) __VA_ARGC__};F(1, 2, 3)"));
    EXPECT_EQ(";0",  pp("{#define F(x, ...) __VA_ARGC__};F(1)"));
    EXPECT_EQ(";1",  pp("{#define F(x, ...) __VA_ARGC__};F(1,)"));
    EXPECT_EQ(";3",  pp("{#define F(x, ...) __VA_ARGC__};F(1,,,)"));
    EXPECT_EQ(";1",  pp("{#define F(x, y, ...) __VA_ARGC__};F(1, 2, 3)"));
    EXPECT_EQ(";2",  pp("{#define F(x, y, ...) __VA_ARGC__};F(1, 2, 3, 4)"));
    EXPECT_EQ(";0",  pp("{#define F(x, y, ...) __VA_ARGC__};F(1, 2)"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, GarbageBeforeCloseParen) {
    // Tokens between ... and ) should cause an error
    EXPECT_THROW(pp("{#define F(a, ... x) a}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    EXPECT_THROW(pp("{#define F(... x) x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    // Valid variadic: ... immediately followed by ) (with optional whitespace)
    EXPECT_EQ(";1", pp("{#define F(a, ...) __VA_ARGS__};F(x, 1)"));
    EXPECT_EQ(";1", pp("{#define F(a,  ...  ) __VA_ARGS__};F(x, 1)"));
    EXPECT_TRUE(empty());
}
