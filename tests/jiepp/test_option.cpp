#include "test_helper.hpp"
#include "jiepp/option.hpp"

class OptionTest : public JieppTest {};

// ---- define_macro_option ----

TEST_F(OptionTest, DefineMacroWithValue) {
    auto [name, val] = define_macro_option("FOO=bar");
    EXPECT_EQ(name, "FOO");
    EXPECT_EQ(val, "bar");
}

TEST_F(OptionTest, DefineMacroWithoutValue) {
    auto [name, val] = define_macro_option("DEBUG");
    EXPECT_EQ(name, "DEBUG");
    EXPECT_EQ(val, "1");
}

TEST_F(OptionTest, DefineMacroEmptyValue) {
    auto [name, val] = define_macro_option("X=");
    EXPECT_EQ(name, "X");
    EXPECT_EQ(val, "");
}

TEST_F(OptionTest, DefineMacroValueWithEquals) {
    auto [name, val] = define_macro_option("A=1+2=3");
    EXPECT_EQ(name, "A");
    EXPECT_EQ(val, "1+2=3");
}

// ---- parse_args ----

TEST_F(OptionTest, ParseArgsEmpty) {
    char* argv[] = {const_cast<char*>("jiepp")};
    auto opts = parse_args(1, argv);
    EXPECT_TRUE(opts.input_filepaths.empty());
    EXPECT_FALSE(opts.output_filepath.has_value());
    EXPECT_FALSE(opts.dM);
    EXPECT_FALSE(opts.silent);
    EXPECT_FALSE(opts.remove_comments);
}

TEST_F(OptionTest, ParseArgsInputFile) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("input.iec")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(opts.input_filepaths.size(), 1u);
    EXPECT_EQ(opts.input_filepaths[0], "input.iec");
}

TEST_F(OptionTest, ParseArgsStdinDash) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(opts.input_filepaths.size(), 1u);
    EXPECT_EQ(opts.input_filepaths[0], "-");
}

TEST_F(OptionTest, ParseArgsDJoined) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-DFOO=1")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(opts.define_macros.size(), 1u);
    EXPECT_EQ(opts.define_macros[0], "FOO=1");
}

TEST_F(OptionTest, ParseArgsDSeparate) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-D"), const_cast<char*>("BAR=2")};
    auto opts = parse_args(3, argv);
    ASSERT_EQ(opts.define_macros.size(), 1u);
    EXPECT_EQ(opts.define_macros[0], "BAR=2");
}

TEST_F(OptionTest, ParseArgsIJoined) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-I/usr/inc")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(opts.syspaths.size(), 1u);
    EXPECT_EQ(opts.syspaths[0], "/usr/inc");
}

TEST_F(OptionTest, ParseArgsISeparate) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-I"), const_cast<char*>("/lib")};
    auto opts = parse_args(3, argv);
    ASSERT_EQ(opts.syspaths.size(), 1u);
    EXPECT_EQ(opts.syspaths[0], "/lib");
}

TEST_F(OptionTest, ParseArgsOutputOption) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-o"), const_cast<char*>("out.iec")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.output_filepath.has_value());
    EXPECT_EQ(*opts.output_filepath, "out.iec");
}

TEST_F(OptionTest, ParseArgsBoolFlags) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--remove-comments"),
                    const_cast<char*>("-dM"),
                    const_cast<char*>("--silent")};
    auto opts = parse_args(4, argv);
    EXPECT_TRUE(opts.remove_comments);
    EXPECT_TRUE(opts.dM);
    EXPECT_TRUE(opts.silent);
}

TEST_F(OptionTest, ParseArgsRemoveCommentsShort) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-nC")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.remove_comments);
}

TEST_F(OptionTest, ParseArgsMaxOptions) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max-include-depth"), const_cast<char*>("10"),
                    const_cast<char*>("--max-expansion-depth"), const_cast<char*>("32"),
                    const_cast<char*>("--max-if-nesting"), const_cast<char*>("16")};
    auto opts = parse_args(9, argv);
    ASSERT_TRUE(opts.max_include_depth.has_value());
    EXPECT_EQ(*opts.max_include_depth, 10);
    ASSERT_TRUE(opts.max_expansion_depth.has_value());
    EXPECT_EQ(*opts.max_expansion_depth, 32);
    ASSERT_TRUE(opts.max_if_nesting.has_value());
    EXPECT_EQ(*opts.max_if_nesting, 16);
}

TEST_F(OptionTest, ParseArgsSnakeCaseVariants) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max_include_depth"), const_cast<char*>("5")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.max_include_depth.has_value());
    EXPECT_EQ(*opts.max_include_depth, 5);
}

TEST_F(OptionTest, ParseArgsRecursionLimit) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--recursion-limit"), const_cast<char*>("100")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.recursion_limit.has_value());
    EXPECT_EQ(*opts.recursion_limit, 100);
}

