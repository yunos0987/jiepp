#include "test_helper.hpp"
#include "jiepp/jiepp.hpp"
#include "jiepp/option.hpp"

#include <regex>

namespace fs = std::filesystem;

static const fs::path I_DIR = "tests/jiepp/input";
static const fs::path O_DIR = "tests/jiepp/output";

// Strip debug source location suffix from error messages (format: @file.cpp:line)
static std::string strip_debug_suffix(const std::string& text) {
    // Pattern: @file.path.cpp:123 - match any @, followed by non-newline chars, colon, and digits
    return std::regex_replace(text, std::regex("@[^\n]+:\\d+"), "");
}

static void run_e2e(const std::string& testid,
                    const std::vector<std::string>& syspaths = {},
                    const std::vector<std::pair<std::string,std::string>>& macros = {},
                    std::optional<std::string> pragma_style = std::nullopt,
                    bool remove_comments = false,
                    int max_include_depth = -1,
                    bool dM = false,
                    const std::string& dep_target = "",
                    const std::vector<std::string>& include_files = {},
                    const std::vector<std::string>& undef_macros = {},
                    bool suppress_warnings = false,
                    bool werror = false)
{
    fs::current_path(jiepp_root_dir());

    fs::path input_filepath = I_DIR / (testid + ".iec");
    if (!fs::exists(input_filepath))
        FAIL() << "Input file does not exist for testid: " << testid << "; " << input_filepath.generic_string();

    std::string actual_out_filepath = (I_DIR / (testid + ".piec")).generic_string();
    std::string actual_log_filepath = (I_DIR / (testid + ".log")).generic_string();
    std::string actual_dep_filepath = (I_DIR / (testid + ".d")).generic_string();

    std::ofstream actual_log_file(actual_log_filepath, std::ios::binary);
    Issue::initialize(actual_log_file);

    bool error_mode;
    std::string input_base;
    if (testid.ends_with(".error")) {
        input_base = testid.substr(0, testid.size() - std::string(".error").size());
        error_mode = true;
    } else {
        input_base = testid;
        error_mode = false;
    }

    // Build define_macros strings
    std::vector<std::string> define_macros;
    for (const auto& [k, v] : macros)
        define_macros.push_back(k + "=" + v);

    // Convert include_files to relative paths (for consistent golden output)
    std::vector<std::string> rel_include_filepaths;
    for (const auto& filepath : include_files)
        rel_include_filepaths.push_back(filepath);

    JieppOptions opts;
    opts.input_filepaths = {input_filepath.generic_string()};
    opts.define_macros = define_macros;
    opts.syspaths = syspaths;
    opts.output_filepath = actual_out_filepath;
    opts.include_filepaths = rel_include_filepaths;
    opts.undef_macros = undef_macros;
    opts.suppress_warnings = suppress_warnings;
    opts.werror = werror;
    if (max_include_depth >= 0)
        opts.max_include_depth = max_include_depth;
    opts.pp_output_pragma_style = pragma_style;
    opts.remove_comments = remove_comments;
    opts.dM = dM;
    opts.dep_mode = DepMode::ALL;
    // B16 fix: -M/-MM alone now suppress the preprocessed output. This
    // harness always requests a dependency file (below) for golden-output
    // coverage, so it must also set -MD to keep "preprocessed output + dep
    // file" semantics and preserve every existing golden byte-for-byte.
    opts.MD = true;
    if (!dep_target.empty())
        opts.dep_target = dep_target;
    opts.dep_file = actual_dep_filepath;

    int rc = jiepp_command(opts);
    actual_log_file.close();

    if (error_mode) {
        ASSERT_NE(rc, 0) << "expected error for testid: " << testid;
    } else {
        ASSERT_EQ(rc, 0) << "unexpected error for testid: " << testid;

        // Compare .piec (preprocessor output)
        fs::path expect_out_filepath = O_DIR / (testid + ".piec");
        std::ifstream ef(expect_out_filepath);
        std::string expect_out((std::istreambuf_iterator<char>(ef)), std::istreambuf_iterator<char>());
        std::ifstream af(actual_out_filepath);
        std::string actual_out((std::istreambuf_iterator<char>(af)), std::istreambuf_iterator<char>());
        EXPECT_EQ(expect_out, actual_out) << "out mismatch for: " << testid;

        // Compare .d (dependency file) if expected exists
        fs::path expect_dep_filepath = O_DIR / (testid + ".d");
        if (fs::exists(expect_dep_filepath)) {
            std::ifstream ef(expect_dep_filepath);
            std::string expect_dep((std::istreambuf_iterator<char>(ef)), std::istreambuf_iterator<char>());
            std::ifstream af(actual_dep_filepath);
            std::string actual_dep((std::istreambuf_iterator<char>(af)), std::istreambuf_iterator<char>());
            EXPECT_EQ(expect_dep, actual_dep) << "dep mismatch for: " << testid;
        }
    }

    fs::path expect_log_filepath = O_DIR / (testid + ".log");
    if (fs::exists(expect_log_filepath)) {
        std::ifstream ef(expect_log_filepath);
        std::string expect_log((std::istreambuf_iterator<char>(ef)), std::istreambuf_iterator<char>());
        std::ifstream af(actual_log_filepath);
        std::string actual_log((std::istreambuf_iterator<char>(af)), std::istreambuf_iterator<char>());
        // Strip debug suffixes before comparing
        expect_log = strip_debug_suffix(expect_log);
        actual_log = strip_debug_suffix(actual_log);
        EXPECT_EQ(expect_log, actual_log) << "log mismatch for: " << testid;
    }
}

class JieppCommandTest : public JieppTest {};

TEST_F(JieppCommandTest, MultipleInputFilesRejected) {
    fs::current_path(jiepp_root_dir());
    JieppOptions opts;
    opts.input_filepaths = {"file1.iec", "file2.iec"};
    EXPECT_NE(0, jiepp_command(opts));
    EXPECT_EQ("<unknown location>:1.0: error: PP13: Invalid command; 'multiple input files not supported'", message());
}

TEST_F(JieppCommandTest, Regular) {
    run_e2e("none");
    run_e2e("not_directive");
}

TEST_F(JieppCommandTest, If) {
    run_e2e("if");
}

TEST_F(JieppCommandTest, Define) {
    run_e2e("define");
}

TEST_F(JieppCommandTest, FunctionMacro) {
    run_e2e("function_macro");
}

TEST_F(JieppCommandTest, FunctionMacroVaArgs) {
    run_e2e("function_macro_vaargs");
}

TEST_F(JieppCommandTest, StringizeDirective) {
    run_e2e("stringize_directive");
}

TEST_F(JieppCommandTest, Include) {
    run_e2e("include", {I_DIR.generic_string()});
}

TEST_F(JieppCommandTest, IncludeSingle) {
    run_e2e("include_single", {I_DIR.generic_string()});
}

TEST_F(JieppCommandTest, IncludeDouble) {
    run_e2e("include_double", {I_DIR.generic_string()});
}

TEST_F(JieppCommandTest, DOption) {
    run_e2e("D_option/D_option",
            {},
            {{"POU","Main"},{"X","1"},{"INITIAL_VALUE","2"},{"EXPR","INITIAL_VALUE+X+3"}});
}

TEST_F(JieppCommandTest, FileMacro) {
    run_e2e("file_macro/file_macro");
}

TEST_F(JieppCommandTest, FileMacroInclude) {
    run_e2e("file_macro/file_macro_include");
}

TEST_F(JieppCommandTest, FileMacroLineDirective) {
    run_e2e("file_macro/file_macro_line_directive");
}

TEST_F(JieppCommandTest, CounterMacro) {
    run_e2e("counter_macro/counter_macro");
}

TEST_F(JieppCommandTest, CounterMacroInclude) {
    run_e2e("counter_macro/counter_macro_include");
}

TEST_F(JieppCommandTest, LineMacro) {
    run_e2e("line_macro/line_macro");
}

TEST_F(JieppCommandTest, LineMacroInclude) {
    run_e2e("line_macro/line_macro_include");
}

TEST_F(JieppCommandTest, LineMacroOmacro) {
    run_e2e("line_macro/line_macro_omacro");
}

