// test_include_stack.cpp -- "In file included from" lines before a diagnostic
// in an included file (SPEC 16 "出力形式"). The lines and when they appear
// were measured with clang 22.1 on the same inputs in C syntax (the names
// differ: clang shows ".\a.h"; jiepp shows the name of SPEC 13, and an
// -include file's include point is "<command line>:" where clang has
// "<built-in>:N:"). Cases C cannot write (two includes on one line, the
// library, the option) follow the same rule.

#include "test_helper.hpp"
#include "jiepp/jiepp.hpp"
#include "jiepp/option.hpp"

namespace {

class IncludeStackTest : public JieppTest {
protected:
    fs::path dir_;
    std::optional<CwdGuard> cwd_;

    void SetUp() override {
        JieppTest::SetUp();
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        dir_ = fs::temp_directory_path() / (std::string("jiepp_include_stack_") + info->name());
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        cwd_.emplace(dir_);
    }

    void TearDown() override {
        cwd_.reset();
        std::error_code ec;
        fs::remove_all(dir_, ec);
        JieppTest::TearDown();
    }

    void write(const std::string& name, const std::string& text) {
        fs::create_directories((dir_ / name).parent_path());
        write_file(dir_ / name, text);
    }

    // Runs jiepp_command() on main.iec (output to a file) and returns its
    // stderr lines (without the Debug build's "@file:line" suffix).
    std::vector<std::string> run(JieppOptions opts = {}, int expected_rc = 0) {
        if (opts.input_filepaths.empty())
            opts.input_filepaths = {"main.iec"};
        opts.output_filepath = "out.piec";
        EXPECT_EQ(expected_rc, jiepp_command(opts));
        return messages();
    }
};

using Lines = std::vector<std::string>;

} // namespace

#ifndef JIEPP_SANDBOX

// The WP5 example: main includes a, a includes b; b has a warning and an
// error. Both are in the same inclusion of b, so the lines come once.
TEST_F(IncludeStackTest, ThreeLevelsAndTwoDiagnosticsOfOneInclusion) {
    write("main.iec", "m0\n{#include 'a.iec'}\n{#warning m_after}\n");
    write("a.iec", "a0\n{#include 'b.iec'}\n{#warning a_after}\n");
    write("b.iec", "{#warning w}\n{#if 1 +}\n{#endif}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:2:",
                  "In file included from a.iec:2:",
                  "b.iec:1.0: warning: PP92: 'w'",
                  "b.iec:2.0: error: PP51: Missing expression; '1 +'",
                  "In file included from main.iec:2:",
                  "a.iec:3.0: warning: PP92: 'a_after'",
                  "main.iec:3.0: warning: PP92: 'm_after'",
              }),
              run({}, 1));
}

// Back to the same inclusion of a after a diagnostic in b: the lines come
// again (they follow the last printed diagnostic, like clang).
TEST_F(IncludeStackTest, RepeatedAfterAnotherFile) {
    write("main.iec", "{#include 'a.iec'}\n");
    write("a.iec", "{#warning a1}\n{#include 'b.iec'}\n{#warning a3}\n{#warning a4}\n");
    write("b.iec", "{#warning b1}\n{#warning b2}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "a.iec:1.0: warning: PP92: 'a1'",
                  "In file included from main.iec:1:",
                  "In file included from a.iec:2:",
                  "b.iec:1.0: warning: PP92: 'b1'",
                  "b.iec:2.0: warning: PP92: 'b2'",
                  "In file included from main.iec:1:",
                  "a.iec:3.0: warning: PP92: 'a3'",
                  "a.iec:4.0: warning: PP92: 'a4'",
              }),
              run());
}

// The same file by two routes, and the same file included twice from one
// file: each inclusion is its own include point.
TEST_F(IncludeStackTest, EachInclusionHasItsOwnIncludePoint) {
    write("main.iec", "{#include 'a.iec'}\n{#include 'b.iec'}\n{#include 'b.iec'}{#include 'b.iec'}\n");
    write("a.iec", "\n\n{#include 'b.iec'}\n");
    write("b.iec", "{#warning b}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "In file included from a.iec:3:",
                  "b.iec:1.0: warning: PP92: 'b'",
                  "In file included from main.iec:2:",
                  "b.iec:1.0: warning: PP92: 'b'",
                  // Two inclusions on one line are still two (clang tells
                  // them apart by the include location; C has one per line).
                  "In file included from main.iec:3:",
                  "b.iec:1.0: warning: PP92: 'b'",
                  "In file included from main.iec:3:",
                  "b.iec:1.0: warning: PP92: 'b'",
              }),
              run());
}

