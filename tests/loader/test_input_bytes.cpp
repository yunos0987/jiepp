#include "test_helper.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

// ---- C11: input reads (read errors, PP10 '<input>') ----
//
// The fixture name starts with RobustnessTest, so the sandbox ctest runs
// these too (they read only through preprocess(std::istream&, ...)).

namespace {

// Hands out `data` once, then throws from underflow(): a read error of the
// caller's stream after some bytes.
class ThrowingStreambuf : public std::streambuf {
public:
    explicit ThrowingStreambuf(std::string data) : data_(std::move(data)) {}
protected:
    int_type underflow() override {
        if (!served_) {
            served_ = true;
            setg(data_.data(), data_.data(), data_.data() + data_.size());
            return traits_type::to_int_type(data_[0]);
        }
        throw std::runtime_error("read failed");
    }
private:
    std::string data_;
    bool served_ = false;
};

} // namespace

class RobustnessTestInputBytes : public JieppTest {};

TEST_F(RobustnessTestInputBytes, ReadErrorIsPP10) {
    Env env = setup();
    ThrowingStreambuf sb("x := 1;\n");
    std::istream in(&sb);
    std::ostringstream out;
    bool pp10 = false;
    try {
        preprocess(in, out, env);
    } catch (const Issue::Exception& e) {
        pp10 = (e.code == Issue::Code::FILE_ERROR);
    } catch (const std::exception& e) {
        ADD_FAILURE() << "an exception other than Issue::Exception escaped: " << e.what();
    }
    EXPECT_TRUE(pp10);
    auto lines = messages();
    ASSERT_EQ(1u, lines.size());
    EXPECT_NE(std::string::npos, lines[0].find("PP10: An error occurred with the file; '<input>'")) << lines[0];
}

// Guard: passes before and after C11. A stream whose open failed is an
// empty input (failbit is not a read error), as in 1.0.0.
TEST_F(RobustnessTestInputBytes, FailedOpenStreamIsEmptyNotPP10) {
    const std::string expected = pp(std::string());
    ASSERT_TRUE(empty());

    std::ifstream missing(fs::temp_directory_path() / "jiepp_c11_no_such_file.iec", std::ios::binary);
    ASSERT_FALSE(missing.is_open());
    EXPECT_EQ(expected, pp(missing));
    EXPECT_TRUE(empty());
}

// Guard: passes before and after C11. A stream that arrives with eofbit set
// is still read from its buffer, as in 1.0.0.
TEST_F(RobustnessTestInputBytes, StreamWithEofbitIsStillRead) {
    const std::string text = "x := 1;\ny := 2;\n";
    const std::string expected = pp(text);
    ASSERT_TRUE(empty());

    std::istringstream in(text);
    in.setstate(std::ios::eofbit);
    EXPECT_EQ(expected, pp(in));
    EXPECT_TRUE(empty());
}

// ---- C11: --max-input-bytes (PP67) in the library ----

