#include "test_helper.hpp"
#include "jiepp/option.hpp"

#include <algorithm>
#include <optional>
#include <string>

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

// P2-4: --error-limit N / --error_limit N; 0 (no limit) is valid.
TEST_F(OptionTest, ParseArgsErrorLimitBothSpellings) {
    for (const char* spelling : {"--error-limit", "--error_limit"}) {
        for (const char* value : {"5", "0", "2147483647"}) {
            char* argv[] = {const_cast<char*>("jiepp"),
                            const_cast<char*>(spelling), const_cast<char*>(value)};
            auto opts = parse_args(3, argv);
            ASSERT_TRUE(opts.error_limit.has_value()) << spelling << " " << value;
            EXPECT_EQ(*opts.error_limit, std::stoi(value)) << spelling;
        }
    }
    char* argv[] = {const_cast<char*>("jiepp")};
    EXPECT_FALSE(parse_args(1, argv).error_limit.has_value());
}

TEST_F(OptionTest, ParseArgsErrorLimitInvalidValues) {
    for (const char* spelling : {"--error-limit", "--error_limit"}) {
        for (const char* bad : {"-1", "x", "3x", "2147483648"}) {
            char* argv[] = {const_cast<char*>("jiepp"),
                            const_cast<char*>(spelling), const_cast<char*>(bad)};
            EXPECT_THROW(parse_args(3, argv), Issue::Exception) << spelling << " " << bad;
            auto msgs = messages();
            ASSERT_EQ(1u, msgs.size()) << bad;
            EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << bad << ": " << msgs[0];
        }
    }
}

TEST_F(OptionTest, ParseArgsErrorLimitMissingValue) {
    for (const char* spelling : {"--error-limit", "--error_limit"}) {
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(spelling)};
        EXPECT_THROW(parse_args(2, argv), Issue::Exception) << spelling;
        EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code()) << spelling;
    }
}

// --max-include-count N / --max_include_count N; 0 (no limit) is valid.
TEST_F(OptionTest, ParseArgsMaxIncludeCountBothSpellings) {
    for (const char* spelling : {"--max-include-count", "--max_include_count"}) {
        for (const char* value : {"5", "0", "2147483647"}) {
            char* argv[] = {const_cast<char*>("jiepp"),
                            const_cast<char*>(spelling), const_cast<char*>(value)};
            auto opts = parse_args(3, argv);
            ASSERT_TRUE(opts.max_include_count.has_value()) << spelling << " " << value;
            EXPECT_EQ(*opts.max_include_count, std::stoi(value)) << spelling;
        }
    }
    char* argv[] = {const_cast<char*>("jiepp")};
    EXPECT_FALSE(parse_args(1, argv).max_include_count.has_value());
}

TEST_F(OptionTest, ParseArgsMaxIncludeCountInvalidValues) {
    for (const char* spelling : {"--max-include-count", "--max_include_count"}) {
        for (const char* bad : {"-1", "x", "3x", "2147483648"}) {
            char* argv[] = {const_cast<char*>("jiepp"),
                            const_cast<char*>(spelling), const_cast<char*>(bad)};
            EXPECT_THROW(parse_args(3, argv), Issue::Exception) << spelling << " " << bad;
            auto msgs = messages();
            ASSERT_EQ(1u, msgs.size()) << bad;
            EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << bad << ": " << msgs[0];
        }
    }
}

TEST_F(OptionTest, ParseArgsMaxIncludeCountMissingValue) {
    for (const char* spelling : {"--max-include-count", "--max_include_count"}) {
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(spelling)};
        EXPECT_THROW(parse_args(2, argv), Issue::Exception) << spelling;
        EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code()) << spelling;
    }
}

// C11: --max-input-bytes N / --max_input_bytes N; 0 (no limit) is valid,
// the 2^24 cap of the depth options (PP04) does not apply.
TEST_F(OptionTest, ParseArgsMaxInputBytesBothSpellings) {
    for (const char* spelling : {"--max-input-bytes", "--max_input_bytes"}) {
        for (const char* value : {"0", "1", "2147483647"}) {
            char* argv[] = {const_cast<char*>("jiepp"),
                            const_cast<char*>(spelling), const_cast<char*>(value)};
            auto opts = parse_args(3, argv);
            ASSERT_TRUE(opts.max_input_bytes.has_value()) << spelling << " " << value;
            EXPECT_EQ(*opts.max_input_bytes, std::stoi(value)) << spelling;
        }
    }
    char* argv[] = {const_cast<char*>("jiepp")};
    EXPECT_FALSE(parse_args(1, argv).max_input_bytes.has_value());
}

