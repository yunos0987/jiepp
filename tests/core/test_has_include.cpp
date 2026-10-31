#include "test_helper.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

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
    // (Q4: documented, not changed); it survives. Like clang ("undefining
    // builtin macro"), the {#undef} itself warns PP35 (SPEC 3.6).
    auto r = pp("{#undef __has_include}{#ifdef __has_include}y{#else}n{#endif}");
    EXPECT_EQ("y", r);
    auto ms = messages();
    ASSERT_EQ(1u, ms.size());
    EXPECT_NE(std::string::npos,
              ms[0].find("warning: PP35: Macro redefined; '__has_include: undefining built-in macro'"))
        << ms[0];
}

// R1: __has_include written in an __assert expression is resolved like in
// {#if} (before macro expansion): a missing file fires, an existing one is
// silent.
TEST_F(HasIncludeTest, HasIncludeInsideAssert) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";", pp_hi("__assert(__has_include(\"existing.iec\"), present);"));
    EXPECT_EQ(";", pp_hi("__assert(__has_include(<existing.iec>) \\and\\ 1, present);"));
    EXPECT_TRUE(empty());
    EXPECT_EQ(";", pp_hi("__assert(__has_include('no_such_file_zz.iec'), missing);"));
    auto ms = messages();
    ASSERT_EQ(1u, ms.size());
    EXPECT_NE(std::string::npos, ms[0].find("PP91: 'missing'")) << ms[0];
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

// ---- __has_include in __assert, __counter_set and {#if} ----
//
// Behaviour pins for the token path (eval_cond_value_tokens(): __assert and
// __counter_set) and the text path ({#if}). The expected values are the
// output and diagnostics of 7e231f7, the commit before the pre-scan and the
// '>' search were made linear; they pass on both.

namespace {

// The output, "|", then every diagnostic (source location stripped), each
// followed by a newline. "<throw>" marks an Issue::Exception.
std::string hi_result(const std::string& out, const std::vector<std::string>& ms) {
    std::string r = out + "|";
    for (const auto& m : ms)
        r += m + "\n";
    return r;
}

struct HiPin {
    const char* expr;
    const char* in_assert;  // __assert(E, m);
    const char* in_counter; // __counter_set(c, E)__counter_peek(c)
};

} // namespace