namespace {

const char* const PP67_TAIL_10 =
    "PP67: Maximum input size exceeded; 'limit 10 bytes; use --max-input-bytes N to raise it (0 = no limit)'";

// preprocess() of `text` with `env`; returns the code of the Issue::Exception
// it threw (or nullopt) and the output in `out`.
std::optional<Issue::Code> run_pp(const std::string& text, Env& env, std::string* out = nullptr) {
    std::istringstream in(text);
    std::ostringstream o;
    std::optional<Issue::Code> code;
    try {
        preprocess(in, o, env);
    } catch (const Issue::Exception& e) {
        code = e.code;
    }
    if (out)
        *out = o.str();
    return code;
}

// `n` bytes of valid input: "x := 1;\n" lines, the last one cut short.
std::string input_of_size(std::size_t n) {
    std::string s;
    while (s.size() < n)
        s += "x := 1;\n";
    s.resize(n);
    return s;
}

bool ends_with(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// Hands out 'a' forever, at most 1000 bytes per xsgetn() call (fewer than
// asked for, so the reader must keep reading until sgetn() returns 0), and
// counts the bytes handed out.
class EndlessStreambuf : public std::streambuf {
public:
    std::size_t handed_out = 0;
protected:
    std::streamsize xsgetn(char* s, std::streamsize n) override {
        const std::streamsize k = std::min<std::streamsize>(n, 1000);
        std::fill(s, s + k, 'a');
        handed_out += static_cast<std::size_t>(k);
        return k;
    }
    int_type underflow() override {
        ch_ = 'a';
        setg(&ch_, &ch_, &ch_ + 1);
        return traits_type::to_int_type(ch_);
    }
private:
    char ch_ = 'a';
};

} // namespace

TEST_F(RobustnessTestInputBytes, OverLimitIsPP67) {
    Env env = setup();
    env.set_max_input_bytes(10);
    EXPECT_EQ(Issue::Code::MAX_INPUT_BYTES_EXCEEDED, run_pp(input_of_size(11), env));
    auto lines = messages();
    ASSERT_EQ(1u, lines.size());
    EXPECT_TRUE(ends_with(lines[0], PP67_TAIL_10)) << lines[0];
}

TEST_F(RobustnessTestInputBytes, ExactLimitOk) {
    std::string unlimited;
    {
        Env env = setup();
        ASSERT_EQ(std::nullopt, run_pp(input_of_size(10), env, &unlimited));
    }
    Env env = setup();
    env.set_max_input_bytes(10);
    std::string limited;
    EXPECT_EQ(std::nullopt, run_pp(input_of_size(10), env, &limited));
    EXPECT_TRUE(empty());
    EXPECT_EQ(unlimited, limited);
}

TEST_F(RobustnessTestInputBytes, BomCounted) {
    const std::string text = "\xEF\xBB\xBF" "x := 1;\n";
    ASSERT_EQ(11u, text.size());
    {
        Env env = setup();
        env.set_max_input_bytes(10);
        EXPECT_EQ(Issue::Code::MAX_INPUT_BYTES_EXCEEDED, run_pp(text, env));
        auto lines = messages();
        ASSERT_EQ(1u, lines.size());
        EXPECT_TRUE(ends_with(lines[0], PP67_TAIL_10)) << lines[0];
    }
    Env env = setup();
    env.set_max_input_bytes(11);
    std::string out;
    EXPECT_EQ(std::nullopt, run_pp(text, env, &out));
    EXPECT_TRUE(empty());
    EXPECT_EQ(std::string::npos, out.find("\xEF\xBB\xBF")) << out;
}

TEST_F(RobustnessTestInputBytes, ZeroIsUnlimited) {
    Env env = setup();
    env.set_max_input_bytes(0);
    EXPECT_EQ(std::nullopt, run_pp(input_of_size(2u * 1024 * 1024), env));
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTestInputBytes, ResetPerPreprocessCall) {
    Env env = setup();
    env.set_max_input_bytes(10);
    EXPECT_EQ(std::nullopt, run_pp(input_of_size(8), env));
    EXPECT_EQ(std::nullopt, run_pp(input_of_size(8), env));
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTestInputBytes, UnboundedStreamStopsEarly) {
    Env env = setup();
    const std::size_t limit = 1024 * 1024;
    env.set_max_input_bytes(static_cast<int>(limit));
    EndlessStreambuf sb;
    std::istream in(&sb);
    std::ostringstream out;
    std::optional<Issue::Code> code;
    try {
        preprocess(in, out, env);
    } catch (const Issue::Exception& e) {
        code = e.code;
    }
    EXPECT_EQ(Issue::Code::MAX_INPUT_BYTES_EXCEEDED, code);
    EXPECT_GT(sb.handed_out, limit);
    EXPECT_LE(sb.handed_out, limit + 64 * 1024);
}

TEST_F(RobustnessTestInputBytes, IgnoreDoesNotSuppressPP67) {
    Issue::add_ignoring(Issue::Code::MAX_INPUT_BYTES_EXCEEDED);
    Env env = setup();
    env.set_max_input_bytes(10);
    EXPECT_EQ(Issue::Code::MAX_INPUT_BYTES_EXCEEDED, run_pp(input_of_size(11), env));
    auto lines = messages();
    ASSERT_EQ(1u, lines.size());
    EXPECT_TRUE(ends_with(lines[0], PP67_TAIL_10)) << lines[0];
}

TEST_F(RobustnessTestInputBytes, ChunkBoundaryExactLimit) {
    for (std::size_t n : {std::size_t{65536}, std::size_t{131072}}) {
        Env env = setup();
        env.set_max_input_bytes(static_cast<int>(n));
        EXPECT_EQ(std::nullopt, run_pp(input_of_size(n), env)) << n;
    }
    EXPECT_TRUE(empty());
}

TEST_F(RobustnessTestInputBytes, ChunkBoundaryOneOver) {
    for (std::size_t n : {std::size_t{65536}, std::size_t{131072}}) {
        Env env = setup();
        env.set_max_input_bytes(static_cast<int>(n));
        EXPECT_EQ(Issue::Code::MAX_INPUT_BYTES_EXCEEDED, run_pp(input_of_size(n + 1), env)) << n;
    }
    EXPECT_EQ(2u, messages().size());
}

// ---- C11: files read by {#include} (normal build only) ----

#ifndef JIEPP_SANDBOX
class InputBytesTest : public JieppTest {
protected:
    fs::path dir_;

