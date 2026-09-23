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
    const std::string input = "{# /**/};{# (**)};{# //};{# */};{# /*};";
    EXPECT_EQ(";;;;;", pp(input));
    auto cs = codes();
    ASSERT_EQ(5u, cs.size());
    for (auto c : cs)
        EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, c);
}

TEST_F(DirectiveTest, InvalidDirectiveName) {
    // Identifier-like key with non-identifier chars → ERROR (not WARNING)
    // parse_directive keeps +, (, ) in the key since they're not separator chars
    EXPECT_THROW(pp("{#endif.}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, code());
    EXPECT_THROW(pp("{#if+}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_DIRECTIVE_NAME, code());
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
    // on top of the single lex-time UNKNOWN_DIRECTIVE warning already
    // raised for the same malformed text (DirectiveToken::ready()).
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

    EXPECT_EQ("\n", pp("{#warning a\nb}"));
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, code());

    EXPECT_THROW(pp("{#error a\nb}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

TEST_F(DirectiveTest, NewlineSplitsDirectiveName) {
    // A newline breaks up what would otherwise be one directive-name
    // token, so '#def\nine' is looked up (and rejected) as unknown
    // directive 'def', just like the plain unknown-directive case above.
    EXPECT_NO_THROW(pp("{#def\nine X 1}"));
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, code());
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
