#include "test_helper.hpp"
#include <algorithm>
#include <vector>

static std::string strip(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    return s;
}

class CtrlSyntaxTest : public JieppTest {};

// Fact: if/elif/else/endif selects the first true branch
TEST_F(CtrlSyntaxTest, Simple) {
    // first branch taken
    EXPECT_EQ("1;e;", strip(pp("{#if true}1;{#endif}e;")));
    EXPECT_EQ("1;e;", strip(pp("{#if true}1;{#else}2;{#endif}e;")));
    EXPECT_EQ("1;e;", strip(pp("{#if true}1;{#elif false}2;{#endif}e;")));
    EXPECT_EQ("1;e;", strip(pp("{#if true}1;{#elif false}2;{#else}3;{#endif}e;")));
    EXPECT_EQ("1;e;", strip(pp("{#if true}1;{#elif false}2;{#elif false}3;{#else}4;{#endif}e;")));
    // second branch taken
    EXPECT_EQ("e;",   strip(pp("{#if false}1;{#endif}e;")));
    EXPECT_EQ("2;e;", strip(pp("{#if false}1;{#else}2;{#endif}e;")));
    EXPECT_EQ("2;e;", strip(pp("{#if false}1;{#elif true}2;{#endif}e;")));
    EXPECT_EQ("2;e;", strip(pp("{#if false}1;{#elif true}2;{#else}3;{#endif}e;")));
    EXPECT_EQ("2;e;", strip(pp("{#if false}1;{#elif true}2;{#elif false}3;{#else}4;{#endif}e;")));
    // no branch taken
    EXPECT_EQ("e;",   strip(pp("{#if false}1;{#endif}e;")));
    EXPECT_EQ("2;e;", strip(pp("{#if false}1;{#else}2;{#endif}e;")));
    EXPECT_EQ("e;",   strip(pp("{#if false}1;{#elif false}2;{#endif}e;")));
    EXPECT_EQ("3;e;", strip(pp("{#if false}1;{#elif false}2;{#else}3;{#endif}e;")));
    EXPECT_EQ("3;e;", strip(pp("{#if false}1;{#elif false}2;{#elif true}3;{#else}4;{#endif}e;")));
    EXPECT_TRUE(empty());
}

// Fact: consecutive independent if blocks each evaluate their own condition
TEST_F(CtrlSyntaxTest, Sequence) {
    // first block false, second block true
    EXPECT_EQ("2;3;e;",   strip(pp("{#if false}1;{#endif}2;{#if true}3;{#endif}e;")));
    EXPECT_EQ("2;3;4;e;", strip(pp("{#if false}1;{#else}2;{#endif}3;{#if true}4;{#endif}e;")));
    EXPECT_EQ("3;4;e;",   strip(pp("{#if false}1;{#elif false}2;{#endif}3;{#if true}4;{#endif}e;")));
    EXPECT_EQ("3;4;5;e;", strip(pp("{#if false}1;{#elif false}2;{#else}3;{#endif}4;{#if true}5;{#endif}e;")));
    EXPECT_EQ("4;5;6;e;", strip(pp("{#if false}1;{#elif false}2;{#elif false}3;{#else}4;{#endif}5;{#if true}6;{#endif}e;")));
    // first block true, second block true
    EXPECT_EQ("2;e;",   strip(pp("{#if false}1;{#endif}2;{#if false}3;{#endif}e;")));
    EXPECT_EQ("2;3;e;", strip(pp("{#if false}1;{#else}2;{#endif}3;{#if false}4;{#endif}e;")));
    EXPECT_EQ("3;e;",   strip(pp("{#if false}1;{#elif false}2;{#endif}3;{#if false}4;{#endif}e;")));
    EXPECT_EQ("3;4;e;", strip(pp("{#if false}1;{#elif false}2;{#else}3;{#endif}4;{#if false}5;{#endif}e;")));
    EXPECT_EQ("4;5;e;", strip(pp("{#if false}1;{#elif false}2;{#elif false}3;{#else}4;{#endif}5;{#if false}6;{#endif}e;")));
    // first block true, second block false
    EXPECT_EQ("1;2;e;", strip(pp("{#if true}1;{#endif}2;{#if false}3;{#endif}e;")));
    EXPECT_EQ("1;3;e;", strip(pp("{#if true}1;{#else}2;{#endif}3;{#if false}4;{#endif}e;")));
    EXPECT_EQ("1;3;e;", strip(pp("{#if true}1;{#elif true}2;{#endif}3;{#if false}4;{#endif}e;")));
    EXPECT_EQ("1;4;e;", strip(pp("{#if true}1;{#elif true}2;{#else}3;{#endif}4;{#if false}5;{#endif}e;")));
    EXPECT_EQ("1;5;e;", strip(pp("{#if true}1;{#elif true}2;{#elif true}3;{#else}4;{#endif}5;{#if false}6;{#endif}e;")));
    EXPECT_TRUE(empty());
}