TEST_F(HasIncludeTest, TokenPathResolutionEdgeCases) {
    std::ofstream(tmp_dir_ / "it's.iec") << "ITS;\n";
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out;
        try {
            out = pp_hi(src);
        } catch (const Issue::Exception&) {
            out = "<throw>";
        }
        return hi_result(out, messages());
    };
    static const HiPin pins[] = {
        {"__has_include('existing.iec')",
         ";|",
         "1|"},
        {"__has_include('missing_hi.iec')",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include(<existing.iec>)",
         ";|",
         "1|"},
        {"__has_include(<missing_hi.iec>)",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include (* c *) ('existing.iec')",
         ";|",
         "1|"},
        {"__has_include (*! d *) ('existing.iec')",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include (*! d *) ('existing.iec')'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include (*! d *) ('existing.iec')'\n"},
        {"__has_include(existing)",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(existing)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(existing)'\n"},
        {"__has_include(",
         "|<unknown location>:1.0: error: PP34: Argument count mismatch; '__assert: missing closing parenthesis'\n",
         "|<unknown location>:1.0: error: PP34: Argument count mismatch; '__counter_set: missing closing parenthesis'\n"},
        {"__has_include(<a)b>)",
         ", m);|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a)b>'\n",
         ")0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a)b>'\n"},
        {"__has_include(__has_include('existing.iec'))",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(1)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(1)'\n"},
        {"__has_include('a' __has_include('existing.iec'))",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include('a' 1)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include('a' 1)'\n"},
        {"__has_include(<a> q)",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a> q)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a> q)'\n"},
        {"x__has_include('existing.iec')",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; 'x__has_include('existing.iec')'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; 'x__has_include('existing.iec')'\n"},
        {"__has_include",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include('existing.iec') + __has_include(<missing_hi.iec>)",
         ";|",
         "1|"},
        {"__has_include('it$'s.iec')",
         ";|",
         "1|"},
        {"__has_include(\n'existing.iec'\n)",
         "\n\n;|",
         "\n\n1|"},
        // The '>' that ends a <...> path is searched in the call text,
        // inside string literals and multi-character tokens too.
        {"__has_include(<a '>' b>)",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a '>' b>)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a '>' b>)'\n"},
        {"__has_include(<a >= b>)",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a >= b>)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a >= b>)'\n"},
        {"__has_include(<a <= b>)",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include(<a <> b>)",
         ";|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a <> b>)'\n",
         "0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a <> b>)'\n"},
        {"__has_include(<>)",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include(<a (* c *) b>)",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include(<a // c\nb>)",
         "\n;|<unknown location>:2.0: error: PP91: 'm'\n",
         "\n0|"},
        {"__has_include(<'x'>)",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        {"__has_include('a>b.iec')",
         ";|<unknown location>:1.0: error: PP91: 'm'\n",
         "0|"},
        // An invalid escape in a quoted path: PP21 is reported once for
        // every candidate call whose text reaches the literal, so nested
        // calls repeat it.
        {"__has_include('a$q')",
         ";|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''a$q''\n<unknown location>:1.0: error: PP91: 'm'\n",
         "0|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''a$q''\n"},
        {"__has_include('a$q' __has_include('b$q' __has_include('c$q')))",
         ";|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''a$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''b$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''c$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''b$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''c$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''c$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include('a$q' __has_include('b$q' 0))'\n",
         "0|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''a$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''b$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''c$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''b$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''c$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''c$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include('a$q' __has_include('b$q' 0))'\n"},
        {"__has_include(__has_include(__has_include('d$q')))",
         ";|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''d$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''d$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''d$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(__has_include(0))'\n",
         "0|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''d$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''d$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''d$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(__has_include(0))'\n"},
        {"__has_include(<z> __has_include('e$q') __has_include('existing.iec'))",
         ";|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''e$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''e$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<z> 0 1)'\n",
         "0|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''e$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''e$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<z> 0 1)'\n"},
        {"__has_include(<z> (*! __has_include('f$q') *))",
         ";|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''f$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<z> (*! __has_include('f$q') *))'\n",
         "0|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''f$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<z> (*! __has_include('f$q') *))'\n"},
        {"__has_include(__has_include(<a)) b>)",
         ", m);|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(__has_include(<a)) b>'\n",
         ")0|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(__has_include(<a)) b>'\n"},
        {"__has_include(<__has_include('>)')",
         "|<unknown location>:1.0: error: PP34: Argument count mismatch; '__assert: missing closing parenthesis'\n",
         "|<unknown location>:1.0: error: PP34: Argument count mismatch; '__counter_set: missing closing parenthesis'\n"},
    };
    for (const auto& p : pins) {
        const std::string e = p.expr;
        EXPECT_EQ(p.in_assert, run("__assert(" + e + ", m);")) << e;
        EXPECT_EQ(p.in_counter, run("__counter_set(c, " + e + ")__counter_peek(c)")) << e;
    }
}

TEST_F(HasIncludeTest, TextPathAngleEdgeCases) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    static const std::pair<const char*, const char*> pins[] = {
        // No '>' before the next call; the only '>' at the end.
        {"{#if __has_include(<a q) + __has_include(<b q) + 1}y{#else}n{#endif}",
         "n|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a q) + __has_include(<b q) + 1'\n"},
        {"{#if __has_include(<a q) + __has_include(<b q) + 1>}y{#else}n{#endif}",
         "n|<unknown location>:1.0: error: PP52: Invalid expression; '__has_include(<a q) + __has_include(<b q) + 1>'\n"},
        {"{#if __has_include(<a q) + __has_include(<existing.iec>)}y{#else}n{#endif}",
         "n|"},
        // Resolved on the raw text, before comments are blanked.
        {"{#if __has_include(<a//b>)}y{#else}n{#endif}",
         "n|"},
        {"{#if __has_include(<existing.iec> ) \\and\\ __has_include(<a > q)}y{#else}n{#endif}",
         "n|<unknown location>:1.0: error: PP52: Invalid expression; '1 \\and\\ __has_include(<a > q)'\n"},
        {"{#if __has_include('a$q' __has_include('b$q'))}y{#else}n{#endif}",
         "n|<unknown location>:1.0: error: PP21: Invalid escape sequence; ''a$q''\n<unknown location>:1.0: error: PP21: Invalid escape sequence; ''b$q''\n<unknown location>:1.0: error: PP52: Invalid expression; '__has_include('a$q' 0)'\n"},
    };
    for (const auto& [src, expected] : pins)
        EXPECT_EQ(expected, run(src)) << src;
}

// ---- Built-in calls nested in an __assert expression or a __counter_set value ----
//
// The token-path pre-scan does not look inside the arguments of a nested call
// of __assert, __error, __counter, __counter_peek or __counter_set that is
// still the built-in: the inner call handles its own arguments. Tests marked
// "guard" pass with and without that rule.

namespace {

const char* const HI_PP91_BRACKETS =
    "<unknown location>:1.0: error: PP91: '[__has_include('existing.iec')]'\n";

} // namespace

TEST_F(HasIncludeTest, NestedBuiltinMessageKeepsHasIncludeText) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    const std::string lit = HI_PP91_BRACKETS;
    // A message shows the text as written, as it does at the top level.
    EXPECT_EQ(";|" + lit, run("__error([__has_include('existing.iec')]);")); // guard
    EXPECT_EQ(";|" + lit, run("__assert(1 __error([__has_include('existing.iec')]), m);"));
    EXPECT_EQ(";|" + lit, run("__assert(__assert(0, [__has_include('existing.iec')]) 1, o);"));
    EXPECT_EQ("1|" + lit,
              run("__counter_set(c, __error([__has_include('existing.iec')]) 1)__counter_peek(c)"));
    EXPECT_EQ("1|" + lit,
              run("__counter_set(c, __assert(0, [__has_include('existing.iec')]) 1)__counter_peek(c)"));
}

