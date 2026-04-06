#include "test_helper.hpp"

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