    void SetUp() override {
        JieppTest::SetUp();
        dir_ = fs::temp_directory_path() / "jiepp_c11_input_bytes";
        std::error_code ec;
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_);
        write(dir_ / "a.inc", A_TEXT);
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

    // preprocess() of the file `main` (pushed as the current file, so a
    // relative {#include} resolves next to it); returns the thrown code.
    std::optional<Issue::Code> run_main(const fs::path& main, Env& env) {
        std::optional<Issue::Code> code;
        try {
            pp_file(main, env);
        } catch (const Issue::Exception& e) {
            code = e.code;
            Issue::pop();
            env.pop_file();
        }
        return code;
    }

    static constexpr const char* A_TEXT = "A := 1;\n";
    static constexpr std::size_t A = 8;
};

TEST_F(InputBytesTest, IncludesCounted) {
    const std::string main_text = "{#include 'a.inc'}\n";
    fs::path main = dir_ / "main.iec";
    write(main, main_text);
    const int total = static_cast<int>(main_text.size() + A);

    {
        Env env = setup();
        env.set_max_input_bytes(total);
        EXPECT_EQ(std::nullopt, run_main(main, env));
        EXPECT_TRUE(empty());
    }
    Env env = setup();
    env.set_max_input_bytes(total - 1);
    EXPECT_EQ(Issue::Code::MAX_INPUT_BYTES_EXCEEDED, run_main(main, env));
    auto lines = messages();
    ASSERT_EQ(1u, lines.size());
    EXPECT_EQ(0u, lines[0].rfind("a.inc:1.0: error: PP67: Maximum input size exceeded; 'limit " +
                                 std::to_string(total - 1) + " bytes;", 0)) << lines[0];
}

TEST_F(InputBytesTest, RepeatedIncludeCountedOnce) {
    const std::string main_text = "{#include 'a.inc'}\n{#include 'a.inc'}\n";
    fs::path main = dir_ / "main2.iec";
    write(main, main_text);

    Env env = setup();
    env.set_max_input_bytes(static_cast<int>(main_text.size() + A));
    EXPECT_EQ(std::nullopt, run_main(main, env));
    EXPECT_TRUE(empty());
}

// Pins the documented library rule: a file read by an earlier preprocess()
// call on the same Env is served from the token cache and not counted again.
TEST_F(InputBytesTest, CachedFileFromEarlierCallNotCounted) {
    const std::string main_text = "{#include 'a.inc'}\n";
    fs::path main = dir_ / "main3.iec";
    write(main, main_text);

    Env env = setup();
    EXPECT_EQ(std::nullopt, run_main(main, env));
    env.set_max_input_bytes(static_cast<int>(main_text.size()));  // below A + its own size
    EXPECT_EQ(std::nullopt, run_main(main, env));
    EXPECT_TRUE(empty());
}

TEST_F(InputBytesTest, HasIncludeNotCounted) {
    const std::string main_text = "{#if __has_include('a.inc')}x{#endif}\n";
    fs::path main = dir_ / "main4.iec";
    write(main, main_text);

    Env env = setup();
    env.set_max_input_bytes(static_cast<int>(main_text.size()));
    std::optional<Issue::Code> code = run_main(main, env);
    EXPECT_EQ(std::nullopt, code);
    EXPECT_TRUE(empty());
}
#endif
