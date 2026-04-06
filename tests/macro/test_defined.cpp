#include "test_helper.hpp"
#include <algorithm>

static std::string strip(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

class DefinedTest : public JieppTest {};

// Fact: defined checks macro existence, not value; comparison is case-sensitive
TEST_F(DefinedTest, Regular) {
    EXPECT_EQ("1e", strip(pp("{#define a}{#if defined a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#if defined x}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#if defined A}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define abc}{#if defined abc}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define abc}{#if defined xyz}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define abc}{#if defined ABC}1{#endif}e")));
    // keywords are not pre-defined macros
    EXPECT_EQ("e",  strip(pp("{#if defined true}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if defined false}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if defined int}1{#endif}e")));
    // 'defined' itself is not a pre-defined macro in this impl
    EXPECT_EQ("e",  strip(pp("{#if defined defined}1{#endif}e")));
    // numeric operands: no error in C++ impl; evaluates to false
    EXPECT_EQ("2e", strip(pp("{#if defined 0}1{#else}2{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if defined 1}1{#else}2{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if defined 0.0}1{#else}2{#endif}e")));
    EXPECT_TRUE(empty());
    // string-literal operand → INVALID_DEFINED_OPERAND
    EXPECT_THROW(pp("{#if defined ''}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined 'abc'}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined \"\"}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined \"abc\"}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_TRUE(empty());
}

// Fact: defined accepts an identifier with or without enclosing parentheses
TEST_F(DefinedTest, ArgSyntax) {
    // valid forms (undefined macro 'a' → false → output "e")
    EXPECT_EQ("e", strip(pp("{#if defined a}1{#endif}e")));
    EXPECT_EQ("e", strip(pp("{#if defined  a }1{#endif}e")));
    EXPECT_EQ("e", strip(pp("{#if defined(a)}1{#endif}e")));
    EXPECT_EQ("e", strip(pp("{#if defined(  a  )}1{#endif}e")));
    EXPECT_EQ("e", strip(pp("{#if defined (a)}1{#endif}e")));
    EXPECT_EQ("e", strip(pp("{#if defined (  a  )}1{#endif}e")));
    EXPECT_TRUE(empty());
    // missing operand → INVALID_DEFINED_OPERAND
    EXPECT_THROW(pp("{#if defined}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined  }1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined()}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined(  )}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined ()}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined (  )}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    // malformed paren → INVALID_DEFINED_OPERAND in C++ impl
    EXPECT_THROW(pp("{#if defined(}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined  (}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined  (  }1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined(()}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined(  ()}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined(  ()  }1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined(,}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    // nested parens as operand → INVALID_DEFINED_OPERAND
    EXPECT_THROW(pp("{#if defined((a))}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined((  a  ))}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined ((a))}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    EXPECT_THROW(pp("{#if defined (  (  a  )  )}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, code());
    // multi-param call → INVALID_EXPRESSION in C++ impl
    EXPECT_THROW(pp("{#if defined(a, )}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(pp("{#if defined(a, b)}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    // keyword or operator as operand → INVALID_EXPRESSION
    EXPECT_THROW(pp("{#if defined(not a)}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(pp("{#if defined not a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(pp("{#if defined(a = b)}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_THROW(pp("{#if defined a, }1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, code());
    EXPECT_TRUE(empty());
}

// Fact: #undef removes the definition; a previously defined then undef'd macro is not defined
TEST_F(DefinedTest, Misc) {
    EXPECT_EQ("e", strip(pp("{#define a}{#undef a}{#if defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#undef a}{#if not defined a}1{#endif}e")));
    EXPECT_TRUE(empty());
}

// Fact: defined tests existence only; any defined macro evaluates to true regardless of value
TEST_F(DefinedTest, Value) {
    EXPECT_EQ("1e", strip(pp("{#define a false}{#if defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a true}{#if defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a 0}{#if defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a 1}{#if defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define b}{#define a b}{#if defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a b}{#if defined a}1{#endif}e")));
    EXPECT_TRUE(empty());
}

// Fact: defined binds tighter than =; = is left-associative
TEST_F(DefinedTest, Priority) {
    EXPECT_EQ("1e", strip(pp("{#if not defined a = not defined a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if defined a = defined a}1{#endif}e")));
    EXPECT_THROW(pp("{#if defined a = defined a = defined b = defined b}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(pp("{#if defined a = defined a = defined a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(pp("{#if defined a = defined a or defined a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_THROW(pp("{#if defined a or defined a = defined a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::EXPR_TYPE_ERROR, code());
    EXPECT_TRUE(empty());
}

// Fact: defined combines with or/and for compound conditions
TEST_F(DefinedTest, Expr) {
    // or: true when at least one operand is defined
    EXPECT_EQ("e",  strip(pp("{#if defined(a) or defined(b)}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define b}{#if defined(a) or defined(b)}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#if defined(a) or defined(b)}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#define b}{#if defined(a) or defined(b)}1{#endif}e")));
    // and: true only when both operands are defined
    EXPECT_EQ("e",  strip(pp("{#if defined(a) and defined(b)}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define b}{#if defined(a) and defined(b)}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#if defined(a) and defined(b)}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#define b}{#if defined(a) and defined(b)}1{#endif}e")));
    EXPECT_TRUE(empty());
}



