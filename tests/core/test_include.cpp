#include "test_helper.hpp"

static const std::filesystem::path DIR = "tests/core/test_include";

class IncludeTest : public JieppTest {};

TEST_F(IncludeTest, BasicInclude) {
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({o = pp_file(DIR / "include.iec");});
    EXPECT_EQ(R"(0;
(*{#:0 './a.iec'}*)
// a.iec begin


1;
'./a.iec';
6;
// a.iec end
(*{#:2 'tests/core/test_include/include.iec'}*)
a;
(*{#:0 './b.iec'}*)
// b.iec begin


2;
'./b.iec';
6;
// b.iec end
(*{#:4 'tests/core/test_include/include.iec'}*)
b;
3;
)", o);
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, IncludeSingle) {
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({o = pp_file(DIR / "include_single.iec");});
    EXPECT_EQ(R"(0;
(*{#:0 './a.iec'}*)
// a.iec begin


1;
'./a.iec';
6;
// a.iec end
(*{#:2 'tests/core/test_include/include_single.iec'}*)
a;
(*{#:0 './b.iec'}*)
// b.iec begin


2;
'./b.iec';
6;
// b.iec end
(*{#:4 'tests/core/test_include/include_single.iec'}*)
b;
3;
)", o);
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, IncludeDouble) {
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({o = pp_file(DIR / "include_double.iec");});
    EXPECT_EQ(R"(0;
(*{#:0 './a.iec'}*)
// a.iec begin


1;
'./a.iec';
6;
// a.iec end
(*{#:2 'tests/core/test_include/include_double.iec'}*)
a;
(*{#:0 './b.iec'}*)
// b.iec begin


2;
'./b.iec';
6;
// b.iec end
(*{#:4 'tests/core/test_include/include_double.iec'}*)
b;
3;
)", o);
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, InlineInclude) {
    fs::current_path(jiepp_root_dir());
    // File not found for unknown file.
    EXPECT_THROW(pp("{#include 'nonexistent_file.iec'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::FILE_NOT_FOUND, code());
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, IncludeDirectory) {
    fs::current_path(jiepp_root_dir());
    // A directory that exists as a candidate must raise the dedicated
    // INCLUDE_TARGET_IS_DIRECTORY diagnostic (PP14), not a generic FILE_ERROR
    // (from trying to open it as a stream) or FILE_NOT_FOUND.
    EXPECT_THROW(pp("{#include 'tests/core/test_include'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INCLUDE_TARGET_IS_DIRECTORY, code());
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, DirectoryShadowedByLaterSyspath) {
    // A directory candidate found in an earlier syspath must not stop the
    // search: a regular file of the same name in a later syspath still
    // resolves and is included.
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({
        o = pp("{#syspath 'tests/core/test_include/shadow_syspath1'}"
               "{#syspath 'tests/core/test_include/shadow_syspath2'}"
               "{#include 'shadow_target'}");
    });
    EXPECT_NE(std::string::npos, o.find("SHADOWED;"));
    EXPECT_TRUE(empty());
}

#ifdef _WIN32
TEST_F(IncludeTest, PragmaOnceDifferentCaseSamePath) {
    // Windows/NTFS is case-insensitive: two differently-cased spellings of
    // the same include path must be recognised as the same file by
    // {#pragma once} (Loader::fullpath() now returns a canonical path).
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({o = pp_file(DIR / "pragma_once_case_main.iec");});
    static const std::string needle = "header_body;";
    std::size_t count = 0;
    for (std::size_t pos = 0; (pos = o.find(needle, pos)) != std::string::npos;
         pos += needle.size())
        ++count;
    EXPECT_EQ(1u, count);
    EXPECT_NE(std::string::npos, o.find("after;"));
    EXPECT_TRUE(empty());
}
#endif

TEST_F(IncludeTest, CircularInclude) {
    fs::current_path(jiepp_root_dir());
    Env env = setup();
    env.set_max_include_depth(8);
    EXPECT_THROW(pp_file(DIR / "circular_a.iec", env), Issue::Exception);
    EXPECT_EQ("circular_b.iec:1.0: error: PP12: Maximum include depth exceeded; 'circular_a.iec'", message());
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, ExceptionInsideIncludeLeavesStacksBalanced) {
    // item f: expand()'s file-inclusion wrapper (src/core/expand.cpp) now
    // RAII-guards its push_file()/Issue::push() pair (FileContext::FileScope
    // + Issue::LineGuard) instead of a manual push...pop pair. Reuse
    // CircularInclude's PP12 -- it throws from several nested include
    // levels deep at once, so it would have left that many entries unpopped
    // on each stack pre-fix -- and check that both stacks are back to
    // exactly their pre-call depth after the exception, not just "smaller".
    fs::current_path(jiepp_root_dir());
    Env env = setup();
    env.set_max_include_depth(8);
    const auto loc_depth_before  = Issue::loc_stack_.size();
    const auto file_depth_before = env.num_of_files();
    EXPECT_THROW(pp("{#include '" + (DIR / "circular_a.iec").generic_string() + "'}", env),
                 Issue::Exception);
    EXPECT_EQ(loc_depth_before,  Issue::loc_stack_.size());
    EXPECT_EQ(file_depth_before, env.num_of_files());
}

// ─── U1: {#syspath} resolves relative to the directory of the file
// containing the directive, not the process's current working directory
// (same base rule as {#include}). ────────────────────────────────────────

TEST_F(IncludeTest, SyspathRelativeToDeclaringFile) {
    // CWD is deliberately set to a decoy directory that has its own,
    // differently-content lib/sb_target.iec: if {#syspath 'lib'} resolved
    // against CWD (the old, pre-U1 behaviour) instead of main.iec's own
    // directory, this test would pick up the decoy content instead.
    fs::path main_abs = jiepp_root_dir() / DIR / "syspath_base/main.iec";
    std::string o;
    {
        CwdGuard cwd_guard(jiepp_root_dir() / DIR / "syspath_base_cwd_decoy");
        EXPECT_NO_THROW({ o = pp_file(main_abs); });
    }
    EXPECT_NE(std::string::npos, o.find("MAIN_LIB;"));
    EXPECT_EQ(std::string::npos, o.find("DECOY_LIB;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathInIncludedFileRelativeToThatFile) {
    // {#syspath 'lib'} written inside sub/inner.iec (reached via {#include})
    // must resolve against sub/, not against the including file's directory
    // (which also has a decoy lib/inner_target.iec with different content).
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({ o = pp_file(DIR / "syspath_base/outer_includes_sub.iec"); });
    EXPECT_NE(std::string::npos, o.find("SUB_LIB;"));
    EXPECT_EQ(std::string::npos, o.find("TOP_LIB_INNER;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathInSincludedFileRelativeToThatFile) {
    // Same as above, but the file containing {#syspath} is reached via
    // {#sinclude} instead of {#include}.
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({ o = pp_file(DIR / "syspath_base/outer_sinclude_sub2.iec"); });
    EXPECT_NE(std::string::npos, o.find("SUB2_LIB;"));
    EXPECT_EQ(std::string::npos, o.find("TOP_LIB_INNER_S;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathAbsolutePathUnchanged) {
    // An absolute {#syspath} operand is used as-is, exactly like {#include}.
    fs::current_path(jiepp_root_dir());
    fs::path abs_lib_dir = fs::absolute(DIR / "syspath_base/abs_lib");
    fs::path tmp_main = fs::temp_directory_path() / "syspath_abs_main.iec";
    {
        std::ofstream f(tmp_main);
        f << "{#syspath '" << abs_lib_dir.generic_string() << "'}\n"
          << "{#sinclude 'abs_target.iec'}\n";
    }
    std::string o;
    EXPECT_NO_THROW({ o = pp_file(tmp_main); });
    std::error_code ec;
    fs::remove(tmp_main, ec);
    EXPECT_NE(std::string::npos, o.find("ABS_LIB;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathWithoutCurrentFileUsesCwd) {
    // When there is no physical "current file" (e.g. string/library input,
    // matching how pp() drives preprocess() without push_file()), a relative
    // {#syspath} operand falls back to the process's current working
    // directory.
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({
        o = pp("{#syspath '" + (DIR / "syspath_base/lib").generic_string() + "'}"
               "{#sinclude 'sb_target.iec'}");
    });
    EXPECT_NE(std::string::npos, o.find("MAIN_LIB;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathEmptyPathMeansDeclaringFileDir) {
    // {#syspath ''} means "the declaring file's own directory".
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({ o = pp_file(DIR / "syspath_base/main2.iec"); });
    EXPECT_NE(std::string::npos, o.find("EMPTY_SYSPATH_TARGET;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathUnaffectedByLineDirective) {
    // {#line} only rewrites the diagnostic filepath (Issue::filepath()), not
    // the physical current file used by {#syspath} resolution.
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({ o = pp_file(DIR / "syspath_base/main3.iec"); });
    EXPECT_NE(std::string::npos, o.find("MAIN_LIB;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathMacroArgumentUsesDirectiveFile) {
    // The {#syspath} operand may itself be a macro invocation (expanded by
    // preprocess_text() before the path is parsed); the base directory is
    // still the file containing the {#syspath} line, not the file that
    // defined the macro (macrodef/header.iec, which has its own decoy lib/).
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({ o = pp_file(DIR / "syspath_base/main4.iec"); });
    EXPECT_NE(std::string::npos, o.find("MAIN_LIB;"));
    EXPECT_EQ(std::string::npos, o.find("MACRODEF_LIB;"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, SyspathPragmaReemittedAsWritten) {
    // The re-emitted (*{syspath:'...'}*) pragma carries the operand exactly
    // as written (relative text), never the resolved absolute base path.
    fs::current_path(jiepp_root_dir());
    std::string o;
    EXPECT_NO_THROW({ o = pp("{#syspath 'lib'}"); });
    EXPECT_NE(std::string::npos, o.find("(*{syspath:'lib'}*)"))
        << "expected the operand re-emitted as written; got:\n" << o;
    EXPECT_EQ(std::string::npos, o.find(fs::current_path().generic_string()))
        << "pragma must not be rewritten to an absolute path; got:\n" << o;
    EXPECT_TRUE(empty());
}