TEST_F(HasIncludeTest, NestedCounterNameIsNotRewritten) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    // A counter name is checked as written, not after __has_include became 1.
    const std::string top =
        "<unknown location>:1.0: error: PP39: Invalid operand for built-in macro; "
        "'__counter: counter name must be an identifier: __has_include('existing.iec')'\n";
    EXPECT_EQ(";|" + top, run("__counter(__has_include('existing.iec'));")); // guard
    EXPECT_EQ(";|" + top, run("__assert(__counter(__has_include('existing.iec')) + 1, m);"));
    const std::string peek =
        "<unknown location>:1.0: error: PP39: Invalid operand for built-in macro; "
        "'__counter_peek: counter name must be an identifier: __has_include('existing.iec')'\n";
    EXPECT_EQ(";|" + peek, run("__assert(__counter_peek(__has_include('existing.iec')) + 1, m);"));
    EXPECT_EQ("1|" + top,
              run("__counter_set(c, __counter(__has_include('existing.iec')) + 1)__counter_peek(c)"));
}

// guard: the expression of a nested call is evaluated by that call, which
// resolves its own __has_include.
TEST_F(HasIncludeTest, NestedBuiltinExpressionStillResolves) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    EXPECT_EQ(";|", run("__assert(__assert(__has_include('existing.iec'), m) 1, o);"));
    EXPECT_EQ(";|<unknown location>:1.0: error: PP91: 'm'\n",
              run("__assert(__assert(__has_include('missing_hi.iec'), m) 1, o);"));
    EXPECT_EQ("1|",
              run("__assert(__counter_set(c, __has_include('existing.iec')) 1, m)__counter_peek(c)"));
    EXPECT_EQ("1|",
              run("__counter_set(d, __assert(__has_include('existing.iec'), m) 1)__counter_peek(d)"));
}

// guard: calls that are not one of the five built-ins, or that no longer are,
// keep their arguments in the expression, where __has_include is resolved.
TEST_F(HasIncludeTest, NestedNonBuiltinCallsStillResolve) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    EXPECT_EQ(";|", run("__assert(__nosplit(__has_include('existing.iec')) + 1, m);"));
    EXPECT_EQ(";|", run("{#define F(x) x}__assert(F(__has_include('existing.iec')) + 1, m);"));
    EXPECT_EQ(";|<unknown location>:1.0: warning: PP35: Macro redefined; "
              "'__error: redefining built-in macro'\n",
              run("{#define __error(x) x}__assert(__error(__has_include('existing.iec')) + 0, m);"));
    EXPECT_EQ(";|<unknown location>:1.0: warning: PP35: Macro redefined; "
              "'__assert: undefining built-in macro'\n"
              "<unknown location>:1.0: error: PP52: Invalid expression; '__assert(1) + 1'\n",
              run("{#undef __assert}__counter_set(c, __assert(__has_include('existing.iec')) + 1);"));
    // The operand of defined / __has_builtin is a name, not a call.
    EXPECT_EQ(";|<unknown location>:1.0: error: PP52: Invalid expression; '1 (1)'\n",
              run("__assert(defined __error (__has_include('existing.iec')), m);"));
    EXPECT_EQ(";|<unknown location>:1.0: error: PP52: Invalid expression; '1 (1)'\n",
              run("__assert(defined(__error) (__has_include('existing.iec')), m);"));
    EXPECT_EQ(";|<unknown location>:1.0: error: PP52: Invalid expression; "
              "'missing ')' after '__has_builtin''\n",
              run("__assert(__has_builtin(__error (__has_include('existing.iec'))), m);"));
}

