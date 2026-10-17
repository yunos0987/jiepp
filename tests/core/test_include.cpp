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

TEST_F(IncludeTest, HasIncludeWithComment) {
    // __has_include() is resolved from the raw text before the expression
    // is blanked of comments, so a comment after it does not break the
    // rest of the {#if} expression.
    fs::current_path(jiepp_root_dir());
    EXPECT_EQ("y", pp("{#if __has_include('tests/core/test_include/a.iec') // c}y{#else}n{#endif}"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeTest, CommentsAroundIncludeOperand) {
    // Comments around the {#include}/{#sinclude}/{#syspath} operand are
    // whitespace, like in C; the quoted path (or <...> path) itself is
    // unaffected.
    fs::current_path(jiepp_root_dir());
    std::string plain = pp("{#include 'tests/core/test_include/a.iec'}");
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#include 'tests/core/test_include/a.iec' // c}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#include 'tests/core/test_include/a.iec' (* c *)}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#include 'tests/core/test_include/a.iec' /* c */}"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(plain, pp("{#include (* c *) 'tests/core/test_include/a.iec'}"));
    EXPECT_TRUE(empty());

    // <...> path is copied verbatim: a "//" inside it stays part of the
    // path, not a comment.
    std::string angle = pp("{#syspath '.'}{#include <tests/core//test_include/a.iec>}");
    EXPECT_TRUE(empty());
    EXPECT_EQ(angle, pp("{#syspath '.'}{#include <tests/core//test_include/a.iec> (* c *)}"));
    EXPECT_TRUE(empty());

    std::string sinc = pp("{#syspath 'tests/core/test_include'}{#sinclude 'a.iec'}");
    EXPECT_TRUE(empty());
    EXPECT_EQ(sinc, pp("{#syspath 'tests/core/test_include' // c}{#sinclude 'a.iec' (* c *)}"));
    EXPECT_TRUE(empty());

    // A document comment is a token, not whitespace: still an error.
    EXPECT_THROW(pp("{#include 'tests/core/test_include/a.iec' (*! d *)}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PATH, code());
}

TEST_F(IncludeTest, PragmaOnceWithCommentOrExtraTokens) {
    // A comment after "once" is whitespace, and extra tokens after "once"
    // are a warning but the pragma still applies (like clang), so a
    // self-including file still stops recursing instead of hitting
    // MAX_INCLUDE_DEPTH_EXCEEDED (PP12).
    fs::current_path(jiepp_root_dir());
    {
        std::string o;
        EXPECT_NO_THROW({ o = pp_file(DIR / "pragma_once_comment.iec"); });
        static const std::string needle = "X;";
        std::size_t count = 0;
        for (std::size_t pos = 0; (pos = o.find(needle, pos)) != std::string::npos;
             pos += needle.size())
            ++count;
        EXPECT_EQ(1u, count);
        EXPECT_TRUE(empty());
    }
    {
        std::string o;
        EXPECT_NO_THROW({ o = pp_file(DIR / "pragma_once_extra.iec"); });
        static const std::string needle = "Y;";
        std::size_t count = 0;
        for (std::size_t pos = 0; (pos = o.find(needle, pos)) != std::string::npos;
             pos += needle.size())
            ++count;
        EXPECT_EQ(1u, count);
        auto cs = codes();
        ASSERT_EQ(1u, cs.size());
        EXPECT_EQ(Issue::Code::EXTRA_TOKENS_AT_END_OF_DIRECTIVE, cs[0]);
    }
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

#ifndef JIEPP_SANDBOX
// A9: a path with a NUL ($00 decoded in the directive operand) used to be
// truncated at the NUL by the C-string filesystem calls, so
// `'a.iec$00x'` included a.iec. It is PP47 now (the sandbox has no file
// access directives, hence no sandbox build of these tests).
TEST_F(IncludeTest, IncludePathWithNulIsPP47) {
    fs::current_path(jiepp_root_dir());
    Issue::ContinueMode guard({});
    // The <...> and {#sinclude} forms search only the syspath list, so a
    // syspath that contains a.iec is put first: without the NUL check the
    // truncated path would be found and included.
    {
        // Control: the same syspath and a name without the NUL include a.iec.
        std::string o;
        EXPECT_NO_THROW({ o = pp("{#syspath 'tests/core/test_include'}{#include <a.iec>}"); });
        EXPECT_NE(std::string::npos, o.find("a.iec begin")) << o;
        EXPECT_TRUE(empty());
        EXPECT_NO_THROW({ o = pp("{#syspath 'tests/core/test_include'}{#sinclude 'a.iec'}"); });
        EXPECT_NE(std::string::npos, o.find("a.iec begin")) << o;
        EXPECT_TRUE(empty());
    }
    for (const char* src : {"{#include 'tests/core/test_include/a.iec$00x'}",
                            "{#include <tests/core/test_include/a.iec$00x>}",
                            "{#sinclude 'tests/core/test_include/a.iec$00x'}",
                            "{#syspath 'tests/core/test_include'}{#include <a.iec$00x>}",
                            "{#syspath 'tests/core/test_include'}{#sinclude 'a.iec$00x'}"}) {
        std::string o;
        EXPECT_NO_THROW({ o = pp(src); }) << src;
        EXPECT_EQ(std::string::npos, o.find("a.iec begin")) << src << "\n" << o;
        auto msgs = messages();
        ASSERT_EQ(1u, msgs.size()) << src;
        EXPECT_EQ(Issue::Code::INVALID_PATH, PlainTextMessage::parse_code(msgs[0])) << src;
        // The context shows the NUL as $00, not as a raw byte.
        EXPECT_NE(std::string::npos, msgs[0].find("a.iec$00x")) << msgs[0];
        EXPECT_EQ(std::string::npos, msgs[0].find('\0'));
    }
}

TEST_F(IncludeTest, SyspathWithNulIsPP47) {
    fs::current_path(jiepp_root_dir());
    Issue::ContinueMode guard({});
    std::string o;
    EXPECT_NO_THROW({ o = pp("{#syspath 'tests/core/test_include/shadow_syspath1$00x'}"); });
    EXPECT_EQ(std::string::npos, o.find("syspath")) << o;
    auto cs = codes();
    ASSERT_EQ(1u, cs.size());
    EXPECT_EQ(Issue::Code::INVALID_PATH, cs[0]);
}

TEST_F(IncludeTest, HasIncludeWithNulIsFalse) {
    fs::current_path(jiepp_root_dir());
    Issue::ContinueMode guard({});
    // Control: the same path without the NUL exists.
    std::string o = pp("{#if __has_include('tests/core/test_include/a.iec')}HAS{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, o.find("HAS")) << o;
    EXPECT_TRUE(empty());
    o = pp("{#if __has_include('tests/core/test_include/a.iec$00x')}HAS{#else}NO{#endif}");
    EXPECT_EQ(std::string::npos, o.find("HAS")) << o;
    EXPECT_NE(std::string::npos, o.find("NO")) << o;
    EXPECT_TRUE(empty());
    // <...> form: searches only the syspath list. Control first, then the
    // NUL path that would resolve to a.iec if it were truncated at the NUL.
    o = pp("{#syspath 'tests/core/test_include'}"
           "{#if __has_include(<a.iec>)}HAS{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, o.find("HAS")) << o;
    EXPECT_TRUE(empty());
    o = pp("{#syspath 'tests/core/test_include'}"
           "{#if __has_include(<a.iec$00x>)}HAS{#else}NO{#endif}");
    EXPECT_EQ(std::string::npos, o.find("HAS")) << o;
    EXPECT_NE(std::string::npos, o.find("NO")) << o;
    EXPECT_TRUE(empty());
}

// ---- --max-include-count (PP66) ----
//
// The number of executed {#include}/{#sinclude} directives is capped per
// preprocess() call: the (N+1)th raises PP66 (SEVERE) at that directive.
// {#pragma once} skips count too (the path is resolved either way). The
// sandbox has no include directives (PP62), hence no sandbox build of these.

class IncludeCountTest : public JieppTest {
protected:
    fs::path dir_;
    std::string inc_;      // include operand: 'path'
    std::string once_;

    void SetUp() override {
        JieppTest::SetUp();
        dir_ = fs::temp_directory_path() / "jiepp_include_count_test";
        fs::create_directories(dir_);
        write(dir_ / "inc.iec", "X\n");
        write(dir_ / "once.iec", "{#pragma once}\nO\n");
        inc_  = "{#include '" + (dir_ / "inc.iec").generic_string() + "'}\n";
        once_ = "{#include '" + (dir_ / "once.iec").generic_string() + "'}\n";
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
        JieppTest::TearDown();
    }
    static void write(const fs::path& p, const std::string& s) {
        std::ofstream f(p, std::ios::binary);
        f << s;
    }
    static std::string repeat(const std::string& s, int n) {
        std::string r;
        for (int i = 0; i < n; ++i)
            r += s;
        return r;
    }
    static std::size_t count_of(const std::string& hay, const std::string& needle) {
        std::size_t n = 0;
        for (std::size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1))
            ++n;
        return n;
    }
    // True when preprocessing src raised PP66 (any other outcome is false).
    bool raises_pp66(Env& env, const std::string& src) {
        try {
            pp(src, env);
        } catch (const Issue::Exception& e) {
            return e.code == Issue::Code::MAX_INCLUDE_COUNT_EXCEEDED;
        }
        return false;
    }
};

TEST_F(IncludeCountTest, DefaultIsTenThousand) {
    Env env = setup();
    EXPECT_EQ(10000, env.get_max_include_count());
    EXPECT_EQ(DEFAULT_MAX_INCLUDE_COUNT, 10000);
}

TEST_F(IncludeCountTest, StopsAtNPlusOne) {
    {
        Env env = setup();
        env.set_max_include_count(3);
        const std::string o = pp(repeat(inc_, 3), env);
        EXPECT_EQ(3u, count_of(o, "X")) << o;
        EXPECT_TRUE(empty());
    }
    {
        Env env = setup();
        env.set_max_include_count(3);
        EXPECT_TRUE(raises_pp66(env, repeat(inc_, 4)));
        const auto ms = messages();
        ASSERT_EQ(1u, ms.size());
        EXPECT_NE(std::string::npos, ms[0].find("error: PP66: Maximum include count exceeded")) << ms[0];
        EXPECT_NE(std::string::npos,
                  ms[0].find("limit 3; use --max-include-count N to raise it (0 = no limit)")) << ms[0];
    }
}

TEST_F(IncludeCountTest, SincludeIsCounted) {
    Env env = setup();
    env.set_max_include_count(1);
    const std::string src = "{#sinclude '" + (dir_ / "inc.iec").generic_string() + "'}\n";
    EXPECT_TRUE(raises_pp66(env, repeat(src, 2)));
}

TEST_F(IncludeCountTest, PragmaOnceSkipsAreCounted) {
    {
        Env env = setup();
        env.set_max_include_count(4);
        const std::string o = pp(repeat(once_, 4), env);
        EXPECT_EQ(1u, count_of(o, "O")) << o;   // 3 of the 4 are skipped
    }
    {
        Env env = setup();
        env.set_max_include_count(3);
        EXPECT_TRUE(raises_pp66(env, repeat(once_, 4)));
    }
}

TEST_F(IncludeCountTest, ZeroIsUnlimited) {
    Env env = setup();
    env.set_max_include_count(0);
    const std::string o = pp(repeat(inc_, 50), env);
    EXPECT_EQ(50u, count_of(o, "X"));
    EXPECT_TRUE(empty());
}

TEST_F(IncludeCountTest, ResetPerPreprocessCall) {
    Env env = setup();
    env.set_max_include_count(3);
    EXPECT_EQ(3u, count_of(pp(repeat(inc_, 3), env), "X"));
    EXPECT_EQ(3u, count_of(pp(repeat(inc_, 3), env), "X"));
    EXPECT_TRUE(empty());
}

// An include that does not resolve to a syntactically valid path is not an
// executed include (PP47), and a failing one does not need the count either.
TEST_F(IncludeCountTest, InvalidPathIsNotCounted) {
    Env env = setup();
    env.set_max_include_count(1);
    Issue::ContinueMode guard({});
    pp(repeat("{#include 'a$00b'}\n", 3), env);
    const auto cs = codes();
    ASSERT_EQ(3u, cs.size());
    for (auto c : cs)
        EXPECT_EQ(Issue::Code::INVALID_PATH, c);
}

// {#ignore PP66} has no effect: PP66 is SEVERE.
TEST_F(IncludeCountTest, IgnorePP66HasNoEffect) {
    Env env = setup();
    env.set_max_include_count(1);
    EXPECT_TRUE(raises_pp66(env, "{#ignore PP66}\n" + repeat(inc_, 2)));
}
#endif
