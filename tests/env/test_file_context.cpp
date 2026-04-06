#include "test_helper.hpp"
#include "env/env.hpp"

class FileContextTest : public JieppTest {};

TEST_F(FileContextTest, DefaultConstruct) {
    Env env;
    EXPECT_EQ(0, env.num_of_files());
    EXPECT_EQ("", env.current_file());
    EXPECT_TRUE(env.syspaths().empty());
    EXPECT_EQ(1, env.get_lineno());
}

TEST_F(FileContextTest, FileStack) {
    Env env;
    EXPECT_EQ(0, env.num_of_files());
    env.push_file("foo.iec");
    EXPECT_EQ(1, env.num_of_files());
    // kludge: current_file returns absolute path
    std::string actual_current = env.current_file();
    fs::path actual_current_path = fs::path(actual_current);
    EXPECT_TRUE(actual_current_path.is_absolute());
    env.push_file("bar.iec");
    EXPECT_EQ(2, env.num_of_files());
    env.pop_file();
    EXPECT_EQ(1, env.num_of_files());
    env.pop_file();
    EXPECT_EQ(0, env.num_of_files());
}

TEST_F(FileContextTest, PopFileOnEmpty) {
    Env env;
    EXPECT_EQ(0, env.num_of_files());
    // pop_file on empty stacks is a safe no-op
    env.pop_file();
    EXPECT_EQ(0, env.num_of_files());
    EXPECT_EQ("", env.current_file());
    EXPECT_EQ(1, env.get_lineno()); // lineno unchanged
}

TEST_F(FileContextTest, PopFileLinenoRestore) {
    Env env;
    env.push_file("first.iec");
    env.set_lineno(10);
    env.push_file("second.iec");
    EXPECT_EQ(1, env.get_lineno()); // reset to 1 by push_file
    env.set_lineno(25);
    env.pop_file();
    EXPECT_EQ(10, env.get_lineno()); // restored to saved lineno
}

TEST_F(FileContextTest, Syspaths) {
    Env env;
    EXPECT_TRUE(env.syspaths().empty());
    env.add_syspath("/usr/include");
    env.add_syspath("/usr/local/include");
    ASSERT_EQ(2u, env.syspaths().size());

    // kludge: syspaths returns absolute paths
    std::string actual = env.syspaths()[0];
    fs::path actual_path = fs::path(actual);
    EXPECT_TRUE(actual_path.is_absolute());

    actual = env.syspaths()[1];
    actual_path = fs::path(actual);
    EXPECT_TRUE(actual_path.is_absolute());
}

TEST_F(FileContextTest, Lineno) {
    Env env;
    EXPECT_EQ(1, env.get_lineno());
    env.set_lineno(42);
    EXPECT_EQ(42, env.get_lineno());
}
