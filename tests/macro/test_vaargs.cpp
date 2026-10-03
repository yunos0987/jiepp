#include "test_helper.hpp"

#include <algorithm>
#include <cstring>

class VaArgsTest : public JieppTest {};

// NOTE: Leading/trailing Token::WS is trimmed from each VA_ARG (Python-compatible);
// the spacing the call had around each joining comma is then reproduced like
// clang does (one space where the source had whitespace; see CommaSpacing below).
TEST_F(VaArgsTest, Basic) {
    EXPECT_EQ(";1",             pp("{#define F(...) __VA_ARGS__};F(1)"));
    EXPECT_EQ(";1 , 2",         pp("{#define F(...) __VA_ARGS__};F(  1  , 2  )"));
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
    EXPECT_EQ(";2, 3|1", pp("{#define F(x,...) __VA_ARGS__|x};F(1, 2, 3)"));
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
    EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, code());
    EXPECT_THROW(pp("{#define F(... x) x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, code());
    // Valid variadic: ... immediately followed by ) (with optional whitespace)
    EXPECT_EQ(";1", pp("{#define F(a, ...) __VA_ARGS__};F(x, 1)"));
    EXPECT_EQ(";1", pp("{#define F(a,  ...  ) __VA_ARGS__};F(x, 1)"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, VariadicNotLastRaisesPP33) {
    // Fact: '...' must be the last parameter; a parameter following it is
    // INVALID_VARIADIC_PLACEMENT (PP33), not INVALID_DEFINE_SYNTAX (PP30).
    EXPECT_THROW(pp("{#define F(..., a) x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, code());
}

// ---- C6: comma spacing of __VA_ARGS__ in ordinary expansion (clang rule) ----
//
// The call's own spacing around each joining comma is reproduced: exactly one
// space where the source had whitespace or a comment, none where it had none,
// whatever the amount. A newline directly before the token does not count as
// whitespace (clang's Token::LeadingSpace), unlike for stringizing. Expected
// strings are the output of `clang -E -P`.

TEST_F(VaArgsTest, CommaSpacingMatrix) {
    const std::string d = "{#define F(...) [__VA_ARGS__]}";
    struct Row { const char* call; const char* want; };
    const Row rows[] = {
        {"F(a ,  b)",          "[a , b]"},
        {"F(a,b)",             "[a,b]"},
        {"F(a, b)",            "[a, b]"},
        {"F(a ,b)",            "[a ,b]"},
        {"F( a , b )",         "[a , b]"},
        {"F(a,  b,   c)",      "[a, b, c]"},
        {"F(a\n, b)",          "[a, b]"},      // newline before the comma: no space
        {"F(a\n  , b)",        "[a , b]"},
        {"F(a,\nb)",           "[a,b]"},
        {"F(a,\n  b)",         "[a, b]"},
        {"F(a\r\n, b)",        "[a, b]"},
        {"F(a// c\n, b)",      "[a, b]"},
        {"F(a\n(*c*), b)",     "[a , b]"},
        {"F(a (*c*)\n, b)",    "[a, b]"},
        {"F(a (*c*), b)",      "[a , b]"},
        {"F(a(*c*) ,(*d*)b)",  "[a , b]"},
        {"F(, b)",             "[, b]"},
        {"F( , b)",            "[, b]"},
        {"F(a ,)",             "[a ,]"},
        {"F(a , )",            "[a ,]"},
        {"F(a , , b)",         "[a , , b]"},
        {"F(a ,  ,  , b)",     "[a , , , b]"},
        {"F(a,\n,\nb)",        "[a,,b]"},
        {"F(,)",               "[,]"},
        {"F( , )",             "[,]"},
        {"F()",                "[]"},
        {"F(   )",             "[]"},
        {"F(a[x ,y] ,  b)",    "[a[x ,y] , b]"},
    };
    for (const auto& r : rows) {
        SCOPED_TRACE(r.call);
        // Newlines inside the call are emitted after it (line preservation).
        const auto nl = std::count(r.call, r.call + std::strlen(r.call), '\n');
        EXPECT_EQ(std::string(";") + r.want + std::string(static_cast<std::size_t>(nl), '\n') + ";",
                  pp(d + ";" + r.call + ";"));
    }
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingNoCommentStrip) {
    // -nC (comments removed) gives the same comma spacing.
    auto env = setup({}, true);
    EXPECT_EQ(";[a , b];", pp("{#define F(...) [__VA_ARGS__]};F(a (*c*), b);", env));
    EXPECT_EQ(";[a, b];",  pp("{#define F(...) [__VA_ARGS__]};F(a, (*c*)b);", env));
    EXPECT_EQ(";[a,b];",   pp("{#define F(...) [__VA_ARGS__]};F(a,b);", env));
    EXPECT_EQ(";[a , b];", pp("{#define F(...) [__VA_ARGS__]};F(a ,  b);", env));
    EXPECT_EQ(";[, b];",   pp("{#define F(...) [__VA_ARGS__]};F( , b);", env));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingNamedVariadic) {
    EXPECT_EQ(";a , b;", pp("{#define N(args...) args};N(a ,  b);"));
    EXPECT_EQ(";a,b;",   pp("{#define N(args...) args};N(a,b);"));
    EXPECT_EQ(";[a , b];", pp("{#define N(x, args...) [args]};N(p, a ,  b);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingMixedParams) {
    // The fixed parameter is a separate argument; only the variable ones join.
    EXPECT_EQ(";[p,a , b];",
              pp("{#define M(x, ...) [x,__VA_ARGS__]};M(p , a ,  b);"));
    EXPECT_EQ(";[p,a,b];",
              pp("{#define M(x, ...) [x,__VA_ARGS__]};M(p,a,b);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingVaOpt) {
    EXPECT_EQ(";f(p , a , b);",
              pp("{#define O(x, ...) f(x __VA_OPT__(,) __VA_ARGS__)};O(p, a ,  b);"));
    EXPECT_EQ(";<a , b>;",
              pp("{#define V(...) __VA_OPT__(<__VA_ARGS__>)};V(a ,  b);"));
    EXPECT_EQ(";;",
              pp("{#define V(...) __VA_OPT__(<__VA_ARGS__>)};V();"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingGlue) {
    // The first/last produced token pastes with its neighbour: no space is
    // added at either edge of the joined list.
    EXPECT_EQ(";Xa , b;", pp("{#define G(...) X @@ __VA_ARGS__};G(a ,  b);"));
    EXPECT_EQ(";a , bY;", pp("{#define G(...) __VA_ARGS__ @@ Y};G(a ,  b);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingNestedRepass) {
    // __VA_ARGS__ re-passed to another macro keeps the spacing, and the
    // stringizing callee sees it too (it used to see 'a,b').
    EXPECT_EQ(";a , b;",
              pp("{#define F(...) __VA_ARGS__}{#define H(...) F(__VA_ARGS__)};H(a ,  b);"));
    EXPECT_EQ(";x , a , b;",
              pp("{#define F(...) __VA_ARGS__}{#define H(...) F(x , __VA_ARGS__)};H(a ,  b);"));
    EXPECT_EQ(";'a , b';",
              pp("{#define S(...) @__VA_ARGS__}{#define H(...) S(__VA_ARGS__)};H(a ,  b);"));
    EXPECT_EQ(";'a,b';",
              pp("{#define S(...) @__VA_ARGS__}{#define H(...) S(__VA_ARGS__)};H(a,b);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingMultiUse) {
    // Several uses share one memoised expansion; every use is spaced alike.
    EXPECT_EQ(";[a , b;a , b];",
              pp("{#define D(...) [__VA_ARGS__;__VA_ARGS__]};D(a ,  b);"));
    // Plain + stringized use of the same parameter: the stringize spelling
    // counts a newline-only edge, the plain one does not.
    EXPECT_EQ(";[a , b;'a , b'];",
              pp("{#define D(...) [__VA_ARGS__;@__VA_ARGS__]};D(a ,  b);"));
    EXPECT_EQ(";[a, b;'a , b']\n;",
              pp("{#define D(...) [__VA_ARGS__;@__VA_ARGS__]};D(a\n, b);"));
    EXPECT_EQ(";'a , b' | a, b\n;",
              pp("{#define X(...) @__VA_OPT__(__VA_ARGS__) | __VA_ARGS__};X(a\n, b);"));
    EXPECT_EQ(";'a, b' | a,b\n;",
              pp("{#define X(...) @__VA_OPT__(__VA_ARGS__) | __VA_ARGS__};X(a,\nb);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingArgumentsAreExpanded) {
    EXPECT_EQ(";[1 , 1];",
              pp("{#define A 1}{#define F(...) [__VA_ARGS__]};F(A ,  A);"));
    EXPECT_EQ(";[a , b , c];",
              pp("{#define K(...) __VA_ARGS__}{#define F(...) [__VA_ARGS__]};F(K(a , b) ,  c);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingNonVariadicUnchanged) {
    // Guards: a non-variadic macro still uses the body's own spelling, and
    // the amount of whitespace inside one argument is still reproduced.
    EXPECT_EQ(";a,b;",     pp("{#define G(x,y) x,y};G(a ,  b);"));
    EXPECT_EQ(";a , b;",   pp("{#define G(x,y) x , y};G(a,b);"));
    EXPECT_EQ(";[a   b];", pp("{#define W(x) [x]};W(a   b);"));
    EXPECT_TRUE(empty());
}

TEST_F(VaArgsTest, CommaSpacingStringDirective) {
    // {## ...} stringizes the expansion, which now carries the spacing.
    EXPECT_EQ("'[a , b]'", pp("{#define F(...) [__VA_ARGS__]}{## F(a ,  b)}"));
    EXPECT_EQ("'[a,b]'",   pp("{#define F(...) [__VA_ARGS__]}{## F(a,b)}"));
}
