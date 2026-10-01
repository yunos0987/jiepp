#include "test_helper.hpp"

#include <chrono>
#include <cstdlib>
#include <string>

// task_slug hideset-linear: integration-level tests for the persistent
// bitmap-trie HideSet (see design-hideset.md section 6, H1-H8). Each test
// builds an n-level macro-expansion chain (object macro, function macro, or
// object-macro-chain-ending-in-a-function-macro) and checks that the output
// is still exactly what Prosser's algorithm produces, while the underlying
// representation (src/loader/hideset.hpp/cpp) makes these chains O(n log n)
// instead of O(n^2).
//
// Every chain here is written as ONE line with no newlines between the
// {#define ...} directives, so blank-line compaction (core/line_compaction.cpp)
// never triggers and pp()'s output is exactly the expanded tail -- no file
// marker, no blank runs to reason about.

namespace {

// {#define M0 M1}{#define M1 M2}...{#define M{n} <tail>}<use>
std::string ochain_source(int n, const std::string& tail, const std::string& use) {
    std::string s;
    for (int i = 0; i < n; ++i)
        s += "{#define M" + std::to_string(i) + " M" + std::to_string(i + 1) + "}";
    s += "{#define M" + std::to_string(n) + " " + tail + "}";
    s += use;
    return s;
}

// {#define F0(a) F1(a)}...{#define F{n}(a) <tail-using-a>}F0(x);
std::string fchain_source(int n) {
    std::string s;
    for (int i = 0; i < n; ++i)
        s += "{#define F" + std::to_string(i) + "(a) F" + std::to_string(i + 1) + "(a)}";
    s += "{#define F" + std::to_string(n) + "(a) a}";
    s += "F0(x);";
    return s;
}

// {#define G0 G1}...{#define G{n}(a) a+1}G0(y);
std::string fchain_name_source(int n) {
    std::string s;
    for (int i = 0; i < n; ++i)
        s += "{#define G" + std::to_string(i) + " G" + std::to_string(i + 1) + "}";
    s += "{#define G" + std::to_string(n) + "(a) a+1}";
    s += "G0(y);";
    return s;
}

} // namespace

class HideSetChainTest : public JieppTest {};

// ---- H1: object-macro chain ----

TEST_F(HideSetChainTest, H1_ObjectMacroChain4096) {
    constexpr int kN = 4096;
    EXPECT_EQ("END;", pp(ochain_source(kN, "END", "M0;")));
    EXPECT_TRUE(empty());
}

// ---- H2: function-macro chain ----

TEST_F(HideSetChainTest, H2_FunctionMacroChain4096) {
    constexpr int kN = 4096;
    EXPECT_EQ("x;", pp(fchain_source(kN)));
    EXPECT_TRUE(empty());
}

// ---- H3: object chain ending in a function-macro name ----

TEST_F(HideSetChainTest, H3_ObjectChainEndingInFunctionMacro4096) {
    constexpr int kN = 4096;
    EXPECT_EQ("y+1;", pp(fchain_name_source(kN)));
    EXPECT_TRUE(empty());
}

// ---- H4: object chain ending in a self-reference ----
//
// The last macro's body names M0 (and the midpoint Mn/2) directly: both are
// already in the accumulated hide set by the time that body is substituted,
// so they must stay unexpanded -- this exercises membership of a LOW id
// (M0) and a MID id (M{n/2}) in a hide set whose trie root has grown to
// height 2 (n=4096 > 1024 = capacity of a height-1 root).

TEST_F(HideSetChainTest, H4_ObjectChainSelfReference4096) {
    constexpr int kN = 4096;
    std::string tail = "M0 M" + std::to_string(kN / 2);
    EXPECT_EQ("M0 M2048;", pp(ochain_source(kN, tail, "M0;")));
    EXPECT_TRUE(empty());
}

// ---- H5: corner cases (clang-verified; see design-hideset.md section 3) ----

TEST_F(HideSetChainTest, H5_CornerCases) {
    const std::string input =
        "{#define f(x) g}\n"
        "{#define g f}\n"
        "A: f(1)(2)(3)\n"
        "{#define h(a) a*k}\n"
        "{#define k(a) h(a)}\n"
        "B: h(2)(9)\n"
        "{#define AA(x) x BB}\n"
        "{#define BB(x) AA(x)}\n"
        "C: AA(1)(2)(3)\n"
        "{#define Q(x) x Q}\n"
        "D: Q(1)(2)\n"
        "{#define P1 P2(}\n"
        "{#define P2(x) P1 x )}\n"
        "E: P2(P2(1)))\n";
    const std::string expected =
        "\n"
        "\n"
        "A: f(2)(3)\n"
        "\n"
        "\n"
        "B: 2*9*k\n"
        "\n"
        "\n"
        "C: 1 2 BB(3)\n"
        "\n"
        "D: 1 Q(2)\n"
        "\n"
        "\n"
        "E: P2( P2( 1 ) ))\n";
    EXPECT_EQ(expected, pp(input));
    EXPECT_TRUE(empty());
}

// ---- H6: glue (paste) intersection ----

TEST_F(HideSetChainTest, H6_GlueIntersection) {
    EXPECT_EQ("AB", pp("{#define CAT(a,b) a@@b}{#define AB CAT(A,B)}AB"));
    EXPECT_TRUE(empty());
}

