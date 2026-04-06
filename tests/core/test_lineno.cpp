#include "lineno_test_helper.hpp"

// ---- Basic directive, defined operator, object macro, all-token-kinds: line number tracking ----
namespace {
void normalize_diags(std::vector<std::string>& diags) {
    for (auto& diag : diags) {
        while (!diag.empty() && diag.back() == ' ') {
            diag.pop_back();
        }
    }
}
} // namespace

TEST_F(LinenoTest, Directive) {
    const auto test_case = TestCase{
        "directive-with-plain-newline-equivalence",
        R"(
{#define F(x) $
c$
+$
x$
}
{#info}__LINE__;
F(a
+
b);
{#info}__LINE__)",
        R"(





7;
c+a + b

;
11)",
        {
            "<unknown location>:7.0: info: PP93: ''",
            "<unknown location>:11.0: info: PP93: ''",
        },
        true,
    };
    SCOPED_TRACE(test_case.name);
    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}

TEST_F(LinenoTest, DefinedOperator) {
    const auto test_case = TestCase{
        "defined-operator",
        "{#if $\n$\ndefined$\n$\n($\n$\na$\n$\n)$\n$\n}{#endif};{#info}__LINE__",
        "\n\n\n\n\n\n\n\n\n\n;11",
        {"<unknown location>:11.0: info: PP93: ''"},
        true,
    };
    SCOPED_TRACE(test_case.name);
    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}


TEST_F(LinenoTest, Omacro) {
    const auto test_case = TestCase{
        "object-macro",
        "{#define A $\n$\na$\n$\n+$\n$\nb$\n$\n};A;{#info}__LINE__",
        "\n\n\n\n\n\n\n\n;a+b;9",
        {"<unknown location>:9.0: info: PP93: ''"},
        true,
    };

    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}


TEST_F(LinenoTest, AllLineno) {
    const auto test_case = TestCase{
        "all-token-kinds",
        R"(
{#info}__LINE__;


{#info}__LINE__;
{#nop}
{#info}__LINE__;
(*{#nop}*)
{#info}__LINE__;
/*{#nop}*/
{#info}__LINE__;
//{#nop}
{#info}__LINE__;
{nop}
{#info}__LINE__;
(*{nop}*)
{#info}__LINE__;
/*{nop}*/
{#info}__LINE__;
//{nop}
{#info}__LINE__;
(*
 *
 *)
{#info}__LINE__;
(*!
 *
 *)
{#info}__LINE__;
/*
 *
 */
{#info}__LINE__;
/*!
 *
 */
{#info}__LINE__;
//
{#info}__LINE__;
//!
{#info}__LINE__;
{#nop $
$
}
{#info}__LINE__;
{nop $
$
}
{#info}__LINE__;
)",
        R"(
2;


5;

7;

9;

11;

13;
(*{nop}*)
15;
(*{nop}*)
17;
(*{nop}*)
19;
(*{nop}*)
21;
(*
 *
 *)
25;
(*!
 *
 *)
29;
/*
 *
 */
33;
/*!
 *
 */
37;
//
39;
//!
41;



45;
(*{nop }*)


49;
)",
        {
            "<unknown location>:2.0: info: PP93: ''",
            "<unknown location>:5.0: info: PP93: ''",
            "<unknown location>:7.0: info: PP93: ''",
            "<unknown location>:9.0: info: PP93: ''",
            "<unknown location>:11.0: info: PP93: ''",
            "<unknown location>:13.0: info: PP93: ''",
            "<unknown location>:15.0: info: PP93: ''",
            "<unknown location>:17.0: info: PP93: ''",
            "<unknown location>:19.0: info: PP93: ''",
            "<unknown location>:21.0: info: PP93: ''",
            "<unknown location>:25.0: info: PP93: ''",
            "<unknown location>:29.0: info: PP93: ''",
            "<unknown location>:33.0: info: PP93: ''",
            "<unknown location>:37.0: info: PP93: ''",
            "<unknown location>:39.0: info: PP93: ''",
            "<unknown location>:41.0: info: PP93: ''",
            "<unknown location>:45.0: info: PP93: ''",
            "<unknown location>:49.0: info: PP93: ''",
        },
        true,
    };

    const std::string output = pp(test_case.input);
    EXPECT_EQ(test_case.expected_output, output);
    auto actual_diags = messages();
    auto expected_diags = test_case.expected_diags;
    EXPECT_EQ(expected_diags, actual_diags);
    if (test_case.verify_plain_newline_equivalence) {
        const std::string output = pp(replace_dollar_newline(test_case.input));
        EXPECT_EQ(test_case.expected_output, output);
        auto actual_diags = messages();
        auto expected_diags = test_case.expected_diags;
        EXPECT_EQ(expected_diags, actual_diags);
    }
}

