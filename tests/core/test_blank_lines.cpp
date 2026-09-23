#include "test_helper.hpp"
#include "core/line_compaction.hpp"

#include <string>

namespace fs = std::filesystem;

namespace {

// Writes `content` to a fresh temp file and preprocesses it as a *top-level*
// file (its own file-entry line marker, gcc/clang style, with the line
// counter baseline that entails), running blank-line compaction exactly as
// jiepp_command does. Unlike test_helper.hpp's pp_file() (which goes through
// the istream overload — no entry marker of its own — used for
// included-content-style tests), this exercises the same top-level marker
// baseline a real jiepp invocation uses, which several tests below rely on
// for exact marker line numbers. `disppath` is fixed by default so expected
// strings are stable across runs regardless of the temp file's real name.
std::string pp_toplevel(const std::string& content, Env& env,
                         const std::string& disppath = "blank_lines_test.iec") {
    static int counter = 0;
    fs::path path = fs::temp_directory_path() /
                     ("blank_lines_test_" + std::to_string(++counter) + ".iec");
    {
        std::ofstream f(path, std::ios::binary);
        f << content;
    }
    std::vector<Token> ots;
    expand(path.generic_string(), Loader::LoadType::INCLUDE, ots, env, disppath);
    jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
                                jiepp::BlankLineMode::Markers, env.is_standard_pragma_style());
    std::ostringstream o;
    for (auto& t : ots) o << t.text;
    std::error_code ec;
    fs::remove(path, ec);
    return o.str();
}

std::string pp_toplevel(const std::string& content) {
    Env env = setup();
    return pp_toplevel(content, env);
}

// Builds the §2 worked-example input: "A;" then 12 indented {#define}
// lines (four leading spaces each), then "B __LINE__;". All 12 define
// lines are fully consumed by the directive handler, leaving only their
// indentation + newline behind — a run of 12 blank (whitespace-only)
// lines, well over the default 7-line compaction threshold.
std::string worked_example_input() {
    std::string s = "A;\n";
    for (int k = 1; k <= 12; ++k)
        s += "    {#define M" + std::to_string(k) + " " + std::to_string(k) + "}\n";
    s += "B __LINE__;\n";
    return s;
}

// Number of line-marker lines present in `s` (a legitimately-compacted run
// collapses to exactly one marker line; an uncompacted run leaves plain
// blank lines behind with no marker at all).
int count_markers(const std::string& s) {
    int n = 0;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line))
        if (line.starts_with("(*{#:") || line.starts_with("{#:"))
            ++n;
    return n;
}

// Longest run of consecutive whitespace-only lines in `s` (a marker line
// resets the run, matching gcc/clang's own accounting).
int max_blank_run(const std::string& s) {
    int best = 0, run = 0;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.starts_with("(*{#:") || line.starts_with("{#:")) {
            run = 0;
            continue;
        }
        bool blank = line.find_first_not_of(" \t\r") == std::string::npos;
        if (blank) {
            ++run;
            best = std::max(best, run);
        } else {
            run = 0;
        }
    }
    return best;
}

} // namespace

class BlankLinesTest : public JieppTest {};

// ---- §2 worked example ------------------------------------------------

TEST_F(BlankLinesTest, WorkedExampleDefaultAnnotated) {
    // Default (annotated) style: the 12-line run of indented {#define}
    // lines compacts to a single marker. N = cur_before_run(1, after the
    // file-entry marker's own newline) + nl(13, the "A;" line's own
    // terminator plus the 12 define lines' newlines) - 1 = 13.
    const std::string output = pp_toplevel(worked_example_input());
    EXPECT_EQ(
        "(*{#:0 'blank_lines_test.iec'}*)\n"
        "A;\n"
        "(*{#:13 'blank_lines_test.iec'}*)\n"
        "B 14;\n",
        output);
    EXPECT_TRUE(empty());
}

TEST_F(BlankLinesTest, WorkedExampleStandardStyle) {
    // Same input, standard pragma style: markers render as {#:N '...'}
    // instead of (*{#:N '...'}*), same N (compaction is style-agnostic).
    Env env = setup();
    env.set_pragma_style("standard");
    const std::string output = pp_toplevel(worked_example_input(), env);
    EXPECT_EQ(
        "{#:0 'blank_lines_test.iec'}\n"
        "A;\n"
        "{#:13 'blank_lines_test.iec'}\n"
        "B 14;\n",
        output);
    EXPECT_TRUE(empty());
}

