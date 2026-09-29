#include "test_helper.hpp"
#include <algorithm>

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
    // Matches tools/perftest/cases/iterate_fmacros/iterate_fmacros.py's input
    // shape (n=100, well under the default stack's Debug-build recursion
    // ceiling -- about 1240 levels with the 8 MiB default stack -- so this
    // passes in both Debug and Release).
    fs::path input = tmp_dir_ / "nested_fmacro.iec";
    constexpr int kDepth = 100;

    std::string src = "{#define I(a) (a)+1}\nprogram Main\n{st}\n";
    for (int i = 0; i < kDepth; ++i)
        src += "I(";
    src += "0";
    for (int i = 0; i < kDepth; ++i)
        src += ")";
    src += ";\n{end}\nend_program\n";
    write_file(input, src);

    auto r = run("\"" + input.generic_string() + "\"");
    EXPECT_EQ(r.exit_code, 0) << "stderr: " << r.err;

    std::string expected;
    expected += std::string(kDepth, '(');
    expected += "0";
    for (int i = 0; i < kDepth; ++i)
        expected += ")+1";
    expected += ";";

    std::istringstream lines(r.out);
    std::string line;
    bool found = false;
    while (std::getline(lines, line)) {
        if (line == expected) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "expected line: " << expected << "\nstdout: " << r.out;
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