TEST_F(OptionTest, ParseArgsPragmaStyle) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--pp-output-pragma-style"),
                    const_cast<char*>("annotated")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.pp_output_pragma_style.has_value());
    EXPECT_EQ(*opts.pp_output_pragma_style, "annotated");
}

// ---- New CLI options ----

TEST_F(OptionTest, ParseArgsUJoined) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-UFOO")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(opts.undef_macros.size(), 1u);
    EXPECT_EQ(opts.undef_macros[0], "FOO");
}

TEST_F(OptionTest, ParseArgsUSeparate) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-U"), const_cast<char*>("BAR")};
    auto opts = parse_args(3, argv);
    ASSERT_EQ(opts.undef_macros.size(), 1u);
    EXPECT_EQ(opts.undef_macros[0], "BAR");
}

TEST_F(OptionTest, ParseArgsIncludeFile) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-include"), const_cast<char*>("header.iec")};
    auto opts = parse_args(3, argv);
    ASSERT_EQ(opts.include_filepaths.size(), 1u);
    EXPECT_EQ(opts.include_filepaths[0], "header.iec");
}

TEST_F(OptionTest, ParseArgsWarnSuppress) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-w")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.suppress_warnings);
}

TEST_F(OptionTest, ParseArgsWerror) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-Werror")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.werror);
}

TEST_F(OptionTest, ParseArgsDoubleDash) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--"),
                    const_cast<char*>("-weird.iec")};
    auto opts = parse_args(3, argv);
    ASSERT_EQ(opts.input_filepaths.size(), 1u);
    EXPECT_EQ(opts.input_filepaths[0], "-weird.iec");
}

TEST_F(OptionTest, ParseArgsDoubleDashMultiple) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-DFOO"),
                    const_cast<char*>("--"),
                    const_cast<char*>("-bar.iec"),
                    const_cast<char*>("--baz.iec")};
    auto opts = parse_args(5, argv);
    ASSERT_EQ(opts.define_macros.size(), 1u);
    EXPECT_EQ(opts.define_macros[0], "FOO");
    ASSERT_EQ(opts.input_filepaths.size(), 2u);
    EXPECT_EQ(opts.input_filepaths[0], "-bar.iec");
    EXPECT_EQ(opts.input_filepaths[1], "--baz.iec");
}

TEST_F(OptionTest, ParseArgsDoubleDashEmpty) {
    // T5: bare -- with no following arguments
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("--")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.input_filepaths.empty());
}

TEST_F(OptionTest, ParseArgsDepM) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-M")};
    auto opts = parse_args(2, argv);
    EXPECT_EQ(opts.dep_mode, DepMode::ALL);
}

TEST_F(OptionTest, ParseArgsDepMM) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-MM")};
    auto opts = parse_args(2, argv);
    EXPECT_EQ(opts.dep_mode, DepMode::USER);
}

TEST_F(OptionTest, ParseArgsDepMF) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-MF"), const_cast<char*>("deps.d")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.dep_file.has_value());
    EXPECT_EQ(*opts.dep_file, "deps.d");
}

TEST_F(OptionTest, ParseArgsDepMT) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-MT"), const_cast<char*>("target.o")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.dep_target.has_value());
    EXPECT_EQ(*opts.dep_target, "target.o");
}

TEST_F(OptionTest, ParseArgsMultipleInclude) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-include"), const_cast<char*>("a.iec"),
                    const_cast<char*>("-include"), const_cast<char*>("b.iec"),
                    const_cast<char*>("main.iec")};
    auto opts = parse_args(6, argv);
    ASSERT_EQ(2u, opts.include_filepaths.size());
    EXPECT_EQ("a.iec", opts.include_filepaths[0]);
    EXPECT_EQ("b.iec", opts.include_filepaths[1]);
    ASSERT_EQ(1u, opts.input_filepaths.size());
}

TEST_F(OptionTest, CLICodeSeverityClassification) {
    // All PP70-76 (CLI/Option) codes should be ERROR severity
    EXPECT_TRUE(Issue::is_error(Issue::Code::UNKNOWN_OPTION));
    EXPECT_TRUE(Issue::is_error(Issue::Code::INVALID_OPTION_VALUE));
    EXPECT_TRUE(Issue::is_error(Issue::Code::MISSING_OPTION_VALUE));
    EXPECT_TRUE(Issue::is_error(Issue::Code::INVALID_MACRO_DEF));
    EXPECT_TRUE(Issue::is_error(Issue::Code::RECURSION_LIMIT_RANGE));
    EXPECT_TRUE(Issue::is_error(Issue::Code::THREAD_CREATE_FAILED));
    EXPECT_TRUE(Issue::is_error(Issue::Code::STACK_LIMIT_FAILED));
}
