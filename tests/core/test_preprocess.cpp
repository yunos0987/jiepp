#include "test_helper.hpp"

#include <algorithm>

class PreprocessTest : public JieppTest {};

// ---- test_simple ----

TEST_F(PreprocessTest, Simple) {
    // Fact: plain IEC ST source passes through with {pragmas} wrapped in (*{...}*)
    EXPECT_EQ(
        "program Main\nvar\n    v: int;\nend_var\n(*{my_pragma}*)\nv := 1 + 2;\n(*{my_pragma}*)\nend_program\n\nprogram Sub end_program",
        pp("program Main\nvar\n    v: int;\nend_var\n{my_pragma}\nv := 1 + 2;\n{my_pragma}\nend_program\n\nprogram Sub end_program")
    );
    EXPECT_TRUE(empty());
}

// ---- test_bom ----

TEST_F(PreprocessTest, Bom) {
    // UTF-8 BOM (U+FEFF) at the beginning of input is stripped
    const std::string bom = "\xEF\xBB\xBF";
    EXPECT_EQ("", pp(bom));

    const std::string bom_program = bom + "program Main\n        end_program";
    EXPECT_EQ("program Main\n        end_program", pp(bom_program));

    // irregular: BOM in the middle of content passes through
    const std::string irregular = bom + " " + bom;
    EXPECT_EQ(std::string(" ") + bom, pp(irregular));

    EXPECT_TRUE(empty());
}

// ---- test_lexer_error ----