TEST_F(BlankLinesTest, WorkedExampleMaxBlankLinesZeroByteIdentical) {
    // --max-blank-lines 0 disables compaction entirely: the 12 blank
    // (four-space-indented) lines are emitted verbatim, byte-identical to
    // pre-compaction output.
    Env env = setup();
    env.set_max_blank_lines(0);
    const std::string output = pp_toplevel(worked_example_input(), env);
    std::string expected = "(*{#:0 'blank_lines_test.iec'}*)\nA;\n";
    for (int k = 0; k < 12; ++k)
        expected += "    \n";
    expected += "B 14;\n";
    EXPECT_EQ(expected, output);
    EXPECT_TRUE(empty());
}

// ---- [A4] threshold boundary -------------------------------------------

TEST_F(BlankLinesTest, ThresholdSevenBlankLinesUnaffected) {
    // Exactly 7 consecutive blank lines: at the default threshold, must be
    // emitted verbatim (no marker). B lands on physical line 9.
    const std::string input = "A;" + std::string(8, '\n') + "B __LINE__;\n";
    const std::string output = pp_toplevel(input);
    EXPECT_EQ(
        "(*{#:0 'blank_lines_test.iec'}*)\n"
        "A;" +
            std::string(8, '\n') + "B 9;\n",
        output);
    EXPECT_TRUE(empty());
}

TEST_F(BlankLinesTest, ThresholdEightBlankLinesCompacted) {
    // One more blank line (8) crosses the threshold: compacted to a single
    // marker. cur_before_run = 1 (after the entry marker's own newline);
    // nl = 9 (the "A;" line's terminator + 8 blank lines); N = 1 + 9 - 1 = 9.
    const std::string input = "A;" + std::string(9, '\n') + "B __LINE__;\n";
    const std::string output = pp_toplevel(input);
    EXPECT_EQ(
        "(*{#:0 'blank_lines_test.iec'}*)\n"
        "A;\n"
        "(*{#:9 'blank_lines_test.iec'}*)\n"
        "B 10;\n",
        output);
    EXPECT_TRUE(empty());
}

// ---- D4(b): run at end of stream ----------------------------------------

TEST_F(BlankLinesTest, TrailingBlankRunAtEofNoMarker) {
    // 20 trailing blank lines at EOF: no marker is ever inserted for a run
    // that ends the stream (D4b) — the run simply vanishes, leaving only
    // the single newline that terminates "A;"'s own line.
    const std::string input = "A;" + std::string(20, '\n');
    const std::string output = pp_toplevel(input);
    EXPECT_EQ("(*{#:0 'blank_lines_test.iec'}*)\nA;\n", output);
    EXPECT_TRUE(empty());
}

// ---- D4(a): run immediately before an {#include} boundary ---------------

TEST_F(BlankLinesTest, IncludeBoundarySuppressesOwnMarker) {
    // A 20-line blank run immediately followed by {#include ...}: the
    // include's own entry marker already re-synchronises the line counter,
    // so the compactor must not also insert one of its own (D4a) — exactly
    // one marker (the include's) appears at the boundary, not two.
    fs::path dir = fs::temp_directory_path();
    fs::path hdr = dir / "blank_lines_d4a_hdr.iec";
    fs::path main_file = dir / "blank_lines_d4a_main.iec";
    {
        std::ofstream f(hdr, std::ios::binary);
        f << "H;\n";
    }
    {
        std::ofstream f(main_file, std::ios::binary);
        f << "A;" << std::string(20, '\n') << "{#include 'blank_lines_d4a_hdr.iec'}\nC;\n";
    }

    Env env = setup();
    std::vector<Token> ots;
    expand(main_file.generic_string(), Loader::LoadType::INCLUDE, ots, env, "main.iec");
    jiepp::compact_blank_lines(ots, env.get_max_blank_lines(),
                                jiepp::BlankLineMode::Markers, env.is_standard_pragma_style());
    std::ostringstream o;
    for (auto& t : ots) o << t.text;
    const std::string output = o.str();

    auto pos = output.find("A;");
    ASSERT_NE(std::string::npos, pos);
    pos += 2;
    int nl_count = 0;
    while (pos < output.size() && output[pos] == '\n') {
        ++nl_count;
        ++pos;
    }
    EXPECT_EQ(1, nl_count) << "expected exactly one newline before the include boundary marker "
                              "(no compaction marker of its own, D4a); output:\n"
                           << output;
    EXPECT_TRUE(output.compare(pos, 5, "(*{#:") == 0)
        << "expected the include's own entry marker immediately after A;\\n; got: "
        << output.substr(pos, 40);

    std::error_code ec;
    fs::remove(hdr, ec);
    fs::remove(main_file, ec);
}

