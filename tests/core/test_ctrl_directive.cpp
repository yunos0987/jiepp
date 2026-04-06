#include "test_helper.hpp"
#include <algorithm>
#include <vector>

static std::string strip(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

// ---- #if ----

// NOTE: boolean literals are case-sensitive in this implementation.
// Only lowercase "true"/"false" are recognized.
class CtrlDirectiveTest : public JieppTest {};

TEST_F(CtrlDirectiveTest, IfBoolean) {
    EXPECT_EQ("e",  strip(pp("{#if false}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#endif}e")));
    // Mixed-case "falsE"/"truE" are treated as undefined identifiers → false
    EXPECT_EQ("e",  strip(pp("{#if falsE}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if truE}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, IfInteger) {
    EXPECT_EQ("1e", strip(pp("{#if -1}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if -0}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if 0}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if +0}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if +1}1{#endif}e")));
    EXPECT_TRUE(empty());
}

// NOTE: Float/string/date/time literals in #if do not raise INVALID_EXPRESSION.
// Float values are evaluated numerically (non-zero is truthy).
TEST_F(CtrlDirectiveTest, IfFloatTruthy) {
    EXPECT_EQ("1e", strip(pp("{#if -1.0}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#if 0.0}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, IfStringError) {
    EXPECT_THROW(pp("{#if 'xyz'}1{#endif}e"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP51: Missing expression; 'syntax error'", message());

    EXPECT_THROW(pp("{#if ''}1{#endif}e"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP51: Missing expression; 'syntax error'", message());
}

TEST_F(CtrlDirectiveTest, IfIdentifier) {
    // undefined identifier → false
    EXPECT_EQ("e",  strip(pp("{#if a}1{#endif}e")));
    // defined identifier
    EXPECT_EQ("1e", strip(pp("{#define a true}{#if a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a false}{#if a}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, IfEmptyIsError) {
    EXPECT_THROW(pp("{#if}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#if  }1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#define a}{#if a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, IfSyntaxError) {
    // {#if;} has non-empty raw condition but becomes invalid syntax after parsing.
    EXPECT_THROW(pp("{#if;}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

// ---- #else ----

TEST_F(CtrlDirectiveTest, IfElse) {
    EXPECT_EQ("2e", strip(pp("{#if false}1{#else}2{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#else}2{#endif}e")));
    EXPECT_TRUE(empty());
}

// ---- #elif ----

TEST_F(CtrlDirectiveTest, Elif1) {
    EXPECT_EQ("3e", strip(pp("{#if false}1{#elif false}2{#else}3{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if false}1{#elif true}2{#else}3{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#elif false}2{#else}3{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if true}1{#elif true}2{#else}3{#endif}e")));
    EXPECT_EQ("3e", strip(pp("{#if false}1{#elif a}2{#else}3{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#define a true}{#if false}1{#elif a}2{#else}3{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, ElifEmptyError) {
    EXPECT_THROW(pp("{#if false}1{#elif}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#if false}1{#elif }2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    
    EXPECT_THROW(pp("{#define a}{#if false}1{#elif a}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, Elif2) {
    EXPECT_EQ("4e", strip(pp("{#if 0}1{#elif 0}2{#elif 0}3{#else}4{#endif}e")));
    EXPECT_EQ("3e", strip(pp("{#if 0}1{#elif 0}2{#elif 1}3{#else}4{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#if 0}1{#elif 1}2{#elif 0}3{#else}4{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#if 1}1{#elif 0}2{#elif 0}3{#else}4{#endif}e")));
    EXPECT_TRUE(empty());
}

// ---- #ifdef / #ifndef ----

TEST_F(CtrlDirectiveTest, Ifdef) {
    EXPECT_EQ("1e", strip(pp("{#define a}{#ifdef a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#ifdef x}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#ifdef A}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a false}{#ifdef a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a 0}{#ifdef a}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a}{#undef a}{#ifdef a}1{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, Ifndef) {
    EXPECT_EQ("e",  strip(pp("{#define a}{#ifndef a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#ifndef A}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#ifndef x}1{#endif}e")));
    EXPECT_EQ("e",  strip(pp("{#define a false}{#ifndef a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a}{#undef a}{#ifndef a}1{#endif}e")));

    EXPECT_TRUE(empty());
}

// ---- #if with macro replacement ----

TEST_F(CtrlDirectiveTest, IfWithReplace) {
    EXPECT_THROW(pp("{#define a}{#if a}1{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
    EXPECT_EQ("e",  strip(pp("{#define a false}{#if a}1{#endif}e")));
    EXPECT_EQ("1e", strip(pp("{#define a true}{#if a}1{#endif}e")));

    EXPECT_TRUE(empty());
}

// ---- #if expression operators (macro-based cases only; others in test_constfold.cpp) ----

TEST_F(CtrlDirectiveTest, ExprPos) {
    EXPECT_THROW(pp("{#define a}{#if +a}1{#else}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, ExprNeg) {
    EXPECT_THROW(pp("{#define a}{#if -a}1{#else}2{#endif}e"), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, code());
}

TEST_F(CtrlDirectiveTest, ExprNot) {
    EXPECT_EQ("1e", strip(pp("{#define a false}{#if not a}1{#else}2{#endif}e")));
    EXPECT_EQ("2e", strip(pp("{#define a true}{#if not a}1{#else}2{#endif}e")));
    EXPECT_TRUE(empty());
}

TEST_F(CtrlDirectiveTest, ExprExactErrorCases) {
    static const std::vector<std::pair<const char*, Issue::Code>> cases = {
        {"{#define a}{#if +a}1{#else}2{#endif}e",       Issue::Code::MISSING_EXPRESSION},
        {"{#define a}{#if -a}1{#else}2{#endif}e",       Issue::Code::MISSING_EXPRESSION},
        {"{#define a}{#if not a}1{#else}2{#endif}e",    Issue::Code::MISSING_EXPRESSION},
    };
    for (const auto& [i, c] : cases) {
        EXPECT_THROW(pp(i), Issue::Exception) << i;
        EXPECT_EQ(c, code()) << i;
    }
}