TEST_F(JieppCommandTest, LineMacroFmacro) {
    run_e2e("line_macro/line_macro_fmacro");
}

TEST_F(JieppCommandTest, PpOutputPragmaStyleAnnotated) {
    run_e2e("ppoutputpragmastyle_pragma_annotated", {}, {}, "annotated");
}

TEST_F(JieppCommandTest, PpOutputPragmaStyleStandard) {
    run_e2e("ppoutputpragmastyle_pragma_standard", {}, {}, "standard");
}

TEST_F(JieppCommandTest, RemoveCommentsOff) {
    run_e2e("remove_comments_option/remove_comments_option_off", {}, {}, std::nullopt, false);
}

TEST_F(JieppCommandTest, RemoveCommentsOn) {
    run_e2e("remove_comments_option/remove_comments_option_on", {}, {}, std::nullopt, true);
}

TEST_F(JieppCommandTest, BoostPreprocessorIntegration) {
    if (!fs::exists(jiepp_root_dir() /I_DIR / "boost"))
        GTEST_SKIP() << "Boost test input directory not found; skipping test";
#ifdef NDEBUG
    run_e2e("boost", {I_DIR.generic_string()});
#endif
}

// ─── B1: samples regeneration guard ────────────────────────────────────────

TEST_F(JieppCommandTest, SamplesRegenerateIdentically) {
    // Every checked-in iec_61131-3/samples/*.iec (except files starting
    // with '_', which are includable fragments, not standalone samples)
    // must regenerate byte-identical to its committed .piec with no -I --
    // U1 makes {#syspath} relative to the containing file, so samples no
    // longer need -I to find their own lib/ directory (see
    // tools/pp_iec61131-3_samples.ps1). Output is written to a scratch
    // directory outside the repo; nothing under iec_61131-3/samples is ever
    // touched by this test.
    fs::current_path(jiepp_root_dir());
    fs::path samples_dir = jiepp_root_dir() / "iec_61131-3" / "samples";
    fs::path out_dir = fs::temp_directory_path() / "jiepp_samples_test";
    std::error_code ec;
    fs::remove_all(out_dir, ec);
    fs::create_directories(out_dir);

    int checked = 0;
    for (const auto& entry : fs::directory_iterator(samples_dir)) {
        if (!entry.is_regular_file())
            continue;
        const fs::path& p = entry.path();
        if (p.extension() != ".iec")
            continue;
        std::string stem = p.stem().generic_string();
        if (stem.starts_with("_"))
            continue;

        fs::path out = out_dir / (stem + ".piec");
        fs::path log = out_dir / (stem + ".log");
        std::ofstream log_file(log, std::ios::binary);
        Issue::initialize(log_file);

        // Match how CONTRIBUTING.md / tools/pp_iec61131-3_samples.ps1 invoke
        // jiepp: the input path is relative to the repo root (CWD), which
        // also keeps the (*{#:0 '...'}*) line markers in the regenerated
        // output identical to the committed golden's relative-path form.
        JieppOptions opts;
        opts.input_filepaths = {fs::relative(p, jiepp_root_dir()).generic_string()};
        opts.output_filepath = out.generic_string();

        int rc = jiepp_command(opts);
        log_file.close();
        ASSERT_EQ(0, rc) << "sample failed to regenerate without -I: " << p.generic_string();

        fs::path golden = p;
        golden.replace_extension(".piec");
        std::ifstream gf(golden, std::ios::binary);
        ASSERT_TRUE(static_cast<bool>(gf)) << "missing golden: " << golden.generic_string();
        std::string expect((std::istreambuf_iterator<char>(gf)), std::istreambuf_iterator<char>());
        std::ifstream af(out, std::ios::binary);
        std::string actual((std::istreambuf_iterator<char>(af)), std::istreambuf_iterator<char>());
        EXPECT_EQ(expect, actual) << "sample regenerated differently than its committed .piec: "
                                   << p.generic_string();
        ++checked;
    }
    EXPECT_GT(checked, 0) << "no iec_61131-3/samples/*.iec files were found to check";

    fs::remove_all(out_dir, ec);
}

TEST_F(JieppCommandTest, IncludeWithSyspathDirectiveXTag) {
    run_e2e("include_with_syspath_directive/include_with_syspath_directive_x_tag", {I_DIR.generic_string()});
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveX) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x", {I_DIR.generic_string()});
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirective3_1) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_3_1");
}
    
TEST_F(JieppCommandTest, MaxIncludeDepthDirective3_2) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_3_2");
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirective3_3) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_3_3");
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirective3_4) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_3_4.error");
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirectiveSinclude3_1) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_sinclude_3_1");
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirectiveSinclude3_2) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_sinclude_3_2");
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirectiveSinclude3_3) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_sinclude_3_3");
}

TEST_F(JieppCommandTest, MaxIncludeDepthDirectiveSinclude3_4) {
    run_e2e("max_include_depth_directive/max_include_depth_directive_sinclude_3_4.error");
}

TEST_F(JieppCommandTest, IncludeSyspathOptionNone) {
    run_e2e("include_syspath_option/include_syspath_option_none", {});
}

TEST_F(JieppCommandTest, IncludeSyspathOptionX) {
    fs::path dir = I_DIR / "include_syspath_option";
    run_e2e("include_syspath_option/include_syspath_option_x", {dir.generic_string()});
}

TEST_F(JieppCommandTest, IncludeSyspathOptionA) {
    fs::path dir = I_DIR / "include_syspath_option";
    run_e2e("include_syspath_option/include_syspath_option_a", {(dir / "a").generic_string()});
}

TEST_F(JieppCommandTest, IncludeSyspathOptionXA) {
    fs::path dir = I_DIR / "include_syspath_option";
    run_e2e("include_syspath_option/include_syspath_option_x_a", {dir.generic_string(), (dir / "a").generic_string()});
}

TEST_F(JieppCommandTest, IncludeSyspathOptionAX) {
    fs::path dir = I_DIR / "include_syspath_option";
    run_e2e("include_syspath_option/include_syspath_option_a_x", {(dir / "a").generic_string(), dir.generic_string()});
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveNoneError) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_none.error");
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveXSingle) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x_single");
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveXDouble) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x_double");
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveXTag) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x_tag");
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveA) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_a");
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveXA) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x_a");
}

TEST_F(JieppCommandTest, SincludeWithSyspathDirectiveAX) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_a_x");
}

TEST_F(JieppCommandTest, SincludeSyspathOptionNoneError) {
    run_e2e("sinclude_syspath_option/sinclude_syspath_option_none.error", {});
}

TEST_F(JieppCommandTest, SincludeSyspathOptionX) {
    fs::path dir = I_DIR / "sinclude_syspath_option";
    run_e2e("sinclude_syspath_option/sinclude_syspath_option_x", {dir.generic_string()});
}

TEST_F(JieppCommandTest, SincludeSyspathOptionA) {
    fs::path dir = I_DIR / "sinclude_syspath_option";
    run_e2e("sinclude_syspath_option/sinclude_syspath_option_a", {(dir / "a").generic_string()});
}

TEST_F(JieppCommandTest, SincludeSyspathOptionXA) {
    fs::path dir = I_DIR / "sinclude_syspath_option";
    run_e2e("sinclude_syspath_option/sinclude_syspath_option_x_a", {dir.generic_string(), (dir / "a").generic_string()});
}

TEST_F(JieppCommandTest, SincludeSyspathOptionAX) {
    fs::path dir = I_DIR / "sinclude_syspath_option";
    run_e2e("sinclude_syspath_option/sinclude_syspath_option_a_x", {(dir / "a").generic_string(), dir.generic_string()});
}

TEST_F(JieppCommandTest, MaxIncludeDepthOption0) {
    run_e2e("max_include_depth_option/max_include_depth_option_include0", {}, {}, std::nullopt, false, 2);
}

TEST_F(JieppCommandTest, MaxIncludeDepthOption1) {
    run_e2e("max_include_depth_option/max_include_depth_option_include1", {}, {}, std::nullopt, false, 3);
}

