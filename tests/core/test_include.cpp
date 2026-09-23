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
