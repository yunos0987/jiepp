#include "test_helper.hpp"

// C-style whitespace collapsing for stringizing: `@x` (incl. __VA_ARGS__ and
// __VA_OPT__) and the {#string}/{##}/{#wstring} directive family. See the
// design (stringize-ws) for the R1-R8 rules this file exercises.
//
// S(x) and V(...)/VO*(...) below are the per-test stringize macros; the
// surrounding ";...;" mirrors the existing tests/macro/test_stringize.cpp
// convention so the exact pp() output shape (including any source newline
// re-emitted after a multi-line call) is visible in each expectation.

class StringizeWhitespaceTest : public JieppTest {};

// ---- 1. Spaces, tabs, newlines ----

TEST_F(StringizeWhitespaceTest, SpacesTabsNewlines) {
    EXPECT_EQ(" ;'a b';", pp("{#define S(x) @x} ;S(a   b);"));
    EXPECT_EQ(" ;'a b';", pp("{#define S(x) @x} ;S(a\t\tb);"));
    EXPECT_EQ(" ;'a';",   pp("{#define S(x) @x} ;S(  a  );"));
    EXPECT_EQ(" ;'';",    pp("{#define S(x) @x} ;S(   );"));
    EXPECT_TRUE(empty());
}

// ---- 2. Comments are whitespace, default and -nC ----

TEST_F(StringizeWhitespaceTest, CommentsAreWhitespace) {
    EXPECT_EQ(" ;'a b';", pp("{#define S(x) @x} ;S(a (* c *) b);"));
    EXPECT_EQ(" ;'a b';", pp("{#define S(x) @x} ;S(a/**/b);"));
    EXPECT_EQ(" ;'a';",   pp("{#define S(x) @x} ;S( (*c*) a (*d*) );"));
    EXPECT_EQ(" ;'';",    pp("{#define S(x) @x} ;S(/**/);"));
    EXPECT_TRUE(empty());
}

TEST_F(StringizeWhitespaceTest, CommentsAreWhitespaceNoCommentStrip) {
    auto env = setup({}, true);
    EXPECT_EQ(" ;'a b';", pp("{#define S(x) @x} ;S(a (* c *) b);", env));
    EXPECT_EQ(" ;'a b';", pp("{#define S(x) @x} ;S(a/**/b);", env));
    EXPECT_EQ(" ;'a';",   pp("{#define S(x) @x} ;S( (*c*) a (*d*) );", env));
    EXPECT_EQ(" ;'';",    pp("{#define S(x) @x} ;S(/**/);", env));
    EXPECT_TRUE(empty());
}

// ---- 3. Adjacency and arrays (comma inside one argument is not a separator) ----

TEST_F(StringizeWhitespaceTest, AdjacencyAndArrays) {
    EXPECT_EQ(" ;'a+b';",        pp("{#define S(x) @x} ;S(a+b);"));
    EXPECT_EQ(" ;'f( a , b )';", pp("{#define S(x) @x} ;S(f( a , b ));"));
    EXPECT_EQ(" ;'a[x, y]';",    pp("{#define S(x) @x} ;S(a[x,  y]);"));
    EXPECT_EQ(" ;'a[x,y]'|'b[1,2]';",
              pp("{#define G(p,q) @p|@q} ;G(a[x,y], b[1,2]);"));
    EXPECT_TRUE(empty());
}

// ---- 4. String/char literal interiors are preserved verbatim ----

TEST_F(StringizeWhitespaceTest, LiteralInteriorKept) {
    EXPECT_EQ(" ;'$27x   y$27 z';", pp("{#define S(x) @x} ;S('x   y'   z);"));
    EXPECT_EQ(" ;'$22p  q$22';",    pp("{#define S(x) @x} ;S(\"p  q\");"));
    EXPECT_EQ(" ;'a$$nb';",         pp("{#define S(x) @x} ;S(a$nb);"));
    EXPECT_TRUE(empty());
}

// ---- 5. Doc comments and pragmas are tokens: interiors kept verbatim ----