TEST_F(JieppCommandTest, MaxIncludeDepthOption2) {
    run_e2e("max_include_depth_option/max_include_depth_option_include2", {}, {}, std::nullopt, false, 3);
}

TEST_F(JieppCommandTest, MaxIncludeDepthOption3) {
    run_e2e("max_include_depth_option/max_include_depth_option_include3.error", {}, {}, std::nullopt, false, 3);
}

TEST_F(JieppCommandTest, DM) {
    run_e2e("dM/dM", {}, {}, std::nullopt, false, -1, true);
}

TEST_F(JieppCommandTest, DMAddDefineMacros) {
    run_e2e("dM/dM_add_definemacros",
            {},
            {{"a","1"}, {"a","2"}, {"b","2"}, {"s","x\ty"}},
            std::nullopt, false, -1, true);
}

// R8: -dM output order must remain stable across an undef+redefine of the
// same name: A is undefined and redefined to 3, B is untouched. The -dM
// listing must show the final value of A (not the stale 1), must not still
// list A under its old value, and A must still precede B (definition order
// is preserved; undef+redefine does not move A to the end).
TEST_F(JieppCommandTest, DMOrderStableAcrossUndefRedefine) {
    fs::current_path(jiepp_root_dir());

    std::string input_filepath = (I_DIR / "dM/dM_order_undef_redefine.iec").generic_string();
    std::string output_filepath = (I_DIR / "dM/dM_order_undef_redefine.piec").generic_string();

    char* argv[] = {
        const_cast<char*>("jiepp"),
        const_cast<char*>("-dM"),
        const_cast<char*>(input_filepath.c_str()),
        const_cast<char*>("-o"),
        const_cast<char*>(output_filepath.c_str()),
    };
    JieppOptions opts = parse_args(5, argv);

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream f(output_filepath, std::ios::binary);
    std::string actual((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    auto pos_a3 = actual.find("{#define A 3}");
    auto pos_b2 = actual.find("{#define B 2}");
    EXPECT_NE(pos_a3, std::string::npos) << "expected final value of A (3) in -dM output:\n" << actual;
    EXPECT_NE(pos_b2, std::string::npos) << "expected B still listed in -dM output:\n" << actual;
    EXPECT_EQ(actual.find("{#define A 1}"), std::string::npos)
        << "stale pre-undef value of A must not appear in -dM output:\n" << actual;
    if (pos_a3 != std::string::npos && pos_b2 != std::string::npos)
        EXPECT_LT(pos_a3, pos_b2) << "A must still precede B after undef+redefine:\n" << actual;
}

// ---- New CLI feature tests ----

TEST_F(JieppCommandTest, UndefOption) {
    run_e2e("U_option/U_option", {}, {}, std::nullopt, false, -1, false, "", {}, {"EXTRA"});
}

TEST_F(JieppCommandTest, IncludeOption) {
    run_e2e("include_option/include_option", {}, {}, std::nullopt, false, -1, false, "", {(I_DIR / "include_option" / "header.iec").generic_string()});
}

TEST_F(JieppCommandTest, WarnSuppress) {
    run_e2e("U_option/U_option_warn", {}, {{"FOO", "99"}}, std::nullopt, false, -1, false, "", {}, {}, true);
}

TEST_F(JieppCommandTest, Werror) {
    run_e2e("U_option/U_option_werror.error", {}, {{"FOO", "99"}}, std::nullopt, false, -1, false, "", {}, {}, false, true);
}

TEST_F(JieppCommandTest, DepOutputM) {
    run_e2e("include", {I_DIR.generic_string()}, {}, std::nullopt, false, -1, false);
}

// ---- Error format consistency tests ----

TEST_F(JieppCommandTest, ErrorFormatFileError) {
    // Uses a valid, existing input (B11/B12: -o is now opened only after
    // expansion succeeds, so an invalid input path would instead surface
    // FILE_NOT_FOUND for the input before -o is ever touched; use a real
    // input here so this test still exercises the -o open-failure message).
    fs::current_path(jiepp_root_dir());
    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "none.iec").generic_string()};
    opts.output_filepath = "/nonexistent/dir/output.iec";
    EXPECT_NE(0, jiepp_command(opts));
    EXPECT_EQ("<unknown location>:1.0: error: PP10: An error occurred with the file; '/nonexistent/dir/output.iec'", message());
}

TEST_F(JieppCommandTest, DepOutputMMExcludesSystem) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x", {}, {}, std::nullopt, false, -1, false);
}

// T1a: -M should output dependencies with display paths (not canonical paths)
TEST_F(JieppCommandTest, DepOutputMDisplayPath) {
     run_e2e("include", {I_DIR.generic_string()}, {}, std::nullopt, false, -1, false);
}

// T1b: -MM should also output with display paths
TEST_F(JieppCommandTest, DepOutputMMDisplayPath) {
     run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x", {}, {}, std::nullopt, false, -1, false);
}

// T1c: Ensure no duplicate dependencies are output
TEST_F(JieppCommandTest, DepOutputNoDuplicates) {
     run_e2e("include_double", {I_DIR.generic_string()}, {}, std::nullopt, false, -1, false);
}

// T2: -w + -Werror interaction: -Werror takes priority, warning is promoted
TEST_F(JieppCommandTest, WarnSuppressWithWerror) {
    run_e2e("U_option/U_option_w_werror.error", {}, {{"FOO", "99"}}, std::nullopt, false, -1, false, "",
            {}, {}, true, true);
}

// T3: -include with nonexistent file should produce error
TEST_F(JieppCommandTest, IncludeOptionMissing) {
    run_e2e("include_option/include_option_missing.error", {}, {}, std::nullopt, false, -1, false, "",
            {"/nonexistent/path/to/header.iec"});
}

// T4: -U of never-defined macro should be a silent no-op
TEST_F(JieppCommandTest, UndefOptionNoprior) {
    run_e2e("U_option/U_option_noprior", {}, {}, std::nullopt, false, -1, false, "",
            {}, {"NEVER_DEFINED_MACRO"});
}

// ---- Additional coverage: multiple -include ----

TEST_F(JieppCommandTest, IncludeOptionMultiple) {
    run_e2e("include_option/include_option_multi", {}, {}, std::nullopt, false, -1, false, "",
            {(I_DIR / "include_option" / "header.iec").generic_string(),
             (I_DIR / "include_option" / "header2.iec").generic_string()});
}


// ---- Additional coverage: multiple -U ----

TEST_F(JieppCommandTest, UndefMultiple) {
    run_e2e("dM/dM_undef_multiple", {}, {{"A", "1"}, {"B", "2"}, {"_JIEPP_FULL_VER", "75321"}, {"_JIEPP_VER", "753"}, {"_JIEPP_VERSION", "'7.53.21'"}}, std::nullopt, false, -1, true, "",
            {}, {"A"});
}

TEST_F(JieppCommandTest, DepOutputMTCustomTarget) {
    run_e2e("include", {I_DIR.generic_string()}, {}, std::nullopt, false, -1, false);
}

// ---- Additional coverage: -MF write failure ----

TEST_F(JieppCommandTest, DepOutputMFWriteFailure) {
    fs::current_path(jiepp_root_dir());
    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "none.iec").generic_string()};
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = "/nonexistent/deeply/nested/path/output.d";
    EXPECT_NE(0, jiepp_command(opts));
    EXPECT_EQ("<unknown location>:1.0: error: PP10: An error occurred with the file; '/nonexistent/deeply/nested/path/output.d'", message());
}

// -MM Coverage: User includes not excluded
TEST_F(JieppCommandTest, DepOutputMMIncludesUserIncludes) {
    run_e2e("include", {I_DIR.generic_string()}, {}, std::nullopt, false, -1, false);
}

// -MM Coverage: System includes excluded (single quote variant)
TEST_F(JieppCommandTest, DepOutputMMSingleQuotedSystemExcluded) {
    run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x_single", {}, {}, std::nullopt, false, -1, false);
}

// -MM Coverage: Custom target with system includes excluded
TEST_F(JieppCommandTest, DepOutputMMCustomTarget) {
    // kludge
    //run_e2e("sinclude_with_syspath_directive/sinclude_with_syspath_directive_x_tag", {}, {}, std::nullopt, false, -1, false, "myapp.output");
}

