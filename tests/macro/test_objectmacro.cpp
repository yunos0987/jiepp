#include "test_helper.hpp"

class ObjectMacroTest : public JieppTest {};

TEST_F(ObjectMacroTest, Simple) {
    EXPECT_EQ(";2", pp("{#define N 2};N"));
    EXPECT_EQ(";2", pp("/*{#define N 2}*/;N"));
    EXPECT_EQ(";2", pp("(*{#define N 2}*);N"));
    EXPECT_EQ("\n2", pp("//{#define N 2}\nN"));
    // deep expand
    EXPECT_EQ(";;2;2", pp("{#define N 2};{#define M N};N;M"));
    // string literal not expanded
    EXPECT_EQ(";2;'N'", pp("{#define N 2};N;'N'"));
    // line directive
    EXPECT_EQ(";(*{#:5}*)", pp("{#define N 5};{#line N}"));
    EXPECT_EQ(";(*{#:5}*)", pp("{#define F(a) a};{#line F(5)}"));
    // dollar-sign line continuation
    EXPECT_EQ("\n;2", pp("{#define N $\n2};N"));
    EXPECT_EQ("\n;N", pp("{#define N$\n2};N"));
    EXPECT_EQ("\n;2", pp("{#define N \n2};N"));
    EXPECT_EQ("\n;N", pp("{#define N\n2};N"));
    EXPECT_EQ(";2;;3", pp("{#define N 2};N;{#define N 3};N"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
}

TEST_F(ObjectMacroTest, RedefineWarning) {
    EXPECT_NO_THROW(pp("{#define N 2};N;{#define N 3};N"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
}

TEST_F(ObjectMacroTest, At2) {
    EXPECT_EQ(";xyzw;", pp("{#define N xy@@zw};N;"));
    EXPECT_EQ(";xyzw;", pp("{#define N   xy  @@  zw  };N;"));
    EXPECT_EQ(";a  b  cd  e  f;", pp("{#define N a  b  c@@d  e  f};N;"));
    EXPECT_EQ(";a bc d;", pp("{#define N a b  @@  c d };N;"));
    EXPECT_EQ(";xy+-zw;", pp("{#define N xy+@@-zw};N;"));
    EXPECT_EQ(";ab ab ab ab;", pp("{#define N a@@b a  @@b a@@  b a  @@  b};N;"));
    EXPECT_EQ(";ab;", pp("{#define N a@@b};N;"));
    EXPECT_EQ(";abc;", pp("{#define N a@@b@@c};N;"));
    EXPECT_EQ(";abcd;", pp("{#define N a@@b@@c@@d};N;"));
    EXPECT_EQ(";abcd efg hi;", pp("{#define N a@@b@@c@@d e@@f@@g h@@i};N;"));
    EXPECT_TRUE(empty());
}

TEST_F(ObjectMacroTest, Nobody) {
    EXPECT_EQ(";", pp("{#define N};N"));
    EXPECT_EQ(";", pp("{#define ABC };ABC"));
    EXPECT_TRUE(empty());
}

TEST_F(ObjectMacroTest, Recursive) {
    EXPECT_EQ(";M;;2", pp("{#define N M};N;{#define M 2};N"));
    EXPECT_EQ(";M;M;;N;M", pp("{#define N M};N;M;{#define M N};N;M"));
    EXPECT_EQ(";;A,A,B;A,B,B;", pp("{#define A A,B};{#define B A,B};A;B;"));
    EXPECT_EQ(";;;\nC,C,B,A,A,C,B,A,B,A,A;\nC,B,C,B,A,B,C,B,A,B,A;\nC,C,B,C,B,A,C,C,B,A,A;",
              pp("{#define A C,B,A};{#define B C,B,A};{#define C C,B,A};\nA;\nB;\nC;"));
    EXPECT_TRUE(empty());
}

TEST_F(ObjectMacroTest, Whitespace) {
    EXPECT_EQ(";2;", pp("{#define N   2  };N;"));
    EXPECT_EQ(";2;", pp("{#define N /*~*/  2};N;"));
    EXPECT_EQ(";2;", pp("{#define N 2/*~*/   };N;"));
    EXPECT_EQ(";2;", pp("{#define N /*~*/  2/*~*/   };N;"));
    EXPECT_EQ("\n\n\n2", pp("{#define N $\n$\n2}\nN"));
    EXPECT_EQ("\n\n\n2", pp("{#define N \n\n2}\nN"));
    EXPECT_TRUE(empty());
}

TEST_F(ObjectMacroTest, Pragma) {
    EXPECT_EQ(";(*{line 1}*)", pp("{#define P line}{#define L __LINE__};{P L}"));
    EXPECT_EQ("(*{0 1}*);", pp("{__COUNTER__ __COUNTER__};"));
    EXPECT_TRUE(empty());
}

TEST_F(ObjectMacroTest, Redefine) {
    // same value redefinition (no warning)
    EXPECT_NO_THROW(pp("{#define N 2};N;{#define N 2};N"));
    EXPECT_TRUE(empty());

    // different value redefinition (warning, no throw)
    EXPECT_NO_THROW(pp("{#define N 2};N;{#define N 3};N"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());

    // same body through another macro name is still equal after later expansion
    EXPECT_NO_THROW(pp("{#define N X};N;{#define X 2};{#define N X};N"));
    EXPECT_TRUE(empty());

    // blocking warning parity with Python Die.Blocking
    Issue::add_blocking(Issue::Code::MACRO_REDEFINED);
    EXPECT_THROW({ pp("{#define N 2}{#define N 3}"); }, Issue::Exception);
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
    Issue::add_blocking(Issue::Code::MACRO_REDEFINED);
    EXPECT_THROW({ pp("{#define X 2}{#define Y 2}{#define N X}{#define N Y}"); }, Issue::Exception);
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
}


