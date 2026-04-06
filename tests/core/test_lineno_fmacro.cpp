#include "lineno_test_helper.hpp"

// ---- Function macro: line number tracking ----
namespace {
void normalize_diags(std::vector<std::string>& diags) {
    for (auto& diag : diags) {
        while (!diag.empty() && diag.back() == ' ') {
            diag.pop_back();
        }
    }
}
} // namespace

TEST_F(LinenoTest, Fmacro) {
    const std::vector<TestCase> cases = {
        {
            "same-line-call",
            "{#define F(x, y) x * y};F(a + b, c + d);{#info}__LINE__",
            ";a + b * c + d;1",
            {"<unknown location>:1.0: info: PP93: ''"},
        },
        {
            "newline-before-info",
            "{#define F(x, y) x * y};F(a + b, c + d);\n{#info}__LINE__",
            ";a + b * c + d;\n2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-before-info",
            "{#define F(x, y) x * y};F(a + b, c + d);\n\n{#info}__LINE__",
            ";a + b * c + d;\n\n3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-before-semicolon",
            "{#define F(x, y) x * y};F(a + b, c + d)\n;{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-before-semicolon",
            "{#define F(x, y) x * y};F(a + b, c + d)\n\n;{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-before-closing-paren",
            "{#define F(x, y) x * y};F(a + b, c + d\n);{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-before-closing-paren",
            "{#define F(x, y) x * y};F(a + b, c + d\n\n);{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-inside-second-argument-expression",
            "{#define F(x, y) x * y};F(a + b, c + \nd);{#info}__LINE__",
            ";a + b * c +  d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-inside-second-argument-expression",
            "{#define F(x, y) x * y};F(a + b, c + \n\nd);{#info}__LINE__",
            ";a + b * c +   d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-after-comma",
            "{#define F(x, y) x * y};F(a + b,\n c + d);{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-after-comma",
            "{#define F(x, y) x * y};F(a + b,\n\n c + d);{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-before-comma",
            "{#define F(x, y) x * y};F(a + b\n, c + d);{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-before-comma",
            "{#define F(x, y) x * y};F(a + b\n\n, c + d);{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-before-comma-duplicate",
            "{#define F(x, y) x * y};F(a + b\n, c + d);{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-before-comma-duplicate",
            "{#define F(x, y) x * y};F(a + b\n\n, c + d);{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-after-opening-paren",
            "{#define F(x, y) x * y};F(\na + b, c + d);{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-after-opening-paren",
            "{#define F(x, y) x * y};F(\n\na + b, c + d);{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-between-name-and-lparen",
            "{#define F(x, y) x * y};F\n(a + b, c + d);{#info}__LINE__",
            ";a + b * c + d\n;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-between-name-and-lparen",
            "{#define F(x, y) x * y};F\n\n(a + b, c + d);{#info}__LINE__",
            ";a + b * c + d\n\n;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "newline-before-invocation",
            "{#define F(x, y) x * y};\nF(a + b, c + d);{#info}__LINE__",
            ";\na + b * c + d;2",
            {"<unknown location>:2.0: info: PP93: ''"},
        },
        {
            "double-newline-before-invocation",
            "{#define F(x, y) x * y};\n\nF(a + b, c + d);{#info}__LINE__",
            ";\n\na + b * c + d;3",
            {"<unknown location>:3.0: info: PP93: ''"},
        },
        {
            "dollar-newline-at-body-start",
            "{#define F(x, y) $\nx * y};F(a + b, c + d);{#info}__LINE__",
            "\n;a + b * c + d;2",
            {"<unknown location>:2.0: info: PP93: ''"},
            true,
        },
        {
            "double-dollar-newline-at-body-start",
            "{#define F(x, y) $\n$\nx * y};F(a + b, c + d);{#info}__LINE__",
            "\n\n;a + b * c + d;3",
            {"<unknown location>:3.0: info: PP93: ''"},
            true,
        },
        {
            "dollar-newline-at-body-end",
            "{#define F(x, y) x * y$\n};F(a + b, c + d);{#info}__LINE__",
            "\n;a + b * c + d;2",
            {"<unknown location>:2.0: info: PP93: ''"},
            true,
        },
        {
            "double-dollar-newline-at-body-end",
            "{#define F(x, y) x * y$\n$\n};F(a + b, c + d);{#info}__LINE__",
            "\n\n;a + b * c + d;3",
            {"<unknown location>:3.0: info: PP93: ''"},
            true,
        },
        {
            "dollar-newline-in-body-middle",
            "{#define F(x, y) x *$\ny};F(a + b, c + d);{#info}__LINE__",
            "\n;a + b *c + d;2",
            {"<unknown location>:2.0: info: PP93: ''"},
            true,
        },
        {
            "double-dollar-newline-in-body-middle",
            "{#define F(x, y) x *$\n$\ny};F(a + b, c + d);{#info}__LINE__",
            "\n\n;a + b *c + d;3",
            {"<unknown location>:3.0: info: PP93: ''"},
            true,
        },
        {
            "dollar-newline-at-body-start-duplicate",
            "{#define F(x, y) $\nx * y};F(a + b, c + d);{#info}__LINE__",
            "\n;a + b * c + d;2",
            {"<unknown location>:2.0: info: PP93: ''"},
            true,
        },
        {
            "double-dollar-newline-at-body-start-duplicate",
            "{#define F(x, y) $\n$\nx * y};F(a + b, c + d);{#info}__LINE__",
            "\n\n;a + b * c + d;3",
            {"<unknown location>:3.0: info: PP93: ''"},
            true,
        },
        {
            "mixed-dollars-and-real-newlines",
            R"({#define F(x, y) $
$
x$
*$
y$
};
F
(
a
+
b
,
c
+
d
)
;{#info}__LINE__)",
            "\n\n\n\n\n;\na + b*c + d\n\n\n\n\n\n\n\n\n\n;17",
            {"<unknown location>:17.0: info: PP93: ''"},
            true,
        },
        {
            "mixed-doubles-dollars-and-real-newlines",
            R"({#define F(x, y) $
$
x$
$
*$
$
y$
$
};

F

(

a

+

b

,

c

+

d

)

;{#info}__LINE__)",
            "\n\n\n\n\n\n\n\n;\n\na  +  b*c  +  d\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n;31",
            {"<unknown location>:31.0: info: PP93: ''"},
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

TEST_F(LinenoTest, FmacroInvalidPragma) {
    const auto test_case = TestCase{
        "pragma-stops-at-inner-st",
        R"(
{#define F() $
	function f: int $
	{st} $
	f := 2; $
	{end} $
	end $
}
)",
        "",
        {"<unknown location>:4.0: error: PP22: Invalid syntax for pragma; '{'"},
        true,
    };
    SCOPED_TRACE(test_case.name);
    EXPECT_THROW(pp(test_case.input), Issue::Exception);
    auto actual = messages();
    auto expected = test_case.expected_diags;
    EXPECT_EQ(expected, actual);
    if (test_case.verify_plain_newline_equivalence) {
        EXPECT_THROW(pp(replace_dollar_newline(test_case.input)), Issue::Exception);
        auto actual = messages();
        auto expected = test_case.expected_diags;
        EXPECT_EQ(expected, actual);
    }
}
