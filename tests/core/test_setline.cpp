#include "test_helper.hpp"

class SetlineTest : public JieppTest {};

// ---- #line basic ----

TEST_F(SetlineTest, Regular) {
    // Same-line setline: pragma emitted, __LINE__ reflects new number
    EXPECT_EQ("(*{#:10}*)10;",   pp("{#line 10}__LINE__;"));
    EXPECT_EQ("(*{#:10}*)\n11;", pp("{#line 10}\n__LINE__;"));

    // //-comment containing setline is processed
    EXPECT_EQ("(*{#:20}*)\n21;", pp("// {#line 20}\n__LINE__;"));

    // Multiple setlines: line count advances correctly
    EXPECT_EQ("\n2;\n(*{#:10}*)\n11;\n(*{#:20}*)\n21;\n",
        pp("\n__LINE__;\n{#line 10}\n__LINE__;\n// {#line 20}\n__LINE__;\n"));
    EXPECT_TRUE(empty());

    // With filename: pragma includes filename, Error records show file:line
    EXPECT_EQ("(*{#:10 'a.iec'}*)10;",   pp("{#line 10 'a.iec'}__LINE__;"));
    EXPECT_EQ("(*{#:10 'a.iec'}*)\n11;", pp("{#line 10 'a.iec'}\n__LINE__;"));

    pp("{#line 10 'a.iec'}\n{#info}\n{#line 20 'b.iec'}\n{#info}");
    auto recs = messages();
    ASSERT_EQ(2u, recs.size());
    EXPECT_EQ("a.iec:11.0: info: PP93: ''", recs[0]);
    EXPECT_EQ("b.iec:21.0: info: PP93: ''", recs[1]);
}

// ---- #line syntax ----

TEST_F(SetlineTest, Syntax) {
    // Valid: with colon separator
    EXPECT_EQ("(*{#:10 'a.iec'}*)\n(*{st}*)11;(*{end}*)",
        pp("{#line  :  10  'a.iec'  }\n{st}__LINE__;{end}"));
    // Valid: without colon
    EXPECT_EQ("(*{#:10 'a.iec'}*)\n(*{st}*)11;(*{end}*)",
        pp("{#line  10  'a.iec'  }\n{st}__LINE__;{end}"));
    // Valid: line number only, with colon
    EXPECT_EQ("(*{#:10}*)\n(*{st}*)11;(*{end}*)",
        pp("{#line  :  10  }\n{st}__LINE__;{end}"));
    // Valid: line number only, without colon
    EXPECT_EQ("(*{#:10}*)\n(*{st}*)11;(*{end}*)",
        pp("{#line  10  }\n{st}__LINE__;{end}"));
    EXPECT_TRUE(empty());

    // Invalid: no line number
    EXPECT_THROW(pp("{#line:}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line:  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line:  \n  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line  \n  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    // Invalid: empty directive
    EXPECT_THROW(pp("{#:}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#:  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#:  \n  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#  \n  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());

    EXPECT_TRUE(empty());
}

// ---- #line on same line (oneline) ----

TEST_F(SetlineTest, Oneline) {
    // Multiple setlines on the same line
    EXPECT_EQ("(*{#:10}*)10;(*{#:20}*)20;",
        pp("{#line 10}{#info}__LINE__;{#line 20}{#info}__LINE__;"));
    const auto first_oneline = codes();
    ASSERT_EQ(2u, first_oneline.size());
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, first_oneline[0]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, first_oneline[1]);
}

TEST_F(SetlineTest, OnelineCComment) {
    // Via C-style comments (no space between /* and directive)
    EXPECT_EQ("(*{#:10}*)10;(*{#:20}*)20;",
        pp("/*{#line 10}*/{#info}__LINE__;/*{#line 20}*/{#info}__LINE__;"));
}

TEST_F(SetlineTest, OnelinePascalComment) {
    // Via Pascal-style comments (no space between (* and directive)
    EXPECT_EQ("(*{#:10}*)10;(*{#:20}*)20;",
        pp("(*{#line 10}*){#info}__LINE__;(*{#line 20}*){#info}__LINE__;"));
    const auto pascal_oneline = codes();
    ASSERT_EQ(2u, pascal_oneline.size());
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, pascal_oneline[0]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, pascal_oneline[1]);
}

// ---- #line complex ----

TEST_F(SetlineTest, Complex) {
    // Mix of setlines with comments and line advances
    // Note: Pascal/C block comments with leading space hide directives inside
    EXPECT_EQ(
        "\n2;\n(*{#:100}*)100;\n(*\n\n*) (*{#:200}*)200;\n(*{#:300}*) //\n301;\n",
        pp(
            "\n"
            "{#info}__LINE__;\n"
            "{#line 100}{#info}__LINE__;\n"
            "(*\n"
            "\n"
            "*) {#line 200}{#info}__LINE__;\n"
            "{#line 300} //\n"
            "{#info}__LINE__;\n"
        )
    );
    const auto complex_codes = codes();
    ASSERT_EQ(4u, complex_codes.size());
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[0]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[1]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[2]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[3]);
}

// ---- #line filepath ----

TEST_F(SetlineTest, Filepath) {
    // Plain filename (no quotes) → error
    EXPECT_THROW(pp("{#line 2 a.iec}__FILE__;"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    // Trailing spaces trimmed but still unquoted → error
    EXPECT_THROW(pp("{#line 2   a.iec  }__FILE__;"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    // Single-quoted filename
    EXPECT_EQ("(*{#:2 'a.iec'}*)'a.iec';", pp("{#line 2 'a.iec'}__FILE__;"));
    // Double-quoted filename
    EXPECT_EQ("(*{#:2 'a.iec'}*)'a.iec';", pp("{#line 2 \"a.iec\"}__FILE__;"));

    // $$ in unquoted filename → error
    EXPECT_THROW(pp("{#line 2 a$$.iec}__FILE__;"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    // $$ in single-quoted filename
    EXPECT_EQ("(*{#:2 'a$$.iec'}*)'a$$.iec';", pp("{#line 2 'a$$.iec'}__FILE__;"));
    // $$ in double-quoted filename
    EXPECT_EQ("(*{#:2 'a$$.iec'}*)'a$$.iec';", pp("{#line 2 \"a$$.iec\"}__FILE__;"));
    EXPECT_TRUE(empty()) << "unexpected errors";

    // Error records reflect the setline filepath
    pp("{#line 2 'a.iec'}{#info}");
    EXPECT_EQ("a.iec:2.0: info: PP93: ''", message());

    pp("{#line 2 'a$$.iec'}{#info}");
    EXPECT_EQ("a$.iec:2.0: info: PP93: ''", message());
}

