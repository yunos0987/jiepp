#include "test_helper.hpp"

// A newline that a macro expansion prints (a decoded $n/$l/$r/$0A/$0D in the
// macro body or in a directive operand) is not a source line: like C, where
// a macro expansion never changes __LINE__, it must not advance the line
// counter used by __LINE__, diagnostics and line markers. In the line-marker
// output, the next source line after such an expansion starts with a marker
// that resyncs the printed lines to the source (like gcc/clang).

namespace {
class OutputOnlyLinesTest : public JieppTest {};
} // namespace

TEST_F(OutputOnlyLinesTest, ObjectMacroOnce) {
    EXPECT_EQ("\na\nb;\n(*{#:2}*)\n3;", pp("{#define Z a$nb}\nZ;\n__LINE__;"));          // before: 4
    EXPECT_TRUE(empty());
}

TEST_F(OutputOnlyLinesTest, SeveralExpansionsOnOneLine) {
    EXPECT_EQ("\na\nb a\nb a\nb;\n(*{#:2}*)\n3;", pp("{#define Z a$nb}\nZ Z Z;\n__LINE__;")); // before: 6
}

TEST_F(OutputOnlyLinesTest, ExpansionsOnSeveralLines) {
    EXPECT_EQ("\na\nb;\n(*{#:2}*)\na\nb;\n(*{#:3}*)\n4;", pp("{#define Z a$nb}\nZ;\nZ;\n__LINE__;"));   // before: 6
}

TEST_F(OutputOnlyLinesTest, NestedMacro) {
    EXPECT_EQ("\n\na\nb a\nb;\n(*{#:3}*)\n4;",
              pp("{#define Z a$nb}\n{#define Y Z Z}\nY;\n__LINE__;"));           // before: 6
}

TEST_F(OutputOnlyLinesTest, FunctionMacroBody) {
    EXPECT_EQ("\nq\nq;\n(*{#:2}*)\n3;", pp("{#define F(x) x$nx}\nF(q);\n__LINE__;"));      // before: 4
}

TEST_F(OutputOnlyLinesTest, ArgumentUsedTwice) {
    EXPECT_EQ("\n\na\nb a\nb;\n(*{#:3}*)\n4;",
              pp("{#define Z a$nb}\n{#define F(x) x x}\nF(Z);\n__LINE__;"));    // before: 8
}

TEST_F(OutputOnlyLinesTest, LineInsideBodyAfterNewline) {
    // __LINE__ in a replacement is the invocation line, like gcc/clang.
    EXPECT_EQ("\na\n2;\n(*{#:2}*)\n3;", pp("{#define Z a$n__LINE__}\nZ;\n__LINE__;"));     // before: 3 and 4
}

TEST_F(OutputOnlyLinesTest, MultiLineCallWithNewlineInBody) {
    EXPECT_EQ("\nq\n3\n(*{#:2}*)\n;\n4;",
              pp("{#define F(x) x$n__LINE__}\nF(\nq);\n__LINE__;"));            // before: 4 and 5
}

TEST_F(OutputOnlyLinesTest, CallCompletedFromReplacement) {
    // The newline inside the argument list comes from H's body.
    EXPECT_EQ("\n\n[a b]\n;\n(*{#:3}*)\n4;",
              pp("{#define H F(a$nb}\n{#define F(x) [x]}\nH);\n__LINE__;"));   // before: 5
}

TEST_F(OutputOnlyLinesTest, AllNewlineEscapes) {
    EXPECT_EQ("\n\n\n\na\nb a\nb a\nb a\nb;\n(*{#:5}*)\n6;",
              pp("{#define Z a$lb}\n{#define W a$rb}\n{#define V a$0Ab}\n{#define U a$0Db}\n"
                 "Z W V U;\n__LINE__;"));                                        // before: 10
}

TEST_F(OutputOnlyLinesTest, StringLiteralAndDocCommentInBody) {
    EXPECT_EQ("\n'a\nb';\n(*{#:2}*)\n3;", pp("{#define Z 'a$nb'}\nZ;\n__LINE__;"));       // before: 4
    EXPECT_EQ("\n(*!a\nb*);\n(*{#:2}*)\n3;", pp("{#define Z (*!a$nb*)}\nZ;\n__LINE__;"));  // before: 4
}