// Nothing before a diagnostic in the main input, before or after one in an
// included file.
TEST_F(IncludeStackTest, MainInputDiagnosticsHaveNoLines) {
    write("main.iec", "{#warning m1}\n{#include 'a.iec'}\n{#warning m3}\n{#include 'a.iec'}\n");
    write("a.iec", "{#warning a}\n");
    EXPECT_EQ((Lines{
                  "main.iec:1.0: warning: PP92: 'm1'",
                  "In file included from main.iec:2:",
                  "a.iec:1.0: warning: PP92: 'a'",
                  "main.iec:3.0: warning: PP92: 'm3'",
                  "In file included from main.iec:4:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              run());
}

// {#line} renames the includer and renumbers its lines: the include point is
// shown as a diagnostic there would be. {#line} in the included file itself
// does not repeat the lines (the inclusion is the same).
TEST_F(IncludeStackTest, IncludePointAfterLine) {
    write("main.iec", "x\n{#line 100 'renamed.iec'}\n{#include 'a.iec'}\n");
    write("a.iec", "{#warning a1}\n{#line 7 'zz.iec'}\n{#warning a2}\n{#include 'b.iec'}\n");
    write("b.iec", "{#warning b}\n");
    EXPECT_EQ((Lines{
                  "In file included from renamed.iec:100:",
                  "a.iec:1.0: warning: PP92: 'a1'",
                  "zz.iec:7.0: warning: PP92: 'a2'",
                  "In file included from renamed.iec:100:",
                  "In file included from zz.iec:8:",
                  "b.iec:1.0: warning: PP92: 'b'",
              }),
              run());
}

// Every kind of printed diagnostic gets them: an error of the include itself
// (in the includer), an unterminated {#if} at the end of the file, {#info}.
TEST_F(IncludeStackTest, AnySeverity) {
    write("main.iec", "{#include 'a.iec'}\n");
    write("a.iec", "{#info i}\n{#include 'b.iec'}\n");
    write("b.iec", "{#if 1}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "a.iec:1.0: info: PP93: 'i'",
                  "In file included from main.iec:1:",
                  "In file included from a.iec:2:",
                  "b.iec:1.0: error: PP24: Unterminated conditional directive; 'unclosed if'",
              }),
              run({}, 1));

    write("a.iec", "{#include 'nope.iec'}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "a.iec:1.0: error: PP11: No such file or directory; 'nope.iec'",
              }),
              run({}, 1));
}

// -include: the include point is the command line, with no line number (gcc
// "<command-line>:"; clang "<built-in>:N:"). Nothing before the main input's.
TEST_F(IncludeStackTest, CommandLineInclude) {
    write("main.iec", "{#warning main}\n");
    write("pre.iec", "{#warning pre}\n{#include 'q.iec'}\n");
    write("q.iec", "{#warning q}\n");
    write("p2.iec", "{#warning p2}\n");
    JieppOptions opts;
    opts.include_filepaths = {"pre.iec", "p2.iec"};
    EXPECT_EQ((Lines{
                  "In file included from <command line>:",
                  "pre.iec:1.0: warning: PP92: 'pre'",
                  "In file included from <command line>:",
                  "In file included from pre.iec:2:",
                  "q.iec:1.0: warning: PP92: 'q'",
                  "In file included from <command line>:",
                  "p2.iec:1.0: warning: PP92: 'p2'",
                  "main.iec:1.0: warning: PP92: 'main'",
              }),
              run(opts));
}

// The main input from standard input is "<stdin>" (clang: "<stdin>:1:").
TEST_F(IncludeStackTest, StandardInput) {
    write("a.iec", "{#warning a}\n");
    std::istringstream in("x\n{#include 'a.iec'}\n");
    std::streambuf* saved = std::cin.rdbuf(in.rdbuf());
    JieppOptions opts;
    opts.input_filepaths = {"-"};
    const auto lines = run(opts);
    std::cin.rdbuf(saved);
    EXPECT_EQ((Lines{
                  "In file included from <stdin>:2:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              lines);
}

// A diagnostic that is not printed ({#ignore}, -w, --silent) neither prints
// the lines nor counts as printed: the next printed one in the same
// inclusion gets them.
TEST_F(IncludeStackTest, SuppressedDiagnosticsGetNoLines) {
    write("main.iec", "{#include 'a.iec'}\n");
    write("a.iec", "{#ignore PP92}{#warning hidden}\n{#info shown}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "a.iec:2.0: info: PP93: 'shown'",
              }),
              run());

    write("a.iec", "{#warning hidden}\n{#info shown}\n");
    JieppOptions w;
    w.suppress_warnings = true;
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "a.iec:2.0: info: PP93: 'shown'",
              }),
              run(w));

    JieppOptions silent;
    silent.silent = true;
    EXPECT_EQ(Lines{}, run(silent));
}

// --no-include-stack: one line per diagnostic, as before.
TEST_F(IncludeStackTest, NoIncludeStackOption) {
    write("main.iec", "{#include 'a.iec'}\n");
    write("a.iec", "{#warning a}\n{#include 'b.iec'}\n");
    write("b.iec", "{#warning b}\n");
    JieppOptions opts;
    opts.no_include_stack = true;
    EXPECT_EQ((Lines{
                  "a.iec:1.0: warning: PP92: 'a'",
                  "b.iec:1.0: warning: PP92: 'b'",
              }),
              run(opts));
}

