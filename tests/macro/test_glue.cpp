#include "test_helper.hpp"

// ---- @@ (token pasting) tests ported from Python test_pp_glue.py ----
// Internal _glue() API is tested here via function macro expansion.

class GlueTest : public JieppTest {};

TEST_F(GlueTest, Regular) {
    EXPECT_EQ(";ab;",         pp("{#define F(a,b) a@@b};F(a,b);"));
    EXPECT_EQ(";abcABC;",     pp("{#define F(a,b) a@@b};F(abc,ABC);"));
    EXPECT_EQ(";1234;",       pp("{#define F(a,b) a@@b};F(12,34);"));
    // Typed numeric literals form Token::ANY after pasting
    EXPECT_EQ(";16#216#5;",   pp("{#define F(a,b) a@@b};F(16#2,16#5);"));
    EXPECT_EQ(";16#ab16#cd;", pp("{#define F(a,b) a@@b};F(16#ab,16#cd);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, Type) {
    EXPECT_EQ(";ab;",  pp("{#define F(a,b) a@@b};F(a,b);"));
    EXPECT_EQ(";a23;", pp("{#define F(a,b) a@@b};F(a,23);"));
    EXPECT_EQ(";23a;", pp("{#define F(a,b) a@@b};F(23,a);"));
    EXPECT_EQ(";2345;",pp("{#define F(a,b) a@@b};F(23,45);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, Whitespace) {
    // Last real token of left arg is pasted with first real token of right arg.
    // Surrounding whitespace in args is preserved in the output.
    EXPECT_EQ(";a bc;", pp("{#define F(a,b) a@@b};F(a b ,c);"));
    EXPECT_EQ(";a bc;", pp("{#define F(a,b) a@@b};F(  a b,c);"));
    EXPECT_EQ(";ab c;", pp("{#define F(a,b) a@@b};F(a,b c  );"));
    EXPECT_EQ(";ab c;", pp("{#define F(a,b) a@@b};F(a,  b c);"));
    EXPECT_EQ(";a b cA B C;", pp("{#define F(a,b) a@@b};F(  a b c  ,    A B C     );"));
    EXPECT_EQ(";aA;",   pp("{#define F(a,b) a@@b};F(  a  ,    A     );"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, WhitespaceOnly) {
    // Whitespace-only args are treated as empty; empty left operand yields right param name.
    EXPECT_EQ(";b;", pp("{#define F(a,b) a@@b};F(  ,   );"));
    EXPECT_EQ(";b;", pp("{#define F(a,b) a@@b};F(,);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, Error) {
    // Pasting identifier or number with special tokens (@, @@, string literal) is invalid.
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(xyz,@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(xyz,@@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(xyz,'');"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@,xyz);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@@,xyz);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F('',xyz);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(123,@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(123,@@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(123,'');"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@,123);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@@,123);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F('',123);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_TRUE(empty());
}

// ---- @@ (token pasting) in function macro bodies ----

TEST_F(GlueTest, PasteInFuncMacro) {
    EXPECT_EQ(";xyzw;",          pp("{#define F(a, b) a@@b};F(xy,zw);"));
    EXPECT_EQ(";xyzw;",          pp("{#define F(a, b) a@@b};F(  xy  ,  zw  );"));
    EXPECT_EQ(";A  B  CD  E  F;",pp("{#define F(a, b) a@@b};F(A  B  C,D  E  F);"));
    EXPECT_EQ(";a bc d;",        pp("{#define F(a, b) a@@b};F( a b , c d );"));
    EXPECT_EQ(";xy+-zw;",        pp("{#define F(a, b) a+@@-b};F(xy,zw);"));
    EXPECT_EQ(";xyzw;xyzw;xyzw;",
              pp("{#define F(a, b) a  @@b;a@@  b;a  @@  b};F(xy,zw);"));
    EXPECT_EQ(";a#b;", pp("{#define F(a, b) a@@b};F(a#, b);"));
    EXPECT_EQ(";a#b;", pp("{#define F(a, b) a@@b};F(a, #b);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, PasteInFuncMacroNoArgs) {
    EXPECT_EQ(";ab;",       pp("{#define F() a@@b};F();"));
    EXPECT_EQ(";abc;",      pp("{#define F() a@@b@@c};F();"));
    EXPECT_EQ(";abcd;",     pp("{#define F() a@@b@@c@@d};F();"));
    EXPECT_EQ(";abcd efg hi;",
              pp("{#define F() a@@b@@c@@d e@@f@@g h@@i};F();"));
    EXPECT_TRUE(empty());
}

// NOTE: C++ implementation: when left operand of @@ is empty, the result uses
// the right parameter identifier, not its expanded value.
TEST_F(GlueTest, PasteEmptyArgs) {
    EXPECT_EQ(";b;", pp("{#define F(a,b) a@@b};F(,);"));
    EXPECT_EQ(";b;", pp("{#define F(a,b) a@@b};F(  ,);"));
    EXPECT_EQ(";b;", pp("{#define F(a,b) a@@b};F(,  );"));
    EXPECT_EQ(";b;", pp("{#define F(a,b) a@@b};F(  ,  );"));
    EXPECT_TRUE(empty());
}

// NOTE: C++ impl: bracket token arguments cause ARGUMENT_COUNT_MISMATCH, not
// INVALID_TOKEN_PASTING, when token pasting would form invalid results.
TEST_F(GlueTest, PasteErrors) {
    EXPECT_THROW(pp("{#define F(a,b) b@@a};F(],[);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F(a,b) b@@a};F(]]],[[[);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_TRUE(empty());
}

// ---- @@ with newlines in arguments ----

TEST_F(GlueTest, PasteLines) {
    // Fact: @@ with newlines in arguments concatenates tokens, newlines are consumed
    EXPECT_EQ(";x yz  w\n\n\n;4;x  yz   w\n\n\n\n\n;9",
              pp("{#define F(a, b) a@@b};F(x\ny,z\n\nw);__LINE__;F(x\n\ny,z\n\n\nw);__LINE__"));
    EXPECT_TRUE(empty());
}


