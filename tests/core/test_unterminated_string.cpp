#include "test_helper.hpp"

#include "loader/lexer.hpp"

// PP29 (UNTERMINATED_STRING_LITERAL): a string literal the lexer ended at a
// newline or at the end of the input, not at its closing quote (D1: such a
// literal is also never merged into the following one -- see
// push_string_token() in lexer_literal.cpp).

namespace {
class UnterminatedStringTest : public JieppTest {};

// STRING/WSTRING tokens from `text`, as (text, unterminated) pairs; any
// other token type (whitespace, identifiers, ...) is not of interest here.
std::vector<std::pair<std::string, bool>> str_marks(const std::string& text) {
    std::vector<std::pair<std::string, bool>> marks;
    for (auto& t : iec3_tokens_from_string(text, /*remove_comments=*/false)) {
        if (t.type == Token::STRING || t.type == Token::WSTRING)
            marks.emplace_back(t.text, t.unterminated);
    }
    return marks;
}

// The exact PP29 message text at line `line` for context `ctx`, matching
// PlainTextMessage::message()'s "<unknown location>:L.0: ..." form used by
// pp()/pp_file() when no real file has been pushed.
std::string w29(int line, const std::string& ctx) {
    return "<unknown location>:" + std::to_string(line) +
           ".0: warning: PP29: Missing terminating quote character; '" + ctx + "'";
}

// The exact PP49 (EXTRA_TOKENS_AT_END_OF_DIRECTIVE) message text at line
// `line` for context `ctx`; see test_else_endif_extra_tokens.cpp for the
// full PP49 coverage this helper is used for here.
std::string w49(int line, const std::string& ctx) {
    return "<unknown location>:" + std::to_string(line) +
           ".0: warning: PP49: Extra tokens at end of directive; '" + ctx + "'";
}

} // namespace

// ---- lexer marks (D1) ----

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAtEof) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}}), str_marks("'a"));
}

TEST_F(UnterminatedStringTest, LexerDoesNotMarkClosedLiteral) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a'", false}}), str_marks("'a'"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAtLf) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}}), str_marks("'a\nb"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAtCrLf) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}}), str_marks("'a\r\nb"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedAfterEscapedQuote) {
    // $' is an escaped quote, not a closing one: still unterminated.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a$'", true}}), str_marks("'a$'\n"));
}

TEST_F(UnterminatedStringTest, LexerDoesNotMarkDollarNewlineContinuation) {
    // $ + newline is a line continuation, not an end-of-literal newline.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a$\nb'", false}}), str_marks("'a$\nb'"));
}

TEST_F(UnterminatedStringTest, LexerMarksUnclosedWstring) {
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"\"a", true}}), str_marks("\"a"));
}

// ---- D1: no merge into an unterminated literal ----

TEST_F(UnterminatedStringTest, NoMergeIntoUnterminatedPrecedingLiteral) {
    // 'a' (closed) followed by 'b (unterminated): the closed literal still
    // merges the way any adjacent pair does (Python-compatible splicing),
    // and the merged token inherits the new piece's unterminated mark.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'ab", true}}), str_marks("'a' 'b\n"));
}

TEST_F(UnterminatedStringTest, NoMergeAfterUnterminatedLiteral) {
    // 'a (unterminated) must not absorb the 'b' that follows it on the next
    // line: before the fix, push_string_token() merged them into a single
    // "'b'" token, silently dropping the "a" content.
    EXPECT_EQ((std::vector<std::pair<std::string, bool>>{{"'a", true}, {"'b'", false}}),
              str_marks("'a\n'b'"));
}

// ---- D1: output and line count unaffected; the PP29 warnings below are
// core's (next section), not D1's -- D1 by itself (commit 1) adds none. ----

TEST_F(UnterminatedStringTest, OutputUnchangedNoMergeSingleQuote) {
    // Before the fix: "x := 'abdef';\nz := 'r\n" -- the lexer's
    // Python-compatible merge dropped the unterminated literal's own
    // content into the following one's.
    EXPECT_EQ("x := 'abc\n'def';\n", pp("x := 'abc\n'def';\n"));
    // 'abc is genuinely unterminated; 'def' closes normally.
    EXPECT_EQ(w29(1, "'abc"), message());
}

TEST_F(UnterminatedStringTest, OutputUnchangedNoMergeDoubleQuote) {
    EXPECT_EQ("y := \"ab\n\"cd\";\n", pp("y := \"ab\n\"cd\";\n"));
    EXPECT_EQ(w29(1, "\"ab"), message());
}

TEST_F(UnterminatedStringTest, OutputUnchangedBothLiteralsUnterminated) {
    EXPECT_EQ("z := 'q\n'r\n", pp("z := 'q\n'r\n"));
    EXPECT_EQ((std::vector<std::string>{w29(1, "'q"), w29(2, "'r")}), messages());
}