TEST_F(OptionTest, ParseArgsMaxInputBytesInvalidValues) {
    for (const char* spelling : {"--max-input-bytes", "--max_input_bytes"}) {
        for (const char* bad : {"-1", "x", "3abc", "2147483648"}) {
            char* argv[] = {const_cast<char*>("jiepp"),
                            const_cast<char*>(spelling), const_cast<char*>(bad)};
            EXPECT_THROW(parse_args(3, argv), Issue::Exception) << spelling << " " << bad;
            auto msgs = messages();
            ASSERT_EQ(1u, msgs.size()) << bad;
            EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << bad << ": " << msgs[0];
        }
    }
}

TEST_F(OptionTest, ParseArgsMaxInputBytesMissingValue) {
    for (const char* spelling : {"--max-input-bytes", "--max_input_bytes"}) {
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(spelling)};
        EXPECT_THROW(parse_args(2, argv), Issue::Exception) << spelling;
        EXPECT_EQ(Issue::Code::MISSING_OPTION_VALUE, code()) << spelling;
    }
}

TEST_F(OptionTest, ParseArgsSnakeCaseVariants) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max_include_depth"), const_cast<char*>("5")};
    auto opts = parse_args(3, argv);
    ASSERT_TRUE(opts.max_include_depth.has_value());
    EXPECT_EQ(*opts.max_include_depth, 5);
}

// ---- underscore spellings of every hyphenated long option ----

namespace {

struct LongOptionRow {
    const char* hyphen;           // "--remove-comments"
    const char* value;            // nullptr for a flag
    std::string (*summary)(const JieppOptions&);
};

std::string opt_int(const std::optional<int>& v) {
    return v ? std::to_string(*v) : std::string("unset");
}

// Every long option in parse_args() whose name contains '-' (11 of them).
const LongOptionRow LONG_OPTION_ROWS[] = {
    {"--max-include-depth", "7", [](const JieppOptions& o) { return opt_int(o.max_include_depth); }},
    {"--max-expansion-depth", "8", [](const JieppOptions& o) { return opt_int(o.max_expansion_depth); }},
    {"--max-expansion-steps", "9", [](const JieppOptions& o) { return opt_int(o.max_expansion_steps); }},
    {"--max-include-count", "10", [](const JieppOptions& o) { return opt_int(o.max_include_count); }},
    {"--max-input-bytes", "11", [](const JieppOptions& o) { return opt_int(o.max_input_bytes); }},
    {"--error-limit", "12", [](const JieppOptions& o) { return opt_int(o.error_limit); }},
    {"--max-if-nesting", "13", [](const JieppOptions& o) { return opt_int(o.max_if_nesting); }},
    {"--max-blank-lines", "14", [](const JieppOptions& o) { return opt_int(o.max_blank_lines); }},
    {"--pp-output-pragma-style", "standard", [](const JieppOptions& o) {
         return o.pp_output_pragma_style ? *o.pp_output_pragma_style : std::string("unset"); }},
    {"--recursion-limit", "15", [](const JieppOptions& o) { return opt_int(o.recursion_limit); }},
    {"--remove-comments", nullptr, [](const JieppOptions& o) {
         return std::string(o.remove_comments ? "on" : "off"); }},
};

std::string underscore_spelling(std::string s) {
    std::replace(s.begin() + 2, s.end(), '-', '_');
    return s;
}

} // namespace

TEST_F(OptionTest, LongOptionsAcceptBothSpellings) {
    EXPECT_EQ(11u, sizeof(LONG_OPTION_ROWS) / sizeof(LONG_OPTION_ROWS[0]));
    for (const auto& row : LONG_OPTION_ROWS) {
        std::string hyphen = row.hyphen;
        std::string under = underscore_spelling(hyphen);
        ASSERT_NE(hyphen, under) << hyphen;

        std::string results[2];
        int n = 0;
        for (const std::string& spelling : {hyphen, under}) {
            std::string a1 = spelling;
            std::string a2 = row.value ? row.value : "";
            char* argv[] = {const_cast<char*>("jiepp"), a1.data(), a2.data()};
            auto opts = parse_args(row.value ? 3 : 2, argv);
            results[n++] = row.summary(opts);
        }
        EXPECT_NE("unset", results[0]) << hyphen;
        EXPECT_NE("off", results[0]) << hyphen;
        EXPECT_EQ(results[0], results[1]) << hyphen << " vs " << under;
    }
}

TEST_F(OptionTest, UnderscoreSpellingKeepsTypedSpellingInDiagnostics) {
    char* missing[] = {const_cast<char*>("jiepp"), const_cast<char*>("--max_include_depth")};
    EXPECT_THROW(parse_args(2, missing), Issue::Exception);
    // messages() drains the buffer, so call it once.
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_EQ("jiepp: error: PP72: Option requires a value; '--max_include_depth'", msgs[0]);
}

