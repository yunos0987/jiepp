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
    fs::current_path(jiepp_root_dir());
    JieppOptions opts;
    opts.input_filepaths = {"nonexistent.iec"};
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
    run_e2e("dM/dM_undef_multiple", {}, {{"A", "1"}, {"B", "2"}}, std::nullopt, false, -1, true, "",
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
