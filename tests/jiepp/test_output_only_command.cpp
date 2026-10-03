#include "test_helper.hpp"
#include "jiepp/jiepp.hpp"
#include "jiepp/option.hpp"

// jiepp command-line output for a macro whose body prints a newline ($n):
// line markers resync the printed lines to the source; -P prints no marker;
// --max-blank-lines 0 still resyncs (it only disables compaction).

namespace {
class OutputOnlyLinesCommandTest : public JieppTest {};

std::string run_jiepp_on(const std::string& content, JieppOptions opts,
                         const std::string& name = "u6_output_only.iec") {
    const fs::path dir = fs::temp_directory_path();
    CwdGuard cwd(dir);
    {
        std::ofstream f(dir / name, std::ios::binary);
        f << content;
    }
    const fs::path out = dir / (name + ".piec");
    opts.input_filepaths = {name};
    opts.output_filepath = out.generic_string();
    EXPECT_EQ(0, jiepp_command(opts));
    std::ifstream i(out, std::ios::binary);
    std::string r((std::istreambuf_iterator<char>(i)), std::istreambuf_iterator<char>());
    std::error_code ec;
    fs::remove(dir / name, ec);
    fs::remove(out, ec);
    return r;
}

const std::string kInput = "{#define Z a$nb}\nZ;\nL2 __LINE__;\n";
} // namespace

TEST_F(OutputOnlyLinesCommandTest, MarkersResync) {
    // before: "(*{#:0 'u6_output_only.iec'}*)\n\na\nb;\nL2 4;\n"
    EXPECT_EQ("(*{#:0 'u6_output_only.iec'}*)\n\na\nb;\n(*{#:2 'u6_output_only.iec'}*)\nL2 3;\n",
              run_jiepp_on(kInput, {}));
}

TEST_F(OutputOnlyLinesCommandTest, StandardStyle) {
    JieppOptions opts;
    opts.pp_output_pragma_style = "standard";
    EXPECT_EQ("{#:0 'u6_output_only.iec'}\n\na\nb;\n{#:2 'u6_output_only.iec'}\nL2 3;\n",
              run_jiepp_on(kInput, opts));
}

TEST_F(OutputOnlyLinesCommandTest, NoLineMarkers) {
    JieppOptions opts;
    opts.no_line_markers = true;
    EXPECT_EQ("a\nb;\nL2 3;\n", run_jiepp_on(kInput, opts));   // before: L2 4
    // Blank lines printed by the body are still removed under -P.
    EXPECT_EQ("a\nb;\nL2 3;\n",
              run_jiepp_on("{#define Z a$n$n$nb}\nZ;\nL2 __LINE__;\n", opts)); // before: L2 6
}

TEST_F(OutputOnlyLinesCommandTest, MaxBlankLinesZeroStillResyncs) {
    JieppOptions opts;
    opts.max_blank_lines = 0;
    EXPECT_EQ("(*{#:0 'u6_output_only.iec'}*)\n\na\n\n\nb;\n(*{#:2 'u6_output_only.iec'}*)\nL2 3;\n",
              run_jiepp_on("{#define Z a$n$n$nb}\nZ;\nL2 __LINE__;\n", opts));
}

TEST_F(OutputOnlyLinesCommandTest, CompactedRunInsideExpansion) {
    // before: (*{#:11 ...}*) and L2 13
    EXPECT_EQ("(*{#:0 'u6_output_only.iec'}*)\n\na\n(*{#:1 'u6_output_only.iec'}*)\nb;\nL2 3;\n",
              run_jiepp_on("{#define Z a$n$n$n$n$n$n$n$n$n$nb}\nZ;\nL2 __LINE__;\n", {}));
}

TEST_F(OutputOnlyLinesCommandTest, NoMarkerAtEndOfFile) {
    EXPECT_EQ("(*{#:0 'u6_output_only.iec'}*)\n\nL1 a\nb",
              run_jiepp_on("{#define Z a$nb}\nL1 Z", {}));
}