TEST_F(OutputOnlyLinesTest, DiagnosticLineNumber) {
    EXPECT_EQ("\na\nb;\n", pp("{#define Z a$nb}\nZ;\n{#warning w}"));
    EXPECT_EQ(std::vector<std::string>{"<unknown location>:3.0: warning: PP92: 'w'"},
              messages());                                                       // before: 4.0
}

TEST_F(OutputOnlyLinesTest, AfterLineDirective) {
    EXPECT_EQ("\n(*{#:99}*)\na\nb;\n(*{#:100}*)\n101;",
              pp("{#define Z a$nb}\n{#line 100}\nZ;\n__LINE__;"));               // before: 102
}

TEST_F(OutputOnlyLinesTest, DirectiveOperands) {
    EXPECT_EQ("a\nb\n(*{#:1}*)\n2;", pp("{#token a$nb}\n__LINE__;"));                       // before: 3
    EXPECT_EQ("'a$nb'\n2;", pp("{## a$nb}\n__LINE__;"));                         // before: 3
    EXPECT_EQ("\n'a$nb'\n3;", pp("{#define Z a$nb}\n{#string Z}\n__LINE__;"));   // before: 4
    EXPECT_EQ("\n\n3;", pp("{#if 1$n+0}\n{#endif}\n__LINE__;"));                // before: 4
    EXPECT_EQ("\n\n\n4;", pp("{#define O 1$n+0}\n{#if O}\n{#endif}\n__LINE__;")); // before: 5
    EXPECT_EQ("\n(*{foo a\nb}*)\n(*{#:2}*)\n3;", pp("{#define Z a$nb}\n{foo Z}\n__LINE__;"));  // before: 4
    EXPECT_TRUE(empty());
}

TEST_F(OutputOnlyLinesTest, MessageDirectiveLines) {
    pp("{#info a$nb}\n{#info}");
    EXPECT_EQ((std::vector<std::string>{
                  "<unknown location>:1.0: info: PP93: 'a",
                  "b'",
                  "<unknown location>:2.0: info: PP93: ''",
              }),
              messages());                                                       // before: 2.0 / 3.0
}

TEST_F(OutputOnlyLinesTest, CompactionInsideAndAfterExpansion) {
    // 10 printed newlines inside Z: compacted to a marker that names the
    // source line of "b;" (2), not the printed line (before: (*{#:10}*), 13).
    EXPECT_EQ("\na\n(*{#:1}*)\nb;\n3;",
              pp("{#define Z a$n$n$n$n$n$n$n$n$n$nb}\nZ;\n__LINE__;"));
    // Source blank lines after an expansion (before: (*{#:11}*) ... 13).
    EXPECT_EQ("\na\nb;\n(*{#:11}*)\n12;",
              pp("{#define Z a$nb}\nZ;\n\n\n\n\n\n\n\n\n\n__LINE__;"));
}

TEST_F(OutputOnlyLinesTest, MultiLineStringArgument) {
    // The argument's own $-newline is re-emitted by the call, so its copy in
    // the replacement is not counted a second time (CLI before: (*{#:14 ...}*)).
    EXPECT_EQ("\n'a$\nb'\n(*{#:2}*)\n;\n4;\n(*{#:13}*)\n14;",
              pp("{#define F(x) x}\nF('a$\nb');\n__LINE__;\n\n\n\n\n\n\n\n\n\n__LINE__;"));
}

TEST_F(OutputOnlyLinesTest, RegressionWatchpoints) {
    // Identical redefinition: no PP35; a different amount of whitespace
    // (one vs two newlines) is still "identical" (C17 6.10.3p2).
    EXPECT_EQ("\n\n\n[a[x,y]];",
              pp("{#define Z a$nb}\n{#define Z a$nb}\n{#define F(x) [x]}\nF(a[x,y]);"));
    EXPECT_EQ("\n", pp("{#define Z a$nb}\n{#define Z a$n$nb}"));
    EXPECT_TRUE(empty());
}
