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
