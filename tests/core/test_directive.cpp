#include "test_helper.hpp"

class DirectiveTest : public JieppTest {};

TEST_F(DirectiveTest, Basic) {
    EXPECT_EQ("", pp("{#define: N 3}"));
    EXPECT_EQ("", pp("{#define N 3}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NoColon) {
    EXPECT_EQ(";3", pp("{#define N 3};N"));
    EXPECT_EQ(";", pp("{#define N};N"));
    EXPECT_EQ(";xyzw", pp("{#define F(a,b) a@@b};F(xy,zw)"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineComment) {
    EXPECT_EQ("\n3", pp("//{#define: N 3}\nN"));
    EXPECT_EQ("\n3", pp("//{#define N 3}\nN"));
    EXPECT_EQ("\n3", pp("// {#\tdefine:\tN\t3}\nN"));
    EXPECT_EQ("\n3", pp("// {#\tdefine\tN\t3}\nN"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, Unknown) {
    // PP45 (UNKNOWN_DIRECTIVE) is ERROR: each throws in library mode.
    // "{# /**/}" and friends are no longer here: a comment before the name
    // is skipped like whitespace now (see CommentOnlyDirectiveIsNull), so
    // only text that is not a comment at all -- an unclosed "*/"/"/*" --
    // still reaches UNKNOWN_DIRECTIVE via this path.
    EXPECT_THROW(pp("{# */};"), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, code());
    EXPECT_THROW(pp("{# /*};"), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, code());
}

TEST_F(DirectiveTest, CommentOnlyDirectiveIsNull) {
    // A directive whose name-position text is only whitespace and comments
    // is the empty (null) directive, like gcc/clang's "# /**/" and "# //".
    EXPECT_EQ(";", pp("{# /**/};"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";", pp("{# (**)};"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";", pp("{# //};"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";", pp("{#(* c *)};"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, CommentAroundDirectiveName) {
    // A comment between "{#" and the name, or right after the name, is
    // whitespace, like gcc/clang's "#/*c*/define".
    EXPECT_EQ(";1", pp("{#(*c*)define X 1};X"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";1", pp("{#/*c*/define X 1};X"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";1", pp("{# (* c *) define X 1};X"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";1", pp("{#define(*c*)X 1};X"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";1", pp("{#define/*c*/X 1};X"));
    EXPECT_TRUE(empty());

    // A comment before "if"/"else"/"endif" is likewise skipped.
    EXPECT_EQ("y", pp("{#(*c*)if 1}y{#(*c*)else}n{#(*c*)endif}"));
    EXPECT_TRUE(empty());

    // Nesting is still counted correctly when the inner {#if} is commented.
    EXPECT_EQ("y", pp("{#if 0}{#(*c*)if 1}{#endif}x{#endif}y"));
    EXPECT_TRUE(empty());

    // A comment before ":" behaves the same as without one.
    EXPECT_EQ(pp("{#:100}\n__LINE__"), pp("{#(*c*):100}\n__LINE__"));
    EXPECT_TRUE(empty());

    // A document comment is a token, not skipped: the name stays "(*! d *)".
    EXPECT_THROW(pp("{#(*! d *)define X 1}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, code());
}

TEST_F(DirectiveTest, CommentsInIfExpression) {
    // Comments in a {#if}/{#elif} expression are whitespace, like in C.
    EXPECT_EQ("y", pp("{#if 1 // c}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", pp("{#if 1 (* c *)}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", pp("{#if 1 /* c */}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", pp("{#if (* c *) 0 (* d *) + 1}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("n", pp("{#if 0 (* c *)}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", pp("{#if 0}n{#elif 1 // c}y{#endif}"));
    EXPECT_TRUE(empty());
    // A comment in a macro body used in the expression is whitespace too
    // (macro bodies keep comments unless -nC).
    EXPECT_EQ("y", pp("{#define A 1 (* c *) + 1}{#if A = 2}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", pp("{#define F(x) x (* c *) + 1}{#if F(1) = 2}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("y", pp("{#if 1 (* c *) \\and\\ (* d *) 1}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
    // A document comment is a token, not whitespace: a syntax error.
    EXPECT_THROW(pp("{#if 1 (*! d *)}y{#endif}"), Issue::Exception);
}

TEST_F(DirectiveTest, InvalidDirectiveName) {
    // Identifier-like key with non-identifier chars → ERROR (not WARNING)
    // parse_directive keeps +, (, ) in the key since they're not separator chars
    EXPECT_THROW(pp("{#endif.}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, code());
    EXPECT_THROW(pp("{#if+}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, code());
}

// ---- B2: PP45/PP46 are reported at dispatch time (correct line number,
// suppressed inside an inactive {#if 0} block) ----

TEST_F(DirectiveTest, UnknownDirectiveReportsCorrectLineNumber) {
    // {#foo} is on physical line 3 (two blank lines precede it). PP45 is
    // ERROR, so library mode throws.
    EXPECT_THROW(pp(";\n;\n{#foo}"), Issue::Exception);
    auto actual_diags = messages();
    const std::vector<std::string> expected_diags = {
        "<unknown location>:3.0: error: PP45: Unknown directive; 'foo'",
    };
    EXPECT_EQ(expected_diags, actual_diags);
}

TEST_F(DirectiveTest, InvalidDirectiveNameReportsCorrectLineNumber) {
    // {#foo.bar} is on physical line 3.
    EXPECT_THROW(pp(";\n;\n{#foo.bar}"), Issue::Exception);
    auto actual_diags = messages();
    const std::vector<std::string> expected_diags = {
        "<unknown location>:3.0: error: PP46: Invalid directive name; 'foo.bar'",
    };
    EXPECT_EQ(expected_diags, actual_diags);
}

TEST_F(DirectiveTest, UnknownDirectiveInsideInactiveIfNotReported) {
    // gcc does not diagnose an unrecognized directive-shaped body inside a
    // block skipped by {#if 0}; PP45/PP46 must be suppressed there too, now
    // that they fire at dispatch time (which sees the ctrl-stack state)
    // instead of at lex time (which did not).
    EXPECT_EQ("", pp("{#if 0}{#foo}{#foo.bar}{#endif}"));
    EXPECT_TRUE(empty());
}

// ---- B3: a directive-shaped token cached across a repeated #include of the
// same file must still be diagnosed once per inclusion, not once overall ----

TEST_F(DirectiveTest, UnknownDirectiveReportedOncePerInclusion) {
    fs::current_path(jiepp_root_dir());
    static const fs::path dir = "tests/core/test_include";
    // PP45 is ERROR now: use ContinueMode so both occurrences are counted
    // instead of the first one throwing.
    Issue::ContinueMode guard({});
    EXPECT_NO_THROW(pp_file(dir / "unknown_directive_double.iec"));
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, cs[0]);
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, cs[1]);
}

TEST_F(DirectiveTest, SpecialInDirective) {
    EXPECT_EQ(";'0';'1'", pp("{#define L ${## __COUNTER__$}};L;L"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, Ignore) {
    EXPECT_THROW(pp("{#line x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_NO_THROW(pp("{#ignore PP41}{#line x}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, IgnorePP45SuppressesUnknownDirective) {
    // is_ignored(code) && !is_severe(code) still applies now that PP45 is
    // ERROR instead of WARNING: {#ignore PP45} suppresses it completely.
    EXPECT_NO_THROW(pp("{#ignore PP45}{#foo}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, IgnoreMultipleCodes) {
    // Each {#ignore} suppresses a different code independently
    EXPECT_THROW(pp("{#line x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    EXPECT_NO_THROW(pp("{#ignore PP41}{#line x}"));
    EXPECT_TRUE(empty());
    
    // Verify a second ignore also works
    EXPECT_NO_THROW(pp("{#ignore PP42}{#ignore abc}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, IgnoreInvalidOperand) {
    EXPECT_THROW(pp("{#ignore 41}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore foo}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PPxx}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PP}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
}

TEST_F(DirectiveTest, IgnoreOperandStrictFormat) {
    // Malformed operands are rejected with PP42, even though they used to be
    // accepted silently (or partially) by the old std::stoul-based parser.
    EXPECT_THROW(pp("{#ignore PP-1}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PP41x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PP999}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    EXPECT_THROW(pp("{#ignore PP4}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    // Only the first code is registered by the old parser; now the whole
    // directive is rejected instead.
    EXPECT_THROW(pp("{#ignore PP28 PP41}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());
    // Lowercase "pp" is not accepted (codes are case-sensitive "PP" + 2 digits).
    EXPECT_THROW(pp("{#ignore pp41}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, code());

    // Surrounding whitespace and a trailing comment are still fine, and the
    // code is actually registered as ignored.
    EXPECT_NO_THROW(pp("{#ignore  PP41  }{#line x}"));
    EXPECT_TRUE(empty());
    EXPECT_NO_THROW(pp("{#ignore PP41 (* why *)}{#line x}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, IgnoreRetiredCodeAccepted) {
    // PP03 is a well-formed but retired/unassigned code (U3); {#ignore} still
    // accepts it silently since only the "PP" + 2-digit format is validated.
    EXPECT_EQ("x", pp("{#ignore PP03}x"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, OperationNotAllowedDefine) {
    EXPECT_THROW(pp("{#define defined}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

TEST_F(DirectiveTest, OperationNotAllowedDefineWithBody) {
    // Not just the bare {#define defined} form: any redefinition of the
    // "defined" operator (object-like with a body, or function-like) must be
    // rejected at the define site, not silently accepted (which would corrupt
    // every later {#if defined(...)} in the file).
    EXPECT_THROW(pp("{#define defined 1}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
    EXPECT_THROW(pp("{#define defined(x) x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
    // A later {#if defined(...)} must still work using the real operator.
    EXPECT_EQ(";yes;", pp("{#define FOO 1};{#if defined(FOO)}yes;{#endif}"));
}

TEST_F(DirectiveTest, OperationNotAllowedUndef) {
    EXPECT_THROW(pp("{#undef defined}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

TEST_F(DirectiveTest, DirectiveInsideMacroArgumentExecutedOnce) {
    // A directive embedded in a macro-call argument must execute exactly once,
    // during argument collection, not once per occurrence of the parameter in
    // the macro body.
    EXPECT_EQ(";[Y] [Y]\n\n",
              pp("{#define F(x) [x] [x]};F(\n{#warning side-effect}\nY)"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, cs[0]);
}

TEST_F(DirectiveTest, ControlDirectiveInsideMacroArgumentRejected) {
    // Control directives ({#if}/{#elif}/{#else}/{#endif}/{#ifdef}/{#ifndef})
    // found while collecting a macro call's argument list cannot be executed
    // safely (they would run once per parameter occurrence, or not at all if
    // the parameter is never referenced), so they are rejected diagnostically
    // instead of silently misbehaving.
    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#if TRUE}\nY\n{#endif}\n)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

TEST_F(DirectiveTest, OutputDirectiveInsideMacroArgumentRejected) {
    // F3: a directive whose handler pushes token(s) directly to the output
    // stream (its output would be emitted before the enclosing macro call's
    // own expansion) is rejected -- not silently misordered -- when found
    // while collecting a macro call's argument list.
    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#include 'nope.iec'}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());

    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#sinclude 'nope.iec'}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());

    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#line 100}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());

    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#string Y}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());

    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#wstring Y}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());

    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#token Y}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());

    EXPECT_THROW(pp("{#define F(x) [x]};F(\n{#syspath 'lib'}\nY)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::OPERATION_NOT_ALLOWED, code());
}

TEST_F(DirectiveTest, StateDirectiveInsideMacroArgumentStillAllowed) {
    // F3: state-only directives (their handler takes no `ots` parameter, so
    // they are structurally incapable of emitting output) remain permitted
    // inside a macro-call argument list.
    EXPECT_EQ(";[Y]\n\n", pp("{#define F(x) [x]};F(\n{#warning noted}\nY)"));
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, code());

    EXPECT_EQ(";[Y]\n\n", pp("{#define G(x) [x]};G(\n{#pragma once}\nY)"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, UnknownDirectiveInsideMacroArgumentSingleDiagnostic) {
    // F3 design-review guard: an unrecognised directive name (dkind == -1)
    // must not be misclassified as an "output directive" -- in two's-
    // complement, -1 has every bit set, so -1 & MASK_OUTPUT is nonzero --
    // and must not receive a second, wrong OPERATION_NOT_ALLOWED diagnostic
    // on top of the single UNKNOWN_DIRECTIVE error that dispatch_directive()
    // (expand.cpp) raises for the same malformed text (B1: this used to be
    // raised at lex time by DirectiveToken::ready(), which no longer does).
    // PP45 is ERROR now: use ContinueMode so processing runs to completion
    // instead of throwing, so the "exactly one diagnostic" guard can still
    // be checked against the full output.
    Issue::ContinueMode guard({});
    EXPECT_EQ(";[1]\n\n", pp("{#define F(x) [x]};F(\n{#bogus}\n1)"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, cs[0]);
}

TEST_F(DirectiveTest, UndefOfEnclosingMacroInsideItsOwnArgumentList) {
    // F14: {#undef F} embedded in F's own call argument list used to
    // Symtab::undef() the Macro out from under the FunctionMacro* already
    // captured for this call (use-after-free: garbage argument-count
    // numbers, MSVC debug-heap fill pattern 0xDDDDDDDD / -572662307). The
    // retirement list keeps the superseded Macro alive for the Symtab's
    // lifetime, so this call still substitutes with the *old* definition,
    // and F is genuinely undefined afterwards.
    EXPECT_EQ(";[1]\n\n;F", pp("{#define F(x) [x]};F(\n{#undef F}\n1);F"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, RedefineOfEnclosingMacroInsideItsOwnArgumentList) {
    // F14: {#define F(x) <x>} embedded in F's own call argument list used to
    // destroy the old Macro (Symtab::define()'s replace-in-place branch) out
    // from under the live FunctionMacro* for this call. The current call
    // must still use the *old* definition; the next call uses the new one.
    EXPECT_EQ(";[1]\n\n;<2>",
              pp("{#define F(x) [x]};F(\n{#define F(x) <x>}\n1);F(2)"));
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, code());
}

TEST_F(DirectiveTest, MaxBlankLinesDirectiveInvalidOperand) {
    // F6: parse failure -> INVALID_LIMIT_OPERAND; a negative (but
    // parseable) value -> INVALID_PARAMETER_VALUE (0 is valid: it disables
    // compaction, see BlankLinesTest.MaxBlankLinesDirectiveZeroDisables).
    EXPECT_THROW(pp("{#max_blank_lines abc}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, code());
    EXPECT_THROW(pp("{#max_blank_lines -1}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PARAMETER_VALUE, code());
}

TEST_F(DirectiveTest, LimitDirectiveOperand) {
    // Comments around the {#max_*} operand are whitespace (SPEC §11).
    {
        Env env = setup();
        pp("{#max_blank_lines (* c *) 2 // d}", env);
        EXPECT_EQ(2, env.get_max_blank_lines());
        EXPECT_TRUE(empty());
    }
    // Trailing garbage after the integer is now rejected instead of being
    // silently ignored by std::stoi().
    EXPECT_THROW(pp("{#max_blank_lines 2x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, code());
    EXPECT_THROW(pp("{#max_blank_lines 2 3}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, code());
    EXPECT_THROW(pp("{#max_blank_lines 2.9}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, code());
    // A document comment is a token, not whitespace: still garbage.
    EXPECT_THROW(pp("{#max_blank_lines 2 (*! d *)}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, code());

    // Unchanged behavior.
    EXPECT_THROW(pp("{#max_blank_lines abc}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, code());
    EXPECT_THROW(pp("{#max_blank_lines -1}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PARAMETER_VALUE, code());
}

TEST_F(DirectiveTest, Undef) {
    // Define, undef, redefine
    EXPECT_EQ(";2;;N;;3", pp("{#define N 2};N;{#undef N};N;{#define N 3};N"));
    // Undef of undefined macro is a no-op
    EXPECT_EQ(";N;;2;;N", pp("{#undef N};N;{#define N 2};N;{#undef N};N"));
    // Python parity: trailing spaces remain part of the operand, so this undef is a no-op
    EXPECT_EQ(";2;;N", pp("{#define N 2};N;{#undef   N   };N"));
    EXPECT_TRUE(empty());
}

// ---- U4: a raw newline inside a directive body is whitespace ----
//
// Previously a raw newline was appended as a space only inside an
// *ordinary* pragma; inside a directive it was silently dropped, so e.g.
// "{#define X a\nb}" concatenated into "ab" instead of joining with a
// space. It now follows the same rule as an ordinary pragma: append one
// space unless the body already ends in whitespace. A '$' immediately
// before the newline still joins with nothing (unaffected, see
// DollarNewlineStillJoins below).

TEST_F(DirectiveTest, NewlineInDirectiveActsAsWhitespace) {
    // LF, CRLF and CR all count as one whitespace-producing newline. Each
    // leading "\n" is the swallowed directive-internal newline echoed as a
    // blank line before the directive's own output (same mechanism as the
    // pre-existing '$'-continuation tests in ObjectMacroTest.Simple).
    EXPECT_EQ("\n[a b]", pp("{#define X a\nb}[X]"));
    EXPECT_EQ("\n[a b]", pp("{#define X a\r\nb}[X]"));
    EXPECT_EQ("\n[a b]", pp("{#define X a\rb}[X]"));
    // A newline right after the macro name still separates name from body.
    EXPECT_EQ("\n[1]", pp("{#define X\n1}[X]"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NewlineAfterWhitespaceAddsNoExtraSpace) {
    // A newline immediately following existing whitespace contributes no
    // space of its own; the two spaces in the result both come from the
    // literal spaces already present around the newline.
    EXPECT_EQ("\n[a  b]", pp("{#define X a \n b}[X]"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, DollarNewlineStillJoins) {
    // '$' immediately before a newline still joins with nothing (like a C
    // backslash-newline continuation); U4 only changes the *raw* newline
    // case, so this is unaffected.
    EXPECT_EQ("\n[ab]", pp("{#define X a$\nb}[X]"));
    EXPECT_EQ("\n[ab]", pp("{#define X a$\r\nb}[X]"));
    EXPECT_EQ("\n[ab]", pp("{#define X a$\rb}[X]"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NewlineInFunctionMacroParams) {
    // A raw newline may split a function-like macro's parameter list.
    EXPECT_EQ("\n[1+2]", pp("{#define F(a,\nb) a+b}[F(1,2)]"));
    // A newline between the macro name and its '(' turns the '(' into
    // ordinary replacement text, so F becomes an OBJECT macro (C parity).
    EXPECT_EQ("\n[(a) a](a)", pp("{#define F\n(a) a}[F](a)"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NewlineInIfExpression) {
    EXPECT_EQ("\ny", pp("{#if true and\ntrue}y{#endif}"));
    EXPECT_EQ("\ny", pp("{#if true\nand true}y{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NewlineInStringAndMessageDirectives) {
    // {#string} emits its own content before the swallowed newline is
    // echoed, so the "\n" trails here instead of leading.
    EXPECT_EQ("'a b'\n", pp("{#string a\nb}"));
    EXPECT_TRUE(empty());

    // A raw newline typed directly in the directive is folded to one space
    // by the lexer, like in any multi-line directive: one message, with a
    // real space where the newline was.
    EXPECT_EQ("\n", pp("{#warning a\nb}"));
    {
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::WARNING_MESSAGE, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("'a b'")) << msgs[0];
    }

    // $n decodes to a real line break and is printed as one: the message
    // itself now spans two physical lines, so messages() (which splits on
    // std::getline) sees two entries.
    pp("{#warning a$nb}");
    {
        auto msgs = messages();
        ASSERT_EQ(2u, msgs.size());
        EXPECT_EQ(Issue::Code::WARNING_MESSAGE, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("'a")) << msgs[0];
        EXPECT_NE(std::string::npos, msgs[1].find("b'")) << msgs[1];
    }

    EXPECT_THROW(pp("{#error a\nb}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

TEST_F(DirectiveTest, NewlineSplitsDirectiveName) {
    // A newline breaks up what would otherwise be one directive-name
    // token, so '#def\nine' is looked up (and rejected) as unknown
    // directive 'def', just like the plain unknown-directive case above.
    // PP45 is ERROR, so library mode throws.
    EXPECT_THROW(pp("{#def\nine X 1}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, code());
}

// Several other diagnostics embed a decoded directive operand (raw_arg, or
// text derived from it downstream, such as a {#if}/{#elif} operand reaching
// constfold's expression evaluator) the same way {#define} does
// (FuncMacroTest.DefineDiagnosticStaysOnOneLine) -- verify each re-escapes a
// $n escape (via Util::escape_line_breaks(), src/util/text.hpp) so the
// diagnostic stays on one physical line instead of splitting messages() into
// two entries. Message directives ({#error}/{#warning}/...) are deliberately
// excluded: they print the user's own message verbatim (see
// NewlineInStringAndMessageDirectives).
TEST_F(DirectiveTest, OperandDiagnosticsStayOnOneLine) {
    Issue::ContinueMode guard({});

    {
        SCOPED_TRACE("{#line abc$n}");
        pp("{#line abc$n}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#syspath abc$n}");
        pp("{#syspath abc$n}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_PATH, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        // handle_include() shares the same INVALID_PATH ISSUE() site as
        // handle_syspath() above (strip_path() failure), but is a distinct
        // handler -- verify it separately.
        SCOPED_TRACE("{#include abc$n}");
        pp("{#include abc$n}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_PATH, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#ignore XYZ$n}");
        pp("{#ignore XYZ$n}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#max_include_depth x$n}");
        pp("{#max_include_depth x$n}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_LIMIT_OPERAND, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        SCOPED_TRACE("{#pp_output_pragma_style bogus$n}");
        pp("{#pp_output_pragma_style bogus$n}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        // The unknown-directive dispatch site (expand.cpp) has the same
        // defect for its decoded `key`.
        SCOPED_TRACE("{#foo$nbar}");
        pp("{#foo$nbar}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        // eval_const_expr()'s all-whitespace path (constfold.cpp): {#if}'s
        // operand decodes to a lone real newline, which is entirely
        // whitespace.
        SCOPED_TRACE("{#if $n}");
        pp("{#if $n}{#endif}");
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_NE(std::string::npos, msgs[0].find("$n")) << msgs[0];
    }
    {
        // eval_const_expr()'s incomplete-parse path (constfold.cpp), reached
        // via {#elif} instead of {#if}: the operand decodes to "1+" followed
        // by a real newline, which is not all whitespace but still fails to
        // parse as a complete expression. This legitimately reports two
        // diagnostics -- the bison grammar's own "syntax error" (msg has no
        // embedded operand text) via cf::CfParser::error(), then
        // eval_const_expr()'s own MISSING_EXPRESSION carrying the operand
        // text -- so unlike the other sub-cases above, a raw-newline defect
        // here would silently grow msgs.size() further (one extra entry per
        // embedded newline) rather than just splitting a single entry.
        SCOPED_TRACE("{#if 0}{#elif 1+$n}{#endif}");
        pp("{#if 0}{#elif 1+$n}{#endif}");
        auto msgs = messages();
        ASSERT_EQ(2u, msgs.size());
        EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, PlainTextMessage::parse_code(msgs[1]));
        EXPECT_NE(std::string::npos, msgs[1].find("$n")) << msgs[1];
    }
}

// A {#if} expression that fails to parse as a complete expression (as
// opposed to being empty/all-whitespace) is treated as false, like gcc/clang
// do for a malformed #if -- not as whatever partial value the constant-
// folding parser had accumulated before it gave up.
TEST_F(DirectiveTest, SyntaxErrorExpressionIsFalse) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("ok;", pp("{#if 1 x}yes;{#endif}ok;"));
}

TEST_F(DirectiveTest, NewlineInDirectiveLineCountUnchanged) {
    // The newline consumed while folding it to whitespace is still counted
    // for line numbering, and is echoed as a blank line in the output --
    // the same extra_nl mechanism already exercised by the pre-existing
    // '$'-continuation tests in ObjectMacroTest.Simple.
    EXPECT_EQ("\n2", pp("{#define X a\nb}__LINE__"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NewlineInDirectiveDdModeReemitsSpace) {
    // -dD re-emits the {#define} line using the normalized body (the raw
    // newline already folded to a single space), not the original text.
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output = pp("{#define X a\nb}", env);
    EXPECT_NE(std::string::npos, output.find("{#define X a b}")) << "output:\n" << output;
}

// D2/R2: -dD must not echo a {#define} that was rejected (defined nothing)
// -- only defines that actually happened.
TEST_F(DirectiveTest, DdModeDoesNotEchoRejectedDefine) {
    Issue::ContinueMode guard({});
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output =
        pp("{#define F(x,x) [x]}{#define V(..., a) a}{#define OK 1}", env);
    EXPECT_EQ(std::string::npos, output.find("{#define F(")) << "output:\n" << output;
    EXPECT_EQ(std::string::npos, output.find("{#define V(")) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("{#define OK 1}")) << "output:\n" << output;
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::DUPLICATE_MACRO_PARAMETER, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_VARIADIC_PLACEMENT, cs[1]);
}

// R3: -dD must not echo a {#define} whose parameter list was rejected as
// malformed either -- same rule as DdModeDoesNotEchoRejectedDefine, but for
// the new parameter-list-syntax checks instead of duplicate-parameter /
// variadic-placement ones.
TEST_F(DirectiveTest, DdModeDoesNotEchoMalformedParamList) {
    Issue::ContinueMode guard({});
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output =
        pp("{#define F(a b) x}{#define G(a,) x}{#define OK(a) a}", env);
    EXPECT_EQ(std::string::npos, output.find("{#define F(")) << "output:\n" << output;
    EXPECT_EQ(std::string::npos, output.find("{#define G(")) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("{#define OK(a) a}")) << "output:\n" << output;
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_DEFINE_SYNTAX, cs[1]);
}

TEST_F(DirectiveTest, RemoveCommentsInMacroBodies) {
    // U5: -nC (remove_comments) removes comments from macro bodies too,
    // like gcc/clang without -C (the default, -CC, keeps them).
    {
        Env env = setup();
        env.set_remove_comments(true);
        EXPECT_EQ("\n[1   +2]", pp("{#define X 1 (* c *) +2}\n[X]", env));
        EXPECT_TRUE(empty());
    }
    {
        Env env = setup();
        env.set_remove_comments(true);
        EXPECT_EQ("[1   +1]", pp("{#define F(a) a (* c *) +a}[F(1)]", env));
        EXPECT_TRUE(empty());
    }
    // A document comment is a token, not a comment: it survives -nC.
    {
        Env env = setup();
        env.set_remove_comments(true);
        EXPECT_EQ("[1 (*! d *) +2]", pp("{#define X 1 (*! d *) +2}[X]", env));
        EXPECT_TRUE(empty());
    }
    // Default (-CC-like): the comment survives in the body, unchanged.
    EXPECT_EQ("[1 (* c *) +2]", pp("{#define X 1 (* c *) +2}[X]"));
    EXPECT_TRUE(empty());

    // Watchpoint: an identical redefinition (same normalized body) still
    // raises no MACRO_REDEFINED, in both modes.
    {
        Env env = setup();
        env.set_remove_comments(true);
        pp("{#define X 1 (* c *) +2}{#define X 1 +2}", env);
        EXPECT_TRUE(empty());
    }
    EXPECT_EQ("", pp("{#define X 1 (* c *) +2}{#define X 1 +2}"));
    EXPECT_TRUE(empty());

    // -nC + -dD: the echoed {#define}/{#undef} use the normalized (-dM)
    // form, with no comment, not the original source text.
    {
        Env env = setup();
        env.set_remove_comments(true);
        env.set_dd_mode(true);
        const std::string out = pp("{#define X 1 (* c *) +2}{#undef X (* c *)}", env);
        EXPECT_NE(std::string::npos, out.find("{#define X 1   +2}")) << "out:\n" << out;
        EXPECT_EQ(std::string::npos, out.find("(* c")) << "out:\n" << out;
        EXPECT_NE(std::string::npos, out.find("{#undef X}")) << "out:\n" << out;
    }

    // -D's body follows the same -nC policy (setup()'s remove_comments
    // parameter is set before -D is processed).
    {
        Env env = setup({{"Y", "1(*c*)+2"}}, true);
        EXPECT_EQ("[1 +2]", pp("[Y]", env));
        EXPECT_TRUE(empty());
    }
}

TEST_F(DirectiveTest, NewlineInDirectiveRedefinitionNoWarning) {
    // Redefining X with a raw-newline body that normalizes to the same
    // text as its previous definition must not raise MACRO_REDEFINED.
    EXPECT_EQ(";a b;\n;a b", pp("{#define X a b};X;{#define X a\nb};X"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, NewlineInCommentFormDirectives) {
    // The annotated '/*{' and line-comment '//{' directive forms fold a
    // raw newline the same way as the brace form.
    EXPECT_EQ("\n[a b]", pp("/*{#define X a\nb}*/[X]"));
    EXPECT_EQ("\n\n[a b]", pp("//{#define X a\nb}\n[X]"));
    EXPECT_TRUE(empty());
}

// ---- '//' inside a directive body ends at a raw newline, like C ----
//
// Before this fix, read_pragma_body() folded a raw newline into whitespace
// before the body was lexed, so a '//' comment spanning that newline kept
// eating characters past it and silently swallowed the rest of the
// directive (see LexerPragmaTest.DirectiveLineCommentEndsAtRawNewline for
// the lexer-level check of the folded text). These check the effect
// end-to-end, through each directive handler that re-lexes the decoded
// body.

TEST_F(DirectiveTest, LineCommentInMultiLineDirective) {
    // B1: {#define} - the rest of the directive after the comment is no
    // longer lost.
    for (const std::string& nl : {"\n", "\r\n"}) {
        SCOPED_TRACE(nl);
        EXPECT_EQ("\n[1 +2]", pp("{#define X 1 // c" + nl + "+2}[X]"));
        EXPECT_TRUE(empty());
    }
}

TEST_F(DirectiveTest, LineCommentInIfDirective) {
    // B2: {#if} - the comment no longer swallows the operand's continuation
    // ('+1'), nor the directives that follow it.
    EXPECT_EQ("\ny", pp("{#if 0 // c\n+1}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentInLineDirective) {
    // B3: {#line} - a trailing '//' comment across a raw newline does not
    // swallow the directive's closing '}' and the following text.
    const std::string plain = pp("{#line 100 \n}\n__LINE__");
    const std::string commented = pp("{#line 100 // c\n}\n__LINE__");
    EXPECT_EQ(plain, commented);
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, CommentsAroundLineOperand) {
    // Comments around the {#line}/{#:N} operand are whitespace, like in C.
    std::string plain = pp("{#line 100}\n__LINE__");
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#line 100 // c}\n__LINE__"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#line 100 (* c *)}\n__LINE__"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#line (* c *) 100}\n__LINE__"));
    EXPECT_TRUE(empty());

    // A comment between the line number and the file path is also
    // whitespace.
    std::string with_file = pp("{#line 100 'f.st'}\n__LINE__ __FILE__");
    EXPECT_TRUE(empty());
    EXPECT_EQ(with_file, pp("{#line 100 (* a *) 'f.st' // b}\n__LINE__ __FILE__"));
    EXPECT_TRUE(empty());

    EXPECT_EQ(pp("{#:100}\n__LINE__"), pp("{#:100 (* c *)}\n__LINE__"));
    EXPECT_TRUE(empty());

    // Comments as such are not recognized inside the quoted file path.
    EXPECT_NE(std::string::npos, pp("{#line 100 'f(*x*).st'}\n__FILE__").find("f(*x*).st"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentInMessageDirective) {
    // B4: {#warning} - the comment ends at the raw newline, so 'more' is
    // no longer part of the comment and reaches the message text. This is
    // unaffected by -nC (env.set_remove_comments): the truncation happens
    // in the lexer, before {#warning}'s own remove_comments handling would
    // ever see the comment.
    EXPECT_EQ("\n", pp("{#warning abc // x\nmore}"));
    {
        auto msg = message();
        EXPECT_EQ(Issue::Code::WARNING_MESSAGE, PlainTextMessage::parse_code(msg));
        EXPECT_NE(std::string::npos, msg.find("abc more")) << msg;
    }

    {
        Env env = setup();
        env.set_remove_comments(true);
        const std::string out = pp("{#warning abc // x\nmore}", env);
        EXPECT_EQ("\n", out);
        auto msg = message();
        EXPECT_EQ(Issue::Code::WARNING_MESSAGE, PlainTextMessage::parse_code(msg));
        EXPECT_NE(std::string::npos, msg.find("abc more")) << msg;
    }
}

TEST_F(DirectiveTest, LineCommentInStringDirective) {
    // B5: {#string}
    EXPECT_EQ("'a b'\n", pp("{#string a // c\nb}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentInTokenDirective) {
    // B6: {#token}
    EXPECT_EQ(pp("{#token a \nb}"), pp("{#token a // c\nb}"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentFromDirectiveBecomesBlockComment) {
    // U7: a '//' comment that ends up in the output with no newline after it
    // (from a directive operand spliced into the output, or a macro body)
    // is rewritten as a block comment so it does not swallow the rest of
    // the output line, like gcc/clang -CC.
    EXPECT_EQ("a /* c*/ b;", pp("{#token a // c} b;"));
    EXPECT_TRUE(empty());
    // A document comment ("//!") becomes "/*!...*/", the same way.
    EXPECT_EQ("a /*! d*/ b;", pp("{#define X a //! d}X b;"));
    EXPECT_TRUE(empty());

    {
        Env env = setup();
        env.set_remove_comments(true);
        // A document comment survives -nC (it is a token, not a comment)
        // and is still rewritten to a block comment.
        EXPECT_EQ("a /*! d*/ b;", pp("{#token a //! d} b;", env));
        EXPECT_TRUE(empty());
    }
    {
        Env env = setup();
        env.set_remove_comments(true);
        // -nC removes an ordinary '//' comment entirely (whitespace), so
        // there is nothing left to rewrite.
        EXPECT_EQ("a   b;", pp("{#token a // c} b;", env));
        EXPECT_TRUE(empty());
    }
}

TEST_F(DirectiveTest, LineCommentInDdModeEcho) {
    // B7: -dD echoes the normalized body -- the comment is gone, not just
    // hidden by the macro's own expansion.
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output = pp("{#define X 1 // c\n+2}", env);
    EXPECT_NE(std::string::npos, output.find("{#define X 1 +2}")) << "output:\n" << output;
}

TEST_F(DirectiveTest, LineCommentInCommentFormDirectives) {
    // B8: the annotated '(*{'/'/*{' and line-comment '//{' directive forms
    // are fixed the same way as the brace form.
    EXPECT_NE(std::string::npos, pp("(*{#define X 1 // c\n+2}*)[X]").find("[1 +2]"));
    EXPECT_NE(std::string::npos, pp("//{#define X 1 // c\n+2}\n[X]").find("[1 +2]"));
    EXPECT_NE(std::string::npos, pp("/*{#define X 1 // c\n+2}*/[X]").find("[1 +2]"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentInDirectiveInsideMacroArgument) {
    // B9: a directive nested inside another macro's argument list.
    EXPECT_NE(std::string::npos,
        pp("{#define ID(x) x}ID({#define Y 1 // c\n+2}[Y])").find("[1 +2]"));
}

TEST_F(DirectiveTest, LineCommentBeforeDirectiveKeyword) {
    // B10: a '//' comment before the directive keyword is also truncated at
    // the raw newline, so the keyword that follows is not swallowed.
    EXPECT_EQ("\n[1]", pp("{#define // c\nX 1}[X]"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentDoesNotAffectLineCounting) {
    // B11: the raw newline that ends the comment is still counted for line
    // numbering, same as any other raw newline folded inside a directive
    // (NewlineInDirectiveLineCountUnchanged above).
    EXPECT_EQ("\n2", pp("{#define X 1 // c\n+2}__LINE__"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, DollarNewlineInLineCommentJoinsParamList) {
    // B12: '$'+newline extends a '//' comment to the next raw newline
    // (like C's backslash-newline continuation), so a parameter list split
    // across that continuation still parses.
    EXPECT_EQ("1+2", pp("{#define F(a, // c$n b) a+b}F(1,2)"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, LineCommentBraceIsLiteralEndToEnd) {
    // B13 (v2): a '{' inside a '//' comment is ordinary content (PP22, kept
    // literal), not a nested pragma opener -- the comment still ends at the
    // raw newline and the directive still expands correctly.
    Issue::ContinueMode guard({});
    EXPECT_EQ("\n[1 +2]", pp("{#define X 1 // see {\n+2}[X]"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, cs[0]);
}

// -dM prints a GNU named variadic back as gcc/clang do -- the name directly
// followed by '...', no space and no comma before it.
TEST_F(DirectiveTest, DumpMacrosNamedVariadic) {
    Env env = setup();
    pp("{#define F(args...) [args]}{#define G(a, args ...) [a|args]}"
       "{#define V(a, ...) [a|__VA_ARGS__]}", env);
    std::ostringstream os;
    dump_macros(env, os);
    const std::string output = os.str();
    EXPECT_NE(std::string::npos, output.find("{#define F(args...) [args]}\n")) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("{#define G(a,args...) [a|args]}\n")) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("{#define V(a,...) [a|__VA_ARGS__]}\n")) << "output:\n" << output;
    EXPECT_TRUE(empty());
}

// -dD echoes the original source text of an accepted {#define}, so a named
// variadic's own whitespace (e.g. the space before '...') survives.
TEST_F(DirectiveTest, DdModeEchoesNamedVariadicDefine) {
    Env env = setup();
    env.set_dd_mode(true);
    const std::string output = pp("{#define F(args ...) x}", env);
    EXPECT_NE(std::string::npos, output.find("{#define F(args ...) x}")) << "output:\n" << output;
    EXPECT_TRUE(empty());
}

// An invalid '$' escape in a directive operand (e.g. "$q") is kept literally
// instead of emptying the operand, and processing of the directive
// continues normally after the single PP21 -- see decode_directive_text()
// (directive_parser.cpp). ContinueMode is needed because PP21 is ERROR and
// would otherwise stop processing at the first row.
TEST_F(DirectiveTest, InvalidEscapeKeptLiterally) {
    Issue::ContinueMode guard({});
    {
        SCOPED_TRACE("{#define A x$q y}A;");
        EXPECT_EQ("x$q y;", pp("{#define A x$q y}A;"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        // Inside a string literal "$q" is an IEC 61131-3 string escape
        // (valid or not is the compiler's business), kept as written
        // without PP21 (U8, B').
        SCOPED_TRACE("{#define A 'a$qb'}A;");
        EXPECT_EQ("'a$qb';", pp("{#define A 'a$qb'}A;"));
        EXPECT_TRUE(codes().empty());
    }
    {
        SCOPED_TRACE("{#define A x$4G y}A;");
        EXPECT_EQ("x$4G y;", pp("{#define A x$4G y}A;"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        // The '$' breaks the macro name at "A"; the rest ("$q 1") becomes
        // part of the macro body, and MISSING_WHITESPACE_AFTER_MACRO_NAME
        // (PP38) also fires because "A" is not followed by whitespace.
        SCOPED_TRACE("{#define A$q 1}A;");
        EXPECT_EQ("$q 1;", pp("{#define A$q 1}A;"));
        auto cs = codes();
        ASSERT_EQ(2u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_EQ(Issue::Code::MISSING_WHITESPACE_AFTER_MACRO_NAME, cs[1]);
    }
    {
        // Once-per-directive gating for PP21 is a later change; until then,
        // the unmatched "$q" in the operand also causes an
        // EXTRA_TOKENS_AT_END_OF_DIRECTIVE (PP49) warning here.
        SCOPED_TRACE("{#define A 1}{#ifdef A$q}yes;{#endif}");
        EXPECT_EQ("yes;", pp("{#define A 1}{#ifdef A$q}yes;{#endif}"));
        auto cs = codes();
        ASSERT_EQ(2u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[1]);
    }
    {
        SCOPED_TRACE("{#define A 1}{#undef A$q}A;");
        EXPECT_EQ("A;", pp("{#define A 1}{#undef A$q}A;"));
        auto cs = codes();
        ASSERT_EQ(2u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[1]);
    }
    {
        SCOPED_TRACE("{#error x$q}");
        pp("{#error x$q}");
        auto msgs = messages();
        ASSERT_EQ(2u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_EQ(Issue::Code::ERROR_MESSAGE, PlainTextMessage::parse_code(msgs[1]));
        EXPECT_NE(std::string::npos, msgs[1].find("x$q")) << msgs[1];
    }
    {
        SCOPED_TRACE("{#warning x$q y}");
        pp("{#warning x$q y}");
        auto msgs = messages();
        ASSERT_EQ(2u, msgs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, PlainTextMessage::parse_code(msgs[0]));
        EXPECT_EQ(Issue::Code::WARNING_MESSAGE, PlainTextMessage::parse_code(msgs[1]));
        EXPECT_NE(std::string::npos, msgs[1].find("x$q y")) << msgs[1];
    }
    {
        SCOPED_TRACE("{#string a$qb}");
        EXPECT_EQ("'a$$qb'", pp("{#string a$qb}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        SCOPED_TRACE("{#wstring a$qb}");
        EXPECT_EQ("\"a$$qb\"", pp("{#wstring a$qb}"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        SCOPED_TRACE("{#token a$qb}c;");
        EXPECT_EQ("a$qbc;", pp("{#token a$qb}c;"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        SCOPED_TRACE("{#line 10 'f$q.iec'}__FILE__;");
        EXPECT_EQ("(*{#:9 'f$$q.iec'}*)'f$$q.iec';",
                   pp("{#line 10 'f$q.iec'}__FILE__;"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        SCOPED_TRACE("{#line 10$q}");
        pp("{#line 10$q}");
        auto cs = codes();
        ASSERT_EQ(2u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, cs[1]);
    }
    {
        SCOPED_TRACE("{#include 'no_such$q.iec'}");
        pp("{#include 'no_such$q.iec'}");
        auto cs = codes();
        ASSERT_EQ(2u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_EQ(Issue::Code::FILE_NOT_FOUND, cs[1]);
    }
    {
        SCOPED_TRACE("{#nop x$q}ok;");
        EXPECT_EQ("ok;", pp("{#nop x$q}ok;"));
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        SCOPED_TRACE("{#def$qine A 1}A;");
        EXPECT_EQ("A;", pp("{#def$qine A 1}A;"));
        auto cs = codes();
        ASSERT_EQ(2u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, cs[1]);
    }
    {
        // Post-UD4, a syntax-error {#if} expression is false, so the group
        // stays inactive and "yes;" is not printed; PP21 is still the first
        // diagnostic since decoding the operand happens before evaluation.
        SCOPED_TRACE("{#if 1$q}yes;{#endif}");
        EXPECT_EQ("", pp("{#if 1$q}yes;{#endif}"));
        auto cs = codes();
        ASSERT_EQ(3u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    }
    {
        // {#ignore PP21} is process-wide for the rest of the input (like
        // every other {#ignore}), so this case runs last -- otherwise it
        // would silence PP21 for the rows above too, since Issue's ignore
        // set is not scoped per pp() call.
        SCOPED_TRACE("{#ignore PP21}{#define A x$q y}A;");
        EXPECT_EQ("x$q y;", pp("{#ignore PP21}{#define A x$q y}A;"));
        EXPECT_TRUE(empty());
    }
}

// A trailing invalid escape inside what would otherwise look like a '//'
// comment must not make lexer_pragma.cpp's LineCommentTracker treat the
// comment as still open: the tracker must see the raw '$' and the escape
// character too, not just the characters decode_directive_text() would have
// decoded. Before this fix, "$q"/"$4" (an unterminated hex escape) fed
// nothing to the tracker, so the '/' immediately before and the '/' right
// after "$q"/"$4" looked adjacent and started a real '//' comment that
// swallowed the rest of the line, including the raw newline used to
// terminate {#define}'s body.
TEST_F(DirectiveTest, InvalidEscapeDoesNotFakeLineComment) {
    Issue::ContinueMode guard({});
    {
        SCOPED_TRACE("{#define B x /$q/ y\\n+2}B;");
        const std::string output = pp("{#define B x /$q/ y\n+2}B;");
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_NE(std::string::npos, output.find("x /$q/ y +2;")) << "output:\n" << output;
    }
    {
        SCOPED_TRACE("{#define B x /$4/ y\\n+2}B;");
        const std::string output = pp("{#define B x /$4/ y\n+2}B;");
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
        EXPECT_NE(std::string::npos, output.find("x /$4/ y +2;")) << "output:\n" << output;
    }
}

// A pending "$X" (the first hex digit of a "$XX" escape) must survive a
// "$"+newline line continuation, the same way it survives an ordinary
// character: the continuation adds nothing to body, so "$X" still pairs
// with whatever character follows it. Before this fix, the continuation
// reset the pending state, so "$2" followed by "$<newline>F" was seen by
// the LineCommentTracker as never completing its hex pair -- here "$2F"
// decodes to 0x2F ('/'), so with the '/' just before it this opens a real
// '//' comment through "F y" that is dropped at the next raw newline,
// leaving "+2" as the rest of the directive body.
TEST_F(DirectiveTest, PendingHexEscapeSurvivesLineContinuation) {
    Issue::ContinueMode guard({});
    const std::string output = pp("{#define C x /$2$\nF y\n+2}C;");
    EXPECT_NE(std::string::npos, output.find("x +2;")) << "output:\n" << output;
}

// UD3: a directive inside a skipped ({#if 0}) group is never decoded for
// diagnostic purposes -- dispatch_directive() (expand.cpp) now only raises
// PP21 once it knows the directive is active/reachable, instead of eagerly
// inside parse_directive() regardless of {#if} state.
TEST_F(DirectiveTest, InvalidEscapeNotReportedInSkippedGroup) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("ok;",
              pp("{#if 0}{#define A x$q}{#error a$q}{#if 1$q}{#endif}{#endif}ok;"));
    EXPECT_TRUE(empty());
}

// UD2: a directive found while collecting a macro call's argument list is
// parsed once by the macro-argument collector to classify it (control vs.
// output vs. ordinary), then handed to dispatch_directive(), which parses
// it again to actually run it. Before this fix both parses used the
// PP21-raising 1-arg parse_directive(), so a single "$q" in the operand was
// reported twice. Now the argument collector uses the 2-arg overload (no
// diagnostic) and only dispatch_directive()'s own parse raises PP21, so the
// whole directive is reported exactly once.
TEST_F(DirectiveTest, InvalidEscapeReportedOnceThroughMacroArgument) {
    Issue::ContinueMode guard({});
    const std::string output =
        pp("{#define F(x) x}F({#define A x$q y} 1);A;");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
    EXPECT_NE(std::string::npos, output.find("x$q y;")) << "output:\n" << output;
}

// Unchanged: a single directive with two invalid escapes in the same
// operand still raises PP21 only once (key-preferred, first-invalid-part
// reporting collapses multiple invalid escapes in one text into one
// diagnostic; see DirectiveParserTest.DecodeTextInvalidEscapes).
TEST_F(DirectiveTest, InvalidEscapeReportedOncePerOperand) {
    Issue::ContinueMode guard({});
    pp("{#define A x$q$q y}");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, cs[0]);
}

// U8 (B'): IEC escapes inside a string literal in a directive operand are
// kept as written, like gcc/clang keep "\'" and "\n" in a macro body.
TEST_F(DirectiveTest, IecEscapesInDirectiveStringsKeptAsWritten) {
    EXPECT_EQ(R"('it$'s';)", pp(R"({#define X 'it$'s'}X;)"));
    EXPECT_EQ(R"('a$nb';)", pp(R"({#define X 'a$nb'}X;)"));
    EXPECT_EQ(R"('a$$b', 'a$41b';)", pp(R"({#define X 'a$$b', 'a$41b'}X;)"));
    EXPECT_EQ(R"("it's $"q$" $0041";)", pp(R"({#define X "it's $"q$" $0041"}X;)"));
    EXPECT_EQ(R"('a}b{c:d e';)", pp(R"({#define X 'a$}b${c$:d$ e'}X;)"));
    EXPECT_EQ("a\nb 'a$nb';", pp(R"({#define X a$nb 'a$nb'}X;)"));
    EXPECT_EQ(R"(1 'it$'s' 1;)", pp(R"({#define F(a) a 'it$'s' a}F(1);)"));
    EXPECT_EQ(R"('it$'s')", pp(R"({#token 'it$'s'})"));
    EXPECT_EQ(R"('$27it$$$27s$27')", pp(R"({#string 'it$'s'})"));
    EXPECT_EQ(R"("$0027a$$nb$0027")", pp(R"({#wstring 'a$nb'})"));
    EXPECT_EQ(R"('a$qb';)", pp(R"({#define X 'a$qb'}X;)"));
    EXPECT_TRUE(empty());
    pp(R"({#warning 'can$'t'})");
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_NE(std::string::npos, msgs[0].find(R"('can$'t')")) << msgs[0];
}

// A string in a directive hides '//' from the '//'-at-raw-newline tracking
// in read_pragma_body() exactly where tokenize() will.
TEST_F(DirectiveTest, LineCommentTrackingFollowsRawStrings) {
    EXPECT_EQ("\n'a$'b' +2;", pp("{#define B 'a$'b' // c\n+2}B;"));
    EXPECT_EQ("\n'x$'//y' +2;", pp("{#define B 'x$'//y'\n+2}B;"));
    EXPECT_EQ("\n'x$ny' +1;", pp("{#define B (* it's *) 'x$ny' // c\n+1}B;"));
    EXPECT_TRUE(empty());
}

// Text written with escaped quotes -- the form -dM prints -- decodes as
// before B': a quote from $' / $27 does not start a raw string.
TEST_F(DirectiveTest, EscapedQuoteFormUnchanged) {
    EXPECT_EQ(R"('it$'s';)", pp(R"({#define W $'it$$$'s$'}W;)"));
    EXPECT_EQ("\n'a//b' +1;", pp("{#define B $'a//b$'\n+1}B;"));
    EXPECT_EQ("\n'a' +1;", pp("{#define B $'a$' // c\n+1}B;"));
    // An apostrophe that never closes is not a string literal.
    EXPECT_EQ("don't\nstop;", pp("{#define X don't$nstop}X;"));
    EXPECT_EQ("\ndon't // c +2;", pp("{#define B don't // c\n+2}B;"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, DumpMacrosRoundTripsRawStrings) {
    Env env = setup();
    pp(R"({#define X 'it$'s'}{#define Y 'a$nb' c$nd}{#define F(a) 'a$'b' a})", env);
    std::ostringstream os;
    dump_macros(env, os);
    const std::string dumped = os.str();
    EXPECT_NE(std::string::npos, dumped.find("{#define X $'it$$$'s$'}\n")) << dumped;
    EXPECT_NE(std::string::npos, dumped.find("{#define Y $'a$$nb$' c$nd}\n")) << dumped;
    EXPECT_NE(std::string::npos, dumped.find("{#define F(a) $'a$$$'b$' a}\n")) << dumped;
    // What -dM printed reads back as the same definitions.
    EXPECT_EQ("'it$'s' 'a$nb' c\nd 'a$'b' 1;",
              pp(R"({#define X $'it$$$'s$'}{#define Y $'a$$nb$' c$nd})"
                 R"({#define F(a) $'a$$$'b$' a}X Y F(1);)"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveTest, RawStringRegressionWatchpoints) {
    EXPECT_EQ(R"('it$'s';)", pp(R"({#define X 'it$'s'}{#define X 'it$'s'}X;)"));
    EXPECT_TRUE(empty());
    EXPECT_EQ("[a[x,y][x,y]];", pp("{#define F(a) [a[x,y]]}F(a[x,y]);"));
    EXPECT_TRUE(empty());
}

// {#wstring} writes quotes as four-digit hex escapes: IEC 61131-3 reads
// "$hhhh" in a double-byte string, so "$22ab" would be one character.
TEST_F(DirectiveTest, WstringQuotesUseFourDigitHex) {
    EXPECT_EQ(R"("$0022ab$0022")", pp(R"({#define Q "ab"}{#wstring Q})"));
    EXPECT_EQ(R"("it$0027s")", pp("{#wstring it's}"));
    EXPECT_EQ(R"('it$27s')", pp("{#string it's}"));
    EXPECT_TRUE(empty());
}
