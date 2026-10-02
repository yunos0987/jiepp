#include "test_helper.hpp"

#include "util/path.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// {#pragma once} identifies a file by what it is on disk, not by how its path
// is spelled (gcc/clang use the inode / file ID). The current file is pushed
// with a lexically normalised path while {#include} resolves to a canonical
// one, so a root file reached through another spelling (letter case, "..",
// an 8.3 short name, a symlink, a hard link) used to be included a second time.

namespace {

class PragmaOnceIdentityTest : public JieppTest {
protected:
    void SetUp() override {
        JieppTest::SetUp();
        dir_ = fs::temp_directory_path() / "jiepp_pragma_once_identity";
        std::error_code ec;
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_ / "sub");
        // The file includes itself by its real name: a second pass over it
        // prints another "X;" unless the first pass was recognised.
        write(dir_ / "x.iec", "{#pragma once}\nX;\n{#include 'x.iec'}\n");
        // A plain header and a main file that includes it twice by name.
        write(dir_ / "h.iec", "{#pragma once}\nH;\n");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
        JieppTest::TearDown();
    }

    static void write(const fs::path& p, const std::string& text) {
        std::ofstream f(p, std::ios::binary);
        f << text;
    }

    static std::size_t count(const std::string& s, const std::string& needle) {
        std::size_t n = 0;
        for (std::size_t pos = 0; (pos = s.find(needle, pos)) != std::string::npos; pos += needle.size())
            ++n;
        return n;
    }

    // Runs `spelling` as the root file; its body includes x.iec by real name.
    std::string run_root(const fs::path& spelling) {
        return pp_file(spelling);
    }

    fs::path dir_;
};

} // namespace

TEST_F(PragmaOnceIdentityTest, RootBySamePathStopsSelfInclude) {
    EXPECT_EQ(1u, count(run_root(dir_ / "x.iec"), "X;"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaOnceIdentityTest, RootThroughDotDotSpelling) {
    EXPECT_EQ(1u, count(run_root(dir_ / "sub" / ".." / "x.iec"), "X;"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaOnceIdentityTest, IncludedThroughDotDotSpelling) {
    write(dir_ / "m.iec", "{#include 'h.iec'}\n{#include 'sub/../h.iec'}\n{#include './h.iec'}\n");
    EXPECT_EQ(1u, count(pp_file(dir_ / "m.iec"), "H;"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaOnceIdentityTest, RootThroughHardLink) {
    std::error_code ec;
    fs::create_hard_link(dir_ / "x.iec", dir_ / "link.iec", ec);
    if (ec)
        GTEST_SKIP() << "hard links are not available: " << ec.message();
    EXPECT_EQ(1u, count(run_root(dir_ / "link.iec"), "X;"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaOnceIdentityTest, IncludedThroughHardLink) {
    std::error_code ec;
    fs::create_hard_link(dir_ / "h.iec", dir_ / "hlink.iec", ec);
    if (ec)
        GTEST_SKIP() << "hard links are not available: " << ec.message();
    write(dir_ / "m.iec", "{#include 'h.iec'}\n{#include 'hlink.iec'}\n");
    EXPECT_EQ(1u, count(pp_file(dir_ / "m.iec"), "H;"));
    EXPECT_TRUE(empty());
}

#ifdef _WIN32

TEST_F(PragmaOnceIdentityTest, RootWithDifferentLetterCase) {
    // NTFS is case-insensitive: X.IEC and x.iec are one file.
    EXPECT_EQ(1u, count(run_root(dir_ / "X.IEC"), "X;"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaOnceIdentityTest, RootThroughShortName) {
    // GetShortPathNameW returns a different spelling only when the volume has
    // 8.3 names enabled; the directory name is long enough to need one.
    std::wstring wide = (dir_ / "x.iec").wstring();
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetShortPathNameW(wide.c_str(), buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0]))
        GTEST_SKIP() << "no short name for the test file";
    fs::path shortp(std::wstring(buf, n));
    std::string a = shortp.generic_string();
    std::string b = (dir_ / "x.iec").generic_string();
    if (a == b)
        GTEST_SKIP() << "8.3 names are disabled on this volume";
    EXPECT_EQ(1u, count(run_root(shortp), "X;"));
    EXPECT_TRUE(empty());
}

#else

TEST_F(PragmaOnceIdentityTest, RootThroughSymlink) {
    std::error_code ec;
    fs::create_symlink(dir_ / "x.iec", dir_ / "sym.iec", ec);
    if (ec)
        GTEST_SKIP() << "symlinks are not available: " << ec.message();
    EXPECT_EQ(1u, count(run_root(dir_ / "sym.iec"), "X;"));
    EXPECT_TRUE(empty());
}

TEST_F(PragmaOnceIdentityTest, IncludedThroughSymlink) {
    std::error_code ec;
    fs::create_symlink(dir_ / "h.iec", dir_ / "hsym.iec", ec);
    if (ec)
        GTEST_SKIP() << "symlinks are not available: " << ec.message();
    write(dir_ / "m.iec", "{#include 'h.iec'}\n{#include 'hsym.iec'}\n");
    EXPECT_EQ(1u, count(pp_file(dir_ / "m.iec"), "H;"));
    EXPECT_TRUE(empty());
}

#endif

TEST(FileIdTest, SameFileSameIdDifferentFilesDifferentIds) {
    const fs::path d = fs::temp_directory_path() / "jiepp_file_id_test";
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d / "s");
    { std::ofstream(d / "a") << "a"; }
    { std::ofstream(d / "b") << "b"; }
    Util::FileId a1, a2, b;
    ASSERT_TRUE(Util::file_id((d / "a").string(), a1));
    ASSERT_TRUE(Util::file_id((d / "s" / ".." / "a").string(), a2));
    ASSERT_TRUE(Util::file_id((d / "b").string(), b));
    EXPECT_TRUE(a1 == a2);
    EXPECT_FALSE(a1 == b);
    Util::FileId none;
    EXPECT_FALSE(Util::file_id((d / "missing").string(), none));
    EXPECT_FALSE(Util::file_id("", none));
    fs::remove_all(d, ec);
}