TEST_F(HideSetChainTest, H6_GlueIntersectionDifferentHideSets) {
    EXPECT_EQ("done",
              pp("{#define CAT(a,b) a@@b}{#define AB done}{#define X CAT(A,B)}X"));
    EXPECT_TRUE(empty());
}

// ---- H7: Release-only timing guard ----
//
// Release-only (like BoostPreprocessorIntegration): a Debug build is not
// representative of real performance (see AGENTS.md's Profiling section),
// so there is no Debug timing assertion. On the base (develop + arg-once,
// pre-hideset-linear), n=16384 measured ~19s (ochain) / >25s (fchain) /
// ~18s (fchain_name) -- this guard is expected to FAIL (RED) there. The new
// representation predicts < 0.2s for each, so EXPECT_LT(3.0) has a >= 15x
// margin in both directions.
#ifdef NDEBUG
TEST_F(HideSetChainTest, H7_TimingGuard) {
    constexpr int kN = 16384;

    auto timed = [](const std::string& src) {
        auto t0 = std::chrono::steady_clock::now();
        std::string out = pp(src);
        auto t1 = std::chrono::steady_clock::now();
        return std::make_pair(out, std::chrono::duration<double>(t1 - t0).count());
    };

    auto [ochain_out, ochain_s] = timed(ochain_source(kN, "END", "M0;"));
    EXPECT_EQ("END;", ochain_out);
    EXPECT_LT(ochain_s, 3.0) << "ochain n=" << kN << " took " << ochain_s << "s";

    auto [fchain_out, fchain_s] = timed(fchain_source(kN));
    EXPECT_EQ("x;", fchain_out);
    EXPECT_LT(fchain_s, 3.0) << "fchain n=" << kN << " took " << fchain_s << "s";

    auto [fchain_name_out, fchain_name_s] = timed(fchain_name_source(kN));
    EXPECT_EQ("y+1;", fchain_name_out);
    EXPECT_LT(fchain_name_s, 3.0) << "fchain_name n=" << kN << " took " << fchain_name_s << "s";
}
#endif

// ---- H8: large-n byte-identical output for deep nested function macros ----
//
// Deferred item from design-arg-once.md: SmokeTest::DeeplyNestedFunctionMacroExpansion
// / section 11 (tests/jiepp/test_smoke.cpp) only ever search for one expected
// line in stdout; this asserts the WHOLE stdout (including the file marker,
// the {st}/{end} pragma lines, and exact blank-line placement) at a size
// (n=1000) that would be badly quadratic (and so slow enough to make this
// test impractical) before the hide-set fix.

class HideSetChainCliTest : public JieppTest {
protected:
    fs::path exe_;
    fs::path tmp_dir_;

    void SetUp() override {
        JieppTest::SetUp();
#ifdef JIEPP_EXE_PATH
        exe_ = fs::path(JIEPP_EXE_PATH);
#endif
        if (exe_.empty() || !fs::exists(exe_)) {
            GTEST_SKIP() << "jiepp executable not found: " << exe_;
        }
        tmp_dir_ = fs::temp_directory_path() / "jiepp_hideset_chain_test";
        fs::create_directories(tmp_dir_);
    }

    void TearDown() override {
        if (!tmp_dir_.empty() && fs::exists(tmp_dir_)) {
            std::error_code ec;
            fs::remove_all(tmp_dir_, ec);
        }
        JieppTest::TearDown();
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

    // Same CLI-invocation shape as tests/jiepp/test_smoke.cpp's run(): a
    // real jiepp.exe process, so the 8 MiB default stack (not this test
    // binary's own stack) is what gets exercised at n=1000 nesting depth.
    RunResult run(const std::string& args) {
        fs::path out_f = tmp_dir_ / "stdout.txt";
        fs::path err_f = tmp_dir_ / "stderr.txt";
        std::string cmd;
#ifdef _WIN32
        cmd = "\"\"" + exe_.generic_string() + "\" " + args;
        cmd += " >\"" + out_f.generic_string() + "\"";
        cmd += " 2>\"" + err_f.generic_string() + "\"";
        cmd += "\"";
#else
        cmd = "'" + exe_.generic_string() + "' " + args;
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

TEST_F(HideSetChainCliTest, H8_DeepNestedFunctionMacroWholeOutput) {
    constexpr int kDepth = 1000;
    fs::path input = tmp_dir_ / "h8_deep_nested.iec";

    std::string src = "{#define I(a) (a)+1}\nprogram Main\n{st}\n";
    for (int i = 0; i < kDepth; ++i)
        src += "I(";
    src += "0";
    for (int i = 0; i < kDepth; ++i)
        src += ")";
    src += ";\n{end}\nend_program\n";
    write_file(input, src);

    auto r = run("--max-expansion-depth 2000 \"" + input.generic_string() + "\"");
    ASSERT_EQ(r.exit_code, 0) << "stderr: " << r.err;

    std::string expected = "(*{#:0 '" + input.generic_string() + "'}*)\n";
    expected += "\n"; // {#define I(a) (a)+1} consumes its line, leaving a blank one
    expected += "program Main\n";
    expected += "(*{st}*)\n";
    expected += std::string(kDepth, '(');
    expected += "0";
    for (int i = 0; i < kDepth; ++i)
        expected += ")+1";
    expected += ";\n";
    expected += "(*{end}*)\n";
    expected += "end_program\n";

    EXPECT_EQ(expected, r.out);
}
