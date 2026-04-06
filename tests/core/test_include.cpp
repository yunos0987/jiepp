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

TEST_F(IncludeTest, CircularInclude) {
    fs::current_path(jiepp_root_dir());
    Env env = setup();
    env.set_max_include_depth(8);
    EXPECT_THROW(pp_file(DIR / "circular_a.iec", env), Issue::Exception);
    EXPECT_EQ("circular_b.iec:1.0: error: PP12: Maximum include depth exceeded; 'circular_a.iec'", message());
    EXPECT_TRUE(empty());
}