TEST_F(StringizeWhitespaceTest, DocCommentAndPragmaAreTokens) {
    EXPECT_EQ(" ;'a (*! d  e *) b';", pp("{#define S(x) @x} ;S(a   (*! d  e *)   b);"));
    EXPECT_EQ(" ;'a //! d b'\n;",     pp("{#define S(x) @x} ;S(a //! d\n b);"));
    EXPECT_EQ(" ;'a {attribute   x} b';",
              pp("{#define S(x) @x} ;S(a   {attribute   x}   b);"));
    auto env = setup({}, true);
    EXPECT_EQ(" ;'a (*! d *) b';", pp("{#define S(x) @x} ;S(a (*! d *) b);", env));
    EXPECT_TRUE(empty());
}

// ---- 6. Argument from a macro expansion (expand-first semantics unchanged) ----

TEST_F(StringizeWhitespaceTest, ArgumentFromExpansion) {
    EXPECT_EQ(" ;'a b';",
              pp("{#define B a   b}{#define S(x) @x}{#define S2(x) S(x)} ;S2(B);"));
    EXPECT_EQ(" ;'a a b c';",
              pp("{#define B a   b}{#define S(x) @x}{#define S2(x) S(x)} ;S2(a   B   c);"));
    EXPECT_EQ(" ;'a b';",
              pp("{#define BC a (* c *) b}{#define S(x) @x}{#define S2(x) S(x)} ;S2(BC);"));
    {
        auto env = setup({}, true);
        EXPECT_EQ(" ;'a b';",
                  pp("{#define BC a (* c *) b}{#define S(x) @x}{#define S2(x) S(x)} ;S2(BC);",
                     env));
    }
    EXPECT_EQ(" ;'x y';",
              pp("{#define E}{#define S(x) @x}{#define S2(x) S(x)} ;S2(x E y);"));
    EXPECT_EQ(" ;'x';",
              pp("{#define E}{#define S(x) @x}{#define S2(x) S(x)} ;S2(E x);"));
    EXPECT_TRUE(empty());
}

// ---- 7. @__VA_ARGS__/@args comma spacing reproduces clang ----

TEST_F(StringizeWhitespaceTest, VariadicCommaSpacing) {
    EXPECT_EQ(" ;'a , b';",  pp("{#define V(...) @__VA_ARGS__} ;V(a ,  b);"));
    EXPECT_EQ(" ;'a, b';",   pp("{#define V(...) @__VA_ARGS__} ;V(a, b);"));
    EXPECT_EQ(" ;'a,b';",    pp("{#define V(...) @__VA_ARGS__} ;V(a,b);"));
    EXPECT_EQ(" ;'a , b';",  pp("{#define V(...) @__VA_ARGS__} ;V(  a  ,  b  );"));
    EXPECT_EQ(" ;'a, ,b';",  pp("{#define V(...) @__VA_ARGS__} ;V(a,  ,b);"));
    EXPECT_EQ(" ;'a,,b';",   pp("{#define V(...) @__VA_ARGS__} ;V(a,,b);"));
    EXPECT_EQ(" ;',';",      pp("{#define V(...) @__VA_ARGS__} ;V( , );"));
    EXPECT_EQ(" ;'a , b';",  pp("{#define V(...) @__VA_ARGS__} ;V( a /**/ , b );"));
    EXPECT_EQ(" ;'a ,b';",   pp("{#define N(args...) @args} ;N(a ,b);"));
    EXPECT_EQ(" ;'a ,b';",   pp("{#define W(x, ...) @__VA_ARGS__} ;W(1,  a  ,b);"));
    EXPECT_EQ(" ;'a[x, y], c';",
              pp("{#define V(...) @__VA_ARGS__} ;V(a[x, y], c);"));
    EXPECT_TRUE(empty());
}

// ---- 8. @__VA_OPT__(...) stringizing ----