// A call whose ')' is missing extends to the end of the operand; nothing in
// it is looked at by the outer pre-scan, and the call reports PP34 itself.
TEST_F(HasIncludeTest, UnclosedNestedBuiltinCall) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    const std::string pp34 =
        "<unknown location>:1.0: error: PP34: Argument count mismatch; "
        "'__error: missing closing parenthesis'\n";
    // guard (normal build): the nested call ends the outer call's argument early.
    EXPECT_EQ(", m);|" + pp34,
              run("__assert(1 __error([__has_include('existing.iec')], [)]), m);"));
    // The ']' closes the call's '(' as far as the outer call is concerned, so
    // the operand is `__error(__has_include('existing.iec') ]`.
    EXPECT_EQ(";|" + pp34 + "<unknown location>:1.0: error: PP51: Missing expression\n",
              run("__assert(__error(__has_include('existing.iec') ], m);"));
}

// A built-in name inside a __has_include call is path text when the call
// resolves (guard); when it does not, the scan goes on at the next token and
// skips the nested call, whose message then keeps its text.
TEST_F(HasIncludeTest, BuiltinInsideHasIncludeCall) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    const std::string m = "<unknown location>:1.0: error: PP91: 'm'\n";
    EXPECT_EQ(";|" + m, run("__assert(__has_include(<__error(x)>), m);"));
    EXPECT_EQ("0|", run("__counter_set(c, __has_include(<__error(x)>))__counter_peek(c)"));
    const std::string malformed =
        std::string(HI_PP91_BRACKETS) +
        "<unknown location>:1.0: error: PP52: Invalid expression; "
        "'__has_include('a' )'\n";
    EXPECT_EQ(";|" + malformed,
              run("__assert(__has_include('a' __error([__has_include('existing.iec')])), m);"));
    EXPECT_EQ("0|" + malformed,
              run("__counter_set(c, __has_include('a' __error([__has_include('existing.iec')])))"
                  "__counter_peek(c)"));
}

// Only names written in the expression count: a call made by a macro is
// ordinary text (guard), and a `defined` made by a macro does not protect the
// name after it, which is skipped as a call.
TEST_F(HasIncludeTest, MacroProducedBuiltinCallIsNotSkipped) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    EXPECT_EQ(";|<unknown location>:1.0: error: PP91: '[1]'\n",
              run("{#define E __error}__assert(E([__has_include('existing.iec')]) 1, m);"));
    EXPECT_EQ(";|<unknown location>:1.0: error: PP52: Invalid expression; '1([__has_include('existing.iec')])'\n",
              run("{#define D defined}__assert(D __error([__has_include('existing.iec')]), m);"));
}

// guard: the text path of {#if} resolves __has_include before lexing, so it
// still reaches into the arguments of a nested built-in call.
TEST_F(HasIncludeTest, IfConditionStillResolvesNestedBuiltinArguments) {
    Issue::ContinueMode guard({});
    auto run = [this](const std::string& src) {
        std::string out = pp_hi(src);
        return hi_result(out, messages());
    };
    EXPECT_EQ("A|<unknown location>:1.0: error: PP91: '[1]'\n",
              run("{#if __assert(0, [__has_include('existing.iec')]) 1}A{#endif}"));
    EXPECT_EQ("A|", run("{#if __assert(1, __has_include('existing.iec')) 1}A{#endif}"));
    EXPECT_EQ("A|<unknown location>:1.0: error: PP91: '[0]'\n",
              run("{#if __assert(0, [__has_include('missing_hi.iec')]) 1}A{#endif}"));
}

