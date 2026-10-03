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
    auto opts = parse_args(7, argv);
    ASSERT_TRUE(opts.max_include_depth.has_value());
    EXPECT_EQ(*opts.max_include_depth, 10);
    ASSERT_TRUE(opts.max_expansion_depth.has_value());
    EXPECT_EQ(*opts.max_expansion_depth, 32);
    ASSERT_TRUE(opts.max_if_nesting.has_value());
    EXPECT_EQ(*opts.max_if_nesting, 16);
}

TEST_F(OptionTest, ParseArgsMaxExpansionSteps) {
    {
        char* argv[] = {const_cast<char*>("jiepp"),
                        const_cast<char*>("--max-expansion-steps"), const_cast<char*>("5")};
        auto opts = parse_args(3, argv);
        ASSERT_TRUE(opts.max_expansion_steps.has_value());
        EXPECT_EQ(*opts.max_expansion_steps, 5);
    }
    {
        // Snake-case alias; 0 (no limit) is valid, unlike the depth options.
        char* argv[] = {const_cast<char*>("jiepp"),
                        const_cast<char*>("--max_expansion_steps"), const_cast<char*>("0")};
        auto opts = parse_args(3, argv);
        ASSERT_TRUE(opts.max_expansion_steps.has_value());
        EXPECT_EQ(*opts.max_expansion_steps, 0);
    }
    {
        // Above 2^24 (PP04's cap for depth-like options does not apply).
        char* argv[] = {const_cast<char*>("jiepp"),
                        const_cast<char*>("--max-expansion-steps"), const_cast<char*>("2147483647")};
        auto opts = parse_args(3, argv);
        ASSERT_TRUE(opts.max_expansion_steps.has_value());
        EXPECT_EQ(*opts.max_expansion_steps, 2147483647);
    }
    {
        char* argv[] = {const_cast<char*>("jiepp")};
        auto opts = parse_args(1, argv);
        EXPECT_FALSE(opts.max_expansion_steps.has_value());
    }
}

TEST_F(OptionTest, ParseArgsMaxExpansionStepsInvalidValues) {
    for (const char* bad : {"-1", "abc", "3x", "2147483648"}) {
        char* argv[] = {const_cast<char*>("jiepp"),
                        const_cast<char*>("--max-expansion-steps"), const_cast<char*>(bad)};
        EXPECT_THROW(parse_args(3, argv), Issue::Exception) << bad;
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << bad;
        EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << bad << ": " << msgs[0];
    }
}

TEST_F(OptionTest, ParseArgsMaxExpansionStepsMissingValue) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("--max-expansion-steps")};
    EXPECT_THROW(parse_args(2, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code());
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

TEST_F(OptionTest, ParseArgsPragmaStyleInvalid) {
    // Unlike the in-source {#pp-output-pragma-style} directive (a non-fatal
    // WARNING), an invalid CLI value is a hard startup ERROR, matching
    // gcc/clang's convention for bad enum-like option arguments.
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--pp-output-pragma-style"),
                    const_cast<char*>("bogus")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_OPTION_VALUE, code());
}

// ---- F5: --max-blank-lines CLI option ----

TEST_F(OptionTest, ParseArgsMaxBlankLines) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max-blank-lines"), const_cast<char*>("4")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.max_blank_lines.has_value());
    EXPECT_EQ(*opts.max_blank_lines, 4);
}

TEST_F(OptionTest, ParseArgsMaxBlankLinesZero) {
    // 0 is a valid, meaningful value (disables compaction entirely), unlike
    // the positive-only max-include-depth/max-expansion-depth/max-if-nesting.
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max-blank-lines"), const_cast<char*>("0")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.max_blank_lines.has_value());
    EXPECT_EQ(*opts.max_blank_lines, 0);
}

TEST_F(OptionTest, ParseArgsMaxBlankLinesNegative) {
    // F4: exactly one diagnostic, not the old double/contradictory pair
    // ("must be a non-negative integer" followed by "requires a valid
    // integer" for input that in fact parsed fine). Note: messages()/code()/
    // message() all drain the same DiagBox buffer on read, so capture
    // messages() exactly once and derive every assertion from that snapshot.
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max-blank-lines"), const_cast<char*>("-3")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size()) << "expected exactly one diagnostic";
    EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << msgs[0];
    EXPECT_NE(std::string::npos, msgs[0].find("must be a non-negative integer")) << msgs[0];
}

TEST_F(OptionTest, ParseArgsMaxBlankLinesNonNumeric) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max-blank-lines"), const_cast<char*>("abc")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size()) << "expected exactly one diagnostic";
    EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << msgs[0];
    EXPECT_NE(std::string::npos, msgs[0].find("requires a valid integer")) << msgs[0];
}

TEST_F(OptionTest, ParseArgsMaxBlankLinesTrailingGarbage) {
    // F4: std::stoi alone would silently accept "3abc" as 3; the added
    // pos == arg.size() check rejects trailing non-numeric garbage.
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max-blank-lines"), const_cast<char*>("3abc")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size()) << "expected exactly one diagnostic";
    EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << msgs[0];
    EXPECT_NE(std::string::npos, msgs[0].find("requires a valid integer")) << msgs[0];
}

TEST_F(OptionTest, ParseArgsMaxBlankLinesMissingValue) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("t.iec"),
                    const_cast<char*>("--max-blank-lines")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code());
}

