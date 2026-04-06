#include "lineno_test_helper.hpp"

// ---- Conditional directives (#if/#else/#endif): line number tracking ----
namespace {
void normalize_diags(std::vector<std::string>& diags) {
    for (auto& diag : diags) {
        while (!diag.empty() && diag.back() == ' ') {
            diag.pop_back();
        }
    }
}
} // namespace

TEST_F(LinenoTest, IfDirective) {
    const std::vector<TestCase> cases = {
        {
            "if-true-branch",
            "{#if $\n$\n0$\n$\n+$\n$\n1$\n$\n}\n\n{#info}__LINE__\n\n+\n\n{#info}__LINE__;{#endif}",
            "\n\n\n\n\n\n\n\n\n\n11\n\n+\n\n15;",
            {
                "<unknown location>:11.0: info: PP93: ''",
                "<unknown location>:15.0: info: PP93: ''",
            },
            true,
        },
        {
            "if-else-branch",
            "{#if $\n$\n0$\n$\n+$\n$\n0$\n$\n}\n\n{#else}\n\n{#info}__LINE__\n\n+\n\n{#info}__LINE__;{#endif}",
            "\n\n\n\n\n\n\n\n\n\n\n\n13\n\n+\n\n17;",
            {
                "<unknown location>:13.0: info: PP93: ''",
                "<unknown location>:17.0: info: PP93: ''",
            },
            true,
        },
        {
            "if-define-true",
            "{#if 1}{#define N $\n1$\n};{#else}{#define N $\n0$\n};{#endif};N;{#info}__LINE__",
            "\n\n;\n\n;1;5",
            {"<unknown location>:5.0: info: PP93: ''"},
            true,
        },
        {
            "if-define-false",
            "{#if 0}{#define N $\n1$\n};{#else}{#define N $\n0$\n};{#endif};N;{#info}__LINE__",
            "\n\n\n\n;;0;5",
            {"<unknown location>:5.0: info: PP93: ''"},
            true,
        },
    };

    for (const auto& test_case : cases) {
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
}
