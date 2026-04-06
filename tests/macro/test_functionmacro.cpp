#include "test_helper.hpp"

#include "loader/lexer.hpp"

static std::vector<Token> ts(const std::string& text) {
    return iec3_tokens_from_string(text, false);
}

// ---- function macro: simple ----

class FuncMacroTest : public JieppTest {};

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
    // collect non-blank, whitespace-trimmed lines
    std::vector<std::string> lines;
    std::istringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        auto first = line.find_first_not_of(" \t\r");
        auto last  = line.find_last_not_of(" \t\r");
        if (first != std::string::npos)
            lines.push_back(line.substr(first, last - first + 1));
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

// NOTE: '{#define: ...}' (colon after define) is silently consumed without error.
// '{#define F(() ...}' (invalid param list) triggers INVALID_DEFINE_SYNTAX.
TEST_F(FuncMacroTest, SyntaxErrorInDefine) {
    // Fact: define with colon produces no output and no diagnostic
    EXPECT_EQ("", pp("{#define: F( a}"));
    EXPECT_TRUE(empty());

    // Fact: define with invalid param list (open paren inside params) triggers an error
    EXPECT_THROW(pp("{#define  F(() a}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, code());
    EXPECT_TRUE(empty());
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


