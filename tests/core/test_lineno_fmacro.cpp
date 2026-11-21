#include "lineno_test_helper.hpp"

// ---- Function macro: line number tracking ----

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
            // U4: a raw newline is now whitespace, so replacing '$\n' with
            // a plain newline here would produce "* c" instead of the
            // no-space "*c" that '$' still produces; plain-newline
            // equivalence no longer holds.
            "dollar-newline-in-body-middle",
            "{#define F(x, y) x *$\ny};F(a + b, c + d);{#info}__LINE__",
            "\n;a + b *c + d;2",
            {"<unknown location>:2.0: info: PP93: ''"},
            false,
        },
        {
            "double-dollar-newline-in-body-middle",
            "{#define F(x, y) x *$\n$\ny};F(a + b, c + d);{#info}__LINE__",
            "\n\n;a + b *c + d;3",
            {"<unknown location>:3.0: info: PP93: ''"},
            false,
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
            // trailing run: 10 blank lines exceed the default 7-line
            // compaction threshold; N = cur_before_run(7) + nl(10) - 1 = 16.
            // U4: the embedded '*$\ny$' segment above would gain a space
            // ("* c") under plain-newline substitution; equivalence no
            // longer holds (see dollar-newline-in-body-middle above).
            "\n\n\n\n\n;\na + b*c + d\n(*{#:16}*)\n;17",
            {"<unknown location>:17.0: info: PP93: ''"},
            false,
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
            // leading run: 8 blank lines -> N = 1 + 8 - 1 = 8.
            // trailing run: 20 blank lines -> N = cur_before_run(11) + nl(20) - 1 = 30.
            // U4: same '*$\n$\ny$$' middle segment as above; equivalence no
            // longer holds.
            "(*{#:8}*)\n;\n\na  +  b*c  +  d\n(*{#:30}*)\n;31",
            {"<unknown location>:31.0: info: PP93: ''"},
            false,
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

TEST_F(LinenoTest, FmacroLineInBody) {
    // (a) __LINE__ textually inside a function-macro's own replacement list:
    // for a multi-line invocation it must report the line of the closing ')',
    // matching clang (gcc reports the line the macro name appeared on).
    EXPECT_EQ(";1 3\n\n;\n4;",
              pp("{#define F(x) x __LINE__};F(\n  1\n);\n__LINE__;"));
    EXPECT_TRUE(empty());

    // (b) __LINE__ passed as an argument: the line it is written on, not the
    // closing ')' line (clang and gcc, measured).
    EXPECT_EQ(";2\n\n;\n4;",
              pp("{#define F(x) x};F(\n__LINE__\n);\n__LINE__;"));
    EXPECT_TRUE(empty());

    // (c) __LINE__ after the call is unaffected (already covered by
    // LinenoTest.Fmacro, spot-checked again here alongside (a)/(b)).
    EXPECT_EQ(";a\n\n;3;",
              pp("{#define F(x) x};F(\n  a\n);__LINE__;"));
    EXPECT_TRUE(empty());
}

TEST_F(LinenoTest, FmacroInvalidPragma) {
    const auto test_case = TestCase{
        "pragma-stops-at-inner-pragma",
        R"(
{#define F() $
	function f: int $
	{my_pragma} $
	f := 2; $
	{my_pragma} $
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

// The non-blank lines of `s` without spaces, joined by '|': the line
// numbers below are what is checked, not the blank lines kept around them.
static std::string nonblank_lines(const std::string& s) {
    std::string r, line;
    std::istringstream in(strip(s));
    while (std::getline(in, line))
        if (!line.empty())
            r += (r.empty() ? "" : "|") + line;
    return r;
}

// SPEC 12.1: a __LINE__ written in an argument of a call spanning lines is
// the line it is written on, also through an object macro, a stringizing
// macro or a nested call (whose body __LINE__ is its own ')' line). Inputs
// measured with clang and gcc (the numbers are clang's; gcc agrees except
// for the body of a nested call, where it takes the call's first line).
TEST_F(LinenoTest, FmacroLineInArgumentIsItsOwnLine) {
    // #define F(x) x / F( / __LINE__ / ) / F(__LINE__ / ) / __LINE__: 3 5 7
    EXPECT_EQ("3|5|7",
              nonblank_lines(pp("{#define F(x) x}\nF(\n__LINE__\n)\nF(__LINE__\n)\n__LINE__\n")));
    // Stringized after the expansion: "4".
    EXPECT_EQ("'4'",
              nonblank_lines(pp("{#define S(x) @x}\n{#define XS(x) S(x)}\nXS(\n__LINE__\n)\n")));
    // An object macro in the arguments: the line of its name.
    EXPECT_EQ("34",
              nonblank_lines(pp("{#define O __LINE__}\n{#define F(a,b) a b}\nF(O,\nO\n)\n")));
    // F(__LINE__, / __LINE__): 2 3.
    EXPECT_EQ("23|4",
              nonblank_lines(pp("{#define F(a,b) a b}\nF(__LINE__,\n__LINE__)\n__LINE__\n")));
    // A nested call: its body's __LINE__ is its own ')' line, 5 and 9.
    EXPECT_EQ("5|9",
              nonblank_lines(pp("{#define L() __LINE__}\n{#define F(x) x}\nF(\nL(\n)\n)\nF(L\n(\n)\n)\n")));
    // Variadic and nested: 1,4,5 6.
    EXPECT_EQ("1,4,56",
              nonblank_lines(pp("{#define F(...) __VA_ARGS__}\n{#define G(x,...) x __VA_ARGS__}\n"
                       "F(1,\n__LINE__,\nG(__LINE__,\n__LINE__)\n)\n")));
    // The body's own __LINE__ stays the ')' line: 3 4.
    EXPECT_EQ("34|5",
              nonblank_lines(pp("{#define F(x) x __LINE__}\nF(\n__LINE__\n)\n__LINE__\n")));
    // A call made by a macro: the tokens it brought are at its line, 3 4.
    EXPECT_EQ("34",
              nonblank_lines(pp("{#define F(a,b) a b}\n{#define H F(__LINE__,}\nH\n__LINE__)\n")));
    // A call that a name from an argument forms with the rest of the
    // replacement is in the outer call's replacement: the outer ')' line, 6
    // (clang; gcc 3).
    EXPECT_EQ("6|7",
              nonblank_lines(pp("{#define F(x,y) x y}\n{#define N() __LINE__}\nF(N\n,\n()\n)\n"
                                "__LINE__\n")));
    // A pasted __LINE__ is made in the body: the ')' line, 3 and 6.
    EXPECT_EQ("3|6",
              nonblank_lines(pp("{#define C(a,b) a@@b}\nC(__LI,\nNE__)\nC(\n__LI,NE__\n)\n")));
    EXPECT_TRUE(empty());
}

// SPEC 12.1: a __LINE__ on a later line of a multi-line directive operand is
// the line it is written on (clang and gcc: `#if 1 && \` / ` __LINE__ == 2`
// is true), also when a macro on that line expands to __LINE__.
TEST_F(LinenoTest, LineInMultiLineDirectiveOperand) {
    EXPECT_EQ("T", nonblank_lines(pp("{#if 1 &&$\n __LINE__ = 2}T{#else}F{#endif}\n")));
    EXPECT_EQ("T", nonblank_lines(pp("{#if 1 &&\n __LINE__ = 2}T{#else}F{#endif}\n")));
    EXPECT_EQ("T", nonblank_lines(pp("{#if 0 ||$\n$\n __LINE__ = 3}T{#else}F{#endif}\n")));
    EXPECT_EQ("T", nonblank_lines(pp("{#if 0}{#elif 1 &&$\n __LINE__ = 2}T{#endif}\n")));
    EXPECT_EQ("T",
              nonblank_lines(pp("{#define X __LINE__}\n{#if 1 &&$\n X = 3}T{#endif}\n")));
    // The first line is the directive's own line.
    EXPECT_EQ("T", nonblank_lines(pp("\n{#if __LINE__ = 2 &&$\n 1}T{#endif}\n")));
    // A function macro on a later line: its body's __LINE__ is the line of
    // its ')' (clang and gcc: `#define B() __LINE__` / `#if 1 && \` /
    // ` B() == 3` is true), also when the ')' is on a line of its own.
    EXPECT_EQ("T", nonblank_lines(pp("{#define B() __LINE__}\n{#if 1 &&$\n B() = 3}T{#endif}\n")));
    EXPECT_EQ("T", nonblank_lines(pp("{#define B() __LINE__}\n{#if 1 &&\n B(\n ) = 4}T{#endif}\n")));
    // In a message: B() on line 6 is 6, then B() and L on line 7 are 7.
    {
        Issue::ContinueMode guard({});
        pp("{#define B() __LINE__}\n{#define L __LINE__}\n\n\n\n{#info B()\n B() L}\n");
        EXPECT_EQ(std::vector<std::string>({"<unknown location>:6.0: info: PP93: '6  7 7'"}),
                  messages());
    }
    // A pragma body: the line it is written on.
    EXPECT_EQ("(*{attra2b}*)|4", nonblank_lines(pp("{attr a\n __LINE__$\n b}\n__LINE__\n")));
    EXPECT_EQ("(*{attr33}*)",
              nonblank_lines(pp("{#define B() __LINE__}\n{attr B(\n) __LINE__}\n")));
    EXPECT_TRUE(empty());
}