// Fact: an unclosed {#directive block raises INVALID_PP_SYNTAX
TEST_F(CtrlSyntaxTest, Error) {
    static const char* cases[] = {
        "{#if  true  };",
        "{#elif true };",
        "{#else      };",
        "{#endif     };",
        "{#if  true  };{#if  true  };",
        "{#elif true };{#if  true  };",
        "{#else      };{#if  true  };",
        "{#endif     };{#if  true  };",
        "{#if  true  };{#elif true };",
        "{#elif true };{#elif true };",
        "{#else      };{#elif true };",
        "{#endif     };{#elif true };",
        "{#if  true  };{#else      };",
        "{#elif true };{#else      };",
        "{#else      };{#else      };",
        "{#endif     };{#else      };",
        "{#elif true };{#endif     };",
        "{#else      };{#endif     };",
        "{#endif     };{#endif     };",
        "{#if  true  };{#if  true  };{#if  true  };",
        "{#elif true };{#if  true  };{#if  true  };",
        "{#else      };{#if  true  };{#if  true  };",
        "{#endif     };{#if  true  };{#if  true  };",
        "{#if  true  };{#elif true };{#if  true  };",
        "{#elif true };{#elif true };{#if  true  };",
        "{#else      };{#elif true };{#if  true  };",
        "{#endif     };{#elif true };{#if  true  };",
        "{#if  true  };{#else      };{#if  true  };",
        "{#elif true };{#else      };{#if  true  };",
        "{#else      };{#else      };{#if  true  };",
        "{#endif     };{#else      };{#if  true  };",
        "{#if  true  };{#endif     };{#if  true  };",
        "{#elif true };{#endif     };{#if  true  };",
        "{#else      };{#endif     };{#if  true  };",
        "{#endif     };{#endif     };{#if  true  };",
        "{#if  true  };{#if  true  };{#elif true };",
        "{#elif true };{#if  true  };{#elif true };",
        "{#else      };{#if  true  };{#elif true };",
        "{#endif     };{#if  true  };{#elif true };",
        "{#if  true  };{#elif true };{#elif true };",
        "{#elif true };{#elif true };{#elif true };",
        "{#else      };{#elif true };{#elif true };",
        "{#endif     };{#elif true };{#elif true };",
        "{#if  true  };{#else      };{#elif true };",
        "{#elif true };{#else      };{#elif true };",
        "{#else      };{#else      };{#elif true };",
        "{#endif     };{#else      };{#elif true };",
        "{#if  true  };{#endif     };{#elif true };",
        "{#elif true };{#endif     };{#elif true };",
        "{#else      };{#endif     };{#elif true };",
        "{#endif     };{#endif     };{#elif true };",
        "{#if  true  };{#if  true  };{#else      };",
        "{#elif true };{#if  true  };{#else      };",
        "{#else      };{#if  true  };{#else      };",
        "{#endif     };{#if  true  };{#else      };",
        "{#if  true  };{#elif true };{#else      };",
        "{#elif true };{#elif true };{#else      };",
        "{#else      };{#elif true };{#else      };",
        "{#endif     };{#elif true };{#else      };",
        "{#if  true  };{#else      };{#else      };",
        "{#elif true };{#else      };{#else      };",
        "{#else      };{#else      };{#else      };",
        "{#endif     };{#else      };{#else      };",
        "{#if  true  };{#endif     };{#else      };",
        "{#elif true };{#endif     };{#else      };",
        "{#else      };{#endif     };{#else      };",
        "{#endif     };{#endif     };{#else      };",
        "{#if  true  };{#if  true  };{#endif     };",
        "{#elif true };{#if  true  };{#endif     };",
        "{#else      };{#if  true  };{#endif     };",
        "{#endif     };{#if  true  };{#endif     };",
        "{#elif true };{#elif true };{#endif     };",
        "{#else      };{#elif true };{#endif     };",
        "{#endif     };{#elif true };{#endif     };",
        "{#elif true };{#else      };{#endif     };",
        "{#else      };{#else      };{#endif     };",
        "{#endif     };{#else      };{#endif     };",
        "{#if  true  };{#endif     };{#endif     };",
        "{#elif true };{#endif     };{#endif     };",
        "{#else      };{#endif     };{#endif     };",
        "{#endif     };{#endif     };{#endif     };",
    };

    for (const char* input : cases) {
        EXPECT_THROW({ pp(input); }, Issue::Exception);
        auto c = code();
        EXPECT_TRUE(
            c == Issue::Code::INVALID_PP_SYNTAX ||
            c == Issue::Code::UNTERMINATED_CONDITIONAL ||
            c == Issue::Code::ELIF_ERROR ||
            c == Issue::Code::ELSE_ERROR ||
            c == Issue::Code::ENDIF_ERROR
        ) << "input: " << input;
    }
    EXPECT_TRUE(empty());
}