TEST_F(OptionTest, UnderscoreSpellingInvalidValueKeepsTypedSpelling) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--pp_output_pragma_style"), const_cast<char*>("bogus")};
    EXPECT_THROW(parse_args(3, argv), Issue::Exception);
    auto msgs = messages();
    ASSERT_EQ(1u, msgs.size());
    EXPECT_NE(std::string::npos, msgs[0].find("PP71")) << msgs[0];
    EXPECT_NE(std::string::npos, msgs[0].find("--pp_output_pragma_style")) << msgs[0];
}

TEST_F(OptionTest, MixedSpellingStaysUnknownOption) {
    for (const char* spelling : {"--max_include-depth", "--max-include_depth", "--remove_comments-",
                                 "--remove-comments_", "--Remove_Comments", "--remove__comments"}) {
        SCOPED_TRACE(spelling);
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(spelling),
                        const_cast<char*>("5")};
        EXPECT_THROW(parse_args(3, argv), Issue::Exception);
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(std::string("jiepp: error: PP70: Unknown command-line option; '") + spelling + "'",
                  msgs[0]);
    }
}

TEST_F(OptionTest, UnderscoreIsNotMappedForOtherArguments) {
    // Single-dash options, a name that is not an option, and options without
    // '-' in the name are not normalised.
    for (const char* spelling : {"-Werror_undef", "-_include", "-n_C", "--foo_bar", "--disppath_x",
                                 "--si_lent", "--ver_sion", "--he_lp", "--max_include_depth_"}) {
        SCOPED_TRACE(spelling);
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(spelling)};
        EXPECT_THROW(parse_args(2, argv), Issue::Exception);
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size());
        EXPECT_EQ(std::string("jiepp: error: PP70: Unknown command-line option; '") + spelling + "'",
                  msgs[0]);
    }
}

TEST_F(OptionTest, UnderscoreInValuesAndAfterDoubleDashIsKept) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("--max_include_depth"), const_cast<char*>("5"),
                    const_cast<char*>("-o"), const_cast<char*>("out_dir/a_b.iec"),
                    const_cast<char*>("--disppath"), const_cast<char*>("x_y"),
                    const_cast<char*>("-include"), const_cast<char*>("pre_a.iec"),
                    const_cast<char*>("-DA_B=c_d"),
                    const_cast<char*>("--"), const_cast<char*>("--remove_comments"),
                    const_cast<char*>("in_put.iec")};
    auto opts = parse_args(13, argv);
    EXPECT_EQ(5, *opts.max_include_depth);
    EXPECT_EQ("out_dir/a_b.iec", *opts.output_filepath);
    EXPECT_EQ("x_y", *opts.disppath);
    ASSERT_EQ(1u, opts.include_filepaths.size());
    EXPECT_EQ("pre_a.iec", opts.include_filepaths[0]);
    ASSERT_EQ(1u, opts.define_macros.size());
    EXPECT_EQ("A_B=c_d", opts.define_macros[0]);
    EXPECT_FALSE(opts.remove_comments);
    ASSERT_EQ(2u, opts.input_filepaths.size());
    EXPECT_EQ("--remove_comments", opts.input_filepaths[0]);
    EXPECT_EQ("in_put.iec", opts.input_filepaths[1]);
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
    EXPECT_FALSE(opts.MP);
}

TEST_F(OptionTest, ParseArgsMPFlag) {
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-MM"), const_cast<char*>("-MP")};
    auto opts = parse_args(3, argv);
    EXPECT_TRUE(opts.MP);
    EXPECT_EQ(opts.dep_mode, DepMode::USER);
}

TEST_F(OptionTest, ParseArgsMPAloneIsAccepted) {
    // -MP without -M/-MM/-MD/-MMD is accepted (no PP70) and selects no
    // dependency output by itself, like -MT/-MF alone.
    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-MP")};
    auto opts = parse_args(2, argv);
    EXPECT_TRUE(opts.MP);
    EXPECT_EQ(opts.dep_mode, DepMode::NONE);
    EXPECT_TRUE(empty());
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

TEST_F(OptionTest, ParsesWundef) {
    char* plain[] = {const_cast<char*>("jiepp")};
    EXPECT_TRUE(parse_args(1, plain).warnings.empty());

    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-Wundef")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(1u, opts.warnings.size());
    EXPECT_EQ(WarningAction::ENABLE, opts.warnings[0].action);
    EXPECT_EQ(Issue::Code::UNDEFINED_IDENTIFIER, opts.warnings[0].code);
    EXPECT_FALSE(opts.werror);
}

TEST_F(OptionTest, ParsesWerrorUndef) {
    char* plain[] = {const_cast<char*>("jiepp")};
    EXPECT_TRUE(parse_args(1, plain).warnings.empty());

    char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>("-Werror=undef")};
    auto opts = parse_args(2, argv);
    ASSERT_EQ(1u, opts.warnings.size());
    EXPECT_EQ(WarningAction::PROMOTE, opts.warnings[0].action);
    EXPECT_EQ(Issue::Code::UNDEFINED_IDENTIFIER, opts.warnings[0].code);
    EXPECT_FALSE(opts.werror);

    // A -Werror= name that is neither a warning code nor undef is PP71.
    char* argv2[] = {const_cast<char*>("jiepp"), const_cast<char*>("-Werror=foo")};
    EXPECT_THROW(parse_args(2, argv2), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_OPTION_VALUE, code());
}