// ---- D5: interactions with other features --------------------------------

TEST_F(BlankLinesTest, InactiveIfGroupCompacted) {
    // A 20-line inactive {#if 0} group (directive lines + interior content,
    // all skipped) compacts like any other blank run; __LINE__ after
    // {#endif} still reflects the true physical line count.
    std::string input = "A;\n{#if 0}\n" + std::string(20, '\n') + "{#endif}\nB __LINE__;\n";
    const std::string output = pp_toplevel(input);

    // 2 markers total: the file's own entry marker plus exactly one
    // compaction marker for the whole inactive-if run.
    EXPECT_EQ(2, count_markers(output)) << "output:\n" << output;
    EXPECT_LE(max_blank_run(output), 7) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("B 24;")) << "output:\n" << output;
    EXPECT_TRUE(empty());
}

TEST_F(BlankLinesTest, RemoveCommentsBlockCommentCompacted) {
    // --remove-comments: a 20-line block comment becomes a run of blank
    // lines (loader.cpp turns a removed multi-line C token into
    // Token::newline(n)), which compacts like any other blank run.
    Env env = setup();
    env.set_remove_comments(true);
    const std::string input = "A;\n(*" + std::string(19, '\n') + "*)\nB __LINE__;\n";
    const std::string output = pp_toplevel(input, env);

    // 2 markers total: the file's own entry marker plus exactly one
    // compaction marker for the whole (converted) comment run.
    EXPECT_EQ(2, count_markers(output)) << "output:\n" << output;
    EXPECT_LE(max_blank_run(output), 7) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("B 22;")) << "output:\n" << output;
    EXPECT_TRUE(empty());
}

TEST_F(BlankLinesTest, DDModeDefinesNotCompacted) {
    // -dD: each {#define} line is re-emitted as real content (expand.cpp),
    // so the 20 lines never form a blank run at all — nothing to compact.
    Env env = setup();
    env.set_dd_mode(true);
    std::string input;
    for (int k = 1; k <= 20; ++k)
        input += "{#define D" + std::to_string(k) + " " + std::to_string(k) + "}\n";
    input += "X __LINE__;\n";
    const std::string output = pp_toplevel(input, env);

    // 1 marker total: only the file's own entry marker — no compaction
    // marker, since the 20 {#define} lines are real content, not blank.
    EXPECT_EQ(1, count_markers(output)) << "output:\n" << output;
    std::size_t define_count = 0;
    for (std::size_t pos = 0; (pos = output.find("{#define D", pos)) != std::string::npos;
         pos += 1)
        ++define_count;
    EXPECT_EQ(20u, define_count) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("X 21;")) << "output:\n" << output;
    EXPECT_TRUE(empty());
}

// ---- C3: a comment inside a long blank region splits the run -------------

TEST_F(BlankLinesTest, CommentInsideBlankRegionSplitsRun) {
    // Token::MASK_WS is also set on Token::C (comments), but the
    // compactor's run detector uses t.type == Token::WS exclusively, so a
    // comment line in the middle of an otherwise-long blank stretch is
    // real content: it survives verbatim and splits what would otherwise
    // be one run into two independently-compacted runs.
    const std::string input =
        "A;\n" + std::string(10, '\n') + "(* mid *)\n" + std::string(10, '\n') + "B __LINE__;\n";
    const std::string output = pp_toplevel(input);

    // 3 markers total: the file's own entry marker plus one compaction
    // marker per side of the comment.
    EXPECT_EQ(3, count_markers(output)) << "output:\n" << output;
    EXPECT_LE(max_blank_run(output), 7) << "output:\n" << output;
    EXPECT_NE(std::string::npos, output.find("(* mid *)")) << "comment was not preserved; output:\n"
                                                             << output;
    EXPECT_NE(std::string::npos, output.find("B 23;")) << "output:\n" << output;
    EXPECT_TRUE(empty());
}