// ---- Release-only timing guards ----
//
// Release-only, like SubstCostTest.*_TimingGuard: a Debug build is not
// representative of real performance. Pre-fix times are for the in-process
// pp() on the 7e231f7 Release build (Windows); each was RED (> 3 s).
// Guard inputs must not depend on the speed of the working directory's
// filesystem (a missing-file lookup under /mnt/c in WSL costs about 370 us):
// probes that reach the filesystem use absolute paths under tmp_dir_.
#ifdef NDEBUG
namespace {

std::string hi_repeat(const std::string& s, int n) {
    std::string r;
    r.reserve(s.size() * static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        r += s;
    return r;
}

// `s` as the contents of an IEC string literal: '$' and quotes escaped.
std::string hi_iec_escape(const std::string& s) {
    std::string r;
    for (char ch : s) {
        if (ch == '$' || ch == '\'' || ch == '"')
            r += '$';
        r += ch;
    }
    return r;
}

double hi_seconds(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

// N flat probes of a missing file in __assert and in __counter_set.
// Pre-fix: 9.8 / 9.6 s.
TEST_F(HasIncludeTest, FlatProbes_TimingGuard) {
    constexpr int kN = 20000;
    const std::string probe =
        "__has_include('" + hi_iec_escape(tmp_dir_.generic_string()) + "/nx_hi_probe.iec') + ";
    const std::string probes = hi_repeat(probe, kN);
    const std::pair<std::string, std::string> cases[] = {
        {"__assert(" + probes + "1, m);", ";"},
        {"__counter_set(c, " + probes + "1)__counter_peek(c)", "1"},
    };
    for (const auto& [src, expected] : cases) {
        auto t0 = std::chrono::steady_clock::now();
        std::string out = pp_hi(src);
        double secs = hi_seconds(t0);
        EXPECT_EQ(expected, out);
        EXPECT_TRUE(empty());
        EXPECT_LT(secs, 3.0) << src.substr(0, 16) << "... n=" << kN << " took " << secs << "s";
    }
}

// Nested malformed calls; each input gives one PP52 and is not otherwise
// changed. Pre-fix: 9.7 / 16.2 / 21.3 / 10.4 s.
TEST_F(HasIncludeTest, NestedMalformedCalls_TimingGuard) {
    struct Shape {
        const char* name;
        int n;
        std::string open;
        std::string close;
    };
    const Shape shapes[] = {
        {"nest_ident", 16000, "__has_include(", ")"},
        {"nest_quote", 16000, "__has_include('a' ", ")"},
        {"nest_angle", 16000, "__has_include(<a> q ", ")"},
        {"nest_angle_tail", 4000, "__has_include(<a ", "> q)"},
    };
    Issue::ContinueMode guard({});
    for (const auto& s : shapes) {
        const std::string src =
            "__assert(" + hi_repeat(s.open, s.n) + "1" + hi_repeat(s.close, s.n) + ", m);";
        auto t0 = std::chrono::steady_clock::now();
        std::string out = pp_hi(src);
        double secs = hi_seconds(t0);
        EXPECT_EQ(";", out) << s.name;
        auto ms = messages();
        ASSERT_EQ(1u, ms.size()) << s.name;
        EXPECT_NE(std::string::npos, ms[0].find("error: PP52: Invalid expression; '__has_include("))
            << s.name;
        EXPECT_LT(secs, 3.0) << s.name << " n=" << s.n << " took " << secs << "s";
    }
}

// '<' operands in an {#if} whose '>' is far away: many flat calls with the
// only '>' at the end, and nested calls sharing one '>'.
// Pre-fix: 8.4 / 34.4 s.
TEST_F(HasIncludeTest, TextPathAngleOperands_TimingGuard) {
    const std::pair<const char*, std::string> cases[] = {
        {"if_angle_flat", "{#if " + hi_repeat("__has_include(<a q) + ", 64000) + "1>}y{#else}n{#endif}"},
        {"if_angle_tail", "{#if " + hi_repeat("__has_include(<a ", 128000) + "1" +
                              hi_repeat("> q)", 128000) + "}y{#else}n{#endif}"},
    };
    Issue::ContinueMode guard({});
    for (const auto& [name, src] : cases) {
        auto t0 = std::chrono::steady_clock::now();
        std::string out = pp_hi(src);
        double secs = hi_seconds(t0);
        EXPECT_EQ("n", out) << name;
        auto ms = messages();
        ASSERT_EQ(1u, ms.size()) << name;
        EXPECT_NE(std::string::npos, ms[0].find("error: PP52: Invalid expression; '__has_include("))
            << name;
        EXPECT_LT(secs, 3.0) << name << " took " << secs << "s";
    }
}

#endif  // NDEBUG

#endif  // !JIEPP_SANDBOX