// The include point of a directive written over several lines is the line
// of the path (clang and gcc: the line of the file name token, when it is
// not glued to a C line splice), of the macro name that makes it, or of the
// ')' of a function-macro call that makes it. The directive's own
// diagnostics (PP11) stay at its first line.
TEST_F(IncludeStackTest, MultiLineDirectiveIncludePoint) {
    write("a.iec", "{#warning a}\n");
    write("main.iec", "x\n\n{#include $\n  $\n  'a.iec'}\n{#include\n 'a.iec'}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:5:",
                  "a.iec:1.0: warning: PP92: 'a'",
                  "In file included from main.iec:7:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              run());

    write("main.iec", "{#define H 'a.iec'}\n{#include $\n H}\n{#define P(x) @x}\n{#include P( $\n a.iec $\n )}\n"
                      "{#include H}\n");
    EXPECT_EQ((Lines{
                  "In file included from main.iec:3:",
                  "a.iec:1.0: warning: PP92: 'a'",
                  "In file included from main.iec:7:",
                  "a.iec:1.0: warning: PP92: 'a'",
                  "In file included from main.iec:8:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              run());

    write("main.iec", "x\n{#include $\n  $\n  'nope.iec'}\n");
    EXPECT_EQ((Lines{"main.iec:2.0: error: PP11: No such file or directory; 'nope.iec'"}), run({}, 1));
}

// A name that {#line} gives the includer is shown as it is, even one that
// looks like a placeholder; only an -include file is "<command line>".
TEST_F(IncludeStackTest, LineNameIsShownAsWritten) {
    write("a.iec", "{#warning a}\n");
    write("main.iec", "{#line 10 '<unknown location>'}\n{#include 'a.iec'}\n");
    EXPECT_EQ((Lines{
                  "In file included from <unknown location>:10:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              run());
    write("main.iec", "{#line 10 '<command line>'}\n{#include 'a.iec'}\n");
    EXPECT_EQ((Lines{
                  "In file included from <command line>:10:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              run());
}

// The library reading a top-level file directly (expand(filepath) with its
// default arguments, as tests/core/test_blank_lines.cpp does): the file has
// no include point, so its own diagnostics get no lines, and what it
// includes is shown as included from it.
TEST_F(IncludeStackTest, LibraryTopLevelFile) {
    write("top.iec", "{#warning t}\n{#include 'a.iec'}\n");
    write("a.iec", "{#warning a}\n");
    for (bool main_input : {false, true}) {
        Env env = setup();
        std::vector<Token> ots;
        expand("top.iec", Loader::LoadType::INCLUDE, ots, env, std::nullopt, main_input);
        EXPECT_EQ((Lines{
                      "top.iec:1.0: warning: PP92: 't'",
                      "In file included from top.iec:2:",
                      "a.iec:1.0: warning: PP92: 'a'",
                  }),
                  messages())
            << "main_input " << main_input;
    }
}

// The library: Issue::show_include_stack_ (on by default), and an include
// from the string input is shown at its "<unknown location>" line, the name
// and line its own diagnostics have.
TEST_F(IncludeStackTest, Library) {
    write("a.iec", "{#warning a}\n");
    Env env = setup();
    (void)pp("{#include 'a.iec'}\nx\n", env);
    EXPECT_EQ((Lines{
                  "In file included from <unknown location>:1:",
                  "a.iec:1.0: warning: PP92: 'a'",
              }),
              messages());

    Issue::show_include_stack_ = false;
    Env env2 = setup();
    (void)pp("{#include 'a.iec'}\n", env2);
    EXPECT_EQ((Lines{"a.iec:1.0: warning: PP92: 'a'"}), messages());
}

// The lines are not counted: no change to the exit code, and -Werror
// promotes the diagnostic only.
TEST_F(IncludeStackTest, NotCountedNorPromoted) {
    write("main.iec", "{#include 'a.iec'}\n");
    write("a.iec", "{#warning a}\n");
    JieppOptions werror;
    werror.werror = true;
    EXPECT_EQ((Lines{
                  "In file included from main.iec:1:",
                  "a.iec:1.0: error: PP92: 'a'",
              }),
              run(werror, 1));
    EXPECT_EQ(1, Issue::error_count_);
}

#endif // JIEPP_SANDBOX

// The option parses in either spelling.
TEST_F(IncludeStackTest, OptionSpellings) {
    for (const char* spelling : {"--no-include-stack", "--no_include_stack"}) {
        const char* argv[] = {"jiepp", spelling, "main.iec"};
        const JieppOptions opts = parse_args(3, const_cast<char**>(argv));
        EXPECT_TRUE(opts.no_include_stack) << spelling;
    }
    const char* argv[] = {"jiepp", "main.iec"};
    EXPECT_FALSE(parse_args(2, const_cast<char**>(argv)).no_include_stack);
}
