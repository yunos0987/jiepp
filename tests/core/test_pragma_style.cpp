#include "test_helper.hpp"

class PragmaStyleTest : public JieppTest {};

// Tests for {#pp-output-pragma-style} directive.
// annotated (default): pragmas → (*{...}*), line directives → (*{#:N}*)
// standard:            pragmas → {...},     line directives → {#:N}

TEST_F(PragmaStyleTest, Annotated) {
    // {#pp-output-pragma-style annotated} switches to annotated output
    const std::string input =
        "\n"
        "{#pp-output-pragma-style annotated}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "(*{#:1}*)\n"
        "program (*{id Main}*) end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaStyleTest, Standard) {
    // {#pp-output-pragma-style standard} switches to standard output
    const std::string input =
        "\n"
        "{#pp-output-pragma-style standard}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "{#:1}\n"
        "program {id Main} end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaStyleTest, UnknownValue) {
    // Unknown STYLE operand: non-fatal WARNING (PP48), pragma ignored,
    // effective style left unchanged (stays at the default: annotated).
    const std::string input =
        "\n"
        "{#pp-output-pragma-style bogus}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "(*{#:1}*)\n"
        "program (*{id Main}*) end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND, code());
}

TEST_F(PragmaStyleTest, UnknownValueUnderscoreAlias) {
    // Underscore-spelled directive alias validates the same way.
    const std::string input =
        "\n"
        "{#pp_output_pragma_style bogus}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "(*{#:1}*)\n"
        "program (*{id Main}*) end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND, code());
}

TEST_F(PragmaStyleTest, TrailingCommentAccepted) {
    // F7: the operand is tokenized (mirroring the {#max_*} limit
    // directives), so a trailing comment is stripped rather than making the
    // whole operand look like garbage.
    const std::string input =
        "\n"
        "{#pp-output-pragma-style standard (* trailing comment *)}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "{#:1}\n"
        "program {id Main} end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaStyleTest, TrailingGarbageRejected) {
    // F7: trailing non-comment garbage after the recognised value is still
    // rejected (only whitespace/comments are stripped, not arbitrary
    // tokens) -- non-fatal WARNING, style left unchanged.
    const std::string input =
        "\n"
        "{#pp-output-pragma-style standard extra}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "(*{#:1}*)\n"
        "program (*{id Main}*) end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND, code());
}

TEST_F(PragmaStyleTest, EmptyOperandRejected) {
    // F7: an empty (whitespace/comment-only) operand is rejected the same
    // way as any other unrecognised value.
    const std::string input =
        "\n"
        "{#pp-output-pragma-style (* just a comment *)}\n"
        "{#line 2}\n"
        "program {id Main} end\n";
    const std::string expected =
        "\n"
        "\n"
        "(*{#:1}*)\n"
        "program (*{id Main}*) end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND, code());
}

TEST_F(PragmaStyleTest, Switch) {
    // mid-stream switches: annotated → standard → annotated
    const std::string input =
        "\n"
        "{#pp-output-pragma-style annotated}\n"
        "{#line 2}\n"
        "program {id Main1} end\n"
        "{#pp-output-pragma-style standard}\n"
        "{#line 3}\n"
        "program {id Main2} end\n"
        "{#pp-output-pragma-style annotated}\n"
        "{#line 5}\n"
        "program {id Main3} end\n";
    const std::string expected =
        "\n"
        "\n"
        "(*{#:1}*)\n"
        "program (*{id Main1}*) end\n"
        "\n"
        "{#:2}\n"
        "program {id Main2} end\n"
        "\n"
        "(*{#:4}*)\n"
        "program (*{id Main3}*) end\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_TRUE(empty());
}

