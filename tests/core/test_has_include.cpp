#include "test_helper.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class HasIncludeTest : public JieppTest {
protected:
    fs::path tmp_dir_;
    fs::path tmp_file_;

    void SetUp() override {
        JieppTest::SetUp();
        tmp_dir_ = fs::temp_directory_path() / "jiepp_has_include_test";
        fs::create_directories(tmp_dir_);
        tmp_file_ = tmp_dir_ / "existing.iec";
        std::ofstream(tmp_file_) << "// test file\n";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
        JieppTest::TearDown();
    }

    // preprocess with syspath pointing to tmp_dir_
    std::string pp_hi(const std::string& input) {
        Env env = setup();
        env.add_syspath(tmp_dir_.generic_string());
        return pp(input, env);
    }
};

// ---- Basic existence check ----

TEST_F(HasIncludeTest, ExistingQuoted) {
    auto r = pp_hi("{#if __has_include(\"existing.iec\")}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_EQ(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

TEST_F(HasIncludeTest, NonExistingQuoted) {
    auto r = pp_hi("{#if __has_include(\"nonexistent.iec\")}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("NO"));
    EXPECT_EQ(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}

TEST_F(HasIncludeTest, ExistingAngle) {
    auto r = pp_hi("{#if __has_include(<existing.iec>)}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_EQ(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

TEST_F(HasIncludeTest, NonExistingAngle) {
    auto r = pp_hi("{#if __has_include(<nonexistent.iec>)}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("NO"));
    EXPECT_EQ(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}

// ---- Combination with other operators ----

TEST_F(HasIncludeTest, WithAnd) {
    auto r = pp_hi(
        "{#define FLAG 1}"
        "{#if __has_include(\"existing.iec\") \\and\\ defined(FLAG)}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}

TEST_F(HasIncludeTest, WithNot) {
    auto r = pp_hi(
        "{#if \\not\\ __has_include(\"existing.iec\")}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

// ---- elif ----

TEST_F(HasIncludeTest, InElif) {
    auto r = pp_hi(
        "{#if __has_include(\"nonexistent.iec\")}A"
        "{#elif __has_include(\"existing.iec\")}B"
        "{#else}C{#endif}");
    EXPECT_NE(std::string::npos, r.find("B"));
    EXPECT_EQ(std::string::npos, r.find("A"));
    EXPECT_EQ(std::string::npos, r.find("C"));
    EXPECT_TRUE(empty());
}

// ---- Whitespace tolerance ----

TEST_F(HasIncludeTest, WhitespaceInside) {
    auto r = pp_hi("{#if __has_include( \"existing.iec\" )}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}

// ---- Multiple occurrences in one expression ----

TEST_F(HasIncludeTest, Multiple) {
    auto r = pp_hi(
        "{#if __has_include(\"existing.iec\") \\and\\ __has_include(\"existing.iec\")}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}

// ---- IEC single-quote form ----

TEST_F(HasIncludeTest, ExistingSingleQuote) {
    auto r = pp_hi("{#if __has_include('existing.iec')}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_EQ(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

TEST_F(HasIncludeTest, NonExistingSingleQuote) {
    auto r = pp_hi("{#if __has_include('nonexistent.iec')}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("NO"));
    EXPECT_EQ(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}