TEST_F(PreprocessTest, LexerError) {
    // Fact: pragmas with mismatched terminators in IEC block comments throw INVALID_PRAGMA_SYNTAX
    // (no whitespace between '{' and '#': these stay directives, exercising the closer check
    // for DIRECTIVE-typed pragma bodies)
    EXPECT_THROW(pp("(*{#line: 2 }*/ }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("(*{#line  2 }*/ }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: bare '}' (without *)) as closer in IEC block comment pragmas throws INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("(*{#line: 2 }   }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("(*{#line  2 }   }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: mismatched terminators in C block comment pragmas throw INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("/*{#line: 2 }*) }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("/*{#line: 2 }   }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("/*{#line  2 }*) }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("/*{#line  2 }   }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: spurious }*) or }*/ after bare-brace pragma body throws INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("{#line: 2 }*) }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("{#line: 2 }*/ }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("{#line  2 }*) }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("{#line  2 }*/ }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: the closer check also applies to an ordinary (non-directive) pragma body -- here
    // "{ #line..." is whitespace-before-'#', so it is lexed as a PRAGMA, not a DIRECTIVE, and
    // the mismatched-closer error is raised the same way (thrown before expansion ever runs,
    // so PP28 is never reached).
    EXPECT_THROW(pp("{ #line: 2 }*) }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    EXPECT_TRUE(empty());
}

// ---- test_space_between_brace_and_hash ----

TEST_F(PreprocessTest, SpaceBetweenBraceAndHashIsOrdinaryPragma) {
    // Fact: whitespace (space/tab/newline) between '{' and '#' makes the pragma
    // ordinary (not a directive): the directive is not executed, the whitespace
    // is preserved verbatim in the pragma body, and PP28 is raised.
    EXPECT_EQ("(*{ #define X 1}*)X", pp("{ #define X 1}X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    EXPECT_EQ("(*{\t#define X 1}*)X", pp("{\t#define X 1}X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    EXPECT_EQ("(*{ #define X 1}*)\nX", pp("{\n#define X 1}X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());
}

TEST_F(PreprocessTest, SpaceNewlineMixBetweenBraceAndHash) {
    // Fact: a run mixing plain whitespace and newlines between '{' and '#' is
    // treated the same as a pure-whitespace run: ordinary pragma, PP28, and the
    // run is preserved (embedded newlines collapsed to a single space -- the
    // same normalisation ordinary pragmas already apply, see lexer_pragma.cpp's
    // "Actual newline" handling).
    EXPECT_EQ("(*{ #define X 1}*)\nX", pp("{ \n#define X 1}X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    EXPECT_EQ("(*{\t #x}*)\n", pp("{\t\n #x}"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    // Line counting inside the pragma body stays correct.
    EXPECT_EQ("(*{ #define X 1}*)\n2", pp("{ \n#define X 1}__LINE__"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    // PP28 is reported on the line of the opening '{', not after the embedded newline.
    pp("\n\n{ \n#define X 1}X");
    EXPECT_EQ(
        "<unknown location>:3.0: warning: PP28: Whitespace between '{' and '#'; treated as an ordinary pragma",
        message());
}

TEST_F(PreprocessTest, SpaceBetweenBraceAndHashInCommentForms) {
    // Fact: the same whitespace-before-'#' rule applies uniformly to the
    // "(*{ #", "/*{ #", "//{ #" and "// { #" opener spellings.
    EXPECT_EQ("(*{ #define X 1}*)X", pp("(*{ #define X 1}*)X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    EXPECT_EQ("(*{ #define X 1}*)X", pp("/*{ #define X 1}*/X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    EXPECT_EQ("(*{ #define X 1}*)X", pp("//{ #define X 1}X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());

    EXPECT_EQ("(*{ #define X 1}*)X", pp("// { #define X 1}X"));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());
}

TEST_F(PreprocessTest, SpaceBetweenBraceAndHashStandardStyle) {
    // Fact: with the "standard" pragma style, the ordinary pragma re-renders as
    // a bare {...} (no (* *) wrapper), same as any other pragma.
    auto env = setup();
    env.set_pragma_style("standard");
    EXPECT_EQ("{ #define X 1}X", pp("{ #define X 1}X", env));
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());
}

TEST_F(PreprocessTest, SpaceBetweenBraceAndHashNoWarningInInactiveBlock) {
    // Fact: inside a false {#if} branch, the ordinary-pragma body is never
    // expanded, so PP28 is not raised.
    EXPECT_EQ("", pp("{#if 0}{ #x}{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(PreprocessTest, SpaceBetweenBraceAndHashIgnoredByIgnoreDirective) {
    // Fact: {#ignore PP28} suppresses the warning for subsequent ordinary pragmas.
    EXPECT_EQ("(*{ #x}*)", pp("{#ignore PP28}{ #x}"));
    EXPECT_TRUE(empty());
}

TEST_F(PreprocessTest, CommentBetweenBraceAndHashNoWarning) {
    // Fact: a comment (not whitespace) between '{' and '#' is intentional and is
    // not flagged by PP28.
    EXPECT_EQ("(*{(*c*)#x}*)", pp("{(*c*)#x}"));
    EXPECT_TRUE(empty());
}

TEST_F(PreprocessTest, SpaceBetweenBraceAndHashOutputIsIdempotent) {
    // Fact: pp(pp(x)) == pp(x): the ordinary-pragma re-render is stable under
    // repeated preprocessing (each pass re-raises PP28, which is expected).
    std::string x = "{ #define X 1}X";
    std::string once = pp(x);
    messages(); // drain PP28 from the first pass
    std::string twice = pp(once);
    EXPECT_EQ(once, twice);
}

TEST_F(PreprocessTest, WerrorPromotesWhitespaceBeforeDirective) {
    // Fact: -Werror promotes PP28 from WARNING to ERROR, so it throws.
    Issue::werror_ = true;
    EXPECT_THROW(pp("{ #x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, code());
    Issue::werror_ = false;
}

// ---- test_linecomment ----

TEST_F(PreprocessTest, LineComment) {
    // Fact: line comments pass through unchanged
    EXPECT_EQ("// abc", pp("// abc"));
    EXPECT_EQ("// abc\n//\n//", pp("// abc\n//\n//"));
    EXPECT_EQ(
        "// abc\nprogram Main //\n    // abc\nend_program //",
        pp("// abc\nprogram Main //\n    // abc\nend_program //")
    );
    EXPECT_TRUE(empty());
}

// ---- test_blockcomment1 ----

TEST_F(PreprocessTest, BlockComment1) {
    // Fact: IEC block comments (* *) pass through unchanged
    EXPECT_EQ("(* abc *)", pp("(* abc *)"));
    EXPECT_EQ("(* abc */*)", pp("(* abc */*)"));
    EXPECT_EQ(
        "program (* abc *) Main\n(**)end_program(**)",
        pp("program (* abc *) Main\n(**)end_program(**)")
    );
    EXPECT_TRUE(empty());
}

// ---- test_blockcomment2 ----

TEST_F(PreprocessTest, BlockComment2) {
    // Fact: C block comments /* */ pass through unchanged
    EXPECT_EQ("/* abc */", pp("/* abc */"));
    EXPECT_EQ("/* abc *)*/", pp("/* abc *)*/"));
    EXPECT_EQ(
        "program /* abc */ Main\n/**/end_program/**/",
        pp("program /* abc */ Main\n/**/end_program/**/")
    );
    EXPECT_TRUE(empty());
}

// ---- test_ieccomment1 ----

TEST_F(PreprocessTest, IecComment1) {
    // Fact: IEC doc comments (** *) pass through unchanged
    EXPECT_EQ("(** abc *)", pp("(** abc *)"));
    EXPECT_EQ(
        "program (** abc *) Main\n(***)end_program(***)",
        pp("program (** abc *) Main\n(***)end_program(***)")
    );
    EXPECT_TRUE(empty());
}

// ---- test_ieccomment2 ----

TEST_F(PreprocessTest, IecComment2) {
    // Fact: C doc comments /** */ pass through unchanged
    EXPECT_EQ("/** abc */", pp("/** abc */"));
    EXPECT_EQ(
        "program /** abc */ Main\n/***/end_program/***/",
        pp("program /** abc */ Main\n/***/end_program/***/")
    );
    EXPECT_TRUE(empty());
}

// ---- test_pragma ----

TEST_F(PreprocessTest, Pragma) {
    // Fact: pragmas are wrapped in (*{...}*) regardless of their original form
    EXPECT_EQ("(*{a:0}*)",  pp("{a:0}"));
    EXPECT_EQ("(*{a 0}*)",  pp("{a 0}"));
    EXPECT_EQ("(*{a:0}*)",  pp("//{a:0}"));
    EXPECT_EQ("(*{a:0}*)",  pp("(*{a:0}*)"));
    EXPECT_EQ("(*{a:0}*)",  pp("/*{a:0}*/"));

    // Fact: leading space inside braces is stripped (Python: [ \t\f\v\xa0]* in pragma regex)
    EXPECT_EQ("(*{a : 0 }*)", pp("{ a : 0 }"));
    EXPECT_EQ("(*{a   0 }*)", pp("{ a   0 }"));

    // Fact: pragma in line comment strips the '//' prefix and leading space
    EXPECT_EQ("(*{a : 0 }*)", pp("// { a : 0 }"));

    // Fact: block comment with optional space before '{' is treated as a pragma
    EXPECT_EQ("(*{a : 0 }*)", pp("(* { a : 0 } *)"));
    EXPECT_EQ("(*{a : 0 }*)", pp("/* { a : 0 } */"));

    // Fact: single and double quotes in pragma values pass through unchanged in C++
    EXPECT_EQ("(*{doc: it's a test}*)", pp("{doc: it's a test}"));
    // "it's a test" contains an unterminated 's a test literal (no closing
    // quote before the pragma body ends): PP29 once.
    EXPECT_EQ("<unknown location>:1.0: warning: PP29: Missing terminating quote character; '\'s a test'",
              message());
    EXPECT_EQ("(*{doc: say \"hello\"}*)", pp("{doc: say \"hello\"}"));
    EXPECT_EQ("(*{doc: 'helloworld' }*)", pp("{doc: 'hello' 'world'}"));

    EXPECT_TRUE(empty());
}

// ---- test_pragma_lineno ----

TEST_F(PreprocessTest, PragmaLineno) {
    // Fact: $\n in a pragma acts as a line continuation; newlines are preserved as blank lines
    EXPECT_EQ("(*{id a}*)\n\n\n\n;5", pp("{id $\n$\na$\n$\n};__LINE__"));

    // Fact: bare newlines in pragma body behave like whitespace in the output.
    EXPECT_EQ("(*{id a }*)\n\n\n\n;5", pp("{id \n\na\n\n};__LINE__"));

    EXPECT_EQ("(*{id:a}*)\n\n\n\n;5", pp("{id:$\n$\na$\n$\n};__LINE__"));
    EXPECT_EQ("(*{id: a }*)\n\n\n\n;5", pp("{id:\n\na\n\n};__LINE__"));

    EXPECT_EQ("(*{id }*)\n;2", pp("{id $\n};__LINE__"));
    EXPECT_EQ("(*{id }*)\n;2", pp("{id \n};__LINE__"));

    EXPECT_EQ("(*{id:}*)\n;2", pp("{id:$\n};__LINE__"));
    EXPECT_EQ("(*{id: }*)\n;2", pp("{id:\n};__LINE__"));

    EXPECT_TRUE(empty());
}

// ---- test_utf8_trailing_byte_a0 ----

TEST_F(PreprocessTest, Utf8TrailingByteA0NotWhitespace) {
    // Fact: byte 0xA0 (the trailing byte of U+30E0 "ム", encoded as E3 83 A0 in
    // UTF-8) must not be misclassified as whitespace: doing so would let
    // whitespace-trimming logic (macro body/argument trimming) split the
    // multi-byte character and drop its last byte.
    const std::string word = "\xE3\x82\xB7\xE3\x82\xB9\xE3\x83\x86\xE3\x83\xA0"; // "システム"

    // At the end of a macro body
    EXPECT_EQ("[" + word + "]", pp("{#define T " + word + "}[T]"));

    // Inside a string literal
    EXPECT_EQ("'" + word + "'", pp("'" + word + "'"));

    // As a directive argument (message text of {#error})
    EXPECT_THROW(pp("{#error " + word + "}"), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP91: '" + word + "'", message());
}

// ---- __INCLUDE_LEVEL__ ----

TEST_F(PreprocessTest, IncludeLevel) {
    // At top level (no push_file), include_level is 0
    EXPECT_EQ("0", pp("__INCLUDE_LEVEL__"));
    EXPECT_TRUE(empty());
}

// ---- __BASE_FILE__ ----

TEST_F(PreprocessTest, BaseFile) {
    // With no file context, base_file is '<unknown location>'
    EXPECT_EQ("'<unknown location>'", pp("__BASE_FILE__"));
    EXPECT_TRUE(empty());
}

// ---- __FILE_NAME__ ----

TEST_F(PreprocessTest, FileName) {
    // With no file context, filename is empty -> '<unknown location>'
    EXPECT_EQ("'<unknown location>'", pp("__FILE_NAME__"));
    EXPECT_TRUE(empty());
}

// ---- blank-line compaction start line (X1) ----

// The string API (pp()/preprocess_text()) has no leading line marker, unlike
// a jiepp command-line stream, so blank-line compaction must start counting
// from the stream's own first line (1) instead of 0: a synthetic marker N
// means "the next line is N + 1" (SPECIFICATION.md §9), which must equal
// that line's __LINE__.
TEST_F(PreprocessTest, BlankLineCompactionStartsAtFirstLine) {
    EXPECT_EQ("(*{#:10}*)\nx 11;", pp(std::string(10, '\n') + "x __LINE__;"));      // before: (*{#:9}*)
    EXPECT_EQ("a;\n(*{#:11}*)\nx 12;",
              pp("a;" + std::string(11, '\n') + "x __LINE__;"));                    // before: (*{#:10}*)
    EXPECT_TRUE(empty());
}

// ---- diagnostic line does not leak across preprocess()/preprocess_text() calls (C4) ----

// A library caller normally pairs setup() with each preprocess_text() call
// (one fresh Env per call). Before C4, Issue::loc_stack_'s top line was a
// free-running counter only ever moved by advance_lineno(), shared across
// every such call regardless of Env, so the first text's ending line leaked
// into the second (unrelated) Env's early diagnostics.
TEST_F(PreprocessTest, DiagnosticLineStartsAtEnvLineAcrossSeparateSetupCalls) {
    auto env1 = setup();
    pp("a;\nb;\nc;\n", env1); // 3 lines of plain text, no diagnostics
    EXPECT_TRUE(empty());

    auto env2 = setup();
    EXPECT_THROW(pp("{#error x}", env2), Issue::Exception);
    EXPECT_EQ("<unknown location>:1.0: error: PP91: 'x'", message());
}

// The same fix must not break the opposite, intended case: one Env used for
// two preprocess_text() calls in a row still reports the second call's
// diagnostics starting from that Env's own current line (its counter having
// advanced past the first call's text), not reset back to line 1.
TEST_F(PreprocessTest, DiagnosticLineFollowsEnvCounterAcrossRepeatedCalls) {
    auto env = setup();
    pp("a;\nb;\nc;\n", env); // leaves env's own line counter at line 4
    EXPECT_TRUE(empty());
    EXPECT_EQ(4, env.get_lineno());

    EXPECT_THROW(pp("{#error x}", env), Issue::Exception);
    EXPECT_EQ("<unknown location>:4.0: error: PP91: 'x'", message());
}

// ---- nested pragma brace diagnostics are linear (A2 / S-02) ----
//
// expand_pragma_token() re-lexes a pragma body, and every '{' nested in it
// was reported again (PP22) at each re-lex level: N nested braces gave
// N(N+1)/2 lines. A raw '{' inside the body of a pragma produced by a
// re-lex is no longer reported (the enclosing pass already reported it);
// the "unclosed pragma" PP22 of every level, the closer mismatches and PP23
// still are. For N nested braces: (N-1)+1 at the source lex, (N-2)+1 at the
// first re-lex, then one "unclosed" PP22 for each of the N-2 deeper levels:
// 3N-3 lines.

namespace {

// "PPnn: text" of each diagnostic, without the location prefix (and
// without anything after the closing quote of the context).
std::vector<std::string> diag_tails(std::vector<std::string> ms) {
    std::vector<std::string> r;
    for (auto& m : ms) {
        auto pos = m.find(": PP");
        std::string s = (pos == std::string::npos) ? m : m.substr(pos + 2);
        auto q = s.rfind('\'');
        if (q != std::string::npos)
            s.resize(q + 1);
        r.push_back(s);
    }
    return r;
}

std::string nested_annotated_pragmas(int n, const std::string& inner) {
    std::string s;
    for (int i = 0; i < n; ++i)
        s += "(*{";
    s += inner;
    for (int i = 0; i < n; ++i)
        s += "}*)";
    return s;
}

} // namespace

TEST_F(PreprocessTest, NestedPragmaBraceErrorsAreLinear) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    constexpr int kN = 100;
    const std::string out = pp(std::string(kN, '{') + "x\n");
    // Same output as before the fix.
    EXPECT_EQ(nested_annotated_pragmas(kN, "x ") + "\n", out);
    auto cs = codes();
    // Pre-fix: 5050 = N(N+1)/2.
    EXPECT_EQ(static_cast<std::size_t>(3 * kN - 3), cs.size());
    EXPECT_EQ(cs.size(),
              static_cast<std::size_t>(std::count(cs.begin(), cs.end(),
                                                  Issue::Code::INVALID_PRAGMA_SYNTAX)));
}

TEST_F(PreprocessTest, NestedPragmaBraceErrorsExactList) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    // N = 3: 3N-3 = N(N+1)/2 = 6, so the list is the same as before the fix.
    EXPECT_EQ(nested_annotated_pragmas(3, "x"), pp("{{{x"));
    EXPECT_EQ((std::vector<std::string>{
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{{{x'",
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{{x'",
                  "PP22: Invalid syntax for pragma; '{x'",
              }),
              diag_tails(messages()));

    // N = 4: the raw '{' in "{{x" (the second re-lex) is no longer
    // reported; before the fix it was, after "{{{x" (10 lines, now 9).
    EXPECT_EQ(nested_annotated_pragmas(4, "x"), pp("{{{{x"));
    EXPECT_EQ((std::vector<std::string>{
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{{{{x'",
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{{{x'",
                  "PP22: Invalid syntax for pragma; '{{x'",
                  "PP22: Invalid syntax for pragma; '{x'",
              }),
              diag_tails(messages()));
}

// Guard: an unclosed nested pragma whose opener is written "${" is reported
// at every level (only raw '{' characters inside a re-lexed body are
// deduplicated), the same two messages as before the fix.
TEST_F(PreprocessTest, EscapedNestedPragmaOpenerStillErrors) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    EXPECT_EQ("(*{$(*{$(*{x }*)}*)}*)", pp("{ ${ ${ x }"));
    EXPECT_EQ((std::vector<std::string>{
                  "PP22: Invalid syntax for pragma; '{ ${ x '",
                  "PP22: Invalid syntax for pragma; '{ x '",
              }),
              diag_tails(messages()));
    EXPECT_EQ(2, Issue::error_count_);
}

// Guard: the diagnostics of the source lex and of the first re-lex of a
// pragma body are unchanged (PP20/PP29 come from the re-lex of the body).
TEST_F(PreprocessTest, FirstLevelPragmaRelexDiagnosticsUnchanged_Guard) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    EXPECT_EQ("(*{(* x }*)", pp("{ (* x }"));
    EXPECT_EQ((std::vector<std::string>{
                  "PP20: Unclosed comment; '(* x '",
              }),
              diag_tails(messages()));

    EXPECT_EQ("(*{(*{(* x }*)}*)", pp("{ { (* x }"));
    EXPECT_EQ((std::vector<std::string>{
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP22: Invalid syntax for pragma; '{ (* x '",
                  "PP20: Unclosed comment; '(* x '",
              }),
              diag_tails(messages()));

    EXPECT_EQ("(*{'abc }*)", pp("{ 'abc }"));
    EXPECT_EQ((std::vector<std::string>{
                  "PP29: Missing terminating quote character; ''abc '",
              }),
              diag_tails(messages()));
}

// Guard (design review H2): a pragma rescanned in a macro argument can be
// re-lexed from expanded text that holds a closed nested pragma whose body
// opens an unclosed pragma with "${". That PP22 is the only diagnostic of
// the input and must stay (it decides the error status).
TEST_F(PreprocessTest, RescannedPragmaUnclosedEscapedOpenerStillErrors) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    auto env = setup();
    env.set_pragma_style("standard");
    const std::string out = pp("{#define N ${z$}}\n"
                               "{#define QM ${B $$N$}}\n"
                               "{#define PM ${ QM $}}\n"
                               "{#define F(x) x}\n"
                               "F(PM)\n", env);
    EXPECT_NE(std::string::npos, out.find("{{B ${z}}} }")) << out;
    EXPECT_EQ((std::vector<std::string>{
                  "PP22: Invalid syntax for pragma; '{z'",
              }),
              diag_tails(messages()));
    EXPECT_EQ(1, Issue::error_count_);
}

// Guard for clearing Token::relexed_pragma in expand_pragma_token() (design
// review M1): the expanded pragma text, rescanned in a macro argument, holds
// a raw '{' that no earlier pass reported, so its re-lex must report it.
// Keeping the mark on the rewritten token drops that PP22, the only error
// of the input (verified by removing the clearing line: error count 0).
TEST_F(PreprocessTest, RewrittenPragmaRescanStillReportsRawBrace) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    auto env = setup();
    env.set_pragma_style("standard");
    const std::string out = pp("{#define F(x) x}\n"
                               "{#define E}\n"
                               "{#define B ${b$}}\n"
                               "{#define S(x) @x}\n"
                               "{#define S2(x) S(x)}\n"
                               "{#define M() ${q S2(B)$}}\n"
                               "{#define IN ${ M E () $}}\n"
                               "{#define OUT ${ F E (IN) $}}\n"
                               "F(OUT)\n", env);
    EXPECT_NE(std::string::npos, out.find("{{{q '{b}'} } }")) << out;
    EXPECT_EQ((std::vector<std::string>{
                  "PP22: Invalid syntax for pragma; '{'",
                  "PP29: Missing terminating quote character; ''} '",
                  "PP29: Missing terminating quote character; ''{b'",
              }),
              diag_tails(messages()));
    EXPECT_EQ(1, Issue::error_count_);
}

// Guard (design review H1): PP23 for an unclosed directive found by a
// deeper re-lex is never suppressed. With PP22 ignored it is the only
// error, so dropping it would turn the run into a success.
TEST_F(PreprocessTest, UnclosedDirectiveInDeepRelexStillReportsPP23) {
    Issue::ContinueMode guard({Issue::Code::MAX_EXPANSION_DEPTH_EXCEEDED});
    Issue::Ignoring ignoring({Issue::Code::INVALID_PRAGMA_SYNTAX});
    const std::string out = pp("{ { {#define Q 1 }\nQ\n");
    EXPECT_NE(std::string::npos, out.find("\n1\n")) << out;
    EXPECT_EQ((std::vector<std::string>{
                  "PP23: Invalid preprocessor syntax; '{#define Q 1 '",
              }),
              diag_tails(messages()));
    EXPECT_EQ(1, Issue::error_count_);
}