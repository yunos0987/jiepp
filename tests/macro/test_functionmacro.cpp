#include "test_helper.hpp"

#include "loader/lexer.hpp"

static std::vector<Token> ts(const std::string& text) {
    return iec3_tokens_from_string(text, false);
}

// ---- function macro: simple ----

class FuncMacroTest : public JieppTest {
protected:
    // R3: `{#define <def>}` followed by an {#ifdef F} probe, in continue mode
    // (caller installs the guard). Asserts F was not defined ("U") and exactly
    // one PP30 whose text contains `reason`. messages() is read once.
    void expect_param_list_error(const std::string& def, const std::string& reason) {
        SCOPED_TRACE(def);
        EXPECT_EQ("U", pp("{#define " + def + "}{#ifdef F}D{#else}U{#endif}"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find(reason)) << msgs[0];
    }
};

TEST_F(FuncMacroTest, Simple) {
    EXPECT_EQ(";2+A+ab;", pp("{#define F(a) a+A+ab};F(2);"));
    // No space between args — whitespace after comma is preserved in C++ impl
    EXPECT_EQ(";2+3;",    pp("{#define F(a,b) a+b};F(2,3);"));
    EXPECT_EQ(";2;",      pp("{#define F() 2};F();"));
    // F without parens is not expanded
    EXPECT_EQ(";F;",  pp("{#define F(a) a};F;"));
    EXPECT_EQ(";F ;", pp("{#define F(a) a};F ;"));
    EXPECT_EQ(";F ",  pp("{#define F(a) a};F "));
    // array-subscript arguments do not split on comma
    EXPECT_EQ(";a[0];",       pp("{#define F(x) x};F(a[0]);"));
    EXPECT_EQ(";a[0,1];",     pp("{#define F(x) x};F(a[0,1]);"));
    EXPECT_EQ(";a[0,1,2];",   pp("{#define F(x) x};F(a[0,1,2]);"));
    EXPECT_EQ(";a[0]+b[1];",     pp("{#define F(x,y) x+y};F(a[0],b[1]);"));
    EXPECT_EQ(";a[0,1]+b[2,3];", pp("{#define F(x,y) x+y};F(a[0,1],b[2,3]);"));
    EXPECT_EQ(";a[0,1,2]+b[3,4,5];", pp("{#define F(x,y) x+y};F(a[0,1,2],b[3,4,5]);"));
    EXPECT_EQ(";a[0]+b[1]+c[2];", pp("{#define F(x,y,z) x+y+z};F(a[0],b[1],c[2]);"));
    EXPECT_EQ(";a[0,1]+b[2,3]+c[4,5];", pp("{#define F(x,y,z) x+y+z};F(a[0,1],b[2,3],c[4,5]);"));
    EXPECT_EQ(";a[0,1,2]+b[3,4,5]+c[6,7,8];",
              pp("{#define F(x,y,z) x+y+z};F(a[0,1,2],b[3,4,5],c[6,7,8]);"));
    EXPECT_EQ(";a[b[0]];", pp("{#define F(x) x};F(a[b[0]]);"));
    EXPECT_EQ(";a[b[0],b[1]];", pp("{#define F(x) x};F(a[b[0],b[1]]);"));
    EXPECT_EQ(";a[b[0],b[1],b[2]];", pp("{#define F(x) x};F(a[b[0],b[1],b[2]]);"));
    EXPECT_EQ(";a[b[0,1]];", pp("{#define F(x) x};F(a[b[0,1]]);"));
    EXPECT_EQ(";a[b[0,1],b[2,3]];", pp("{#define F(x) x};F(a[b[0,1],b[2,3]]);"));
    EXPECT_EQ(";a[b[0,1],b[2,3],b[4,5]];",
              pp("{#define F(x) x};F(a[b[0,1],b[2,3],b[4,5]]);"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, EmptyBody) {
    EXPECT_EQ(";;", pp("{#define F()};F();"));
    EXPECT_EQ(";;", pp("{#define F(a)};F();"));
    EXPECT_EQ(";;", pp("{#define F(a,b)};F(,);"));
    EXPECT_EQ(";;", pp("{#define F(a,b,c)};F(,,);"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, DuplicateParameter) {
    EXPECT_THROW(pp("{#define F(x,x) x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, code());
    EXPECT_THROW(pp("{#define F(a,b,a) a+b}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, code());
    EXPECT_THROW(pp("{#define G(x,y,z,x) x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, code());
}

// R2: in continue mode, a duplicate parameter name aborts the whole
// {#define} -- the macro is never defined, so a later call is left as
// plain, unexpanded text (matching gcc/clang).
TEST_F(FuncMacroTest, DuplicateParameterNotDefinedInContinueMode) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";F(1,2);", pp("{#define F(x,x) [x]};F(1,2);"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
    EXPECT_EQ(1, Issue::error_count_);
}

// R2: only the *first* duplicate is reported (clang-like), not one per
// repeated name.
TEST_F(FuncMacroTest, DuplicateParameterReportedOnce) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";G(1,2,3);", pp("{#define G(a,a,a) [a]};G(1,2,3);"));
    // messages() (like codes()/message()) drains the diagnostic buffer, so
    // it must be captured once and both the code and the text checked
    // against that same snapshot -- calling codes() then message() would
    // have the second call see an already-drained (empty) buffer.
    auto msgs1 = messages();
    ASSERT_EQ(1u, msgs1.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, PlainTextMessage::parse_code(msgs1[0]));
    EXPECT_NE(std::string::npos, msgs1[0].find('a'));

    EXPECT_EQ(";H(1,2,3,4);", pp("{#define H(a,b,a,b) a};H(1,2,3,4);"));
    auto msgs2 = messages();
    ASSERT_EQ(1u, msgs2.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, PlainTextMessage::parse_code(msgs2[0]));
    EXPECT_NE(std::string::npos, msgs2[0].find('a'));

    EXPECT_EQ(2, Issue::error_count_);
}

// R2: a rejected redefinition leaves the existing definition of the same
// name untouched -- no MACRO_REDEFINED (PP35), no PP34, and the old
// definition still expands.
TEST_F(FuncMacroTest, DuplicateParameterKeepsExistingDefinition) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;[1];",
              pp("{#define F(x) [x]};{#define F(y,y) <y>};F(1);"));
    auto cs1 = codes();
    ASSERT_EQ(1u, cs1.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs1[0]);

    EXPECT_EQ(";;7;", pp("{#define N 7};{#define N(a,a) a};N;"));
    auto cs2 = codes();
    ASSERT_EQ(1u, cs2.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs2[0]);
}

// R2: under {#ignore PP36}, the diagnostic is suppressed but the macro is
// still not defined (like {#ignore PP33}).
TEST_F(FuncMacroTest, DuplicateParameterIgnoredStillNotDefined) {
    EXPECT_EQ(";F(1,2);",
              pp("{#ignore PP36}{#define F(x,x) [x]};F(1,2);"));
    EXPECT_TRUE(empty());
}

// R2: a duplicate parameter before a trailing '...' is caught the same way
// as an ordinary duplicate.
TEST_F(FuncMacroTest, DuplicateParameterVariadicNotDefined) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";V(1,2,3);", pp("{#define V(a,a,...) [a]};V(1,2,3);"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
}

// D4/R2: a regular parameter named __VA_ARGS__/__VA_ARGC__ collides with
// the implicit name(s) a trailing '...' introduces, so it is a duplicate
// too and the {#define} is abandoned -- it must not be silently
// overwritten by the implicit variadic entry (FunctionMacro's args_ map).
// Without a trailing '...', these names are ordinary parameter names.
TEST_F(FuncMacroTest, VaArgsNamedParameterClashesWithEllipsis) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";F(1,2);",
              pp("{#define F(__VA_ARGS__, ...) [__VA_ARGS__]};F(1,2);"));
    // See DuplicateParameterReportedOnce: capture messages() once (codes()
    // then message() would drain the buffer twice).
    auto msgs1 = messages();
    ASSERT_EQ(1u, msgs1.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, PlainTextMessage::parse_code(msgs1[0]));
    EXPECT_NE(std::string::npos, msgs1[0].find("__VA_ARGS__"));

    EXPECT_EQ(";C(1,2);",
              pp("{#define C(__VA_ARGC__, ...) [__VA_ARGC__]};C(1,2);"));
    auto cs2 = codes();
    ASSERT_EQ(1u, cs2.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs2[0]);

    EXPECT_EQ(";[3];", pp("{#define H(__VA_ARGS__) [__VA_ARGS__]};H(3);"));
    EXPECT_TRUE(empty());
}

// NOTE: Leading/trailing Token::WS is trimmed from macro arguments (Python-compatible).
// ts_flatten strips leading/trailing Token::WS and Token::C (Python-compatible).
TEST_F(FuncMacroTest, WhitespaceInParams) {
    // Leading whitespace and comments trimmed
    EXPECT_EQ(";2;", pp("{#define F(a) a};F(  /*~*/2);"));
    EXPECT_EQ(";2;", pp("{#define F(a) a};F(2/*~*/  );"));
    EXPECT_EQ(";2;", pp("{#define F(a) a};F(  /*~*/2/*~*/  );"));
    EXPECT_EQ(";2 3 4;", pp("{#define F(a) a};F(2 3 4);"));
    EXPECT_EQ(";2 3 4;", pp("{#define F(a) a};F(  /*~*/2 3 4);"));
    EXPECT_EQ(";2 3 4;", pp("{#define F(a) a};F(2 3 4/*~*/  );"));
    EXPECT_EQ(";2 3 4;", pp("{#define F(a) a};F(  /*~*/2 3 4/*~*/  );"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, WhitespaceInBody) {
    EXPECT_EQ(";2/*~*/2;", pp("{#define F(a) a/*~*/a};F(2);"));
    EXPECT_EQ(";2/*~*/2;", pp("{#define F(a) /*~*/  a/*~*/a};F(2);"));
    EXPECT_EQ(";2/*~*/2;", pp("{#define F(a) a/*~*/a  /*~*/};F(2);"));
    EXPECT_EQ(";2/*~*/2;", pp("{#define F(a) /*~*/  a/*~*/a  /*~*/};F(2);"));
    EXPECT_TRUE(empty());
}

// NOTE: Leading/trailing Token::WS and Token::C are trimmed from each argument (Python-compatible).
// Space-only and comment-only args become empty.
TEST_F(FuncMacroTest, EmptyArgs) {
    EXPECT_EQ(";a;", pp("{#define F() a};F();"));
    EXPECT_EQ(";;",  pp("{#define F(a) a};F();"));
    EXPECT_EQ("; ;", pp("{#define F(a,b) a b};F(,);"));
    // Space-only arg trimmed to empty
    EXPECT_EQ(";1++2;", pp("{#define F(a) 1+a+2};F( );"));
    EXPECT_EQ(";+++1;", pp("{#define F(a,b,c) a+b+c+1};F( ,,  );"));
    // Comment-only args trimmed to empty
    EXPECT_EQ(";+1++2;", pp("{#define F(a,b) a+1+b+2};F(/*,*/,(*,*));"));
    EXPECT_EQ(";a[0] ;", pp("{#define F(a,b) a b};F(a[0],);"));
    EXPECT_EQ(";a[0,1] ;", pp("{#define F(a,b) a b};F(a[0,1],);"));
    EXPECT_EQ(";a[0,1,2] ;", pp("{#define F(a,b) a b};F(a[0,1,2],);"));
    EXPECT_EQ("; b[0];", pp("{#define F(a,b) a b};F(,b[0]);"));
    EXPECT_EQ("; b[0,1];", pp("{#define F(a,b) a b};F(,b[0,1]);"));
    EXPECT_EQ("; b[0,1,2];", pp("{#define F(a,b) a b};F(,b[0,1,2]);"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, ArgCountErrors) {
    EXPECT_THROW(pp("{#define E};{#define F()};F(E)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_EQ(";0;0", pp("{#define F0() 0};F0();F0( )"));
    EXPECT_THROW(pp("{#define F0() 0};F0(a)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F0() 0};F0(a,b)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F0() 0};F0(a,b,c)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F0() 0};F0(a[0,1])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F0() 0};F0(a[0,1],b[2,3])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_EQ(";;", pp("{#define F1(x) x};F1();F1( )"));
    EXPECT_EQ(";a", pp("{#define F1(x) x};F1(a)"));
    EXPECT_EQ(";a[0,1]", pp("{#define F1(x) x};F1(a[0,1])"));
    EXPECT_THROW(pp("{#define F1(x) x};F1(a[0,1], b[2,3])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F1(x) x};F1(a[0,1], b[2,3], c[4,5])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2()"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2(a)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_EQ(";a,b", pp("{#define F2(x,y) x,y};F2(a, b)"));
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2(a,b,c)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2(a, b, c, d)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2(a[0,1])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_EQ(";a[0,1],b[2,3]", pp("{#define F2(x,y) x,y};F2(a[0,1], b[2,3])"));
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2(a[0,1], b[2,3], c[4,5])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F2(x,y) x,y};F2(a[0,1], b[2,3], c[4,5], d[6,7])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, NestedCallInArgs) {
    EXPECT_EQ(";2/2+3/3*3/3+1;",
              pp("{#define F(a,b) a+b+1}{#define G(a) a*a}{#define H(a) a/a};F(H(2),G(H(3)));"));
    EXPECT_EQ(";a[0,1]/a[0,1]+b[2,3]/b[2,3]*b[2,3]/b[2,3]+1;",
              pp("{#define F(a,b) a+b+1}{#define G(a) a*a}{#define H(a) a/a};F(H(a[0,1]),G(H(b[2,3])));"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, NestedCallInDefs) {
    EXPECT_EQ(";2*2+3+1;", pp("{#define F(a,b) G(a)+b+1}{#define G(a) a*a};F(2,3);"));
    EXPECT_EQ(";2*2+3+1;", pp("{#define G(a) a*a}{#define F(a,b) G(a)+b+1};F(2,3);"));
    EXPECT_EQ(";2/2*2+3+1;",
              pp("{#define F(a,b) G(a)+b+1}{#define G(a) H(a)*a}{#define H(a) a/a};F(2,3);"));
    EXPECT_EQ(";a[0,1]*a[0,1]+b[2,3]+1;",
              pp("{#define F(a,b) G(a)+b+1}{#define G(a) a*a};F(a[0,1],b[2,3]);"));
    EXPECT_EQ(";a[0,1]*a[0,1]+b[2,3]+1;",
              pp("{#define G(a) a*a}{#define F(a,b) G(a)+b+1};F(a[0,1],b[2,3]);"));
    EXPECT_EQ(";a[0,1]/a[0,1]*a[0,1]+b[2,3]+1;",
              pp("{#define F(a,b) G(a)+b+1}{#define G(a) H(a)*a}{#define H(a) a/a};F(a[0,1],b[2,3]);"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, Recursive) {
    EXPECT_EQ(";F(2,3)+3+1;", pp("{#define F(a,b) F(a,b)+b+1};F(2,3);"));
    EXPECT_EQ(";F(5,7)+3+2;G(11,13)+2+3;",
              pp("{#define F(a,b) G(a,b)+2}{#define G(a,b) F(a,b)+3};F(5,7);G(11,13);"));
    EXPECT_EQ(";;\nA,F(A),A,F(A),G(A);\nB,B,F(B),G(B),G(B);",
              pp("{#define F(x) x,F(x),G(x)};{#define G(x) x,F(x),G(x)};\nF(A);\nG(B);"));
    EXPECT_EQ(";\n;\n;\nH(A),H(A),G(A),F(A),A,F(A),A,H(A),G(A),F(A),A,G(A),F(A),A,F(A),A;\nH(B),G(B),H(B),G(B),F(B),B,B,G(B),H(B),G(B),F(B),B,G(B),F(B),B,B;\nH(C),H(C),G(C),H(C),G(C),F(C),C,C,H(C),H(C),G(C),F(C),C,F(C),C,C;",
              pp("{#define F(x) H(x),G(x),F(x),x};\n{#define G(x) H(x),G(x),F(x),x};\n{#define H(x) H(x),G(x),F(x),x};\nF(A);\nG(B);\nH(C);"));
    EXPECT_EQ(";F(a[0,1],b[2,3])+b[2,3]+1;",
              pp("{#define F(a,b) F(a,b)+b+1};F(a[0,1],b[2,3]);"));
    EXPECT_EQ(";F(a[0,1],b[2,3])+3+2;G(c[4,5],d[6,7])+2+3;",
              pp("{#define F(a,b) G(a,b)+2}{#define G(a,b) F(a,b)+3};F(a[0,1],b[2,3]);G(c[4,5],d[6,7]);"));
    EXPECT_EQ(";;\na[0,1],F(a[0,1]),a[0,1],F(a[0,1]),G(a[0,1]);\nb[2,3],b[2,3],F(b[2,3]),G(b[2,3]),G(b[2,3]);",
              pp("{#define F(x) x,F(x),G(x)};{#define G(x) x,F(x),G(x)};\nF(a[0,1]);\nG(b[2,3]);"));
    EXPECT_EQ(";\n;\n;\nH(a[0,1]),H(a[0,1]),G(a[0,1]),F(a[0,1]),a[0,1],F(a[0,1]),a[0,1],H(a[0,1]),G(a[0,1]),F(a[0,1]),a[0,1],G(a[0,1]),F(a[0,1]),a[0,1],F(a[0,1]),a[0,1];\nH(b[2,3]),G(b[2,3]),H(b[2,3]),G(b[2,3]),F(b[2,3]),b[2,3],b[2,3],G(b[2,3]),H(b[2,3]),G(b[2,3]),F(b[2,3]),b[2,3],G(b[2,3]),F(b[2,3]),b[2,3],b[2,3];\nH(c[4,5]),H(c[4,5]),G(c[4,5]),H(c[4,5]),G(c[4,5]),F(c[4,5]),c[4,5],c[4,5],H(c[4,5]),H(c[4,5]),G(c[4,5]),F(c[4,5]),c[4,5],F(c[4,5]),c[4,5],c[4,5];",
              pp("{#define F(x) H(x),G(x),F(x),x};\n{#define G(x) H(x),G(x),F(x),x};\n{#define H(x) H(x),G(x),F(x),x};\nF(a[0,1]);\nG(b[2,3]);\nH(c[4,5]);"));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, Scope) {
    EXPECT_EQ("\n\n\n\n2;\n3;\n5+7+1;\n2;\n3;\n",
              pp("\n{#define a 2}\n{#define F(a,b) a+b+1}\n{#define b 3}\na;\nb;\nF(5,7);\na;\nb;\n"));
    EXPECT_TRUE(empty());
}

// ---- function macro: argument count validation details ----

TEST_F(FuncMacroTest, Args) {
    // Fact: 0-param macro: only empty argument list is accepted
    EXPECT_EQ(";0", pp("{#define F0() 0};F0()"));
    EXPECT_THROW(pp("{#define F0() 0};F0(a)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F0() 0};F0(a[0,1])"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());

    // Fact: 1-param macro: empty and single args are accepted; 2+ args is an error
    EXPECT_EQ(";",  pp("{#define F1(x) x};F1()"));
    EXPECT_EQ(";a", pp("{#define F1(x) x};F1(a)"));
    EXPECT_THROW(pp("{#define F1(x) x};F1(a, b)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F1(x) x};F1(a, b, c)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_TRUE(empty());
}

// ---- function macro: PP metaprogramming application ----

TEST_F(FuncMacroTest, App) {
    // Fact: complex function macro application implements simple arithmetic via token pasting
    const char* input = R"(
{#define PP_IF_0(t, f) f}
{#define PP_IF_1(t, f) t}
{#define PP_IF(c, t, f) PP_IF_I(c, t, f)}
{#define PP_IF_I(c, t, f) PP_IF_ @@ c (t, f)}
{#define PP_BOOL_0 0}
{#define PP_BOOL_1 1}
{#define PP_BOOL_2 1}
{#define PP_BOOL_3 1}
{#define PP_BOOL_4 1}
{#define PP_BOOL(n) PP_BOOL_I(n)}
{#define PP_BOOL_I(n) PP_BOOL_ @@ n}
{#define PP_INC_0 1}
{#define PP_INC_1 2}
{#define PP_INC_2 3}
{#define PP_INC_3 4}
{#define PP_INC_4 5}
{#define PP_INC_5 6}
{#define PP_INC_6 7}
{#define PP_INC_7 8}
{#define PP_INC(n) PP_INC_I(n)}
{#define PP_INC_I(n) PP_INC_ @@ n}
{#define PP_DEC_0 0}
{#define PP_DEC_1 0}
{#define PP_DEC_2 1}
{#define PP_DEC_3 2}
{#define PP_DEC_4 3}
{#define PP_DEC_5 4}
{#define PP_DEC_6 5}
{#define PP_DEC_7 6}
{#define PP_DEC(n) PP_DEC_I(n)}
{#define PP_DEC_I(n) PP_DEC_ @@ n}
{#define PP_TUPLE_ELEM_1_0(a) a}
{#define PP_TUPLE_ELEM_2_0(a, b) a}
{#define PP_TUPLE_ELEM_2_1(a, b) b}
{#define PP_TUPLE_ELEM_3_0(a, b, c) a}
{#define PP_TUPLE_ELEM_3_1(a, b, c) b}
{#define PP_TUPLE_ELEM_3_2(a, b, c) c}
{#define PP_TUPLE_ELEM(n, i, tup) PP_TUPLE_ELEM_I(n, i, tup)}
{#define PP_TUPLE_ELEM_I(n, i, tup) PP_TUPLE_ELEM_ @@ n @@ _ @@ i tup}
{#define PP_ADD(m, n) PP_TUPLE_ELEM(2, 1, PP_ADD_I(PP_ADD_I(PP_ADD_I(PP_ADD_I(PP_ADD_I(PP_ADD_I(PP_ADD_I(PP_ADD_I((m, n))))))))))}
{#define PP_ADD_I(tup) PP_ADD_II tup}
{#define PP_ADD_II(m, n) PP_IF(PP_BOOL(m), (PP_DEC(m), PP_INC(n)), (m, n))}
PP_ADD(2, 3)
PP_ADD(PP_ADD(1, 2), PP_ADD(2, 3))
)";
    const std::string result = pp(input);
    // collect non-blank, whitespace-trimmed lines, skipping any injected
    // line-marker line (the ~40 consecutive {#define ...} lines above collapse
    // to nothing but blank output lines, which blank-line compaction replaces
    // with a single (*{#:N}*)/{#:N} marker line once the run exceeds the
    // default 7-line threshold)
    std::vector<std::string> lines;
    std::istringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        auto first = line.find_first_not_of(" \t\r");
        auto last  = line.find_last_not_of(" \t\r");
        if (first == std::string::npos)
            continue;
        std::string trimmed = line.substr(first, last - first + 1);
        if (trimmed.starts_with("(*{#:") || trimmed.starts_with("{#:"))
            continue;
        lines.push_back(trimmed);
    }
    ASSERT_EQ(2u, lines.size());
    EXPECT_EQ("5", lines[0]);  // PP_ADD(2, 3) = 5
    EXPECT_EQ("8", lines[1]);  // PP_ADD(PP_ADD(1,2), PP_ADD(2,3)) = PP_ADD(3,5) = 8
    EXPECT_TRUE(empty());
}

// ---- function macro: bracket argument syntax ----

// NOTE: In C++ impl, ']' as an argument (or part of an argument) is accepted.
// '[' without a matching ']' before the closing ')' causes ARGUMENT_COUNT_MISMATCH,
// because '[' opens a subscript context that ']' or ']' of the call must close.
TEST_F(FuncMacroTest, BracketSyntax) {
    // Fact: ']' and sequences of ']' are accepted as argument values
    EXPECT_EQ(";];",   pp("{#define F(a) a};F(]);"));
    EXPECT_EQ(";]]];", pp("{#define F(a) a};F(]]]);"));

    // Fact: bare '[' without matching ']' before ')' causes invalid param count
    EXPECT_THROW(pp("{#define F(a) a};F([);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F(a) a};F([[[);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    // ']' then '[' as second arg: '[' is still unmatched before ')'
    EXPECT_THROW(pp("{#define F(a,b) b 0 a};F(],[);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_TRUE(empty());
}

// NOTE: Balanced brackets '[...]', contents with commas '[1,2,3]', and paren-wrapped
// args '(...)' all pass correctly as single arguments to a 1-param macro.
TEST_F(FuncMacroTest, BracketSyntaxComplex1) {
    // Fact: representative 1-param macro invocations with bracket/paren arguments expand to empty
    // plain values
    EXPECT_EQ(";;", pp("{#define F(x)};F( );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( v );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( g(1,2,3) );"));
    // balanced brackets
    EXPECT_EQ(";;", pp("{#define F(x)};F( [] );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( [1,2,3] );"));
    // paren-wrapped
    EXPECT_EQ(";;", pp("{#define F(x)};F( (v) );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( (g(1,2,3)) );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( ([1,2,3]) );"));
    // bracket-wrapped
    EXPECT_EQ(";;", pp("{#define F(x)};F( [v] );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( [g(1,2,3)] );"));
    // bracket-only ']' values
    EXPECT_EQ(";;", pp("{#define F(x)};F( ] );"));
    EXPECT_EQ(";;", pp("{#define F(x)};F( ]]] );"));
    // bare '[' causes error
    EXPECT_THROW(pp("{#define F(x)};F( [ );"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F(x)};F( [[[ );"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_TRUE(empty());
}

// NOTE: Same bracket rules apply to each argument of a 2-param macro.
TEST_F(FuncMacroTest, BracketSyntaxComplex2) {
    // Fact: representative 2-param macro invocations with bracket/paren arguments expand to empty
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( v , v );"));
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( (v) , (v) );"));
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( [v] , [v] );"));
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( [1,2,3] , [4,5,6] );"));
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( (g(1,2,3)) , (g(4,5,6)) );"));
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( v , (v) );"));
    EXPECT_EQ(";;", pp("{#define F(x, y)};F( g(1,2,3) , [1,2,3] );"));
    EXPECT_TRUE(empty());
}

// ---- function macro: define syntax errors ----

// NOTE: '{#define: ...}' is the colon form of {#define}; an unterminated
// parameter list is PP30.
// '{#define F(() ...}' (invalid param list) triggers INVALID_DEFINE_SYNTAX.
TEST_F(FuncMacroTest, SyntaxErrorInDefine) {
    // Fact: the colon form still requires a well-formed parameter list --
    // an unterminated list ('(a' with no ')') is PP30, like any other define.
    EXPECT_THROW(pp("{#define: F( a}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    EXPECT_EQ(";[1];", pp("{#define: F(a) [a]};F(1);"));
    EXPECT_TRUE(empty());

    // Fact: define with invalid param list (open paren inside params) triggers an error
    EXPECT_THROW(pp("{#define  F(() a}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    EXPECT_TRUE(empty());
}

// R3: a parameter is expected but the next token is neither ',' nor ')'.
TEST_F(FuncMacroTest, ParamListExpectedCommaOrParen) {
    Issue::ContinueMode guard({});
    const char* reason = "expected ',' or ')'";
    expect_param_list_error("F(a b) ok", reason);
    expect_param_list_error("F(x body text", reason);
    expect_param_list_error("F(a+b) ok", reason);
    expect_param_list_error("F(int#1) ok", reason);
    expect_param_list_error("F(T#1s) ok", reason);
    expect_param_list_error("F(a (*! d *)) ok", reason);
    expect_param_list_error("F(a$$b) ok", reason);
    expect_param_list_error("F(a@@b) ok", reason);
}

// R3: a parameter name or '...' is expected but the next token is not a
// valid identifier (iec3_is_identifier).
TEST_F(FuncMacroTest, ParamListExpectedParameterName) {
    Issue::ContinueMode guard({});
    const char* reason = "expected parameter name or '...'";
    expect_param_list_error("F(a,,b) ok", reason);
    expect_param_list_error("F(,a) ok", reason);
    expect_param_list_error("F(a,) ok", reason);
    expect_param_list_error("F(,) ok", reason);
    expect_param_list_error("F(1) ok", reason);
    expect_param_list_error("F(. x) ok", reason);
    expect_param_list_error("F(..) ok", reason);
    expect_param_list_error("F(. . .) ok", reason);
    expect_param_list_error("F('s') ok", reason);
    expect_param_list_error("F(%IX0.1) ok", reason);
    expect_param_list_error("F(${x$}) ok", reason);
    expect_param_list_error("F(${#undef X$}) ok", reason);
    expect_param_list_error("F(@a) ok", reason);
    expect_param_list_error("F((a)) ok", reason);
    expect_param_list_error("F([a]) ok", reason);
    expect_param_list_error("F(\xE5\xA4\x89\xE6\x95\xB0) ok", reason);
}

// R3: the parameter list runs out of tokens before a closing ')'.
TEST_F(FuncMacroTest, ParamListMissingCloseParen) {
    Issue::ContinueMode guard({});
    const char* reason = "missing ')'";
    expect_param_list_error("F(x", reason);
    expect_param_list_error("F(a,", reason);
    expect_param_list_error("F(a, body", reason);
    expect_param_list_error("F(", reason);
    expect_param_list_error("F(...", reason);
    expect_param_list_error("F(a, ...", reason);
    // A raw newline inside the '// c' comment now ends the comment at that
    // newline, like a C '//' comment ending at the end of a line, so the
    // rest of the parameter list ('b) [a|b]') is not swallowed and the
    // {#define} succeeds instead of running out of tokens before ')'
    // (test_directive.cpp LineCommentInMultiLineDirective and friends).
    {
        SCOPED_TRACE("F(a, // c\\n b) [a|b]");
        EXPECT_EQ("\n[1|2]", pp("{#define F(a, // c\n b) [a|b]}F(1,2)"));
        EXPECT_TRUE(empty());
    }
}

// R3: only the first malformed-list error is reported, like the existing
// duplicate-parameter and variadic-placement checks (O1-O4 in the design).
TEST_F(FuncMacroTest, ParamListFirstErrorWins) {
    Issue::ContinueMode guard({});
    {
        SCOPED_TRACE("F(1, a, a)");
        EXPECT_EQ(";F(1,2,3);", pp("{#define F(1, a, a) ok};F(1,2,3);"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, cs[0]);
    }
    {
        SCOPED_TRACE("F(a, a, 1)");
        EXPECT_EQ(";F(1,2,3);", pp("{#define F(a, a, 1) ok};F(1,2,3);"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
    }
    {
        SCOPED_TRACE("F(... 1)");
        EXPECT_EQ(";F(1);", pp("{#define F(... 1) ok};F(1);"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, cs[0]);
    }
    {
        SCOPED_TRACE("F(a, ... x)");
        EXPECT_EQ(";F(1,2);", pp("{#define F(a, ... x) ok};F(1,2);"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, cs[0]);
    }
    {
        SCOPED_TRACE("F(__VA_ARGS__, ...");
        EXPECT_EQ(";F(1);", pp("{#define F(__VA_ARGS__, ...};F(1);"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
    }
}

// R3: a rejected {#define} leaves an existing definition of the same name
// untouched (like the R2 duplicate-parameter behavior).
TEST_F(FuncMacroTest, ParamListErrorKeepsExistingDefinition) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";;[1];", pp("{#define F(x) [x]};{#define F(a b) <a>};F(1);"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, cs[0]);
    EXPECT_EQ(1, Issue::error_count_);
}

// R3: {#ignore PP30} silences the diagnostic but F is still not defined.
TEST_F(FuncMacroTest, ParamListErrorIgnoredStillNotDefined) {
    EXPECT_EQ("U", pp("{#ignore PP30}{#define F(a b) ok}{#ifdef F}D{#else}U{#endif}"));
    EXPECT_TRUE(empty());
}

// R3: in library (non-continue) mode, a malformed parameter list throws.
TEST_F(FuncMacroTest, ParamListErrorThrowsInLibraryMode) {
    EXPECT_THROW(pp("{#define F(a b) ok}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
}

// R3: forms that were valid before the change must keep working exactly as
// before, including jiepp extensions beyond plain C (whitespace/newlines and
// comments inside the list; the '$n'/'$t' escapes for newline/tab).
TEST_F(FuncMacroTest, ParamListValidFormsAccepted) {
    EXPECT_EQ(";2;", pp("{#define F() 2};F();"));
    EXPECT_EQ(";2;", pp("{#define F( ) 2};F();"));
    EXPECT_EQ(";[1,2];", pp("{#define F(...) [__VA_ARGS__]};F(1,2);"));
    EXPECT_EQ(";[1|2,3];", pp("{#define F(a, ...) [a|__VA_ARGS__]};F(1,2,3);"));
    // a takes "1", the trailing variadic takes just "2" -> __VA_ARGC__ is 1.
    EXPECT_EQ(";[1];", pp("{#define F(a,  ...  ) [__VA_ARGC__]};F(1,2);"));
    EXPECT_EQ(";[1|2];", pp("{#define F( a (* c *) , b /* d */ ) [a|b]};F(1,2);"));
    EXPECT_EQ("[1|2]", pp("{#define F( a $n, $t b ) [a|b]}F(1,2)"));
    EXPECT_EQ(";[1|2|3];", pp("{#define F(if, var, and) [if|var|and]};F(1,2,3);"));
    EXPECT_EQ(";[1];", pp("{#define F(defined) [defined]};F(1);"));
    EXPECT_EQ(";[1|2];", pp("{#define F(_x1, X_2) [_x1|X_2]};F(1,2);"));
    EXPECT_EQ(";[1];", pp("{#define: F(a) [a]};F(1);"));
    EXPECT_EQ(";[3];", pp("{#define F(__VA_ARGS__) [__VA_ARGS__]};F(3);"));
    // Real-newline parameter list (as test_directive.cpp:360): output keeps
    // the leading newline from inside the list.
    EXPECT_EQ("\n[1|2]", pp("{#define F( a,\n b ) [a|b]}F(1,2)"));
    EXPECT_TRUE(empty());
}

// R3, D7: raw_arg is decoded before the diagnostic is built (a '$n' escape
// in the source becomes a real newline), so the diagnostic text must be
// re-escaped to stay on one line, like INVALID_PP_SYNTAX in
// lexer_pragma.cpp.
TEST_F(FuncMacroTest, DefineDiagnosticStaysOnOneLine) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define F('s'$n) x}"));
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_NE(std::string::npos, msgs[0].find("$n"));
}

// ---- function macro: redefinition ----

TEST_F(FuncMacroTest, Redefine) {
    // Fact: redefining a macro with a different body emits MACRO_REDEFINED
    // and the new definition takes effect
    EXPECT_EQ(";12;;23",
              pp("{#define F(a, b, c) a@@b};F(1, 2, 3);{#define F(a, b, c) b@@c};F(1, 2, 3)"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());

    // Fact: redefining a macro with identical body emits no warning
    EXPECT_EQ(";12;;12",
              pp("{#define F(a, b, c) a@@b};F(1, 2, 3);{#define F(a, b, c) a@@b};F(1, 2, 3)"));
    EXPECT_TRUE(empty());
}

// ---- FunctionMacro: internal API ----

TEST_F(FuncMacroTest, Init) {
    Env env = setup();
    // no normalization for plain body with no @ or @@
    FunctionMacro f1({"x", "y"}, ts("x + y"));
    EXPECT_EQ(ts("x + y"), f1.replacement(env));
    // @ followed by param has WS removed; gap between two @-param pairs preserved
    FunctionMacro f2({"x", "y"}, ts("@x @y"));
    EXPECT_EQ(ts("@x @y"), f2.replacement(env));
    // WS around @@ is removed by normalize_glue
    FunctionMacro f3({"x", "y"}, ts("x @@ y"));
    EXPECT_EQ(ts("x@@y"), f3.replacement(env));
    // WS around @@ removed; non-WS tokens adjacent to @@ untouched
    FunctionMacro f4({"x", "y"}, ts("( @@ '' )"));
    EXPECT_EQ(ts("(@@'' )"), f4.replacement(env));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, InitError) {
    // Fact: @ followed by a non-param token fires INVALID_STRINGIZING at expansion time
    EXPECT_THROW(pp("{#define F(x) @  xy};F(1)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, code());
    EXPECT_THROW(pp("{#define F(x) @};F(1)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, code());
    EXPECT_THROW(pp("{#define F(x) @  'x'};F(1)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, code());
}

TEST_F(FuncMacroTest, Equal) {
    // same params and body → equal
    FunctionMacro a({"x", "y"}, ts("x * y"));
    FunctionMacro b({"x", "y"}, ts("x * y"));
    EXPECT_TRUE(a.equal(b));
    // different body → not equal
    FunctionMacro c({"x", "y"}, ts("x + y"));
    EXPECT_FALSE(a.equal(c));
    // different param names → not equal
    FunctionMacro d({"a", "b"}, ts("x * y"));
    EXPECT_FALSE(a.equal(d));
    // different param count → not equal
    FunctionMacro e({"x"}, ts("x * y"));
    EXPECT_FALSE(a.equal(e));
    // additional Python parity cases
    FunctionMacro f({"x", "y"}, ts("x + y"));
    FunctionMacro g({"x", "y"}, ts("x + y"));
    EXPECT_TRUE(f.equal(g));
    FunctionMacro h({"..."}, ts("__VA_ARGS__"));
    FunctionMacro i({"..."}, ts("__VA_ARGS__"));
    EXPECT_TRUE(h.equal(i));
    FunctionMacro j({"X", "Y"}, ts("x + y"));
    EXPECT_FALSE(f.equal(j));
    FunctionMacro k({"x", "y", "z"}, ts("x + y"));
    EXPECT_FALSE(f.equal(k));
    FunctionMacro l({"x", "y"}, ts("x + y + z"));
    EXPECT_FALSE(f.equal(l));
    FunctionMacro m({"y", "x"}, ts("x + y"));
    EXPECT_FALSE(f.equal(m));
    FunctionMacro n({"x", "..."}, ts("x + y"));
    EXPECT_FALSE(f.equal(n));
    // different macro type → not equal
    UserDefinedObjectMacro om(ts("x * y"));
    EXPECT_FALSE(a.equal(om));
    EXPECT_TRUE(empty());
}

TEST_F(FuncMacroTest, Replace) {
    Env env = setup();
    // WS around multiple @@ tokens removed
    EXPECT_EQ(ts("x@@y@@z"),
              FunctionMacro({"x", "y", "z"}, ts(" x  @@   y    @@     z      ")).replacement(env));
    // non-param text between @@ tokens: WS adjacent to @@ removed, internal WS preserved
    EXPECT_EQ(ts("x@@y1   y2@@z"),
              FunctionMacro({"x", "y", "z"}, ts("x @@  y1   y2    @@     z")).replacement(env));
    // single @ followed by param: WS between @ and param removed
    EXPECT_EQ(ts("@x"),
              FunctionMacro({"x"}, ts(" @  x   ")).replacement(env));
    // two separate @-param pairs: gap between them preserved
    EXPECT_EQ(ts("@x   @y"),
              FunctionMacro({"x", "y"}, ts(" @  x   @    y     ")).replacement(env));
    // mixed @ and @@: WS adj to @@ removed, WS between @-param removed
    EXPECT_EQ(ts("@x@@y @z@@w"),
              FunctionMacro({"x", "y", "z", "w"}, ts("@ x @@ y @ z @@ w")).replacement(env));
    EXPECT_EQ(ts("@abcd@@efg@@hi       @j"),
              FunctionMacro({"abcd", "efg", "hi", "j"},
                            ts(" @  abcd   @@    efg     @@      hi       @        j         "))
                  .replacement(env));
    EXPECT_TRUE(empty());
}

// ---- function macro: GNU named variadic (args...) ----

// The last parameter name followed directly by '...' (no comma) is a
// GNU named variadic parameter, like gcc/clang: it receives the variable
// arguments in place of __VA_ARGS__.
TEST_F(FuncMacroTest, GnuNamedVariadicSubstitution) {
    EXPECT_EQ(";[];[1];[1,2,3];",
              pp("{#define F(args...) [args]};F();F(1);F(1,2, 3);"));
    EXPECT_EQ(";[|];[1|];[1|];[1|2,3];",
              pp("{#define G(a, args...) [a|args]};G();G(1);G(1,);G(1,2,3);"));
    // Whitespace may separate the name from '...'; call-side whitespace
    // around a variadic separator is not preserved (U1, pre-existing).
    EXPECT_EQ(";<x,y>;",
              pp("{#define H( args ... ) <args>};H(x , y);"));
    EXPECT_EQ(";[1|2];",
              pp("{#define I(a,args...) [a|args]};I(1,2);"));
    EXPECT_TRUE(empty());
}

// Regression watchpoint: a comma inside a '[...]' array subscript is
// protected from being read as an argument separator (Jiepp extension,
// §17) exactly as for a plain '...' variadic -- each IEC multi-dimensional
// array argument reaches the named variadic's own name intact.
TEST_F(FuncMacroTest, GnuNamedVariadicPreservesArraySubscriptCommas) {
    EXPECT_EQ(";[a[x,y],b[1,2]];",
              pp("{#define F(args...) [args]};F(a[x,y], b[1,2]);"));
    EXPECT_TRUE(empty());
}

// Stringizing (@args) and pasting (a @@ args) work with the named
// variadic's own name exactly as they do with __VA_ARGS__.
TEST_F(FuncMacroTest, GnuNamedVariadicStringizeAndPaste) {
    EXPECT_EQ(";'';'a,b';",
              pp("{#define S(args...) @args};S();S(a,b);"));
    EXPECT_EQ(";'';'x y';",
              pp("{#define S2(a, args...) @ args};S2(1);S2(1,x y);"));
    EXPECT_EQ(";a;ab;ab,c;",
              pp("{#define P(a, args...) a @@ args};P(a);P(a,b);P(a,b,c);"));
    EXPECT_EQ(";_t;u_t;u,v_t;",
              pp("{#define P2(args...) args @@ _t};P2();P2(u);P2(u,v);"));
    EXPECT_TRUE(empty());
}

// Unlike '...', a named variadic does not make __VA_ARGS__ an implicit
// name -- it is an ordinary identifier in the body (gcc/clang; gcc warns,
// clang is silent, jiepp follows clang).
TEST_F(FuncMacroTest, GnuNamedVariadicVaArgsIsOrdinaryIdentifier) {
    EXPECT_EQ(";<__VA_ARGS__>;<__VA_ARGS__>;",
              pp("{#define V(args...) <__VA_ARGS__>};V();V(1,2);"));
    EXPECT_EQ(";<1|2>;",
              pp("{#define V2(__VA_ARGS__, args...) <__VA_ARGS__|args>};V2(1,2);"));
    EXPECT_EQ(";<1,2>;",
              pp("{#define V3(__VA_ARGS__...) <__VA_ARGS__>};V3(1,2);"));
    EXPECT_TRUE(empty());
}

// Since __VA_ARGS__ is an ordinary identifier for a named variadic,
// stringizing it (which requires a formal parameter operand) is PP31, like
// stringizing any other non-parameter identifier.
TEST_F(FuncMacroTest, GnuNamedVariadicStringizeVaArgsIsError) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";@__VA_ARGS__;", pp("{#define V4(args...) @__VA_ARGS__};V4(1);"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_STRINGIZING, cs[0]);
}

// __VA_OPT__ recognizes a named variadic's variable arguments the same way
// it recognizes __VA_ARGS__'s (gcc/clang warn about this; jiepp does not,
// to keep the diagnostics for both variadic forms consistent).
TEST_F(FuncMacroTest, GnuNamedVariadicVaOpt) {
    EXPECT_EQ(";< >;<x 1>;",
              pp("{#define O(args...) <__VA_OPT__(x) args>};O();O(1);"));
    EXPECT_EQ(";<>;<>;<12>;",
              pp("{#define O2(a, args...) <__VA_OPT__(a @@ args)>};O2();O2(1);O2(1,2);"));
    EXPECT_EQ(";<''|x>;<'1,2'|x1,2>;",
              pp("{#define O3(args...) <@__VA_OPT__(args)|x @@ __VA_OPT__(args)>};O3();O3(1,2);"));
    EXPECT_TRUE(empty());
}

// __VA_ARGC__ (jiepp extension) still counts the variable arguments for a
// named variadic.
TEST_F(FuncMacroTest, GnuNamedVariadicVaArgc) {
    EXPECT_EQ(";0;1;2;",
              pp("{#define Q(args...) __VA_ARGC__};Q();Q(1);Q(1,2);"));
    EXPECT_EQ(";<0>;<1>;<2>;",
              pp("{#define Q2(a, args...) <__VA_ARGC__>};Q2(1);Q2(1,);Q2(1,2,3);"));
    EXPECT_EQ(";X2|'2';",
              pp("{#define Q3(args...) X @@ __VA_ARGC__|@__VA_ARGC__};Q3(a,b);"));
    EXPECT_TRUE(empty());
}

// __VA_ARGC__ stays an implicit, reserved name for a named variadic too --
// using it as a parameter name is PP36, exactly as for a plain '...'.
TEST_F(FuncMacroTest, GnuNamedVariadicVaArgcIsReserved) {
    Issue::ContinueMode guard({});
    {
        SCOPED_TRACE("F(__VA_ARGC__...)");
        EXPECT_EQ("U", pp("{#define F(__VA_ARGC__...) x}{#ifdef F}D{#else}U{#endif}"));
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("__VA_ARGC__"));
    }
    {
        SCOPED_TRACE("F(__VA_ARGC__, args...)");
        EXPECT_EQ("U", pp("{#define F(__VA_ARGC__, args...) x}{#ifdef F}D{#else}U{#endif}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
    }
}

// A named variadic must still be the last parameter, with nothing but ')'
// after its own '...' -- same placement rule as plain '...' (PP33), and the
// same duplicate-name (PP36) / malformed-list (PP30) checks apply.
TEST_F(FuncMacroTest, GnuNamedVariadicMustBeLast) {
    Issue::ContinueMode guard({});
    {
        SCOPED_TRACE("F(args..., b) x");
        EXPECT_EQ("U", pp("{#define F(args..., b) x}{#ifdef F}D{#else}U{#endif}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, cs[0]);
    }
    {
        SCOPED_TRACE("F(args... x) x");
        EXPECT_EQ("U", pp("{#define F(args... x) x}{#ifdef F}D{#else}U{#endif}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, cs[0]);
    }
    {
        SCOPED_TRACE("F(args......) x");
        EXPECT_EQ("U", pp("{#define F(args......) x}{#ifdef F}D{#else}U{#endif}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, cs[0]);
    }
    expect_param_list_error("F(args...", "missing ')'");
    {
        SCOPED_TRACE("F(a, a...) x");
        EXPECT_EQ("U", pp("{#define F(a, a...) x}{#ifdef F}D{#else}U{#endif}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
    }
    expect_param_list_error("F(args. ..) x", "expected ',' or ')'");
}

// jiepp does not delete the comma before an omitted named variadic's
// arguments (the GNU `, ## args` idiom) -- same deviation as for '...',
// documented in SPECIFICATION.md.
TEST_F(FuncMacroTest, GnuNamedVariadicCommaPasteKeepsComma) {
    EXPECT_EQ(";g(x,);", pp("{#define LOG(fmt, args...) g(fmt, @@ args)};LOG(x);"));
    EXPECT_TRUE(empty());
}

// A named variadic and a C99 '...' are different definitions (clang warns
// on this too), but two named-variadic (or two '...') definitions that only
// differ in inconsequential whitespace are the same definition.
TEST_F(FuncMacroTest, GnuNamedVariadicRedefinition) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("", pp("{#define F(args...) args}{#define F(args ...) args}"));
    EXPECT_TRUE(codes().empty());

    pp("{#define G(args...) x}{#define G(...) x}");
    auto cs1 = codes();
    ASSERT_EQ(1u, cs1.size());
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, cs1[0]);

    pp("{#define W(__VA_ARGS__...) x}{#define W(...) x}");
    auto cs2 = codes();
    ASSERT_EQ(1u, cs2.size());
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, cs2[0]);

    pp("{#define K(a, ...) a}{#define K(a, b...) a}");
    auto cs3 = codes();
    ASSERT_EQ(1u, cs3.size());
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, cs3[0]);
}

// ---- function macro: multi-use-parameter slot precomputation (task_slug arg-expand-once, D2) ----
//
// FunctionMacro precomputes, once per definition, a dense "slot" index for
// every formal parameter spelled >= 2 times in the body (over-counting is
// accepted: @/@@ operands and __VA_OPT__ content are counted even though
// some never trigger a real expansion). A parameter spelled 0 or 1 times
// gets no slot (arg_slots()[pidx] == -1). __VA_ARGC__ never counts toward
// the variadic parameter's use count.

TEST_F(FuncMacroTest, ArgSlotsSingleUseParameterHasNoSlot) {
    FunctionMacro f({"a"}, ts("a"));
    ASSERT_EQ(1u, f.arg_slots().size());
    EXPECT_EQ(-1, f.arg_slots()[0]);
    EXPECT_EQ(0, f.num_arg_slots());
    EXPECT_TRUE(f.arg_slot_uses().empty());
}

TEST_F(FuncMacroTest, ArgSlotsDoubleUseParameterGetsSlot) {
    FunctionMacro f({"a"}, ts("a a"));
    ASSERT_EQ(1u, f.arg_slots().size());
    EXPECT_NE(-1, f.arg_slots()[0]);
    EXPECT_EQ(1, f.num_arg_slots());
    ASSERT_EQ(1u, f.arg_slot_uses().size());
    EXPECT_EQ(2, f.arg_slot_uses()[static_cast<std::size_t>(f.arg_slots()[0])]);
}

TEST_F(FuncMacroTest, ArgSlotsStringizeOperandCountsTowardOverCount) {
    // @a a: @a is a stringize operand (never truly expanded) but D2 counts
    // it anyway, over-counting (accepted: never under-counts a real need).
    FunctionMacro f({"a"}, ts("@a a"));
    ASSERT_EQ(1u, f.arg_slots().size());
    EXPECT_NE(-1, f.arg_slots()[0]);
    EXPECT_EQ(1, f.num_arg_slots());
    EXPECT_EQ(2, f.arg_slot_uses()[static_cast<std::size_t>(f.arg_slots()[0])]);
}

TEST_F(FuncMacroTest, ArgSlotsVaArgcExcludedFromVariadicUseCount) {
    // __VA_ARGS__ appears twice (counted), __VA_ARGC__ appears once
    // (excluded): the variadic slot's use count must be 2, not 3.
    FunctionMacro f({"..."}, ts("__VA_ARGS__ __VA_ARGC__ __VA_ARGS__"));
    const auto& args = f.args();
    int va_idx = args.at(FunctionMacro::VA_SYM).first;
    ASSERT_GT(static_cast<int>(f.arg_slots().size()), va_idx);
    EXPECT_NE(-1, f.arg_slots()[static_cast<std::size_t>(va_idx)]);
    EXPECT_EQ(2, f.arg_slot_uses()[static_cast<std::size_t>(f.arg_slots()[static_cast<std::size_t>(va_idx)])]);
}

TEST_F(FuncMacroTest, ArgSlotsDoNotAffectEqual) {
    // Slot precomputation is derived data, not part of macro identity.
    FunctionMacro a({"x"}, ts("x x"));
    FunctionMacro b({"x"}, ts("x x"));
    EXPECT_TRUE(a.equal(b));
    FunctionMacro c({"x"}, ts("x"));
    EXPECT_FALSE(a.equal(c)); // different body, not because of slots
}


