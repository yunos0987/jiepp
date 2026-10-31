#include "test_helper.hpp"
#include "jiepp/jiepp.hpp"
#include "jiepp/option.hpp"

// jiepp command-line output of __arg_lhs / __arg_rhs / __arg_dir
// (SPECIFICATION.md 12.13) inside {ld}...{end}, the way rungst splits FB
// arguments written as `pin := value` or `pin => var`.

namespace {
class ArgSplitCommandTest : public JieppTest {};

struct RunResult {
    int rc;
    std::string out;
};

RunResult run_jiepp_on(const std::string& content, JieppOptions opts,
                       const std::string& name = "arg_split_cmd.iec") {
    const fs::path dir = fs::temp_directory_path();
    CwdGuard cwd(dir);
    {
        std::ofstream f(dir / name, std::ios::binary);
        f << content;
    }
    const fs::path out = dir / (name + ".piec");
    opts.input_filepaths = {name};
    opts.output_filepath = out.generic_string();
    RunResult r;
    r.rc = jiepp_command(opts);
    std::ifstream i(out, std::ios::binary);
    r.out.assign(std::istreambuf_iterator<char>(i), std::istreambuf_iterator<char>());
    std::error_code ec;
    fs::remove(dir / name, ec);
    fs::remove(out, ec);
    return r;
}

const std::string kInput =
    "{#if __has_builtin(__arg_lhs)}\n"
    "{#define ARG(a) <Arg name=__xmlattr(__arg_lhs(a))"
    " dir=__arg_dir(a) value=__xmlattr(__arg_rhs(a))/>}\n"
    "{#endif}\n"
    "{ld}ARG(PT := T#2s)\n"
    "ARG(Q => Done){end}\n";
} // namespace

TEST_F(ArgSplitCommandTest, DefaultStyle) {
    JieppOptions opts;
    opts.no_line_markers = true;
    RunResult r = run_jiepp_on(kInput, opts);
    EXPECT_EQ(0, r.rc);
    EXPECT_EQ("(*{ld}*)<Arg name=\"PT\" dir=0 value=\"T#2s\"/>\n"
              "<Arg name=\"Q\" dir=1 value=\"Done\"/>(*{end}*)\n",
              r.out);
}

TEST_F(ArgSplitCommandTest, StandardStyle) {
    JieppOptions opts;
    opts.no_line_markers = true;
    opts.pp_output_pragma_style = "standard";
    RunResult r = run_jiepp_on(kInput, opts);
    EXPECT_EQ(0, r.rc);
    EXPECT_EQ("{ld}<Arg name=\"PT\" dir=0 value=\"T#2s\"/>\n"
              "<Arg name=\"Q\" dir=1 value=\"Done\"/>{end}\n",
              r.out);
}

// -P (no line markers) with -nC (remove comments), the options rungst uses.
// A real comment is removed; the {ld}/{end} pragma comments stay. `a < b` in
// the value is escaped by __xmlattr, `dir` is quoted through __xmlattr, and
// the call is split at the first separator.
TEST_F(ArgSplitCommandTest, NoLineMarkersAndRemoveComments) {
    JieppOptions opts;
    opts.no_line_markers = true;
    opts.remove_comments = true;
    RunResult r = run_jiepp_on(
        "{#define ARG(a) <Arg name=__xmlattr(__arg_lhs(a))"
        " dir=__xmlattr(__arg_dir(a)) value=__xmlattr(__arg_rhs(a))/>}\n"
        "(* note *)\n"
        "{ld}ARG(PT := a < b) (* c *)\n"
        "ARG(Q => Done){end}\n",
        opts);
    EXPECT_EQ(0, r.rc);
    EXPECT_EQ("(*{ld}*)<Arg name=\"PT\" dir=\"0\" value=\"a &lt; b\"/>\n"
              "<Arg name=\"Q\" dir=\"1\" value=\"Done\"/>(*{end}*)\n",
              r.out);
}

// A call without a separator is an error: the exit status is not 0.
TEST_F(ArgSplitCommandTest, MissingSeparatorFailsTheRun) {
    JieppOptions opts;
    opts.no_line_markers = true;
    RunResult r = run_jiepp_on("{ld}__arg_lhs(PT){end}\n", opts);
    EXPECT_EQ(1, r.rc);
}