TEST_F(OptionTest, ParseArgsMaxBlankLinesUnderscoreAlias) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max_blank_lines"), const_cast<char*>("2")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.max_blank_lines.has_value());
    EXPECT_EQ(*opts.max_blank_lines, 2);
}

// ---- F8: -D/-U missing-value diagnostics ----

TEST_F(OptionTest, ParseArgsDefineMissingValueIsError) {
    // F8: a bare trailing -D (no name, no value) must fail fast with
    // MISSING_OPTION_VALUE, symmetric with -U's existing check, instead of
    // silently pushing an empty spec that only surfaces later (and with a
    // misleading "malformed macro" message) via define_macro_option().
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("t.iec"),
                    const_cast<char*>("-D")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code());
}

TEST_F(OptionTest, ParseArgsDefineEmptyArgvValueIsError) {
    // "-D" followed by a separate, empty-string argv token (e.g. `-D ""`
    // from a shell) previously slipped past the empty-spec check above,
    // which only looked at the glued form ("-Dxxx"): the separate-argv
    // branch pushed the empty string straight into define_macros without
    // any check. Symmetric with the glued "-D" bare-trailing case, this
    // must also raise MISSING_OPTION_VALUE.
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("t.iec"),
                    const_cast<char*>("-D"), const_cast<char*>("")};
    EXPECT_THROW(parse_args(4, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code());
}

TEST_F(OptionTest, ParseArgsDefineEmptyNameStillInvalidMacroDef) {
    // "-D=1" has a non-empty spec ("=1"), so it is not caught by the
    // empty-spec check above; it must still reach define_macro_option() and
    // fail there as an empty macro *name*, INVALID_MACRO_DEF -- this is a
    // parse_args()-then-jiepp_command() split, not testable via parse_args()
    // alone, so it is exercised directly against define_macro_option().
    auto arg = std::string("=1");
    EXPECT_THROW(define_macro_option(arg), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_MACRO_DEF, code());
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

TEST_F(OptionTest, UndefMissingValueIsError) {
    // B14: "-U" with no following value (e.g. the very last argv token) must
    // raise a diagnostic, mirroring how -I/-o require a value, instead of
    // silently pushing an empty undef name that is a harmless no-op downstream.
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("t.iec"),
                    const_cast<char*>("-U")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code());
}

TEST_F(OptionTest, ParseArgsUndefEmptyArgvValueIsError) {
    // "-U" followed by a separate, empty-string argv token (e.g. `-U ""`
    // from a shell) previously slipped past the empty-spec check that only
    // covers the glued form ("-Uxxx"/bare trailing "-U"): the separate-argv
    // branch pushed the empty string straight into undef_macros without any
    // check. Symmetric with UndefMissingValueIsError, this must also raise
    // MISSING_OPTION_VALUE.
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("t.iec"),
                    const_cast<char*>("-U"), const_cast<char*>("")};
    EXPECT_THROW(parse_args(4, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code());
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

TEST_F(OptionTest, ParseArgsPFlag) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-P")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.no_line_markers);
}

TEST_F(OptionTest, ParseArgsPFlagDefault) {
    char* argv[] = {const_cast<char*>("jiepp")};
    auto opts = parse_args(1, argv);
    EXPECT_FALSE(opts.no_line_markers);
}

TEST_F(OptionTest, ParseArgsDDFlag) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-dD")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.dD);
}

TEST_F(OptionTest, ParseArgsDDFlagDefault) {
    char* argv[] = {const_cast<char*>("jiepp")};
    auto opts = parse_args(1, argv);
    EXPECT_FALSE(opts.dD);
}

TEST_F(OptionTest, ParseArgsMDFlag) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-MD")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.MD);
    EXPECT_EQ(opts.dep_mode, DepMode::ALL);
}

TEST_F(OptionTest, ParseArgsMMDFlag) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-MMD")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.MMD);
    EXPECT_EQ(opts.dep_mode, DepMode::USER);
}

TEST_F(OptionTest, ParseArgsMDFlagDefault) {
    char* argv[] = {const_cast<char*>("jiepp")};
    auto opts = parse_args(1, argv);
    EXPECT_FALSE(opts.MD);
    EXPECT_FALSE(opts.MMD);
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

// ---- D1/D2: unknown option -- single diagnostic, no --help dump ----

TEST_F(OptionTest, ParseArgsUnknownOption) {
    // D1: an unknown option must propagate Issue::Exception like every
    // other option-parsing error in this file, instead of printing the full
    // --help usage text to stdout and calling std::exit() itself (which
    // would previously have terminated the whole test binary here).
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("--foo")};
    EXPECT_THROW(parse_args(2, argv), Issue::Exception);
    EXPECT_EQ(Issue::Code::UNKNOWN_OPTION, code());
}

TEST_F(OptionTest, ParseArgsUnknownOptionMessageFormat) {
    // D2: diagnostics not tied to a source file (PP70-76, PP13, PP10 for
    // -o/-MF open failures, PP11 for the top-level input, PP01 from main)
    // render as "jiepp: error: PPxx: message", not the string-input
    // preprocess()/preprocess_text() API's "<unknown location>:N.0: ..."
    // form -- exercised here for UNKNOWN_OPTION via parse_args() called
    // directly, with no ambient jiepp_command()/main() context.
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("--foo")};
    EXPECT_THROW(parse_args(2, argv), Issue::Exception);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ("jiepp: error: PP70: Unknown command-line option; '--foo'", msgs[0]);
}
