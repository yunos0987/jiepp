#include "test_helper.hpp"
#include <algorithm>
#include <chrono>

inline fs::path jiepp_exe_path() {
#ifdef JIEPP_EXE_PATH
    return fs::path(JIEPP_EXE_PATH);
#else
    return fs::path(); // fallback: empty
#endif
}

class SmokeTest : public JieppTest {
protected:
    fs::path exe_ = jiepp_exe_path();
    fs::path tmp_dir_;

    void SetUp() override {
        if (exe_.empty() || !fs::exists(exe_)) {
            GTEST_SKIP() << "jiepp executable not found: " << exe_;
        }
        tmp_dir_ = fs::temp_directory_path() / "jiepp_smoke_test";
        fs::create_directories(tmp_dir_);
    }

    void TearDown() override {
        if (!tmp_dir_.empty() && fs::exists(tmp_dir_)) {
            std::error_code ec;
            fs::remove_all(tmp_dir_, ec);
        }
    }

    static std::string read_file(const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(f), {}};
    }

    static void write_file(const fs::path& p, const std::string& content) {
        std::ofstream f(p, std::ios::binary);
        f << content;
    }

    // Shared input/output shape for the deeply-nested function-macro tests
    // below: I(I(...I(0)...)) at the given depth, inside a program body.
    static std::string nested_macro_source(int depth) {
        std::string src = "{#define I(a) (a)+1}\nprogram Main\n{st}\n";
        for (int i = 0; i < depth; ++i)
            src += "I(";
        src += "0";
        for (int i = 0; i < depth; ++i)
            src += ")";
        src += ";\n{end}\nend_program\n";
        return src;
    }

    static std::string nested_macro_expected_line(int depth) {
        std::string expected(static_cast<std::size_t>(depth), '(');
        expected += "0";
        for (int i = 0; i < depth; ++i)
            expected += ")+1";
        expected += ";";
        return expected;
    }

    static bool output_has_line(const std::string& out, const std::string& expected) {
        std::istringstream lines(out);
        std::string line;
        while (std::getline(lines, line)) {
            if (line == expected)
                return true;
        }
        return false;
    }

    struct RunResult {
        int exit_code;
        std::string out;
        std::string err;
    };

    RunResult run(const std::string& args,
                  const std::string& stdin_file = "") {
        fs::path out_f = tmp_dir_ / "stdout.txt";
        fs::path err_f = tmp_dir_ / "stderr.txt";

        // Build command: exe args [<stdin] >stdout 2>stderr
        std::string cmd;
#ifdef _WIN32
        // On Windows cmd.exe, wrap entire command in outer quotes
        // when the executable path is quoted.
        cmd = "\"\"" + exe_.generic_string() + "\" " + args;
        if (!stdin_file.empty())
            cmd += " <\"" + stdin_file + "\"";
        cmd += " >\"" + out_f.generic_string() + "\"";
        cmd += " 2>\"" + err_f.generic_string() + "\"";
        cmd += "\"";
#else
        cmd = "'" + exe_.generic_string() + "' " + args;
        if (!stdin_file.empty())
            cmd += " <'" + stdin_file + "'";
        cmd += " >'" + out_f.generic_string() + "'";
        cmd += " 2>'" + err_f.generic_string() + "'";
#endif

        int rc = std::system(cmd.c_str());
#ifndef _WIN32
        if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
#endif
        return {rc, read_file(out_f), read_file(err_f)};
    }
};

// ---- 1. --help ----

TEST_F(SmokeTest, Help) {
    auto r = run("--help");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("Usage"), std::string::npos)
        << "stdout: " << r.out;
}

TEST_F(SmokeTest, HelpListsShortAlias) {
    auto help = run("--help");
    EXPECT_NE(help.out.find("--help / -h"), std::string::npos)
        << "stdout: " << help.out;

    auto short_help = run("-h");
    EXPECT_EQ(short_help.exit_code, 0);
    EXPECT_EQ(short_help.out, help.out);
}

// ---- 2. --version ----

