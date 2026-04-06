#include "test_helper.hpp"

class StringizeTest : public JieppTest {};

// ---- @ (stringize) operator ----

TEST_F(StringizeTest, Basic) {
    EXPECT_EQ(" ;'x';",           pp("{#define F(a) @a} ;F(x);"));
    EXPECT_EQ(" ;'[0, 1]';",      pp("{#define F(a) @a} ;F([0, 1]);"));
    EXPECT_EQ(" ;'[[0, 1], 2]';", pp("{#define F(a) @a} ;F([[0, 1], 2]);"));
    EXPECT_EQ(" ;'';",            pp("{#define F(a) @a} ;F();"));
    EXPECT_EQ(" ;'$27$27  $$';",  pp("{#define F(a) @a} ;F(  ''  $  );"));
    EXPECT_EQ(" ;'x  y''2';",     pp("{#define F(a, b) @a@b} ;F(  x  y  ,  2  );"));
    EXPECT_EQ(";'x';",            pp("{#define S(a) @a}{#define F(a) S(a)};F(x);"));
    EXPECT_TRUE(empty());
}

// NOTE: C++ validates @ at call time, not definition time.
TEST_F(StringizeTest, Error) {
    EXPECT_THROW(pp("{#define F(a) @};F(x)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, code());
    EXPECT_THROW(pp("{#define F(a) @p};F(x)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, code());
    EXPECT_THROW(pp("{#define F(a) @ax};F(x)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, code());
    EXPECT_TRUE(empty());
}