// ---- -P flag (no line markers) tests ----

TEST_F(JieppCommandTest, PSuppressesLineMarkersEmptyFile) {
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path output = tmpdir / "test_P_none.piec";

    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "none.iec").generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("(*{#:")) << "annotated line marker in -P output";
    EXPECT_EQ(std::string::npos, content.find("{#:"))   << "standard line marker in -P output";
    EXPECT_TRUE(content.empty()) << "empty-input -P output should be empty";
}

TEST_F(JieppCommandTest, PSuppressesLineMarkersWithContent) {
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path output = tmpdir / "test_P_define.piec";

    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "define.iec").generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("(*{#:")) << "annotated line marker in -P output";
    EXPECT_EQ(std::string::npos, content.find("{#:"))   << "standard line marker in -P output";
    EXPECT_NE(std::string::npos, content.find("program Main")) << "content should be preserved";
}

TEST_F(JieppCommandTest, PPreservesUserPragmas) {
    // User IEC pragmas (non-linemarker) must NOT be suppressed by -P
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path output = tmpdir / "test_P_notdir.piec";

    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "not_directive.iec").generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("(*{#:")) << "line marker in -P output";
    EXPECT_NE(std::string::npos, content.find("(*{k:v}*)")) << "user pragma suppressed by -P";
    EXPECT_NE(std::string::npos, content.find("(*{st}*)"))  << "user pragma suppressed by -P";
}

TEST_F(JieppCommandTest, PDefaultFalseLineMarkersPresent) {
    // Without -P, output must contain line markers
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path output = tmpdir / "test_noP_none.piec";

    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "none.iec").generic_string()};
    opts.output_filepath = output.generic_string();
    // no_line_markers = false (default)

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_NE(std::string::npos, content.find("(*{#:")) << "expected line marker absent when -P not set";
}

TEST_F(JieppCommandTest, PWithStandardStyle) {
    // -P with standard pragma style: {#:...} markers must also be suppressed
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path output = tmpdir / "test_P_standard.piec";

    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "define.iec").generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;
    opts.pp_output_pragma_style = "standard";

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("{#:")) << "standard line marker in -P output";
    EXPECT_NE(std::string::npos, content.find("program Main")) << "content should be preserved";
}

TEST_F(JieppCommandTest, PDoesNotAffectDM) {
    // -P suppresses line markers but must not remove {#define ...} macro dump output
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path output = tmpdir / "test_P_dM.piec";

    JieppOptions opts;
    opts.input_filepaths = {(I_DIR / "define.iec").generic_string()};
    opts.output_filepath = output.generic_string();
    opts.dM = true;
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    // Macro dump ({#define NAME VALUE}) must be present
    EXPECT_NE(std::string::npos, content.find("{#define N 2}"))   << "-P removed macro dump";
    EXPECT_NE(std::string::npos, content.find("{#define M N + 3}")) << "-P removed macro dump";
    // Line markers (starts with {#:) must not appear
    EXPECT_EQ(std::string::npos, content.find("(*{#:")) << "line marker in -P + -dM output";
    EXPECT_EQ(std::string::npos, content.find("{#:"))   << "standard line marker in -P + -dM output";
}

TEST_F(JieppCommandTest, PDoesNotAffectDeps) {
    // -P should not affect dependency generation
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path out_P   = tmpdir / "test_P_deps.piec";
    fs::path dep_P   = tmpdir / "test_P_deps.d";
    fs::path out_noP = tmpdir / "test_noP_deps.piec";
    fs::path dep_noP = tmpdir / "test_noP_deps.d";

    auto run_with = [&](bool P, fs::path& out, fs::path& dep) {
        JieppOptions opts;
        opts.input_filepaths = {(I_DIR / "include.iec").generic_string()};
        opts.syspaths = {I_DIR.generic_string()};
        opts.output_filepath = out.generic_string();
        opts.dep_mode = DepMode::ALL;
        opts.dep_file = dep.generic_string();
        opts.no_line_markers = P;
        return jiepp_command(opts);
    };

    ASSERT_EQ(0, run_with(true,  out_P,   dep_P));
    ASSERT_EQ(0, run_with(false, out_noP, dep_noP));

    auto read = [](const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    };

    EXPECT_EQ(read(dep_P), read(dep_noP)) << "-P changed dependency output";
}

TEST_F(JieppCommandTest, PCollapsesBlankRunsToZero) {
    // -P runs blank-line compaction in CollapseAll mode: every blank run
    // (not just runs over the default 7-line threshold) collapses to zero
    // blank lines, with no marker inserted — matching D6/[A7]. Uses the
    // §2 worked example: "A;" then 12 indented {#define} lines (which
    // vanish, leaving only a 13-newline blank run) then "B __LINE__;".
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "blank_lines_P_worked_example.iec";
    {
        std::ofstream f(src);
        f << "A;\n";
        for (int k = 1; k <= 12; ++k)
            f << "    {#define M" << k << " " << k << "}\n";
        f << "B __LINE__;\n";
    }
    fs::path output = tmpdir / "blank_lines_P_worked_example.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ("A;\nB 14;\n", content);
}