// -W<name> / -Wno-<name> / -Werror=<name> / -Wno-error=<name> (SPEC 13):
// kept in command-line order; <name> is a warning code in either case or
// undef (PP54).
TEST_F(OptionTest, ParsesWarningOptionsInOrder) {
    char* argv[] = {const_cast<char*>("jiepp"),
                    const_cast<char*>("-Wno-PP35"), const_cast<char*>("-Werror=pp49"),
                    const_cast<char*>("-Wno-error=undef"), const_cast<char*>("-WPP54"),
                    const_cast<char*>("-Wno-pp92"), const_cast<char*>("-Wno-error=PP28")};
    auto opts = parse_args(7, argv);
    ASSERT_EQ(6u, opts.warnings.size());
    EXPECT_EQ(WarningAction::DISABLE, opts.warnings[0].action);
    EXPECT_EQ(Issue::Code::MACRO_REDEFINED, opts.warnings[0].code);
    EXPECT_EQ(WarningAction::PROMOTE, opts.warnings[1].action);
    EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, opts.warnings[1].code);
    EXPECT_EQ(WarningAction::DEMOTE, opts.warnings[2].action);
    EXPECT_EQ(Issue::Code::UNDEFINED_IDENTIFIER, opts.warnings[2].code);
    EXPECT_EQ(WarningAction::ENABLE, opts.warnings[3].action);
    EXPECT_EQ(Issue::Code::UNDEFINED_IDENTIFIER, opts.warnings[3].code);
    EXPECT_EQ(WarningAction::DISABLE, opts.warnings[4].action);
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, opts.warnings[4].code);
    EXPECT_EQ(WarningAction::DEMOTE, opts.warnings[5].action);
    EXPECT_EQ(Issue::Code::WHITESPACE_BEFORE_DIRECTIVE, opts.warnings[5].code);
    EXPECT_FALSE(opts.werror);
    EXPECT_TRUE(empty());
}

// -Wno-error (no name) cancels -Werror; the last of the two wins.
TEST_F(OptionTest, ParsesWnoErrorLastWins) {
    char* argv1[] = {const_cast<char*>("jiepp"),
                     const_cast<char*>("-Werror"), const_cast<char*>("-Wno-error")};
    auto opts1 = parse_args(3, argv1);
    EXPECT_FALSE(opts1.werror);
    EXPECT_TRUE(opts1.warnings.empty());

    char* argv2[] = {const_cast<char*>("jiepp"),
                     const_cast<char*>("-Wno-error"), const_cast<char*>("-Werror")};
    EXPECT_TRUE(parse_args(3, argv2).werror);
}

// A name that is not a WARNING code or undef is PP71 with the option as
// typed: an ERROR code, an unassigned code, a bare number, other text.
TEST_F(OptionTest, WarningOptionInvalidNameIsPP71) {
    for (const char* bad : {"-Wno-PP11", "-Wno-PP99", "-Wno-35", "-Wno-foo", "-Wno-",
                            "-Werror=PP70", "-Werror=", "-Wno-error=PP01",
                            "-Wno-error=foo", "-WPP11", "-Wpp99"}) {
        Issue::initialize(output());
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(bad)};
        EXPECT_THROW(parse_args(2, argv), Issue::Exception) << bad;
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << bad;
        EXPECT_EQ(Issue::Code::INVALID_OPTION_VALUE, PlainTextMessage::parse_code(msgs[0])) << bad;
        EXPECT_EQ(0u, msgs[0].find("jiepp: error: PP71: ")) << msgs[0];
        EXPECT_NE(std::string::npos, msgs[0].find(std::string("'") + bad + ":")) << msgs[0];
    }
}

// -W spellings other than -W<code> / -Wundef stay unknown options (PP70):
// no -Wall / -Wextra and no clang-only names.
TEST_F(OptionTest, OtherWOptionsStayUnknown) {
    for (const char* bad : {"-Wall", "-Wextra", "-Wmacro-redefined", "-W", "-WPP", "-Wundefx"}) {
        Issue::initialize(output());
        char* argv[] = {const_cast<char*>("jiepp"), const_cast<char*>(bad)};
        EXPECT_THROW(parse_args(2, argv), Issue::Exception) << bad;
        EXPECT_EQ(Issue::Code::UNKNOWN_OPTION, code()) << bad;
    }
}