TEST_F(SmokeTest, Version) {
    auto r = run("--version");
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_NE(r.out.find("jiepp"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 3. Basic preprocessing ----

TEST_F(SmokeTest, BasicProcess) {
    fs::path input = tmp_dir_ / "basic.iec";
    write_file(input, "{#define X 42}\nresult := X;\n");

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("42"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 4. File not found ----

TEST_F(SmokeTest, FileNotFound) {
    auto r = run("\"" + (tmp_dir_ / "nonexistent.iec").generic_string() + "\"");
    EXPECT_NE(r.exit_code, 0);
}

// ---- 5. stdin pipe ----

TEST_F(SmokeTest, StdinPipe) {
    fs::path input = tmp_dir_ / "stdin_input.iec";
    write_file(input, "{#define Y 99}\nval := Y;\n");

    auto r = run("-", input.generic_string());
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("99"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 5a. stdin is read in binary mode (B2) ----
//
// The Windows CRT opens stdin in text mode, where byte 0x1A (Ctrl+Z) ends the
// input, so `printf 'aa\x1abb\n' | jiepp -P` printed only "aa" and exited 0
// while the same bytes passed as a file were read completely. main() now
// switches stdin to binary mode, as it already does for stdout.

TEST_F(SmokeTest, StdinCtrlZIsNotEndOfInput) {
    fs::path input = tmp_dir_ / "stdin_ctrlz.iec";
    write_file(input, std::string("aa\x1a" "bb\n"));

    auto r = run("-P", input.generic_string());
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_EQ(r.out, std::string("aa\x1a" "bb\n")) << "stdout: " << r.out;
}

TEST_F(SmokeTest, StdinCrlfMatchesFileInput_Guard) {
    // Guard (passes before the change too): CRLF input gives the same stdout
    // from stdin and from a file, including __LINE__, so binary stdin does not
    // change the line accounting.
    fs::path input = tmp_dir_ / "stdin_crlf.iec";
    write_file(input, "a\r\n__LINE__\r\n{#define X 1}\r\nX __LINE__\r\nb\r\n");

    auto from_file = run("-P \"" + input.generic_string() + "\"");
    auto from_stdin = run("-P", input.generic_string());
    EXPECT_EQ(from_file.exit_code, 0) << "stderr: " << from_file.err;
    EXPECT_EQ(from_stdin.exit_code, 0) << "stderr: " << from_stdin.err;
    EXPECT_EQ(from_file.out, from_stdin.out);
    EXPECT_NE(from_stdin.out.find("4"), std::string::npos) << "stdout: " << from_stdin.out;
}

// ---- 5b. stdin display name is "<stdin>", never the literal "-" ----
//
// H: gcc/clang both report "<stdin>" for __FILE__/line markers when the
// input is stdin, whether that is spelled as an explicit "-" argument or as
// no input argument at all (confirmed with GNU cpp 5.3.0 and clang). Cover
// both spellings so a regression that special-cases only one of them is
// still caught.

TEST_F(SmokeTest, StdinDisplayNameIsStdinNotDash) {
    fs::path input = tmp_dir_ / "stdin_dispname.iec";
    write_file(input, "name := __FILE__;\n");

    auto r = run("- -P", input.generic_string());
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("'<stdin>'"), std::string::npos)
        << "stdout: " << r.out;
    EXPECT_EQ(r.out.find("'-'"), std::string::npos)
        << "stdout: " << r.out;
}

TEST_F(SmokeTest, StdinDisplayNameIsStdinNotDashWithNoInputArg) {
    fs::path input = tmp_dir_ / "stdin_dispname_noarg.iec";
    write_file(input, "name := __FILE__;\n");

    auto r = run("-P", input.generic_string());
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("'<stdin>'"), std::string::npos)
        << "stdout: " << r.out;
    EXPECT_EQ(r.out.find("'-'"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 6. -D option ----

TEST_F(SmokeTest, DOption) {
    fs::path input = tmp_dir_ / "d_opt.iec";
    write_file(input, "val := MYVAL;\n");

    auto r = run("-D MYVAL=42 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("42"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 7. -I option (syspath + sinclude) ----

TEST_F(SmokeTest, IOption) {
    // Create library directory and header
    fs::path lib_dir = tmp_dir_ / "mylib";
    fs::create_directories(lib_dir);
    write_file(lib_dir / "lib.iec", "{#define LIB_LOADED 1}\n");

    fs::path input = tmp_dir_ / "i_opt.iec";
    write_file(input, "{#sinclude 'lib.iec'}\nloaded := LIB_LOADED;\n");

    auto r = run("-I \"" + lib_dir.generic_string() + "\" \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("loaded := 1"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 8. -o option ----

TEST_F(SmokeTest, OOption) {
    fs::path input = tmp_dir_ / "o_opt.iec";
    write_file(input, "{#define Z 7}\nout := Z;\n");
    fs::path output = tmp_dir_ / "o_opt_result.iec";

    auto r = run("-o \"" + output.generic_string() + "\" \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_TRUE(fs::exists(output)) << "output file not created";
    std::string content = read_file(output);
    EXPECT_NE(content.find("7"), std::string::npos)
        << "output content: " << content;
}

// ---- 9b. Option-parse error diagnostics (F4) ----

TEST_F(SmokeTest, OptionErrorPrintsNoUnknownErrorLine) {
    // F4: an Issue thrown during option parsing (before jiepp_command() even
    // starts, e.g. --max-blank-lines with a negative value) must not
    // additionally print main()'s generic "PP01: Unknown error" fallback
    // line on top of the specific diagnostic Issue::happen() already
    // printed -- exactly one line on stderr, containing the real code
    // (PP71/INVALID_OPTION_VALUE), never PP01.
    fs::path input = tmp_dir_ / "opt_err.iec";
    write_file(input, "x := 1;\n");

    auto r = run("--max-blank-lines -3 \"" + input.generic_string() + "\"");
    EXPECT_NE(r.exit_code, 0);

    std::size_t newline_count = std::count(r.err.begin(), r.err.end(), '\n');
    EXPECT_EQ(1u, newline_count) << "stderr should contain exactly one line: " << r.err;
    EXPECT_NE(r.err.find("PP71"), std::string::npos) << "stderr: " << r.err;
    EXPECT_EQ(r.err.find("PP01"), std::string::npos)
        << "stderr must not contain a spurious PP01 fallback line: " << r.err;
}

// ---- 8b. -o - means stdout ----

TEST_F(SmokeTest, OutputDashMeansStdout) {
    // gcc/clang convention: "-o -" writes to stdout, not to a file literally
    // named "-". End-to-end check via the real exe (complements the
    // in-process JieppCommandTest.OutputDashMeansStdout).
    fs::path input = tmp_dir_ / "o_dash.iec";
    write_file(input, "{#define Z 7}\nout := Z;\n");
    fs::path dash_marker = tmp_dir_ / "-";
    std::error_code ec;
    fs::remove(dash_marker, ec);

    // Run with cwd effectively at tmp_dir_ by using an absolute input path
    // and relative "-" for -o; std::system() runs with the test process's
    // own cwd, so check for a stray "-" there too.
    fs::path cwd_dash_marker = fs::current_path() / "-";
    fs::remove(cwd_dash_marker, ec);

    auto r = run("-o - \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("7"), std::string::npos)
        << "stdout: " << r.out;
    EXPECT_FALSE(fs::exists(dash_marker));
    EXPECT_FALSE(fs::exists(cwd_dash_marker));
}

// ---- 7b. {#syspath} from stdin falls back to CWD ----

TEST_F(SmokeTest, SyspathFromStdinUsesCwd) {
    // U1: when reading from stdin there is no "containing file" on disk, so
    // a relative {#syspath} operand falls back to the process's current
    // working directory (same as {#syspath} with no current file at all).
    fs::path lib_dir = tmp_dir_ / "stdin_syspath_lib";
    fs::create_directories(lib_dir);
    write_file(lib_dir / "lib.iec", "{#define STDIN_LIB_LOADED 1}\n");

    fs::path input = tmp_dir_ / "stdin_syspath.iec";
    write_file(input, "{#syspath 'stdin_syspath_lib'}\n{#sinclude 'lib.iec'}\nloaded := STDIN_LIB_LOADED;\n");

    RunResult r;
    {
        CwdGuard cwd_guard(tmp_dir_);
        r = run("-", input.generic_string());
    }

    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("loaded := 1"), std::string::npos)
        << "stdout: " << r.out;
}

// ---- 7c. -I stays CWD-relative, not relative to the input file's dir ----

TEST_F(SmokeTest, IOptionRelativeToCwdNotInputDir) {
    // -I is a CLI option (unlike {#syspath}), so it must keep resolving
    // relative to the process's CWD even though the input file lives in a
    // different directory that has its own decoy "mylib".
    fs::path sub_dir = tmp_dir_ / "iopt_sub";
    fs::create_directories(sub_dir);
    write_file(sub_dir / "input.iec", "{#sinclude 'lib.iec'}\nloaded := LIB_LOADED;\n");

    // Decoy: same relative name "mylib/lib.iec", but under the input file's
    // own directory -- must NOT be picked up.
    fs::path decoy_lib_dir = sub_dir / "mylib";
    fs::create_directories(decoy_lib_dir);
    write_file(decoy_lib_dir / "lib.iec", "{#define LIB_LOADED 99}\n");

    // Real target: "mylib/lib.iec" relative to CWD (tmp_dir_).
    fs::path real_lib_dir = tmp_dir_ / "mylib";
    fs::create_directories(real_lib_dir);
    write_file(real_lib_dir / "lib.iec", "{#define LIB_LOADED 1}\n");

    RunResult r;
    {
        CwdGuard cwd_guard(tmp_dir_);
        r = run("-I mylib \"iopt_sub/input.iec\"");
    }

    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("loaded := 1"), std::string::npos)
        << "stdout: " << r.out;
    EXPECT_EQ(r.out.find("loaded := 99"), std::string::npos)
        << "-I picked up the decoy under the input file's own directory; stdout: " << r.out;
}

// ---- 9. Windows stdout binary mode (B15) ----

TEST_F(SmokeTest, StdoutMatchesOutputFileBytes) {
    // B15: on Windows, redirected stdout must be byte-identical to -o FILE
    // output for the same input (both LF, no CRLF translation on stdout).
    fs::path input = tmp_dir_ / "stdout_bytes.iec";
    write_file(input, "x := 1;\ny := 2;\n");
    fs::path output = tmp_dir_ / "stdout_bytes_o.piec";

    auto r_stdout = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r_stdout.exit_code, 0) << "stderr: " << r_stdout.err;

    auto r_file = run("-o \"" + output.generic_string() + "\" \"" + input.generic_string() + "\"");
    EXPECT_EQ(r_file.exit_code, 0) << "stderr: " << r_file.err;

    std::string file_content = read_file(output);
    EXPECT_EQ(file_content, r_stdout.out)
        << "redirected stdout and -o FILE output must be byte-identical";
#ifdef _WIN32
    EXPECT_EQ(std::string::npos, r_stdout.out.find("\r\n"))
        << "stdout must not contain CRLF on Windows";
#endif
}

// ---- 10. Deeply nested function-macro expansion (U1/C1-6) ----
//
// n=100 nesting of a 1-arg function macro: I(I(...I(0)...)). Regression
// coverage for the hide-set sharing optimization in hs_add_all()/hsadd()
// (U1): every level's hide set differs only by the newly-added macro name,
// so a bug that shared a hide set across the wrong tokens would corrupt the
// output while still terminating (unlike a stack overflow, which a much
// larger n would risk instead -- see AGENTS.md on Debug-build recursion
// limits, so this stays well under that).

TEST_F(SmokeTest, DeeplyNestedFunctionMacroExpansion) {
    // Matches bench/cases/iterate_fmacros/iterate_fmacros.py's input
    // shape (n=100, well under the default stack's Debug-build recursion
    // ceiling -- about 1240 levels with the 8 MiB default stack -- so this
    // passes in both Debug and Release).
    fs::path input = tmp_dir_ / "nested_fmacro.iec";
    constexpr int kDepth = 100;

    write_file(input, nested_macro_source(kDepth));

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;

    std::string expected = nested_macro_expected_line(kDepth);
    EXPECT_TRUE(output_has_line(r.out, expected))
        << "expected line: " << expected << "\nstdout: " << r.out;
}

// ---- 11. Default stack size (8 MiB) and the PP63 stack guard ----
//
// n=1000 nesting exceeds the pre-U3 default Debug-build recursion ceiling
// (~161 levels with the old 1 MiB stack), so with the 8 MiB default stack
// this must complete instead of crashing.

TEST_F(SmokeTest, DefaultStackHandlesDeepNesting) {
    constexpr int kDepth = 1000;
    fs::path input = tmp_dir_ / "nested_fmacro_1000.iec";
    write_file(input, nested_macro_source(kDepth));

    // --max-expansion-depth raised well above kDepth: only the stack size is
    // under test here, not the expansion depth limit (see the next test).
    auto r = run("--max-expansion-depth 2000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_TRUE(output_has_line(r.out, nested_macro_expected_line(kDepth)))
        << "expected line not found\nstdout: " << r.out;
}

// n=300 with the default --max-expansion-depth (256): before U3, a Debug
// build crashed around 161 levels -- well short of the depth-256 check --
// instead of ever reaching PP60. With the 8 MiB default stack, PP60 fires as
// designed and the process exits cleanly (no crash).
TEST_F(SmokeTest, DefaultDepthLimitReportsPP60NotCrash) {
    constexpr int kDepth = 300;
    fs::path input = tmp_dir_ / "nested_fmacro_300.iec";
    write_file(input, nested_macro_source(kDepth));

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP60"), std::string::npos) << "stderr: " << r.err;
}

// A small explicit --recursion-limit exhausts the stack well before the
// (raised) expansion-depth limit, so PP63 must fire instead of a crash, and
// instead of PP60 (which the raised depth limit never reaches).
TEST_F(SmokeTest, SmallStackReportsPP63NotCrash) {
    constexpr int kDepth = 1000;
    fs::path input = tmp_dir_ / "nested_fmacro_small_stack.iec";
    write_file(input, nested_macro_source(kDepth));

    auto r = run("--recursion-limit 128 --max-expansion-depth 2000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP63"), std::string::npos) << "stderr: " << r.err;
    EXPECT_EQ(r.err.find("PP60"), std::string::npos) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("program Main"), std::string::npos) << "stdout: " << r.out;
}

// PP63 is SEVERE, so {#ignore PP60} (which lets deep nesting run past PP60
// without stopping) and even {#ignore PP63} itself cannot turn a small
// stack into a crash: PP63 still fires and still stops the run.
TEST_F(SmokeTest, IgnoredPP60ReportsPP63NotCrash) {
    constexpr int kDepth = 1000;

    auto with_prefix = [](const std::string& prefix) {
        return prefix + nested_macro_source(kDepth);
    };

    // Control: without {#ignore PP60}, the lowered depth limit (16) reports
    // PP60 well before the small stack (128 * 8 KiB) would be exhausted.
    {
        fs::path input = tmp_dir_ / "ignored_pp60_control.iec";
        write_file(input, with_prefix("{#max_expansion_depth 16}\n"));
        auto r = run("--recursion-limit 128 \"" + input.generic_string() + "\"");
        EXPECT_EQ(r.exit_code, 1);
        EXPECT_NE(r.err.find("PP60"), std::string::npos) << "stderr: " << r.err;
    }

    // With {#ignore PP60}, nesting continues past the depth limit until the
    // small stack is exhausted, reporting PP63 instead of crashing.
    {
        fs::path input = tmp_dir_ / "ignored_pp60.iec";
        write_file(input, with_prefix("{#max_expansion_depth 16}\n{#ignore PP60}\n"));
        auto r = run("--recursion-limit 128 \"" + input.generic_string() + "\"");
        EXPECT_EQ(r.exit_code, 1);
        EXPECT_NE(r.err.find("PP63"), std::string::npos) << "stderr: " << r.err;
        EXPECT_EQ(r.err.find("PP60"), std::string::npos) << "stderr: " << r.err;
    }

    // {#ignore PP63} is accepted (any well-formed PPnn is) but has no
    // effect: PP63 is SEVERE, so it still fires and still stops the run.
    {
        fs::path input = tmp_dir_ / "ignored_pp60_pp63.iec";
        write_file(input, with_prefix("{#max_expansion_depth 16}\n{#ignore PP60}\n{#ignore PP63}\n"));
        auto r = run("--recursion-limit 128 \"" + input.generic_string() + "\"");
        EXPECT_EQ(r.exit_code, 1);
        EXPECT_NE(r.err.find("PP63"), std::string::npos) << "stderr: " << r.err;
    }
}

// requested_stack_bytes() floors --recursion-limit at 1 MiB on every OS, not
// only Windows: --recursion-limit 16 alone requests 16 x 8 KiB = 128 KiB,
// smaller than the PP63 guard's own 256 KiB headroom (util/stack_guard.hpp).
// Before this floor existed, that made --recursion-limit 16 on POSIX report
// PP63 immediately at expansion depth 0, before any macro nesting at all.
// With the floor, 128 KiB is silently raised to 1 MiB and a moderately
// nested program completes normally, on every OS.
TEST_F(SmokeTest, SmallRecursionLimitHasOneMiBFloor) {
    constexpr int kDepth = 100;
    fs::path input = tmp_dir_ / "nested_fmacro_small_recursion_limit.iec";
    write_file(input, nested_macro_source(kDepth));

    auto r = run("--recursion-limit 16 --max-expansion-depth 1000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_TRUE(output_has_line(r.out, nested_macro_expected_line(kDepth)))
        << "expected line not found\nstdout: " << r.out;
}

// ---- R14/R15: PP34 recovery (task_slug argcount-error-output) through the
// CLI -- a function-macro call with the wrong argument count is not
// expanded; the call becomes just the macro name, and the dropped argument
// list's own directive side effects (collected once during argument
// collection, independently of the call's own fate) still take effect and
// are still echoed under -dD.

TEST_F(SmokeTest, ArgCountMismatchDoesNotExpandUnderDD) {
    fs::path input = tmp_dir_ / "argcount_dd.iec";
    write_file(input, "{#define P(x) x}P(1,{#define Q 9}2) Q");

    auto r = run("-dD -P \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(r.out, "{#define P(x) x}{#define Q 9}P 9");
    EXPECT_NE(r.err.find("PP34"), std::string::npos) << "stderr: " << r.err;
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 1)
        << "stderr should have exactly one PP34 line: " << r.err;
}

TEST_F(SmokeTest, ArgCountMismatchContinuesAndExitsNonZero) {
    fs::path input = tmp_dir_ / "argcount_continue.iec";
    write_file(input, "{#define P(x) x+1}\nX P(2,3) Y\nZ");

    auto r = run("-P \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_TRUE(output_has_line(r.out, "X P Y")) << "stdout: " << r.out;
    EXPECT_TRUE(output_has_line(r.out, "Z")) << "stdout: " << r.out;
    EXPECT_NE(r.err.find("PP34"), std::string::npos) << "stderr: " << r.err;
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 1)
        << "stderr should have exactly one PP34 line: " << r.err;
}

// ---- PP64: the expansion-step cap through the CLI ----

namespace {

// Doubling bomb: D<n> expands to 2^n tokens. Preceded by an ordinary line so
// the partial output is observable.
std::string step_bomb_source(int n, const std::string& prefix = "") {
    std::string s = prefix + "before_bomb\n{#define D0 1}\n";
    for (int i = 1; i <= n; ++i)
        s += "{#define D" + std::to_string(i) + " D" + std::to_string(i - 1) +
             " + D" + std::to_string(i - 1) + "}\n";
    s += "D" + std::to_string(n) + "\n";
    return s;
}

} // namespace

TEST_F(SmokeTest, HelpListsMaxExpansionSteps) {
    auto r = run("--help");
    EXPECT_NE(r.out.find("  --max-expansion-steps N  Maximum macro expansion work in steps "
                         "(default: 16777216; 0 = no limit)\n"),
              std::string::npos)
        << "stdout: " << r.out;
}

TEST_F(SmokeTest, ExpansionStepsLimitStopsWithPP64AndPartialOutput) {
    fs::path input = tmp_dir_ / "steps_bomb.iec";
    write_file(input, step_bomb_source(24));

    auto r = run("--max-expansion-steps 10000 -P \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP64"), std::string::npos) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("limit 10000; use --max-expansion-steps N to raise it (0 = no limit)"),
              std::string::npos) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("before_bomb"), std::string::npos) << "stdout: " << r.out;
}

TEST_F(SmokeTest, ExpansionStepsSilentStillExitsOne) {
    fs::path input = tmp_dir_ / "steps_bomb_silent.iec";
    write_file(input, step_bomb_source(24));

    auto r = run("--silent --max-expansion-steps 10000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_TRUE(r.err.empty()) << "stderr: " << r.err;
}

TEST_F(SmokeTest, ExpansionStepsStopsEvenAfterOtherErrors) {
    // Continue mode keeps going past PP34 but PP64 (SEVERE) still stops it.
    fs::path input = tmp_dir_ / "steps_bomb_continue.iec";
    write_file(input, step_bomb_source(24, "{#define P(x) x}\nP(1,2)\n"));

    auto r = run("--max-expansion-steps 10000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP34"), std::string::npos) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP64"), std::string::npos) << "stderr: " << r.err;
}

#ifndef JIEPP_SANDBOX
TEST_F(SmokeTest, ExpansionStepsIgnoreCannotSuppressPP64) {
    fs::path input = tmp_dir_ / "steps_bomb_ignore.iec";
    write_file(input, step_bomb_source(24, "{#ignore PP64}\n"));

    auto r = run("--max-expansion-steps 10000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP64"), std::string::npos) << "stderr: " << r.err;
}
#endif

TEST_F(SmokeTest, ExpansionStepsStopDoesNotCreateOrTouchOutputFile) {
    fs::path input = tmp_dir_ / "steps_bomb_o.iec";
    write_file(input, step_bomb_source(24));

    fs::path fresh = tmp_dir_ / "fresh_out.iec";
    auto r = run("--max-expansion-steps 10000 -o \"" + fresh.generic_string() + "\" \"" +
                 input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP64"), std::string::npos) << "stderr: " << r.err;
    EXPECT_FALSE(fs::exists(fresh));
    EXPECT_TRUE(r.out.empty()) << "stdout: " << r.out;

    fs::path existing = tmp_dir_ / "existing_out.iec";
    write_file(existing, "old content\n");
    auto r2 = run("--max-expansion-steps 10000 -o \"" + existing.generic_string() + "\" \"" +
                  input.generic_string() + "\"");
    EXPECT_EQ(r2.exit_code, 1);
    EXPECT_EQ(read_file(existing), "old content\n");
}

TEST_F(SmokeTest, ExpansionStepsZeroMeansNoLimit) {
    fs::path input = tmp_dir_ / "steps_no_limit.iec";
    write_file(input, step_bomb_source(16));

    // 2^16 tokens: over a 10000-step limit, fine with 0.
    auto stopped = run("--max-expansion-steps 10000 \"" + input.generic_string() + "\"");
    EXPECT_EQ(stopped.exit_code, 1);
    auto r = run("--max-expansion-steps 0 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
}

#ifdef NDEBUG
// C8: with the default limit (16777216 steps) a 2^30 doubling bomb stops
// quickly with PP64 instead of exhausting memory. Release only: a Debug
// build is several times slower.
TEST_F(SmokeTest, DefaultStepLimitStopsDoublingBombQuickly) {
    fs::path input = tmp_dir_ / "steps_bomb_default.iec";
    write_file(input, step_bomb_source(30));

    const auto t0 = std::chrono::steady_clock::now();
    auto r = run("\"" + input.generic_string() + "\"");
    const auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("PP64"), std::string::npos) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("limit 16777216;"), std::string::npos) << "stderr: " << r.err;
    EXPECT_LE(secs, 10.0) << "took " << secs << " s";
}
#endif

// ---- A2 / S-02: nested pragma brace diagnostics through the CLI ----

namespace {

std::size_t count_lines_with(const std::string& s, const std::string& needle) {
    std::istringstream lines(s);
    std::string line;
    std::size_t n = 0;
    while (std::getline(lines, line))
        if (line.find(needle) != std::string::npos)
            ++n;
    return n;
}

} // namespace

// '{' x 1000: each pragma body is re-lexed until PP60 stops the recursion at
// depth 256. Before the fix every level reported every remaining '{' again
// (224104 PP22 lines, 44 MB of stderr; 14.0-15.8 s measured on Release on
// the development machine). Now the source
// lex and the first re-lex report the raw braces (999 + 998) and every
// level reports its own unclosed pragma once (257 levels): 2254 lines.
TEST_F(SmokeTest, NestedBracePragmaDiagnosticsAreLinear) {
    fs::path input = tmp_dir_ / "nested_brace_1000.iec";
    write_file(input, std::string(1000, '{') + "x\n");

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(count_lines_with(r.err, ": PP22:"), 2254u);
    EXPECT_EQ(count_lines_with(r.err, ": PP60:"), 1u);
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 2255);
}

// Guard (design review H2): the only diagnostic of this input is a PP22 for
// an unclosed "${" pragma found while re-lexing a closed nested pragma in a
// rescanned macro argument; the run must still fail.
TEST_F(SmokeTest, RescannedPragmaUnclosedEscapedOpenerStillFails) {
    fs::path input = tmp_dir_ / "rescanned_escaped_opener.iec";
    write_file(input, "{#define N ${z$}}\n"
                      "{#define QM ${B $$N$}}\n"
                      "{#define PM ${ QM $}}\n"
                      "{#define F(x) x}\n"
                      "F(PM)\n");

    auto r = run("-P --pp-output-pragma-style standard \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(r.out, "{{B ${z}}} }\n");
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find(": PP22: Invalid syntax for pragma; '{z'"), std::string::npos)
        << "stderr: " << r.err;
}

// Guard: a pragma whose text was rewritten by expansion is re-lexed with
// every diagnostic when rescanned; the raw '{' it reports is the only error.
TEST_F(SmokeTest, RewrittenPragmaRescanStillFails) {
    fs::path input = tmp_dir_ / "rewritten_pragma_rescan.iec";
    write_file(input, "{#define F(x) x}\n"
                      "{#define E}\n"
                      "{#define B ${b$}}\n"
                      "{#define S(x) @x}\n"
                      "{#define S2(x) S(x)}\n"
                      "{#define M() ${q S2(B)$}}\n"
                      "{#define IN ${ M E () $}}\n"
                      "{#define OUT ${ F E (IN) $}}\n"
                      "F(OUT)\n");

    auto r = run("-P --pp-output-pragma-style standard \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_EQ(r.out, "{{{q '{b}'} } }\n");
    EXPECT_EQ(count_lines_with(r.err, ": PP22: Invalid syntax for pragma; '{'"), 1u)
        << "stderr: " << r.err;
    EXPECT_EQ(count_lines_with(r.err, ": PP29:"), 2u) << "stderr: " << r.err;
}

#ifndef JIEPP_SANDBOX
// Guard (design review H1): PP23 for an unclosed directive found by a deeper
// re-lex is always reported ({#ignore} is restricted in sandbox builds).
TEST_F(SmokeTest, UnclosedDirectiveInDeepRelexKeepsPP23) {
    fs::path input = tmp_dir_ / "deep_relex_pp23.iec";
    write_file(input, "{#ignore PP22}\n{ { {#define Q 1 }\nQ\n");

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_EQ(count_lines_with(r.err, ": PP23:"), 1u) << "stderr: " << r.err;
    EXPECT_NE(r.err.find(": PP23: Invalid preprocessor syntax; '{#define Q 1 '"),
              std::string::npos) << "stderr: " << r.err;
    // The source lex runs before {#ignore PP22} takes effect.
    EXPECT_EQ(count_lines_with(r.err, ": PP22:"), 2u) << "stderr: " << r.err;
}
#endif

// ---- F2: deeply nested __VA_OPT__ reports PP63 instead of crashing ----
//
// Nested __VA_OPT__ is PP37, but with PP37 ignored or in continue mode its
// content is still substituted, recursing subst() -> subst() once per level
// without passing through expand()'s stack check. 5000 levels exhaust a
// 1 MiB stack (--recursion-limit 128) in both Debug and Release on every OS
// (768 KiB / 5000 is about 157 bytes per level); before the fix the process
// crashed. --max-expansion-steps 0 keeps PP64 (the k(k-1)/2 PP37 step
// charges) from stopping the run first.

namespace {

std::string nested_va_opt_source(const std::string& prefix, int k) {
    std::string src = prefix + "{#define M(...) ";
    for (int i = 0; i < k; ++i)
        src += "__VA_OPT__(";
    src += "x";
    src += std::string(static_cast<std::size_t>(k), ')');
    src += "}M(1) after_m\n";
    return src;
}

} // namespace

#ifndef JIEPP_SANDBOX
TEST_F(SmokeTest, NestedVaOptIgnoredPP37ReportsPP63NotCrash) {
    fs::path input = tmp_dir_ / "nested_va_opt_ignored.iec";
    write_file(input, nested_va_opt_source("{#ignore PP37}", 5000));

    auto r = run("--recursion-limit 128 --max-expansion-steps 0 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err.substr(0, 2000);
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 1) << "stderr: " << r.err.substr(0, 2000);
    // The depth figure counts expand() nesting only, so it is not pinned.
    EXPECT_NE(r.err.find("PP63: Stack nearly exhausted; 'expansion depth "), std::string::npos)
        << "stderr: " << r.err.substr(0, 2000);
    EXPECT_NE(r.err.find("retry with --recursion-limit greater than 128'"), std::string::npos)
        << "stderr: " << r.err.substr(0, 2000);
    EXPECT_EQ(r.err.find("PP37"), std::string::npos) << "stderr: " << r.err.substr(0, 2000);
    // The run stopped inside M(1): nothing after it was output.
    EXPECT_EQ(r.out.find("after_m"), std::string::npos);
}
#endif

// Same input without {#ignore} (restricted in sandbox builds): the CLI's
// continue mode counts each PP37 and keeps substituting, so a sandboxed web
// deployment crashed too. --silent keeps the PP37 lines (millions of them
// before the stack runs low) off stderr. The RobustnessTest* fixture name
// matches the sandbox test presets' filter ^(Sandbox|RobustnessTest), so
// this runs in both normal and sandbox builds.
// If an error limit is added, raise it here well above the PP37 count so
// the PP63 path is still exercised (stopping at the limit would also give
// rc=1, indistinguishable under --silent).
class RobustnessTestSmoke : public SmokeTest {};

TEST_F(RobustnessTestSmoke, NestedVaOptContinueModeReportsPP63NotCrash) {
    fs::path input = tmp_dir_ / "nested_va_opt_continue.iec";
    write_file(input, nested_va_opt_source("", 5000));

    auto r = run("--silent --recursion-limit 128 --max-expansion-steps 0 \"" +
                 input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_TRUE(r.err.empty()) << "stderr: " << r.err.substr(0, 2000);
    EXPECT_EQ(r.out.find("after_m"), std::string::npos);
}

// __nosplit (SPEC 12.11) nested 1000 deep: each level is one expand() call
// (invoke() expands the joined arguments), so the stack guard reports PP63
// instead of the process crashing. Nothing is reported before PP63, so no
// --silent: the single stderr line must be the PP63 one (not PP60/PP64 or an
// option error, which would also give rc=1). --max-expansion-depth and
// --max-expansion-steps are CLI options, accepted in sandbox builds too.
// PP63 fires at a depth of about 170 (Debug) to 400 (Release) here; every
// level re-collects the rest of the input, so 1000 levels keep the margin
// without the quadratic cost of a deeper input.
TEST_F(RobustnessTestSmoke, NestedNosplitReportsPP63NotCrash) {
    const int k = 1000;
    std::string src;
    for (int i = 0; i < k; ++i)
        src += "__nosplit(";
    src += "x";
    src += std::string(static_cast<std::size_t>(k), ')');
    src += " after_n\n";
    fs::path input = tmp_dir_ / "nested_nosplit.iec";
    write_file(input, src);

    auto r = run("--recursion-limit 128 --max-expansion-depth 100000 --max-expansion-steps 0 \"" +
                 input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err.substr(0, 2000);
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 1) << "stderr: " << r.err.substr(0, 2000);
    // The depth figure counts expand() nesting only, so it is not pinned.
    EXPECT_NE(r.err.find("PP63: Stack nearly exhausted; 'expansion depth "), std::string::npos)
        << "stderr: " << r.err.substr(0, 2000);
    EXPECT_NE(r.err.find("retry with --recursion-limit greater than 128'"), std::string::npos)
        << "stderr: " << r.err.substr(0, 2000);
    EXPECT_EQ(r.err.find("PP60"), std::string::npos) << "stderr: " << r.err.substr(0, 2000);
    EXPECT_EQ(r.out.find("after_n"), std::string::npos);
}

// ---- U1: re-lex diagnostics are reported at the file and line of the
// directive or pragma being processed ----

#ifndef JIEPP_SANDBOX
// The pragma's PP20 is reported in the included file at its own '{' line,
// and the includer's at its own line (not at line 1 of either).
TEST_F(SmokeTest, RelexDiagnosticInIncludedFileAtPragmaLine) {
    write_file(tmp_dir_ / "inc.iec", "x\n\n{ (* inc }\n");
    fs::path input = tmp_dir_ / "o.iec";
    write_file(input, "a\n{#include \"inc.iec\"}\n{ (* top }\n");

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.err.find("inc.iec:3.0: error: PP20: Unclosed comment; '(* inc '"), std::string::npos)
        << "stderr: " << r.err;
    EXPECT_NE(r.err.find("o.iec:3.0: error: PP20: Unclosed comment; '(* top '"), std::string::npos)
        << "stderr: " << r.err;
    EXPECT_EQ(std::count(r.err.begin(), r.err.end(), '\n'), 2) << "stderr: " << r.err;
}
#endif

// ---- PP10-PP14 cannot be suppressed by {#ignore} (SPEC 10) ----

#ifndef JIEPP_SANDBOX
TEST_F(SmokeTest, IgnoredPP10CannotHideUnopenableOutputFile) {
    fs::path input = tmp_dir_ / "ign_o.iec";
    write_file(input, "{#ignore PP10}\nx := 1;\n");
    fs::path bad_out = tmp_dir_ / "no_such_dir" / "x.out";

    auto r = run("-o \"" + bad_out.generic_string() + "\" \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP10"), std::string::npos) << "stderr: " << r.err;
    EXPECT_FALSE(fs::exists(bad_out));
    EXPECT_FALSE(fs::exists(bad_out.parent_path()));
}

TEST_F(SmokeTest, IgnoredPP10CannotHideUnopenableDependencyFile) {
    fs::path input = tmp_dir_ / "ign_mf.iec";
    write_file(input, "{#ignore PP10}\nx := 1;\n");
    fs::path bad_dep = tmp_dir_ / "no_such_dir" / "x.d";
    fs::path out = tmp_dir_ / "ign_mf_out.iec";

    auto r = run("-MD -MF \"" + bad_dep.generic_string() + "\" -o \"" + out.generic_string() +
                 "\" \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP10"), std::string::npos) << "stderr: " << r.err;
    EXPECT_FALSE(fs::exists(bad_dep));
    EXPECT_FALSE(fs::exists(bad_dep.parent_path()));
    EXPECT_FALSE(fs::exists(out));
}

TEST_F(SmokeTest, IgnoredPP11CannotHideMissingInclude) {
    fs::path input = tmp_dir_ / "ign_inc.iec";
    write_file(input, "{#ignore PP11}\n{#include \"missing_header.iec\"}\nx := 1;\n");

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP11"), std::string::npos) << "stderr: " << r.err;
}

TEST_F(SmokeTest, IgnoredPP14CannotHideDirectoryInclude) {
    fs::create_directories(tmp_dir_ / "adir");
    fs::path input = tmp_dir_ / "ign_dir.iec";
    write_file(input, "{#ignore PP14}\n{#include \"adir\"}\nx := 1;\n");

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP14"), std::string::npos) << "stderr: " << r.err;
}

TEST_F(SmokeTest, IgnoredPP12CannotHideIncludeDepthExceeded) {
    fs::path input = tmp_dir_ / "ign_depth.iec";
    write_file(input, "{#ignore PP12}\n{#include \"ign_depth.iec\"}\nx := 1;\n");

    auto r = run("--max-include-depth 3 \"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP12"), std::string::npos) << "stderr: " << r.err;
}
#endif

// ---- Windows: UTF-8 application manifest (activeCodePage) ----
//
// jiepp.exe embeds a manifest with activeCodePage=UTF-8, so on Windows 10
// 1903+ its narrow strings (argv, std::filesystem paths, ifstream names) are
// UTF-8 instead of the ANSI code page. These tests spawn jiepp.exe with
// CreateProcessW so the command line can carry characters the ANSI code page
// of jiepp_test itself (932 locally, 1252 on CI) cannot represent. File names
// are built from UTF-8 literals with universal-character-names so that
// neither this source nor jiepp_test's own ACP matters.
//
// CI note: .github/workflows/ci.yml has no Windows sandbox job, so the
// sandbox builds of these tests are only exercised locally.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

struct WinRunResult {
    int exit_code = -1;
    bool timed_out = false;
    std::string out;
    std::string err;
};

std::string u8_to_string(const std::u8string& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}

std::string slurp_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// Runs `exe` through CreateProcessW. `args` is the command line after the
// quoted executable path (the caller quotes arguments); it is copied into a
// writable std::wstring because CreateProcessW may modify its buffer. `cwd`
// (when not empty) becomes lpCurrentDirectory. stdout and stderr go to files
// under `scratch` through inheritable handles (two pipes read one after the
// other could deadlock); stdin is NUL. After 30 seconds the process is
// terminated and timed_out is set.
WinRunResult run_w(const fs::path& exe, const std::wstring& args,
                   const fs::path& cwd, const fs::path& scratch) {
    WinRunResult res;
    fs::path out_f = scratch / "run_w_stdout.txt";
    fs::path err_f = scratch / "run_w_stderr.txt";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE h_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE h_out = CreateFileW(out_f.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE h_err = CreateFileW(err_f.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h_in == INVALID_HANDLE_VALUE || h_out == INVALID_HANDLE_VALUE ||
        h_err == INVALID_HANDLE_VALUE) {
        for (HANDLE h : {h_in, h_out, h_err})
            if (h != INVALID_HANDLE_VALUE)
                CloseHandle(h);
        res.err = "run_w: cannot open the redirection handles";
        return res;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = h_in;
    si.hStdOutput = h_out;
    si.hStdError = h_err;
    PROCESS_INFORMATION pi{};

    std::wstring cmd = L"\"" + exe.wstring() + L"\" " + args;
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr,
                             cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    // The child holds its own copies of the handles.
    CloseHandle(h_in);
    CloseHandle(h_out);
    CloseHandle(h_err);
    if (!ok) {
        res.err = "run_w: CreateProcessW failed, error " + std::to_string(GetLastError());
        return res;
    }

    if (WaitForSingleObject(pi.hProcess, 30000) == WAIT_TIMEOUT) {
        res.timed_out = true;
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
    }
    DWORD code = 0;
    if (GetExitCodeProcess(pi.hProcess, &code))
        res.exit_code = static_cast<int>(code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    res.out = slurp_file(out_f);
    res.err = slurp_file(err_f);
    return res;
}

} // namespace

TEST_F(RobustnessTestSmoke, WindowsManifestActiveCodePageUtf8) {
    HMODULE h = LoadLibraryExW(exe_.c_str(), nullptr,
                               LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    ASSERT_NE(h, nullptr) << "LoadLibraryExW failed, error " << GetLastError();
    std::string manifest;
    // RT_MANIFEST (24) is an ANSI-typed macro in a non-UNICODE build.
    HRSRC rsrc = FindResourceW(h, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(24));
    if (rsrc != nullptr) {
        HGLOBAL g = LoadResource(h, rsrc);
        const DWORD size = SizeofResource(h, rsrc);
        if (g != nullptr)
            manifest.assign(static_cast<const char*>(LockResource(g)), size);
    }
    FreeLibrary(h);

    ASSERT_FALSE(manifest.empty()) << "no RT_MANIFEST resource with ID 1 in " << exe_;
    EXPECT_NE(manifest.find("activeCodePage"), std::string::npos) << manifest;
    EXPECT_NE(manifest.find(">UTF-8</activeCodePage>"), std::string::npos) << manifest;
    // Windows silently ignores a setting in a wrong namespace.
    EXPECT_NE(manifest.find("http://schemas.microsoft.com/SMI/2019/WindowsSettings"),
              std::string::npos)
        << manifest;
}

TEST_F(RobustnessTestSmoke, WindowsArgvNonAnsiPath) {
    // Hangul and an emoji: neither exists in CP932 or CP1252.
    const std::u8string name8 = u8"한\U0001F600.iec";
    const std::string name = u8_to_string(name8);
    write_file(tmp_dir_ / fs::path(name8), "F __FILE__\n");

    // 1. -P: __FILE__ is the argv bytes, as UTF-8.
    auto r = run_w(exe_, L"-P \"" + fs::path(name8).wstring() + L"\"", tmp_dir_, tmp_dir_);
    EXPECT_FALSE(r.timed_out);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_EQ(r.out, "F '" + name + "'\n") << "stderr: " << r.err;

    // 2. Without -P the line marker carries the UTF-8 name.
    r = run_w(exe_, L"\"" + fs::path(name8).wstring() + L"\"", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_NE(r.out.find("'" + name + "'"), std::string::npos) << r.out;

    // 3. -D values are UTF-8 too (a CP932 value could contain a '{' byte).
    write_file(tmp_dir_ / "v.iec", "V\n");
    r = run_w(exe_, L"-P \"-DV=한\" v.iec", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_EQ(r.out, u8_to_string(u8"한") + "\n") << "stderr: " << r.err;

    // 4. -o with a non-ANSI name creates that file.
    const std::u8string out8 = u8"out_한.txt";
    r = run_w(exe_, L"-P -o \"" + fs::path(out8).wstring() + L"\" v.iec", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_TRUE(fs::exists(tmp_dir_ / fs::path(out8)));
    EXPECT_EQ(slurp_file(tmp_dir_ / fs::path(out8)), "V\n");
}

TEST_F(RobustnessTestSmoke, WindowsArgvNoBestFit) {
    // Guard on code page 1252 (every character below exists there, so it
    // passes before and after the manifest); it fails on code page 932,
    // where the ANSI conversion of argv replaced the characters.
    write_file(tmp_dir_ / "u.iec", "DECOY\n");
    const std::u8string uuml8 = u8"ü.iec";
    write_file(tmp_dir_ / fs::path(uuml8), "REAL\n");
    auto r = run_w(exe_, L"-P \"" + fs::path(uuml8).wstring() + L"\"", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_EQ(r.out, "REAL\n") << "stderr: " << r.err;

    // A yen sign must stay a character: best-fit conversion to a backslash
    // would turn this into the path sub\..\a.iec, which resolves to a.iec.
    write_file(tmp_dir_ / "a.iec", "A\n");
    r = run_w(exe_, L"-P \"sub¥..¥" L"a.iec\"", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 1) << "stdout: " << r.out;
    EXPECT_NE(r.err.find("PP11"), std::string::npos) << "stderr: " << r.err;
    EXPECT_EQ(r.out.find('A'), std::string::npos) << "stdout: " << r.out;
}

#ifndef JIEPP_SANDBOX
TEST_F(SmokeTest, WindowsSourceIncludeNonAsciiName) {
    // The file name comes from the source text (UTF-8), not from argv.
    const std::u8string inc8 = u8"日本語.inc";
    const std::string inc = u8_to_string(inc8);
    write_file(tmp_dir_ / fs::path(inc8), "JP_INC\n");
    write_file(tmp_dir_ / "main.iec",
               "{#include '" + inc + "'}\n"
               "{#if __has_include('" + inc + "')}HAS{#endif}\n");
    auto r = run_w(exe_, L"-P main.iec", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_EQ(r.out, "JP_INC\nHAS\n") << "stderr: " << r.err;
}

TEST_F(SmokeTest, WindowsCwdNonAsciiDir) {
    // fs::current_path() converts through the ANSI code page: a working
    // directory the code page cannot represent made the run fail with PP01.
    const std::u8string dir8 = u8"cwd_한";
    const std::u8string sub8 = u8"sub_한";
    const std::string sub = u8_to_string(sub8);
    fs::path cwd = tmp_dir_ / fs::path(dir8);
    fs::create_directories(cwd / fs::path(sub8));
    write_file(cwd / fs::path(sub8) / "x.inc", "X_INC\n");
    write_file(cwd / "main.iec", "{#include '" + sub + "/x.inc'}\nM\n");
    auto r = run_w(exe_, L"-P main.iec", cwd, tmp_dir_);
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;
    EXPECT_EQ(r.out, "X_INC\nM\n") << "stderr: " << r.err;
}
#endif

#ifdef JIEPP_SANDBOX
// The jiepp.exe manifest makes its paths UTF-8, so a sandbox diagnostic
// applies the UTF-8 C1 rule to the path: the U+009B (CSI) in the file name
// below must not reach stderr. (The in-process tests run in an ANSI code
// page and take the other branch of the rule, so only this CLI test covers
// it.) The fixture name matches the sandbox presets' ^(Sandbox|RobustnessTest)
// filter; CI has no Windows sandbox job, so this runs locally only.
class SandboxCliUtf8PathC1Test : public SmokeTest {};

TEST_F(SandboxCliUtf8PathC1Test, PathC1PairRemoved) {
    const std::u8string name8 = u8"x\u009By.iec";
    write_file(tmp_dir_ / fs::path(name8), "{#line abc}\n");
    auto r = run_w(exe_, L"-P \"" + fs::path(name8).wstring() + L"\"", tmp_dir_, tmp_dir_);
    EXPECT_EQ(r.exit_code, 1) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("xy.iec:"), std::string::npos) << "stderr: " << r.err;
    EXPECT_NE(r.err.find("PP41"), std::string::npos) << "stderr: " << r.err;
    EXPECT_EQ(r.err.find("\xC2\x9B"), std::string::npos) << "stderr: " << r.err;
}
#endif

#endif // _WIN32