TEST_F(UnterminatedStringTest, LineCounterUnaffectedByUnmergedLiteral) {
    EXPECT_EQ("'a\n'b'\nX 3;\n", pp("'a\n'b'\nX __LINE__;\n"));
    EXPECT_EQ(w29(1, "'a"), message());
}

TEST_F(UnterminatedStringTest, ClosedLiteralsStillMerge) {
    EXPECT_EQ("'abcd'\n", pp("'ab'\n'cd'"));
    EXPECT_TRUE(empty());
}

// ---- PP29: plain text ----

TEST_F(UnterminatedStringTest, PlainTextTwoLiteralsBothReported) {
    EXPECT_EQ("x := 'abc\ny;\nz := \"def\nw;\n", pp("x := 'abc\ny;\nz := \"def\nw;\n"));
    EXPECT_EQ((std::vector<std::string>{w29(1, "'abc"), w29(3, "\"def")}), messages());
}

TEST_F(UnterminatedStringTest, EndOfInput) {
    pp("x := 'abc");
    EXPECT_EQ(w29(1, "'abc"), message());
}

TEST_F(UnterminatedStringTest, Crlf) {
    pp("x := 'abc\r\ny;\r\n");
    EXPECT_EQ(w29(1, "'abc"), message());
}

TEST_F(UnterminatedStringTest, DollarNewlineContinuationNotReported) {
    EXPECT_EQ("x := 'ab$\ncd';\n", pp("x := 'ab$\ncd';\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, ContinuationThenUnclosedReportsStartLine) {
    EXPECT_EQ("y := 'ab$\ncd\nz 3;\n", pp("y := 'ab$\ncd\nz __LINE__;\n"));
    EXPECT_EQ(w29(1, "'ab$"), message());
}

TEST_F(UnterminatedStringTest, EscapedQuoteIsNotClosing) {
    pp("x := 'it$'s\n");
    EXPECT_EQ(w29(1, "'it$'s"), message());
}

TEST_F(UnterminatedStringTest, TypedStringPrefixUnclosed) {
    EXPECT_EQ("x := STRING#'abc\n", pp("x := STRING#'abc\n"));
    EXPECT_EQ(w29(1, "'abc"), message());
}

TEST_F(UnterminatedStringTest, TypedWstringPrefixUnclosed) {
    EXPECT_EQ("WSTRING#\"d", pp("WSTRING#\"d"));
    EXPECT_EQ(w29(1, "\"d"), message());
}

TEST_F(UnterminatedStringTest, NoWarningForClosedOrCommentedQuotes) {
    for (const std::string& src : {
             std::string("(* don't *)"),
             std::string("// it's"),
             std::string("/* it's */"),
             std::string("(*! it's *)"),
             std::string("//! it's"),
             std::string("'it$'s'"),
             std::string("''"),
             std::string("'a' 'b'"),
             std::string("\"a$\"b\""),
         }) {
        pp(src);
        EXPECT_TRUE(empty()) << src;
    }
}

TEST_F(UnterminatedStringTest, MergedIntoClosedPieceReportsOnce) {
    EXPECT_EQ("x := 'ab \n", pp("x := 'a' 'b\n"));
    EXPECT_EQ(w29(1, "'ab"), message());
}

// ---- PP29: skipped groups ----

TEST_F(UnterminatedStringTest, InactiveGroupNotScanned) {
    EXPECT_EQ("\n\n\n\n\nok;\n", pp("{#if 0}\nx := 'abc\n{#define Z 'q}\n{#error don't}\n{#endif}\nok;\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, ReachedElseBodyIsScanned) {
    pp("{#if 0}\n{#else}\n'q\n{#endif}\n");
    EXPECT_EQ(w29(3, "'q"), message());
}

// ---- PP29: macro bodies ----

TEST_F(UnterminatedStringTest, MacroBodyReportedOnceAtDefinitionNotPerUse) {
    pp("{#define X 'abc}\nX;\nX;\n");
    EXPECT_EQ(w29(1, "'abc"), message());
}

TEST_F(UnterminatedStringTest, IdenticalRedefinitionReportsPP29TwiceNoPP35) {
    pp("{#define X 'abc}\n{#define X 'abc}\n");
    EXPECT_EQ((std::vector<std::string>{w29(1, "'abc"), w29(2, "'abc")}), messages());
}

TEST_F(UnterminatedStringTest, DifferingRedefinitionReportsPP29TwicePlusPP35) {
    pp("{#define Y 'a}\n{#define Y 'b}\n");
    auto cs = codes();
    ASSERT_EQ(3u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[1]);
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, cs[2]);
}

// ---- PP29: function-macro arguments ----

TEST_F(UnterminatedStringTest, ArgumentReportedOnceWhenReadEvenIfUsedTwice) {
    pp("{#define F(a) a a}\nF('abc\n);\n");
    EXPECT_EQ(w29(2, "'abc"), message());
}

TEST_F(UnterminatedStringTest, ArgumentReportedAtItsOwnLineAcrossACall) {
    pp("{#define G(a,b) b}\nG(1,\n'abc\n);\n");
    EXPECT_EQ(w29(3, "'abc"), message());
}

TEST_F(UnterminatedStringTest, ArgumentReportedOnceEvenWhenUnused) {
    pp("{#define U(a) 0}\nU('abc\n);\n");
    EXPECT_EQ(w29(2, "'abc"), message());
}

TEST_F(UnterminatedStringTest, BracketArgumentWatchpointNotSplit) {
    // Unrelated regression watchpoint (W8): F(a[x,y]) must not be split by
    // the argument-collection changes above.
    EXPECT_EQ("[a[x,y]];", pp("{#define F(a) [a]}F(a[x,y]);"));
    EXPECT_TRUE(empty());
}

// ---- PP29: directive operands ----

TEST_F(UnterminatedStringTest, IncludeOperandReportsPP29ThenPP47) {
    EXPECT_THROW(pp("{#include 'abc}"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_PATH, cs[1]);
}

TEST_F(UnterminatedStringTest, IfdefOperandReportsPP29ThenPP40) {
    EXPECT_THROW(pp("{#ifdef 'x}"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_DEFINED_OPERAND, cs[1]);
}

TEST_F(UnterminatedStringTest, UndefOperandReportsPP29ThenPP49NoThrow) {
    pp("{#undef X 'y}");
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[1]);
}

TEST_F(UnterminatedStringTest, LineOperandReportsPP29ThenPP41) {
    EXPECT_THROW(pp("{#line 10 'x}"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, cs[1]);
}

TEST_F(UnterminatedStringTest, IfConditionReportsPP29ThenPP51) {
    EXPECT_THROW(pp("{#if 'a}{#endif}"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, cs[1]);
}

TEST_F(UnterminatedStringTest, TokenizeDirectiveReportsPP29) {
    EXPECT_EQ("'abc", pp("{#token 'abc}"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
}

TEST_F(UnterminatedStringTest, StringizeDirectiveReportsPP29) {
    EXPECT_EQ("'$27abc'", pp("{#string 'abc}"));
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
}

TEST_F(UnterminatedStringTest, PragmaOnceOperandReportsPP29ThenPP49) {
    pp("{#pragma once 'x}");
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[1]);
}

TEST_F(UnterminatedStringTest, PragmaWithUnrecognisedNameStillReportsPP29) {
    pp("{#pragma foo 'x}");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
}

TEST_F(UnterminatedStringTest, IgnoreOperandReportsPP29ThenPP42) {
    EXPECT_THROW(pp("{#ignore PP41 'x}"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_IGNORE_OPERAND, cs[1]);
}

TEST_F(UnterminatedStringTest, PragmaStyleOperandReportsPP29ThenPP48) {
    pp("{#pp_output_pragma_style standard 'x}");
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_STYLE_OPERAND, cs[1]);
}

TEST_F(UnterminatedStringTest, UnknownDirectiveOperandReportsPP45ThenPP29) {
    Issue::ContinueMode cm({});
    pp("{#foo 'x}");
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNKNOWN_DIRECTIVE, cs[0]);
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[1]);
}

// ---- PP29: conditionals ----

TEST_F(UnterminatedStringTest, ElseAndEndifOperandsAreScanned) {
    pp("{#if 1}\n{#else 'x}\n{#endif 'y}\n");
    EXPECT_EQ((std::vector<std::string>{
                  w29(2, "'x"), w49(2, "{#else 'x}"),
                  w29(3, "'y"), w49(3, "{#endif 'y}")}),
              messages());
}

TEST_F(UnterminatedStringTest, ElifNotEvaluatedButScannedAfterTrueBranchTaken) {
    pp("{#if 0}\n{#elif 1}\n{#elif 'z}\n{#endif}\n");
    EXPECT_EQ(w29(3, "'z"), message());
}

TEST_F(UnterminatedStringTest, ElifOperandScannedWhileEvaluatingDefined) {
    pp("{#if 1}\n{#elif defined 'k}\n{#endif}\n");
    EXPECT_EQ(w29(2, "'k"), message());
}

TEST_F(UnterminatedStringTest, InactiveParentSuppressesElseElifEndifScan) {
    EXPECT_EQ("\n\n\n\n\n\n", pp("{#if 0}\n{#if 1}\n{#elif 'w}\n{#else 'v}\n{#endif 'u}\n{#endif}\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, ElifNotYetDecidedReportsPP29ThenPP51) {
    EXPECT_THROW(pp("{#if 0}\n{#elif 'e}\n{#endif}\n"), Issue::Exception);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::MISSING_EXPRESSION, cs[1]);
}

// ---- PP29: message directives (clang-like: not scanned) ----

TEST_F(UnterminatedStringTest, WarningMessageTextNotScannedForPP29) {
    pp("{#warning it's \"x}");
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, cs[0]);
}

TEST_F(UnterminatedStringTest, InfoMessageWithDollarNAndUnclosedQuoteNotScanned) {
    pp("{#info don't$nstop}");
    EXPECT_EQ((std::vector<std::string>{"<unknown location>:1.0: info: PP93: 'don't",
                                        "stop'"}),
              messages());
}

TEST_F(UnterminatedStringTest, ErrorMessageNotScanned) {
    EXPECT_THROW(pp("{#error don't}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

TEST_F(UnterminatedStringTest, SevereMessageNotScanned) {
    EXPECT_THROW(pp("{#severe don't}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SEVERE_MESSAGE, code());
}

TEST_F(UnterminatedStringTest, MacroExpandedIntoMessageWasAlreadyScannedAtDefinition) {
    pp("{#define M 'abc}{#warning M}");
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, cs[1]);
}

// ---- PP29: ordinary pragmas ----

TEST_F(UnterminatedStringTest, OrdinaryPragmaBodyScannedWhenExpanded) {
    EXPECT_EQ("(*{attribute 'x}*)", pp("{attribute 'x}"));
    EXPECT_EQ(w29(1, "'x"), message());
}

TEST_F(UnterminatedStringTest, OrdinaryPragmaClosedQuoteNotScanned) {
    pp("{attribute 'x'}");
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, OrdinaryPragmaInInactiveGroupNotScanned) {
    pp("{#if 0}{attribute 'x}{#endif}");
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, PragmaInsideMacroBodyReportedOncePerExpansion) {
    EXPECT_EQ("\n(*{a 'x}*) (*{a 'x}*)\n", pp("{#define P ${a 'x$}}\nP P\n"));
    EXPECT_EQ((std::vector<std::string>{w29(2, "'x"), w29(2, "'x")}), messages());
}

// ---- PP29: suppression ----

TEST_F(UnterminatedStringTest, IgnorePP29SuppressesLaterLiterals) {
    EXPECT_EQ("\nx := 'abc\n\n", pp("{#ignore PP29}\nx := 'abc\n{#define X 'q}\n"));
    EXPECT_TRUE(empty());
}

TEST_F(UnterminatedStringTest, IgnorePP29OnlyAffectsLaterLiterals) {
    pp("x := 'a\n{#ignore PP29}\ny := 'b\n");
    EXPECT_EQ(w29(1, "'a"), message());
}

TEST_F(UnterminatedStringTest, LineDirectiveShiftsReportedLine) {
    pp("{#line 100}\nx := 'abc\n");
    EXPECT_EQ(w29(100, "'abc"), message());
}

TEST_F(UnterminatedStringTest, WerrorPromotesPP29) {
    Issue::werror_ = true;
    EXPECT_THROW(pp("x := 'abc"), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, code());
    Issue::werror_ = false;
}

TEST_F(UnterminatedStringTest, SuppressWarningsHidesPP29) {
    Issue::suppress_warnings_ = true;
    EXPECT_EQ("x := 'abc", pp("x := 'abc"));
    Issue::suppress_warnings_ = false;
    EXPECT_TRUE(empty());
}

// ---- PP29: -D ----

TEST_F(UnterminatedStringTest, DashDOperandReportsPP29AtDefinitionOnly) {
    Env env = setup({{"X", "'abc"}}, false);
    EXPECT_EQ(w29(1, "'abc"), message());
    EXPECT_EQ("'abc;", pp("X;", env));
    EXPECT_TRUE(empty());
}

// ---- PP29: same file included twice ----

TEST_F(UnterminatedStringTest, SameFileIncludedTwiceReportsPP29Twice) {
    fs::path tmp = fs::temp_directory_path() / "unterminated_string_include_twice.iec";
    {
        std::ofstream f(tmp);
        f << "x := 'abc\n";
    }
    std::string abs = fs::absolute(tmp).generic_string();
    std::string src = "{#include '" + abs + "'}{#include '" + abs + "'}";
    pp(src);
    std::error_code ec;
    fs::remove(tmp, ec);
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[0]);
    EXPECT_EQ(Issue::Code::UNTERMINATED_STRING_LITERAL, cs[1]);
}

// ---- PP29: code metadata ----

TEST_F(UnterminatedStringTest, IsWarningAndCodeValue) {
    EXPECT_TRUE(Issue::is_warning(Issue::Code::UNTERMINATED_STRING_LITERAL));
    EXPECT_EQ(29u, static_cast<unsigned int>(Issue::Code::UNTERMINATED_STRING_LITERAL));
}