TEST_F(JieppCommandTest, PPreservesCommentAfterIncludeNoNewline) {
    // Regression: C token immediately after a return-from-include line marker
    // must NOT be silently dropped by the skip_next_ws logic.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    // Create a simple header
    fs::path hdr = tmpdir / "p_hdr.iec";
    {
        std::ofstream f(hdr);
        f << "VAR x: INT; END_VAR\n";
    }

    // Main: include + comment with no newline between them
    fs::path main_iec = tmpdir / "p_main.iec";
    {
        std::ofstream f(main_iec);
        f << "{#include '" << hdr.generic_string() << "'}(* trailing comment *)\n";
    }

    fs::path output = tmpdir / "p_comment_after_include.piec";
    JieppOptions opts;
    opts.input_filepaths = {main_iec.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_NE(std::string::npos, content.find("(* trailing comment *)"))
        << "-P incorrectly dropped comment immediately after #include";
    EXPECT_NE(std::string::npos, content.find("x: INT"))
        << "included content missing";
}

// ─── F1: {#pragma once} ────────────────────────────────────────────────

TEST_F(JieppCommandTest, PragmaOnceBasic) {
    // A file with {#pragma once} included twice must emit its content only once.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path hdr = tmpdir / "po_basic_hdr.iec";
    {
        std::ofstream f(hdr);
        f << "{#pragma once}\nVAR x: INT; END_VAR\n";
    }
    fs::path main_iec = tmpdir / "po_basic_main.iec";
    {
        std::ofstream f(main_iec);
        f << "{#include '" << hdr.generic_string() << "'}\n";
        f << "{#include '" << hdr.generic_string() << "'}\n";
    }
    fs::path output = tmpdir / "po_basic.piec";
    JieppOptions opts;
    opts.input_filepaths = {main_iec.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    // "x : INT" should appear exactly once
    auto pos1 = content.find("x: INT");
    ASSERT_NE(std::string::npos, pos1) << "header content missing";
    auto pos2 = content.find("x: INT", pos1 + 1);
    EXPECT_EQ(std::string::npos, pos2) << "{#pragma once} failed: content duplicated";
}

TEST_F(JieppCommandTest, PragmaOnceDiamond) {
    // Diamond include: A→B, A→C, B→hdr{#pragma once}, C→hdr
    // hdr content should appear only once.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path hdr = tmpdir / "po_dia_hdr.iec";
    {
        std::ofstream f(hdr);
        f << "{#pragma once}\nVAR diamond: INT; END_VAR\n";
    }
    fs::path b = tmpdir / "po_dia_b.iec";
    {
        std::ofstream f(b);
        f << "{#include '" << hdr.generic_string() << "'}\n";
    }
    fs::path c = tmpdir / "po_dia_c.iec";
    {
        std::ofstream f(c);
        f << "{#include '" << hdr.generic_string() << "'}\n";
    }
    fs::path main_iec = tmpdir / "po_dia_main.iec";
    {
        std::ofstream f(main_iec);
        f << "{#include '" << b.generic_string() << "'}\n";
        f << "{#include '" << c.generic_string() << "'}\n";
    }
    fs::path output = tmpdir / "po_dia.piec";
    JieppOptions opts;
    opts.input_filepaths = {main_iec.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    auto pos1 = content.find("diamond: INT");
    ASSERT_NE(std::string::npos, pos1) << "diamond header content missing";
    auto pos2 = content.find("diamond: INT", pos1 + 1);
    EXPECT_EQ(std::string::npos, pos2) << "{#pragma once} diamond failed: content duplicated";
}

TEST_F(JieppCommandTest, PragmaOnceSelfInclude) {
    // A file with {#pragma once} that includes itself must not loop.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path selfhdr = tmpdir / "po_self.iec";
    {
        std::ofstream f(selfhdr);
        f << "{#pragma once}\nVAR self: INT; END_VAR\n";
        f << "{#include '" << selfhdr.generic_string() << "'}\n";
    }
    fs::path output = tmpdir / "po_self.piec";
    JieppOptions opts;
    opts.input_filepaths = {selfhdr.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    auto pos1 = content.find("self: INT");
    ASSERT_NE(std::string::npos, pos1) << "content missing";
    auto pos2 = content.find("self: INT", pos1 + 1);
    EXPECT_EQ(std::string::npos, pos2) << "self-include with pragma once duplicated content";
}

TEST_F(JieppCommandTest, PragmaOnceUnknownArgSilentNop) {
    // {#pragma foo} (unknown arg) must be silently ignored — no error.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "po_unknown.iec";
    {
        std::ofstream f(src);
        f << "{#pragma foo}\nVAR y: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "po_unknown.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, content.find("y: INT"));
}

// ─── F5: -dD ────────────────────────────────────────────────────────────────

TEST_F(JieppCommandTest, DDEmitsDefineInline) {
    // -dD: {#define} lines must appear inline in the preprocessed output.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "dd_define.iec";
    {
        std::ofstream f(src);
        f << "{#define FOO 42}\nVAR x: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "dd_define.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;
    opts.dD = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_NE(std::string::npos, content.find("{#define FOO 42}")) << "define line missing from -dD output";
    EXPECT_NE(std::string::npos, content.find("x: INT"))           << "body content missing";
}

TEST_F(JieppCommandTest, DDEmitsUndefInline) {
    // -dD: {#undef} lines must also appear inline.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "dd_undef.iec";
    {
        std::ofstream f(src);
        f << "{#define BAR 1}\n{#undef BAR}\nVAR z: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "dd_undef.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;
    opts.dD = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_NE(std::string::npos, content.find("{#define BAR 1}")) << "#define missing";
    EXPECT_NE(std::string::npos, content.find("{#undef BAR}"))    << "#undef missing";
    EXPECT_NE(std::string::npos, content.find("z: INT"))          << "body missing";
}

TEST_F(JieppCommandTest, DDInactiveIfBranchNotEmitted) {
    // -dD: defines inside an inactive #if branch must NOT appear.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "dd_inactive.iec";
    {
        std::ofstream f(src);
        f << "{#if 0}\n{#define HIDDEN 99}\n{#endif}\n";
        f << "{#define VISIBLE 1}\n";
    }
    fs::path output = tmpdir / "dd_inactive.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;
    opts.dD = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("HIDDEN"))    << "inactive define emitted";
    EXPECT_NE(std::string::npos, content.find("{#define VISIBLE 1}")) << "active define missing";
}

TEST_F(JieppCommandTest, DDWithPFlagPreservesDefines) {
    // -dD -P: line markers suppressed, {#define} lines must remain.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "dd_p.iec";
    {
        std::ofstream f(src);
        f << "{#define X 10}\nVAR v: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "dd_p.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.dD = true;
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("{#:"))        << "line marker survived -P";
    EXPECT_EQ(std::string::npos, content.find("(*{#:"))      << "annotated line marker survived -P";
    EXPECT_NE(std::string::npos, content.find("{#define X 10}")) << "define missing with -P -dD";
}

TEST_F(JieppCommandTest, DDDefaultFalse) {
    // Without -dD, no inline {#define} lines should appear.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "dd_default.iec";
    {
        std::ofstream f(src);
        f << "{#define Y 5}\nVAR w: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "dd_default.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;
    // opts.dD = false (default)

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    EXPECT_EQ(std::string::npos, content.find("{#define"))   << "define appeared without -dD";
    EXPECT_NE(std::string::npos, content.find("w: INT"))     << "body missing";
}

TEST_F(JieppCommandTest, DDDollarEscapePreserved) {
    // Regression: raw_arg is decoded; -dD must re-emit the original token text, not the
    // decoded form, to avoid corrupting dollar-escape sequences (e.g. $$ -> $).
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "dd_dollar.iec";
    {
        std::ofstream f(src);
        // {#define DOLLAR $$} — body is a literal '$' encoded as '$$'
        f << "{#define DOLLAR $$}\n";
    }
    fs::path output = tmpdir / "dd_dollar.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;
    opts.dD = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());

    // Must preserve '$$' encoding, not emit decoded '$'
    EXPECT_NE(std::string::npos, content.find("{#define DOLLAR $$}"))
        << "-dD corrupted dollar-escape: expected {#define DOLLAR $$}";
}

// ─── F6: -MD / -MMD ─────────────────────────────────────────────────────────

TEST_F(JieppCommandTest, MDWritesDepFileAndOutput) {
    // -MD: preprocessed output written AND .d file auto-created.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "md_test.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path output  = tmpdir / "md_test.piec";
    fs::path dep_out = tmpdir / "md_test.d";   // auto-derived from input stem
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    // Preprocessed output must exist and contain body
    std::ifstream pf(output, std::ios::binary);
    std::string pout((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, pout.find("x: INT")) << "preprocessed output missing";

    // .d file must have been written
    ASSERT_TRUE(fs::exists(dep_out)) << ".d file not created by -MD";
    std::ifstream df(dep_out, std::ios::binary);
    std::string dcontent((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, dcontent.find(src.stem().generic_string())) << "dep file missing target";
    EXPECT_NE(std::string::npos, dcontent.find(src.generic_string()))        << "dep file missing source";
}

TEST_F(JieppCommandTest, MDWithMFOverridesDepFile) {
    // -MD -MF custom.d: explicit -MF wins over auto-derived name.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "md_mf.iec";
    {
        std::ofstream f(src);
        f << "VAR y: INT; END_VAR\n";
    }
    fs::path output   = tmpdir / "md_mf.piec";
    fs::path dep_custom = tmpdir / "custom_mf.d";
    fs::remove(dep_custom);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep_custom.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    EXPECT_TRUE(fs::exists(dep_custom)) << "custom dep file not created";
    EXPECT_FALSE(fs::exists(tmpdir / "md_mf.d")) << "auto-named dep file should not exist";

    // Target name must be derived from the input file, not from -MF filename.
    std::string dep_content;
    {
        std::ifstream f(dep_custom);
        ASSERT_TRUE(f) << "cannot open dep file";
        dep_content.assign(std::istreambuf_iterator<char>(f), {});
    }
    EXPECT_NE(dep_content.find("md_mf.output:"), std::string::npos)
        << "target should be derived from input file, got: " << dep_content;
    EXPECT_EQ(dep_content.find("custom_mf.output:"), std::string::npos)
        << "-MF should not affect target name, got: " << dep_content;
}

TEST_F(JieppCommandTest, MMDExcludesSyspaths) {
    // -MMD: system include (syspath) deps should NOT appear in the .d file.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    // Create a "system" header in a separate dir
    fs::path sysdir = tmpdir / "mmd_sysdir";
    fs::create_directories(sysdir);
    fs::path syshdr = sysdir / "syshdr.iec";
    {
        std::ofstream f(syshdr);
        f << "VAR sys: INT; END_VAR\n";
    }
    // User header
    fs::path userhdr = tmpdir / "mmd_user.iec";
    {
        std::ofstream f(userhdr);
        f << "VAR user: INT; END_VAR\n";
    }
    fs::path src = tmpdir / "mmd_main.iec";
    {
        std::ofstream f(src);
        f << "{#include '" << userhdr.generic_string() << "'}\n";
        f << "{#sinclude 'syshdr.iec'}\n";
    }
    fs::path output  = tmpdir / "mmd_main.piec";
    fs::path dep_out = tmpdir / "mmd_main.d";
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.MMD = true;
    opts.dep_mode = DepMode::USER;
    opts.syspaths = {sysdir.generic_string()};
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    ASSERT_TRUE(fs::exists(dep_out)) << ".d file not created by -MMD";
    std::ifstream df(dep_out, std::ios::binary);
    std::string dcontent((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, dcontent.find("mmd_user.iec")) << "user dep missing";
    EXPECT_EQ(std::string::npos, dcontent.find("syshdr.iec"))   << "-MMD included syspath dep";
}

TEST_F(JieppCommandTest, MDDefaultFalse) {
    // Without -MD, no .d file should be created.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();

    fs::path src = tmpdir / "md_nomd.iec";
    {
        std::ofstream f(src);
        f << "VAR z: INT; END_VAR\n";
    }
    fs::path output  = tmpdir / "md_nomd.piec";
    fs::path dep_out = tmpdir / "md_nomd.d";
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    EXPECT_FALSE(fs::exists(dep_out)) << ".d file created without -MD";
}

// ─── F3: __VA_OPT__ ─────────────────────────────────────────────────────────

TEST_F(JieppCommandTest, VaOptBasicEmpty) {
    // __VA_OPT__(content) with empty VA_ARGS: content must NOT be emitted.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_empty.iec";
    {
        std::ofstream f(src);
        f << "{#define F(...) __VA_OPT__(PRESENT)}\nF()\n";
    }
    fs::path output = tmpdir / "vaopt_empty.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());
    EXPECT_EQ(std::string::npos, content.find("PRESENT")) << "__VA_OPT__ emitted content with empty VA_ARGS";
}

TEST_F(JieppCommandTest, VaOptBasicNonEmpty) {
    // __VA_OPT__(content) with non-empty VA_ARGS: content MUST be emitted.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_nonempty.iec";
    {
        std::ofstream f(src);
        f << "{#define F(...) __VA_OPT__(PRESENT)}\nF(a)\n";
    }
    fs::path output = tmpdir / "vaopt_nonempty.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, content.find("PRESENT")) << "__VA_OPT__ did not emit content with non-empty VA_ARGS";
}

TEST_F(JieppCommandTest, VaOptEmptyBody) {
    // __VA_OPT__() with non-empty VA_ARGS: empty body still expands to nothing.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_emptybody.iec";
    {
        std::ofstream f(src);
        // F(x) with empty __VA_OPT__ body should produce nothing (no PREFIX, no SUFFIX)
        f << "{#define F(...) PREFIX __VA_OPT__() SUFFIX}\nF(x)\n";
    }
    fs::path output = tmpdir / "vaopt_emptybody.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, content.find("PREFIX")) << "PREFIX missing";
    EXPECT_NE(std::string::npos, content.find("SUFFIX")) << "SUFFIX missing";
}

TEST_F(JieppCommandTest, VaOptGlue) {
    // __VA_OPT__ in paste context: G() → 'x', G(a) → 'xy'.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_glue.iec";
    {
        std::ofstream f(src);
        f << "{#define G(...) x @@ __VA_OPT__(y)}\n"
          << "G();\n"
          << "G(a);\n";
    }
    fs::path output = tmpdir / "vaopt_glue.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());
    // G() → 'x' (no paste with empty __VA_OPT__ body)
    EXPECT_NE(std::string::npos, content.find("x;")) << "G() should produce 'x'";
    // G(a) → 'xy' (paste of x and y)
    EXPECT_NE(std::string::npos, content.find("xy;")) << "G(a) should produce 'xy'";
}

TEST_F(JieppCommandTest, VaOptStringize) {
    // __VA_OPT__ in stringize context: S() → '', S(hello) → 'hello'.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_stringize.iec";
    {
        std::ofstream f(src);
        f << "{#define S(...) @__VA_OPT__(__VA_ARGS__)}\n"
          << "S();\n"
          << "S(hello);\n";
    }
    fs::path output = tmpdir / "vaopt_stringize.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream of(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(of)), std::istreambuf_iterator<char>());
    // S() → stringize of empty → '' (empty IEC string)
    EXPECT_NE(std::string::npos, content.find("'';")) << "S() should produce empty string ''";
    // S(hello) → stringize of 'hello' → 'hello'
    EXPECT_NE(std::string::npos, content.find("'hello';")) << "S(hello) should produce 'hello'";
}

TEST_F(JieppCommandTest, VaOptOutsideVariadic) {
    // __VA_OPT__ in a non-variadic macro body must produce a PP37 error.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_nonvariadic.iec";
    {
        std::ofstream f(src);
        f << "{#define BAD() __VA_OPT__(X)}\n"
          << "BAD()\n";
    }
    fs::path output = tmpdir / "vaopt_nonvariadic.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_NE(0, jiepp_command(opts)) << "expected PP37 error for __VA_OPT__ outside variadic macro";
    EXPECT_NE(std::string::npos, message().find("PP37")) << "PP37 not in error message";
}

TEST_F(JieppCommandTest, VaOptNested) {
    // Nested __VA_OPT__ inside __VA_OPT__(...) must produce a PP37 error.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "vaopt_nested.iec";
    {
        std::ofstream f(src);
        f << "{#define N(...) __VA_OPT__(__VA_OPT__(X))}\n"
          << "N(a)\n";
    }
    fs::path output = tmpdir / "vaopt_nested.piec";
    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_NE(0, jiepp_command(opts)) << "expected PP37 error for nested __VA_OPT__";
    EXPECT_NE(std::string::npos, message().find("PP37")) << "PP37 not in error message";
}

// ─── B11/B12: -o safety (open only after expansion succeeds) ───────────────

TEST_F(JieppCommandTest, OutputSameAsInputRoundTrips) {
    // B11: -o naming the same path as the input file must not lose the
    // input content -- the whole preprocessed output is now accumulated in
    // memory before -o is (re)opened, so the input is fully read before its
    // own path is truncated.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path self_path = tmpdir / "self_roundtrip.iec";
    {
        std::ofstream f(self_path);
        f << "x := 1;\ny := 2;\n";
    }

    JieppOptions opts;
    opts.input_filepaths = {self_path.generic_string()};
    opts.output_filepath = self_path.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream f(self_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, content.find("x := 1;")) << "input content lost when -o == input path";
    EXPECT_NE(std::string::npos, content.find("y := 2;")) << "input content lost when -o == input path";
}

TEST_F(JieppCommandTest, OutputFileNotTruncatedOnFailure) {
    // B12: a failing run must not truncate a pre-existing -o target. Since
    // -o is now opened only after expansion has fully succeeded, a failure
    // during expansion (before -o is ever opened) leaves the target
    // completely untouched.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "fail_err.iec";
    {
        std::ofstream f(src);
        f << "before;\n{#error boom}\nafter;\n";
    }
    fs::path output = tmpdir / "fail_stale.out";
    const std::string preexisting = "PREEXISTING\n";
    {
        std::ofstream f(output, std::ios::binary);
        f << preexisting;
    }

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.no_line_markers = true;

    ASSERT_NE(0, jiepp_command(opts)) << "expected failure for {#error}";

    ASSERT_TRUE(fs::exists(output)) << "-o target should still exist after a failing run";
    std::ifstream f(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(preexisting, content) << "-o target was truncated on a failing run";
}

TEST_F(JieppCommandTest, DepFileFailureLeavesMainOutputUntouched) {
    // F2: the separate dependency file (-MF, or auto-named by -MD/-MMD) is
    // now written before -o is opened, so a failure writing it (e.g. -MF
    // names a file inside a nonexistent directory) must leave a pre-existing
    // -o target completely untouched -- not deleted (the old bug: a
    // perfectly good, just-written -o used to be removed by the catch block
    // solely because of this unrelated, later failure) and not overwritten
    // with fresh content either, since -o is never opened at all in this path.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "dep_fail_main.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "dep_fail_main.piec";
    const std::string preexisting = "PREEXISTING\n";
    {
        std::ofstream f(output, std::ios::binary);
        f << preexisting;
    }

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = "/nonexistent/deeply/nested/path/dep_fail_main.d";
    opts.no_line_markers = true;

    ASSERT_NE(0, jiepp_command(opts)) << "expected failure writing the dep file";

    ASSERT_TRUE(fs::exists(output)) << "-o target should still exist after a dep-file failure";
    std::ifstream f(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(preexisting, content) << "-o target was modified by an unrelated dep-file failure";
}

TEST_F(JieppCommandTest, OutputOpenFailureRemovesFreshDepFile) {
    // Companion to DepFileFailureLeavesMainOutputUntouched, covering the
    // opposite failure order: the separate dependency file (-MD, in this
    // case) is written successfully first -- per the F2 ordering, the
    // dep file is always written before -o is opened -- but the later
    // opening of -o then fails (nonexistent directory). Without cleanup
    // this would leave a well-formed .d file on disk naming an output that
    // this run never produced, which a Make-based build could mistake for
    // proof that the target is already up to date. This run created the
    // dep file fresh (it did not exist beforehand), so it must be removed;
    // a dep file left over from an earlier, unrelated run that this run
    // never touched would not be (see the catch-site comment in jiepp.cpp).
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "output_open_fail.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path dep = tmpdir / "output_open_fail.d";
    std::error_code ec;
    fs::remove(dep, ec);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = "/nonexistent/deeply/nested/path/output_open_fail.piec";
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep.generic_string();

    ASSERT_NE(0, jiepp_command(opts)) << "expected failure opening -o";

    EXPECT_FALSE(fs::exists(dep)) << "fresh dep file written by this failed run should have been removed";
}

// ─── B16: -M / -MM / -MD / -MMD / -dM output gating ────────────────────────

TEST_F(JieppCommandTest, MSuppressesPreprocessedOutput) {
    // B16: plain -M (dep_mode ALL without -MD/-MMD) must suppress the
    // preprocessed output. F2 addendum: when a separate dep file (-MF) is
    // also given, -o has nothing left to receive (no preprocessed body, and
    // no dep rule either -- that went to -MF), so -o must not even be
    // created; this supersedes the old "created but empty" behavior, which
    // used to truncate/create a 0-byte -o for no functional reason.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "m_alone.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path output  = tmpdir / "m_alone.piec";
    fs::path dep_out = tmpdir / "m_alone.d";
    fs::remove(output);
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep_out.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    // -o has nothing to write (preprocessed body suppressed by -M, dep rule
    // already went to -MF) and must not be created at all.
    EXPECT_FALSE(fs::exists(output)) << "plain -M with -MF must not create -o";

    ASSERT_TRUE(fs::exists(dep_out));
    std::ifstream df(dep_out, std::ios::binary);
    std::string dcontent((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, dcontent.find(src.generic_string())) << "dep file missing source";
}

TEST_F(JieppCommandTest, MOnlyWithOutputWritesRuleToOutput) {
    // F2 addendum: plain -M (no -MD/-MMD) with -o and no -MF has nowhere
    // else to put the dependency rule, so (matching gcc) the rule itself is
    // written into -o; the preprocessed body remains suppressed.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "m_only_out.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "m_only_out.d";
    fs::remove(output);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.dep_mode = DepMode::ALL;
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    ASSERT_TRUE(fs::exists(output));
    std::ifstream pf(output, std::ios::binary);
    std::string pout((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    EXPECT_EQ(std::string::npos, pout.find("x: INT")) << "preprocessed body must stay suppressed, got: " << pout;
    EXPECT_NE(std::string::npos, pout.find(src.generic_string())) << "dependency rule should be written to -o, got: " << pout;
}

TEST_F(JieppCommandTest, MDKeepsPreprocessedOutput) {
    // -MD combines dependency output with normal preprocessed output --
    // positive control for the MSuppressesPreprocessedOutput fix above.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "md_keeps.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path output  = tmpdir / "md_keeps.piec";
    fs::path dep_out = tmpdir / "md_keeps.d";
    fs::remove(output);
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep_out.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream pf(output, std::ios::binary);
    std::string pout((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, pout.find("x: INT")) << "-MD must keep preprocessed output";

    ASSERT_TRUE(fs::exists(dep_out));
}

TEST_F(JieppCommandTest, DMAloneSuppressesPreprocessedOutput) {
    // -dM alone (no -M/-MM/-MD/-MMD) must dump macros only, suppressing the
    // preprocessed body -- SPECIFICATION.md already documents this; the old
    // not_out formula gated suppression on an unrelated dep-file check.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "dm_alone.iec";
    {
        std::ofstream f(src);
        f << "{#define FOO 1}\nVAR x: INT; END_VAR\n";
    }
    fs::path output = tmpdir / "dm_alone.piec";
    fs::remove(output);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.dM = true;
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream pf(output, std::ios::binary);
    std::string pout((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, pout.find("{#define FOO 1}")) << "-dM must dump macro definitions";
    EXPECT_EQ(std::string::npos, pout.find("VAR x: INT")) << "-dM alone must suppress preprocessed body";
}

// ─── I1: dependency-rule path escaping ──────────────────────────────────────

TEST_F(JieppCommandTest, DepRulePathEscaping) {
    // I1: a dependency prerequisite path containing a space must be escaped
    // as "\ " in the generated Makefile rule (gcc/clang convention);
    // otherwise a consuming make/ninja would misparse the rule.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path inc_dir = tmpdir / "dep escape dir";
    fs::create_directories(inc_dir);
    fs::path hdr = inc_dir / "esc_hdr.iec";
    {
        std::ofstream f(hdr);
        f << "VAR h: INT; END_VAR\n";
    }
    fs::path src = tmpdir / "dep_escape_main.iec";
    {
        std::ofstream f(src);
        f << "{#include 'dep escape dir/esc_hdr.iec'}\n";
    }
    fs::path output  = tmpdir / "dep_escape_main.piec";
    fs::path dep_out = tmpdir / "dep_escape_main.d";
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep_out.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    ASSERT_TRUE(fs::exists(dep_out));
    std::ifstream df(dep_out, std::ios::binary);
    std::string dcontent((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, dcontent.find("dep\\ escape\\ dir/esc_hdr.iec"))
        << "space in dependency path not escaped: " << dcontent;
    EXPECT_EQ(std::string::npos, dcontent.find("dep escape dir/esc_hdr.iec"))
        << "unescaped space present in dep rule: " << dcontent;
}

TEST_F(JieppCommandTest, DepRulePathTabIsEscaped) {
    // F13: a dependency path containing a TAB must be escaped as backslash +
    // an actual TAB byte, mirroring how a space is escaped as backslash + an
    // actual space byte (gcc/clang mkdeps munge() convention) -- not the
    // 2-character C-style "\t" sequence the old code emitted. A raw tab
    // (0x09) is a control character that Win32 itself refuses in real
    // filenames, so this test cannot use an actual tab-named file/directory
    // on disk (confirmed: creating one from a POSIX shell silently remaps
    // the byte into the Private-Use-Area surrogate range, which is not the
    // same file from a native Win32 program's point of view). --disppath
    // overrides only the *displayed* dependency path (see expand.cpp's
    // add_dependency call), independent of the real file read from disk, so
    // it exercises write_dep_rules()/escape_make_path()'s tab branch through
    // the real production seam without needing such a file to exist.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "dep_tab_main.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path dep_out = tmpdir / "dep_tab_main.d";
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.disppath = "dep\ttab\tname.iec";
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep_out.generic_string();
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    ASSERT_TRUE(fs::exists(dep_out));
    std::ifstream df(dep_out, std::ios::binary);
    std::string dcontent((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, dcontent.find("dep\\\ttab\\\tname.iec"))
        << "TAB in dependency path must be escaped as backslash + an actual TAB byte, got: " << dcontent;
    EXPECT_EQ(std::string::npos, dcontent.find("\\tname"))
        << "TAB must not be escaped as the literal 2-character \"\\t\" sequence, got: " << dcontent;
}

TEST_F(JieppCommandTest, DepTargetFromMTIsVerbatim) {
    // F10: unlike an auto-derived target, a user-supplied -MT value must be
    // emitted verbatim in the rule head (gcc/clang convention -- only the
    // unimplemented -MQ would Make-escape it), even though it contains
    // Make-special characters ('$' and a space) that would otherwise be
    // escaped if this were the auto-derived target.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "mt_verbatim.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path dep_out = tmpdir / "mt_verbatim.d";
    fs::remove(dep_out);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.dep_mode = DepMode::ALL;
    opts.dep_file = dep_out.generic_string();
    opts.dep_target = "my$target with space";
    opts.no_line_markers = true;

    ASSERT_EQ(0, jiepp_command(opts));

    ASSERT_TRUE(fs::exists(dep_out));
    std::ifstream df(dep_out, std::ios::binary);
    std::string dcontent((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
    EXPECT_EQ(0u, dcontent.find("my$target with space:"))
        << "-MT value must be emitted verbatim as the rule head, got: " << dcontent;
    EXPECT_EQ(std::string::npos, dcontent.find("my$$target"))
        << "-MT value must not be Make-escaped, got: " << dcontent;
}

// ─── F6: {#max_blank_lines N} directive ────────────────────────────────────

namespace {
// Counts occurrences of the annotated-style compaction/entry marker prefix
// in `s`. A run compiled with no compaction beyond the file's own entry
// marker yields 1; a compacted run adds one marker per compacted run.
std::size_t count_annotated_markers(const std::string& s) {
    std::size_t n = 0;
    for (std::size_t pos = 0; (pos = s.find("(*{#:", pos)) != std::string::npos; pos += 5)
        ++n;
    return n;
}
} // namespace

TEST_F(JieppCommandTest, MaxBlankLinesDirectiveLowersThreshold) {
    // F6: {#max_blank_lines N} lowers the compaction threshold below the
    // built-in default of 7 -- a run of blank lines too short to compact at
    // the default is compacted once the directive takes effect.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "max_blank_lines_directive_lowers.iec";
    {
        std::ofstream f(src);
        f << "{#max_blank_lines 2}\nA;" << std::string(4, '\n') << "B;\n";
    }
    fs::path output = tmpdir / "max_blank_lines_directive_lowers.piec";

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream f(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(2u, count_annotated_markers(content))
        << "expected the 4-line blank run to be compacted under the "
           "directive-lowered threshold (2); output:\n" << content;
}

TEST_F(JieppCommandTest, MaxBlankLinesDirectiveZeroDisables) {
    // F6: {#max_blank_lines 0} disables compaction entirely, even for a run
    // far longer than the default threshold.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "max_blank_lines_directive_zero.iec";
    {
        std::ofstream f(src);
        f << "{#max_blank_lines 0}\nA;" << std::string(20, '\n') << "B;\n";
    }
    fs::path output = tmpdir / "max_blank_lines_directive_zero.piec";

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream f(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(1u, count_annotated_markers(content))
        << "expected no compaction marker beyond the file's own entry "
           "marker; output:\n" << content;
    EXPECT_NE(std::string::npos, content.find(std::string(20, '\n')))
        << "expected the 20-line blank run to survive verbatim; output:\n" << content;
}

TEST_F(JieppCommandTest, MaxBlankLinesCliOverridesDirective) {
    // F6: a CLI --max-blank-lines value locks the parameter (fix_), so a
    // later {#max_blank_lines} directive attempting to raise it back up is a
    // silent no-op -- the CLI-supplied threshold keeps governing.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "max_blank_lines_cli_overrides.iec";
    {
        std::ofstream f(src);
        f << "{#max_blank_lines 100}\nA;" << std::string(4, '\n') << "B;\n";
    }
    fs::path output = tmpdir / "max_blank_lines_cli_overrides.piec";

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();
    opts.max_blank_lines = 2;

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream f(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(2u, count_annotated_markers(content))
        << "expected the CLI-locked threshold (2) to still govern despite "
           "the in-source directive requesting 100; output:\n" << content;
}

TEST_F(JieppCommandTest, MaxBlankLinesLastValueWins) {
    // F6: compact_blank_lines() runs exactly once, after the whole file has
    // been expanded, reading env.get_max_blank_lines() at that single
    // point -- so the LAST {#max_blank_lines} directive executed during
    // expansion governs the ENTIRE output, including blank runs that
    // occurred earlier in the source, before that directive was even seen.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "max_blank_lines_last_value_wins.iec";
    {
        std::ofstream f(src);
        f << "A;\n{#max_blank_lines 100}\n" << std::string(3, '\n')
          << "{#max_blank_lines 1}\nB;\n";
    }
    fs::path output = tmpdir / "max_blank_lines_last_value_wins.piec";

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = output.generic_string();

    ASSERT_EQ(0, jiepp_command(opts));

    std::ifstream f(output, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    EXPECT_EQ(2u, count_annotated_markers(content))
        << "expected the 3-line blank run (which occurred while 100 was in "
           "effect) to be compacted under the LAST effective value (1), "
           "confirming the whole-output post-pass semantics; output:\n" << content;
}

// ─── -o - means stdout ─────────────────────────────────────────────────────

TEST_F(JieppCommandTest, OutputDashMeansStdout) {
    // gcc/clang convention: "-o -" writes the preprocessed output to stdout,
    // not to a literal file named "-". Captured in-process by swapping
    // std::cout's streambuf (jiepp_command() always writes through
    // &std::cout when opts.output_filepath is unset or "-").
    fs::current_path(jiepp_root_dir());
    fs::path dash_marker = fs::current_path() / "-";
    std::error_code rm_ec;
    fs::remove(dash_marker, rm_ec); // defensive: clear any stray "-" up front

    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "o_dash.iec";
    {
        std::ofstream f(src);
        f << "{#define Z 7}\nout := Z;\n";
    }

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = "-";

    std::ostringstream captured;
    std::streambuf* saved_cout = std::cout.rdbuf(captured.rdbuf());
    int rc = jiepp_command(opts);
    std::cout.rdbuf(saved_cout);

    ASSERT_EQ(0, rc);
    EXPECT_NE(std::string::npos, captured.str().find("out := 7"))
        << "stdout: " << captured.str();
    EXPECT_FALSE(fs::exists(dash_marker))
        << "-o - must not create a file literally named '-'";
}

TEST_F(JieppCommandTest, OutputDashWithMDDerivesDepFileFromInput) {
    // With -o - and -MD (no -MF), the dep file name must be derived from the
    // input file's basename, like gcc, not from the literal "-" output arg.
    fs::current_path(jiepp_root_dir());
    auto tmpdir = fs::temp_directory_path();
    fs::path src = tmpdir / "o_dash_md.iec";
    {
        std::ofstream f(src);
        f << "VAR x: INT; END_VAR\n";
    }
    fs::path dep_out = tmpdir / "o_dash_md.d"; // auto-derived from input stem
    fs::remove(dep_out);
    fs::path dash_marker = fs::current_path() / "-";
    std::error_code rm_ec;
    fs::remove(dash_marker, rm_ec);
    fs::path dash_dep_marker = fs::current_path() / "-.d";
    fs::remove(dash_dep_marker, rm_ec);

    JieppOptions opts;
    opts.input_filepaths = {src.generic_string()};
    opts.output_filepath = "-";
    opts.MD = true;
    opts.dep_mode = DepMode::ALL;

    std::ostringstream captured;
    std::streambuf* saved_cout = std::cout.rdbuf(captured.rdbuf());
    int rc = jiepp_command(opts);
    std::cout.rdbuf(saved_cout);

    ASSERT_EQ(0, rc);
    EXPECT_TRUE(fs::exists(dep_out))
        << "expected dep file derived from input basename: " << dep_out.generic_string();
    EXPECT_FALSE(fs::exists(dash_dep_marker))
        << "-o - must not derive a dep file named '-.d'";
    EXPECT_FALSE(fs::exists(dash_marker));
}