TEST_F(StringizeWhitespaceTest, VaOptStringize) {
    EXPECT_EQ(" ;'x y 1';",
              pp("{#define VO(...) @__VA_OPT__(x   y   __VA_ARGS__)} ;VO(1);"));
    EXPECT_EQ(" ;'';",
              pp("{#define VO(...) @__VA_OPT__(x   y   __VA_ARGS__)} ;VO();"));
    EXPECT_EQ(" ;'a , b';",
              pp("{#define VO2(...) @__VA_OPT__(__VA_ARGS__)} ;VO2(a ,  b);"));
    EXPECT_EQ(" ;'<a ,b>';",
              pp("{#define VO4(...) @__VA_OPT__(<__VA_ARGS__>)} ;VO4( a ,b );"));
    // Pins: non-stringize __VA_OPT__/__VA_ARGS__ output is unchanged (D3).
    EXPECT_EQ(" ;g(a , b,c);",
              pp("{#define L(f, ...) g(f __VA_OPT__(,) __VA_ARGS__)} ;L(a, b ,c);"));
    EXPECT_EQ(" ;[a,b];",
              pp("{#define P(...) [__VA_ARGS__]} ;P(a ,  b);"));
    EXPECT_TRUE(empty());
}

// ---- 9. {#string}/{##}/{#wstring} directive family ----

TEST_F(StringizeWhitespaceTest, StringDirective) {
    EXPECT_EQ("'a b'",             pp("{#string a    b}"));
    EXPECT_EQ("'a b'",             pp("{#string a\tb}"));
    EXPECT_EQ("'a b'",             pp("{#string a (* c *) b}"));
    EXPECT_EQ("\"a b\"",           pp("{#wstring   a   b  }"));
    EXPECT_EQ("'a b'",             pp("{#define B a   b}{## B}"));
    EXPECT_EQ("'(*! d *) a'",      pp("{## (*! d *)  a}"));
    EXPECT_EQ("'$27x   y$27 z'",   pp("{## 'x   y'   z}"));
    EXPECT_EQ("'x y'",             pp("{#define E}{## x E y}"));
    EXPECT_EQ("'x'",               pp("{#define E}{## E x}"));
    EXPECT_EQ("'a b'",             pp("{## a$nb}"));
    EXPECT_EQ("'a b'",             pp("{## a$t$tb}"));
    EXPECT_EQ("'x$$y z'",          pp("{#string $ x$$y z }"));
    EXPECT_EQ("'a b'",             pp("{## a // c$nb}"));
    EXPECT_EQ("'ab'",              pp("{#define F(x) x}{## F(a$n)b}"));
    EXPECT_EQ("'a b'",             pp("{#define F(x) x}{## F(a$n) b}"));
    EXPECT_EQ("'x ab'",            pp("{#define F(x) x}{## x F$n(a)b}"));
    EXPECT_TRUE(empty());
}

// ---- 10. Redefinition is unaffected by the whitespace rule ----

TEST_F(StringizeWhitespaceTest, RedefinitionUnaffected) {
    pp("{#define S(x) @x}{#define S(x) @x}");
    EXPECT_TRUE(empty());
    pp("{#define B a   b}{#define B a b}");
    EXPECT_TRUE(empty());
    pp("{#define V(...) @__VA_ARGS__}{#define V(...) @__VA_ARGS__}");
    EXPECT_TRUE(empty());

    pp("{#define B2 a b}{#define B2 ab}");
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());

    auto env = setup();
    pp("{#define B a   b}", env);
    EXPECT_EQ("a   b", env.lookup("B")->str());
}

// ---- 11. Line numbers / newline layout are unaffected ----

TEST_F(StringizeWhitespaceTest, LineNumbersUnaffected) {
    EXPECT_EQ("\n'a b'\n\n;\n5;",
              pp("{#define S(x) @x}\nS(a\n\n b);\n__LINE__;"));
    EXPECT_EQ("\n'ab'\n3;",
              pp("{#define F(x) x}\n{## F(a$n)b}\n__LINE__;"));
    EXPECT_TRUE(empty());
}