TEST_F(CtrlSyntaxTest, ExactCases) {
    static const std::vector<std::pair<const char*, const char*>> cases = {
        {"{#if false}1;{#if true }2;{#endif}3;{#endif}e;", "e;"},
        {"{#if true }1;{#if false}2;{#endif}3;{#endif}e;", "1;3;e;"},
        {"{#if true }1;{#if true }2;{#endif}3;{#endif}e;", "1;2;3;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#endif}4;{#endif}e;", "2;3;4;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#endif}4;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#endif}4;{#endif}e;", "1;e;"},
        {"{#if false}1;{#elif false}2;{#if true }3;{#endif}4;{#endif}e;", "e;"},
        {"{#if false}1;{#elif true }2;{#if false}3;{#endif}4;{#endif}e;", "2;4;e;"},
        {"{#if false}1;{#elif true }2;{#if true }3;{#endif}4;{#endif}e;", "2;3;4;e;"},
        {"{#if true }1;{#elif false}2;{#if false}3;{#endif}4;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#if true }3;{#endif}4;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#if false}3;{#endif}4;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#if true }3;{#endif}4;{#endif}e;", "1;e;"},
        {"{#if false}1;{#elif false}2;{#elif false}3;{#if false}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if false}1;{#elif false}2;{#elif false}3;{#if true }4;{#endif}5;{#endif}e;", "e;"},
        {"{#if false}1;{#elif false}2;{#elif true }3;{#if false}4;{#endif}5;{#endif}e;", "3;5;e;"},
        {"{#if false}1;{#elif false}2;{#elif true }3;{#if true }4;{#endif}5;{#endif}e;", "3;4;5;e;"},
        {"{#if false}1;{#elif true }2;{#elif false}3;{#if false}4;{#endif}5;{#endif}e;", "2;e;"},
        {"{#if false}1;{#elif true }2;{#elif false}3;{#if true }4;{#endif}5;{#endif}e;", "2;e;"},
        {"{#if false}1;{#elif true }2;{#elif true }3;{#if false}4;{#endif}5;{#endif}e;", "2;e;"},
        {"{#if false}1;{#elif true }2;{#elif true }3;{#if true }4;{#endif}5;{#endif}e;", "2;e;"},
        {"{#if true }1;{#elif false}2;{#elif false}3;{#if false}4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#elif false}3;{#if true }4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#elif true }3;{#if false}4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#elif true }3;{#if true }4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#elif false}3;{#if false}4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#elif false}3;{#if true }4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#elif true }3;{#if false}4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#elif true }3;{#if true }4;{#endif}5;{#endif}e;", "1;e;"},
        {"{#if false}1;{#if false}2;{#elif true }3;{#else}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if false}1;{#if true }2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if false}1;{#if true }2;{#elif true }3;{#else}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if true }1;{#if false}2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;", "1;4;5;e;"},
        {"{#if true }1;{#if false}2;{#elif true }3;{#else}4;{#endif}5;{#endif}e;", "1;3;5;e;"},
        {"{#if true }1;{#if true }2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;", "1;2;5;e;"},
        {"{#if true }1;{#if true }2;{#elif true }3;{#else}4;{#endif}5;{#endif}e;", "1;2;5;e;"},
        {"{#if false}1;{#else}2;{#if false}3;{#elif true }4;{#else}5;{#endif}6;{#endif}e;", "2;4;6;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;", "2;3;6;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#elif true }4;{#else}5;{#endif}6;{#endif}e;", "2;3;6;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#elif true }4;{#else}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#elif true }4;{#else}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if false}1;{#if false}2;{#if true }3;{#endif}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if false}1;{#if true }2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if false}1;{#if true }2;{#if true }3;{#endif}4;{#endif}5;{#endif}e;", "e;"},
        {"{#if true }1;{#if false}2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;", "1;5;e;"},
        {"{#if true }1;{#if false}2;{#if true }3;{#endif}4;{#endif}5;{#endif}e;", "1;5;e;"},
        {"{#if true }1;{#if true }2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;", "1;2;4;5;e;"},
        {"{#if true }1;{#if true }2;{#if true }3;{#endif}4;{#endif}5;{#endif}e;", "1;2;3;4;5;e;"},
        {"{#if false}1;{#else}2;{#if false}3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "2;6;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "2;3;5;6;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "2;3;4;5;6;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if false}1;{#else}2;{#if false}3;{#else}4;{#if true }5;{#endif}6;{#endif}7;{#endif}e;", "2;4;5;6;7;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;", "2;3;7;e;"},
        {"{#if false}1;{#else}2;{#if true }3;{#else}4;{#if true }5;{#endif}6;{#endif}7;{#endif}e;", "2;3;7;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if false}3;{#else}4;{#if true }5;{#endif}6;{#endif}7;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;", "1;e;"},
        {"{#if true }1;{#else}2;{#if true }3;{#else}4;{#if true }5;{#endif}6;{#endif}7;{#endif}e;", "1;e;"},
        {"{#if false}1;{#elif false}2;{#if false}3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "e;"},
        {"{#if false}1;{#elif false}2;{#if true }3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "e;"},
        {"{#if false}1;{#elif false}2;{#if true }3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "e;"},
        {"{#if false}1;{#elif true }2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "2;6;e;"},
        {"{#if false}1;{#elif true }2;{#if false}3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "2;6;e;"},
        {"{#if false}1;{#elif true }2;{#if true }3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "2;3;5;6;e;"},
        {"{#if false}1;{#elif true }2;{#if true }3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "2;3;4;5;6;e;"},
        {"{#if true }1;{#elif false}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#if false}3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#if true }3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif false}2;{#if true }3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#if false}3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#if true }3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
        {"{#if true }1;{#elif true }2;{#if true }3;{#if true }4;{#endif}5;{#endif}6;{#endif}e;", "1;e;"},
    };

    for (const auto& [input, expected] : cases) {
        EXPECT_EQ(expected, strip(pp(input))) << "input: " << input;
    }
    EXPECT_TRUE(empty());
}

// ---- Nested if/elif/else depth 1 and 2: control flow tests ----

// Fact: inner if nested inside depth-1 if/else evaluates correctly
TEST_F(CtrlSyntaxTest, Depth1IfElse) {
    // inner if is in the 'if' branch
    EXPECT_EQ("e;",       pp("{#if false}1;{#if false}2;{#endif}3;{#endif}e;"));
    EXPECT_EQ("e;",       pp("{#if false}1;{#if true}2;{#endif}3;{#endif}e;"));
    EXPECT_EQ("1;3;e;",   pp("{#if true}1;{#if false}2;{#endif}3;{#endif}e;"));
    EXPECT_EQ("1;2;3;e;", pp("{#if true}1;{#if true}2;{#endif}3;{#endif}e;"));
    // inner if is in the 'else' branch
    EXPECT_EQ("2;4;e;",   pp("{#if false}1;{#else}2;{#if false}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("2;3;4;e;", pp("{#if false}1;{#else}2;{#if true}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("1;e;",     pp("{#if true}1;{#else}2;{#if false}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("1;e;",     pp("{#if true}1;{#else}2;{#if true}3;{#endif}4;{#endif}e;"));
    // inner if is in the first 'elif' branch
    EXPECT_EQ("e;",     pp("{#if false}1;{#elif false}2;{#if false}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("e;",     pp("{#if false}1;{#elif false}2;{#if true}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("2;4;e;", pp("{#if false}1;{#elif true}2;{#if false}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("2;3;4;e;", pp("{#if false}1;{#elif true}2;{#if true}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("1;e;",   pp("{#if true}1;{#elif false}2;{#if false}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("1;e;",   pp("{#if true}1;{#elif false}2;{#if true}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("1;e;",   pp("{#if true}1;{#elif true}2;{#if false}3;{#endif}4;{#endif}e;"));
    EXPECT_EQ("1;e;",   pp("{#if true}1;{#elif true}2;{#if true}3;{#endif}4;{#endif}e;"));
    EXPECT_TRUE(empty());
}

// Fact: nested if/elif/else inside depth-1 block evaluates only when the outer branch is taken
TEST_F(CtrlSyntaxTest, Depth1IfElifElse) {
    // inner if/elif/else is in the 'if' branch
    EXPECT_EQ("e;",       pp("{#if false}1;{#if false}2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("e;",       pp("{#if false}1;{#if false}2;{#elif true}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("e;",       pp("{#if false}1;{#if true}2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("e;",       pp("{#if false}1;{#if true}2;{#elif true}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;4;5;e;", pp("{#if true}1;{#if false}2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;3;5;e;", pp("{#if true}1;{#if false}2;{#elif true}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;2;5;e;", pp("{#if true}1;{#if true}2;{#elif false}3;{#else}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;2;5;e;", pp("{#if true}1;{#if true}2;{#elif true}3;{#else}4;{#endif}5;{#endif}e;"));
    // inner if/elif/else is in the 'else' branch
    EXPECT_EQ("2;5;6;e;", pp("{#if false}1;{#else}2;{#if false}3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;4;6;e;", pp("{#if false}1;{#else}2;{#if false}3;{#elif true}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;3;6;e;", pp("{#if false}1;{#else}2;{#if true}3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;3;6;e;", pp("{#if false}1;{#else}2;{#if true}3;{#elif true}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if false}3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if false}3;{#elif true}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if true}3;{#elif false}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if true}3;{#elif true}4;{#else}5;{#endif}6;{#endif}e;"));
    EXPECT_TRUE(empty());
}

// Fact: doubly-nested if/else evaluates correctly; inactive outer blocks skip inner blocks entirely
TEST_F(CtrlSyntaxTest, Depth2IfElse) {
    // inner-most in 'if'
    EXPECT_EQ("e;",         pp("{#if false}1;{#if false}2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("e;",         pp("{#if false}1;{#if false}2;{#if true}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("e;",         pp("{#if false}1;{#if true}2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("e;",         pp("{#if false}1;{#if true}2;{#if true}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;5;e;",     pp("{#if true}1;{#if false}2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;5;e;",     pp("{#if true}1;{#if false}2;{#if true}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;2;4;5;e;", pp("{#if true}1;{#if true}2;{#if false}3;{#endif}4;{#endif}5;{#endif}e;"));
    EXPECT_EQ("1;2;3;4;5;e;", pp("{#if true}1;{#if true}2;{#if true}3;{#endif}4;{#endif}5;{#endif}e;"));
    // inner-most in nested else-1
    EXPECT_EQ("2;6;e;",       pp("{#if false}1;{#else}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;6;e;",       pp("{#if false}1;{#else}2;{#if false}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;3;5;6;e;",   pp("{#if false}1;{#else}2;{#if true}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;3;4;5;6;e;", pp("{#if false}1;{#else}2;{#if true}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if false}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if true}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if true}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    // inner-most in nested else-2
    EXPECT_EQ("2;4;6;7;e;",   pp("{#if false}1;{#else}2;{#if false}3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("2;4;5;6;7;e;", pp("{#if false}1;{#else}2;{#if false}3;{#else}4;{#if true}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("2;3;7;e;",     pp("{#if false}1;{#else}2;{#if true}3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("2;3;7;e;",     pp("{#if false}1;{#else}2;{#if true}3;{#else}4;{#if true}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if false}3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if false}3;{#else}4;{#if true}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if true}3;{#else}4;{#if false}5;{#endif}6;{#endif}7;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#else}2;{#if true}3;{#else}4;{#if true}5;{#endif}6;{#endif}7;{#endif}e;"));
    // inner-most in 1st elif branch
    EXPECT_EQ("e;",         pp("{#if false}1;{#elif false}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("e;",         pp("{#if false}1;{#elif false}2;{#if true}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;6;e;",     pp("{#if false}1;{#elif true}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;3;5;6;e;", pp("{#if false}1;{#elif true}2;{#if true}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("2;3;4;5;6;e;", pp("{#if false}1;{#elif true}2;{#if true}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#elif false}2;{#if false}3;{#if false}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_EQ("1;e;", pp("{#if true}1;{#elif true}2;{#if true}3;{#if true}4;{#endif}5;{#endif}6;{#endif}e;"));
    EXPECT_TRUE(empty());
}
