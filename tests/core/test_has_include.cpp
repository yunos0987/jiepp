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

// ---- Word-boundary requirement ----

TEST_F(HasIncludeTest, KeywordRequiresLeftWordBoundary) {
    // A user function-macro whose name happens to end in "__has_include"
    // must still expand normally: the raw substring scan for the operator
    // must not treat the tail of a longer identifier as the keyword.
    auto r = pp(
        "{#define weird__has_include(x) 99}"
        "{#if weird__has_include('foo') = 99}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_EQ(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

// ---- Directory target ----

TEST_F(HasIncludeTest, DirectoryIsNotAFile) {
    // A directory candidate must not be reported as an existing include
    // target: __has_include('<a directory>') evaluates to false (0), just
    // like an ordinary missing file.
    fs::create_directories(tmp_dir_ / "existing_dir");
    auto r = pp_hi("{#if __has_include('existing_dir')}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("NO"));
    EXPECT_EQ(std::string::npos, r.find("YES"));
    EXPECT_TRUE(empty());
}

// ---- R4 follow-up Q5: __has_include counts as defined (not in sandbox) ----
#ifndef JIEPP_SANDBOX

// H1
TEST_F(HasIncludeTest, CountsAsDefinedForIfdefIfndefAndDefined) {
    {
        auto r = pp("{#ifdef __has_include}y{#else}n{#endif}");
        EXPECT_EQ("y", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#ifndef __has_include}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#if defined(__has_include)}y{#else}n{#endif}");
        EXPECT_EQ("y", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#if defined __has_include}y{#else}n{#endif}");
        EXPECT_EQ("y", r);
        EXPECT_TRUE(empty());
    }
}

// H2
TEST_F(HasIncludeTest, IfdefGuardsActualUse) {
    auto r = pp_hi("{#ifdef __has_include}{#if __has_include('existing.iec')}Y{#endif}{#endif}");
    EXPECT_NE(std::string::npos, r.find("Y"));
    EXPECT_TRUE(empty());
}

// H3
TEST_F(HasIncludeTest, StaysDefinedAfterUndef) {
    // jiepp's __has_include(...) operator cannot be disabled by {#undef}
    // (Q4: documented, not changed); it survives.
    auto r = pp("{#undef __has_include}{#ifdef __has_include}y{#else}n{#endif}");
    EXPECT_EQ("y", r);
    EXPECT_TRUE(empty());
}

// H4: regression -- __has_include_next is not implemented, so it reads as
// undefined and never appears in a macro dump.
TEST_F(HasIncludeTest, HasIncludeNextNotImplemented) {
    auto r = pp("{#ifdef __has_include_next}y{#else}n{#endif}");
    EXPECT_EQ("n", r);

    Env env = setup();
    std::ostringstream os;
    dump_macros(env, os);
    EXPECT_EQ(std::string::npos, os.str().find("__has_include"));
}

// U8 (B'): a quoted path keeps its IEC escapes through the directive
// decoding and is decoded as a path, so it names the same file as before;
// "$'" inside the path no longer ends the literal early.
TEST_F(HasIncludeTest, QuotedPathEscapes) {
    std::ofstream(tmp_dir_ / "it's.iec") << "ITS;\n";
    std::ofstream(tmp_dir_ / "a$b.iec") << "AB;\n";
    EXPECT_EQ("YES", pp_hi(R"({#if __has_include('it$'s.iec')}YES{#else}NO{#endif})"));
    EXPECT_EQ("YES", pp_hi(R"({#if __has_include("it's.iec")}YES{#else}NO{#endif})"));
    EXPECT_EQ("YES", pp_hi(R"({#if __has_include('a$$b.iec')}YES{#else}NO{#endif})"));
    EXPECT_EQ("NO", pp_hi(R"({#if __has_include('no$'such.iec')}YES{#else}NO{#endif})"));
    EXPECT_TRUE(empty());
    EXPECT_NE(std::string::npos, pp_hi(R"({#include 'it$'s.iec'})").find("ITS;"));
    EXPECT_NE(std::string::npos, pp_hi(R"({#include 'a$$b.iec'})").find("AB;"));
    EXPECT_NE(std::string::npos, pp_hi(R"({#include 'a$24b.iec'})").find("AB;"));
    EXPECT_NE(std::string::npos, pp_hi(R"({#define P 'a$$b.iec'}{#include P})").find("AB;"));
    EXPECT_NE(std::string::npos, pp_hi(R"({#sinclude "it's.iec"})").find("ITS;"));
    // A comment after a path with "$'" is whitespace (the U2 blanking
    // follows tokenize(), which now sees the literal as written).
    EXPECT_NE(std::string::npos, pp_hi(R"({#include 'it$'s.iec' // c})").find("ITS;"));
    EXPECT_TRUE(empty());
    // 'a$' never closes, so it is not a string literal and decodes as
    // before B' (to the path 'a').
    EXPECT_THROW(pp_hi(R"({#include 'a$'})"), Issue::Exception);
    EXPECT_EQ(Issue::Code::FILE_NOT_FOUND, code());
}

#endif  // !JIEPP_SANDBOX
