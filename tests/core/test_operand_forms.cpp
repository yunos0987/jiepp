#include "test_helper.hpp"

#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// Pins the accepted and rejected forms of the directive operands that are
// read as a path or a number ({#include}, {#sinclude}, {#syspath}, {#line},
// {#max_*}) and of the __has_include operator: the output and every
// diagnostic (code and context, in order). The expectations were taken from
// the tree before the {#max_*} operand was judged on lex_operand() tokens,
// and the file uses nothing newer, so it also runs unchanged against that
// tree.

namespace {

using Msgs = std::vector<std::string>;

struct Case {
    std::string src;
    std::string out;
    Msgs msgs;
};

// s as a C++ string literal, to paste an actual value into a Case.
std::string lit(const std::string& s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '\n')
            r += "\\n";
        else if (c == '"' || c == '\\')
            r += std::string("\\") + c;
        else
            r += c;
    }
    return r + "\"";
}

std::string lit(const Msgs& ms) {
    std::string r = "{";
    for (std::size_t i = 0; i < ms.size(); ++i)
        r += (i ? ", " : "") + lit(ms[i]);
    return r + "}";
}

class OperandFormTest : public JieppTest {
protected:
    fs::path dir_;

    void SetUp() override {
        JieppTest::SetUp();
        dir_ = fs::temp_directory_path() / "jiepp_operand_form_test";
        std::error_code ec;
        fs::remove_all(dir_, ec);
        fs::create_directories(dir_ / "sys");
        std::ofstream(dir_ / "f.iec", std::ios::binary) << "F\n";
        std::ofstream(dir_ / "sys" / "s.iec", std::ios::binary) << "S\n";
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
        JieppTest::TearDown();
    }

    // Preprocesses src in a fresh Issue state (a {#line} of an earlier run
    // does not leak into the location), with the current directory at dir_
    // and dir_/sys as the only syspath, collecting every diagnostic.
    Env run(const std::string& src, std::string& out, Msgs& msgs) {
        Issue::initialize(output());
        DiagBox::initialize();
        CwdGuard cwd(dir_);
        Issue::ContinueMode cont({});
        Env env = setup();
        env.add_syspath((dir_ / "sys").generic_string());
        out = pp(src, env);
        msgs = messages();
        return env;
    }

    void check(const std::vector<Case>& cases) {
        for (const auto& c : cases) {
            std::string out;
            Msgs msgs;
            run(c.src, out, msgs);
            EXPECT_EQ(c.out, out) << lit(c.src) << " -> " << lit(out);
            EXPECT_EQ(c.msgs, msgs) << lit(c.src) << " -> " << lit(msgs);
        }
    }
};

const std::string kF = "(*{#:0 'f.iec'}*)\nF\n";
const std::string kS = "(*{#:0 's.iec'}*)\nS\n";
const std::string kA = "(*{#:1 '<unknown location>'}*)A";
const std::string kAt = "<unknown location>:1.0: ";

std::string pp47(const std::string& context) {
    return kAt + "error: PP47: Invalid path; '" + context + "'";
}

std::string pp11(const std::string& context) {
    return kAt + "error: PP11: No such file or directory; '" + context + "'";
}

} // namespace

// ---- {#include} / {#sinclude} ----

TEST_F(OperandFormTest, IncludeQuotedAndAnglePaths) {
    check({
        {"{#include 'f.iec'}A", kF + kA, {}},
        {"{#include \"f.iec\"}A", kF + kA, {}},
        {"{#include <s.iec>}A", kS + kA, {}},
        {"{#sinclude 's.iec'}A", kS + kA, {}},
        {"{#sinclude <s.iec>}A", kS + kA, {}},
        {"{#include <s.iec>   }A", kS + kA, {}},
        {"{#include $n'f.iec'$n}A", kF + kA, {}},
    });
}

TEST_F(OperandFormTest, IncludeCommentsAroundThePath) {
    check({
        {"{#include (* c *) 'f.iec' (* d *)}A", kF + kA, {}},
        {"{#include /* c */\"f.iec\"// d$n}A", kF + kA, {}},
        {"{#include // c$n'f.iec'}A", kF + kA, {}},
        {"{#include (* c *)<s.iec>(* d *)}A", kS + kA, {}},
        {"{#include <s.iec> // c}A", kS + kA, {}},
        // Up to the first '>', <...> is path text, comment openers included.
        {"{#include <a//b.iec>}A", kA, {pp11("a//b.iec")}},
        {"{#include <a (* c *) b>}A", kA, {pp11("a (* c *) b")}},
    });
}

TEST_F(OperandFormTest, IncludeMacroExpandedPath) {
    check({
        {"{#define P 'f.iec'}{#include P}A", kF + kA, {}},
        {"{#define P <s.iec>}{#include P}A", kS + kA, {}},
        {"{#define C (* c *)}{#include C 'f.iec'}A", kF + kA, {}},
    });
}

TEST_F(OperandFormTest, IncludeMalformedPathIsPP47) {
    check({
        {"{#include f.iec}A", kA, {pp47("f.iec")}},
        {"{#include 'f.iec' x}A", kA, {pp47("'f.iec' x")}},
        {"{#include 'f' + 'g'}A", kA, {pp47("'f' + 'g'")}},
        {"{#include <s.iec}A", kA, {pp47("<s.iec")}},
        {"{#include s.iec>}A", kA, {pp47("s.iec>")}},
        {"{#include <s.iec> x}A", kA, {pp47("<s.iec> x")}},
        {"{#include 'f.iec' (*! d *)}A", kA, {pp47("'f.iec' (*! d *)")}},
        {"{#include //! d$n'f.iec'}A", kA, {pp47("//! d$n'f.iec'")}},
        {"{#include}A", kA, {kAt + "error: PP47: Invalid path"}},
        {"{#sinclude 'f.iec' x}A", kA, {pp47("'f.iec' x")}},
        {"{#include 'f.iec}A",
         kA,
         {kAt + "warning: PP29: Missing terminating quote character; ''f.iec'", pp47("'f.iec")}},
    });
}

TEST_F(OperandFormTest, IncludeAnglePathEndsAtTheLastGreaterThan) {
    // The path runs from the '<' to the last '>' of the operand; a comment
    // after the first '>' is one blank.
    check({
        {"{#include <s.iec>>}A", kA, {pp11("s.iec>")}},
        {"{#include <s.iec> x>}A", kA, {pp11("s.iec> x")}},
        {"{#include <a> (* c *) b>}A", kA, {pp11("a>   b")}},
    });
}

TEST_F(OperandFormTest, IncludeAdjacentLiteralsAreJoined) {
    // The lexer joins adjacent literals of the operand (SPEC 2) before the
    // path is read.
    check({
        {"{#include 'f.iec' 'g'}A", kA, {pp11("f.iecg")}},
        {"{#include 'f.' 'iec'}A", kF + kA, {}},
    });
}

// ---- {#syspath} ----

TEST_F(OperandFormTest, SyspathForms) {
    check({
        {"{#syspath (* c *) 'sys' (* d *)}{#sinclude 's.iec'}A",
         "(*{syspath:'sys'}*)" + kS + kA,
         {}},
        {"{#syspath \"sys\"}{#sinclude 's.iec'}A", "(*{syspath:'sys'}*)" + kS + kA, {}},
        {"{#syspath <sys>}A", "(*{syspath:'sys'}*)A", {}},
        {"{#syspath sys}A", "A", {pp47("sys")}},
        {"{#syspath 'sys' x}A", "A", {pp47("'sys' x")}},
        {"{#syspath <sys}A", "A", {pp47("<sys")}},
    });
}

// ---- {#line} ----

TEST_F(OperandFormTest, LineForms) {
    check({
        {"{#line 5}\n__LINE__ __FILE__", "(*{#:4}*)\n5 '<unknown location>'", {}},
        {"{#line 5 'f'}\n__LINE__ __FILE__", "(*{#:4 'f'}*)\n5 'f'", {}},
        {"{#line 5 \"f\"}\n__LINE__ __FILE__", "(*{#:4 'f'}*)\n5 'f'", {}},
        {"{#line (* c *) 5 (* d *) 'f' (* e *)}\n__LINE__ __FILE__", "(*{#:4 'f'}*)\n5 'f'", {}},
        {"{#define N 7}{#define F 'g'}{#line N F}\n__LINE__ __FILE__", "(*{#:6 'g'}*)\n7 'g'", {}},
        // No blank is needed between the number and the path.
        {"{#line 5'f'}\n__LINE__ __FILE__", "(*{#:4 'f'}*)\n5 'f'", {}},
        {"{#line 5 'a' 'b'}\n__LINE__ __FILE__", "(*{#:4 'ab'}*)\n5 'ab'", {}},
        {"{#line 05 // c}\n__LINE__", "(*{#:4}*)\n5", {}},
        {"{#line 4294967295}\n", "(*{#:4294967294}*)\n", {}},
    });
}

TEST_F(OperandFormTest, LineRejectedForms) {
    const std::string pp41 = kAt + "error: PP41: Invalid operand for 'set_line'";
    check({
        {"{#line 5 <f>}A", "A", {pp41 + "; '5 <f>'"}},
        {"{#line +5}A", "A", {pp41 + "; '+5'"}},
        {"{#line -5}A", "A", {pp41 + "; '-5'"}},
        {"{#line 4294967296}A", "A", {pp41 + "; '4294967296'"}},
        {"{#line 5abc}A", "A", {pp41 + "; '5abc'"}},
        {"{#line 5.0}A", "A", {pp41 + "; '5.0'"}},
        {"{#line 5_0}A", "A", {pp41 + "; '5_0'"}},
        {"{#line 5 'f' x}A", "A", {pp41 + "; '5 'f' x'"}},
        {"{#line 5 (*! d *)}A", "A", {pp41 + "; '5 (*! d *)'"}},
        {"{#line 'f'}A", "A", {pp41 + "; ''f''"}},
        {"{#line}A", "A", {pp41}},
    });
}

// ---- {#max_*} ----

TEST_F(OperandFormTest, LimitDirectiveValues) {
    struct Limit {
        const char* directive;
        int (Env::*getter)() const;
    };
    const Limit limits[] = {
        {"max_include_depth", &Env::get_max_include_depth},
        {"max_expansion_depth", &Env::get_max_expansion_depth},
        {"max_if_nesting", &Env::get_max_if_nesting},
        {"max_blank_lines", &Env::get_max_blank_lines},
    };
    const std::pair<const char*, int> operands[] = {
        {"5", 5},
        {"+3", 3},
        {"007", 7},
        {" (* c *) 5 (* d *) ", 5},
        {"/* c */5// d", 5},
        {"5$n", 5},
        {"$n5 /* a $n b */", 5},
        // An unclosed comment after the integer runs to the end of the
        // operand; it adds no diagnostic.
        {"5 (* x$}", 5},
    };
    for (const auto& l : limits) {
        for (const auto& [operand, value] : operands) {
            const std::string src = std::string("{#") + l.directive + " " + operand + "}A";
            std::string out;
            Msgs msgs;
            Env env = run(src, out, msgs);
            EXPECT_EQ(value, (env.*l.getter)()) << src;
            EXPECT_EQ("A", out) << src;
            EXPECT_EQ(Msgs{}, msgs) << src << " -> " << lit(msgs);
        }
    }
}

TEST_F(OperandFormTest, LimitDirectiveRejectedOperands) {
    const std::string pp43 = kAt + "error: PP43: Invalid operand for limit directive";
    for (const char* d : {"max_include_depth", "max_expansion_depth", "max_if_nesting",
                          "max_blank_lines"}) {
        std::vector<Case> cases;
        for (const char* operand :
             {"5x", "5 3", "2.9", "0x10", "16#10", "1_0", "- 3", "+ 3", "-(* c *)3", "'5'",
              "5 'x'", "(*! d *) 5", "5 @@", "x", "2147483648"}) {
            cases.push_back({std::string("{#") + d + " " + operand + "}A", "A",
                             {pp43 + "; '" + operand + "'"}});
        }
        cases.push_back({std::string("{#") + d + "}A", "A", {pp43}});
        // The operand is not lexed for diagnostics: an unterminated literal,
        // a pragma or a directive decoded from $-escapes adds no PP29, PP22
        // or PP23.
        cases.push_back({std::string("{#") + d + " 5 'x}A", "A", {pp43 + "; '5 'x'"}});
        cases.push_back({std::string("{#") + d + " 5 ${x$}}A", "A", {pp43 + "; '5 {x}'"}});
        cases.push_back({std::string("{#") + d + " 5 ${#x$}}A", "A", {pp43 + "; '5 {#x}'"}});
        cases.push_back({std::string("{#") + d + " 5$00}A", "A", {pp43 + "; '5$00'"}});
        check(cases);
    }
}

TEST_F(OperandFormTest, LimitDirectiveSignedValueReachesTheSetter) {
    // "-3", "+0" and "-2147483648" are integers, so not PP43; the setter
    // rejects them (PP44).
    const std::string pp44 = kAt + "error: PP44: Invalid parameter value; 'max_include_depth'";
    check({
        {"{#max_include_depth -3}A", "A", {pp44}},
        {"{#max_include_depth +0}A", "A", {pp44}},
        {"{#max_include_depth -2147483648}A", "A", {pp44}},
    });
}

// ---- __has_include ----

#ifndef JIEPP_SANDBOX

TEST_F(OperandFormTest, HasIncludeForms) {
    const char* const yes[] = {
        "__has_include('f.iec')",
        "__has_include(\"f.iec\")",
        "__has_include(<s.iec>)",
        "__has_include( 'f.iec' )",
        "__has_include (<s.iec>)",
        "defined(__has_include)",
        "defined __has_include",
        "defined(__has_include) and __has_include('f.iec')",
        "__has_include('f.iec') and __has_include(<s.iec>)",
    };
    for (const char* cond : yes) {
        check({{std::string("{#if ") + cond + "}Y{#else}N{#endif}", "Y", {}},
               {std::string("__assert(") + cond + ", m)A", "A", {}}});
    }
    for (const char* cond : {"__has_include('nope.iec')", "__has_include(<f.iec>)"})
        check({{std::string("{#if ") + cond + "}Y{#else}N{#endif}", "N", {}}});
}

TEST_F(OperandFormTest, HasIncludeOperandIsNotMacroExpanded) {
    const std::string pp52 = kAt + "error: PP52: Invalid expression; '__has_include('f.iec')'";
    check({
        {"{#define P 'f.iec'}{#if __has_include(P)}Y{#else}N{#endif}", "N", {pp52}},
        {"{#define P 'f.iec'}__assert(__has_include(P), m)A", "A", {pp52}},
    });
}

TEST_F(OperandFormTest, HasIncludeWithoutParentheses) {
    const std::string pp52 = kAt + "error: PP52: Invalid expression; '__has_include 'f.iec''";
    check({
        {"{#if __has_include}Y{#else}N{#endif}", "N", {}},
        {"{#if __has_include 'f.iec'}Y{#else}N{#endif}", "N", {pp52}},
        {"__assert(__has_include 'f.iec', m)A", "A", {pp52}},
    });
}

TEST_F(OperandFormTest, HasIncludeTextAndTokenPathsDiffer) {
    // The {#if} condition is read as text before it is lexed; __assert reads
    // the tokens of its operand. Pinned where the two give different results.
    check({
        {"{#if __has_include(/* c */ 'f.iec' /* d */)}Y{#else}N{#endif}",
         "N",
         {kAt + "error: PP52: Invalid expression; '__has_include(  'f.iec'  )'"}},
        {"__assert(__has_include(/* c */ 'f.iec' /* d */), m)A", "A", {}},
        {"{#if __has_include('f' '.iec')}Y{#else}N{#endif}",
         "N",
         {kAt + "error: PP52: Invalid expression; '__has_include('f.iec' )'"}},
        {"__assert(__has_include('f' '.iec'), m)A", "A", {}},
        {"{#if (* __has_include('a$q') *) 1}Y{#else}N{#endif}",
         "Y",
         {kAt + "error: PP21: Invalid escape sequence; '(* __has_include('a$q') *) 1'",
          kAt + "error: PP21: Invalid escape sequence; ''a$q''"}},
        {"__assert((* __has_include('a$q') *) 1, m)A", "A", {}},
        {"{#if __has_include(1 '__has_include(\"a$q\")')}Y{#else}N{#endif}",
         "N",
         {kAt + "error: PP21: Invalid escape sequence; '\"a$q\"'",
          kAt + "error: PP52: Invalid expression; '__has_include(1 '0')'"}},
        {"__assert(__has_include(1 '__has_include(\"a$q\")'), m)A",
         "A",
         {kAt + "error: PP21: Invalid escape sequence; '\"a$q\"'",
          kAt + "error: PP52: Invalid expression; '__has_include(1 '__has_include(\"a$q\")')'"}},
    });
}

#else // JIEPP_SANDBOX

// The sandbox test presets run only the tests named Sandbox*.
class SandboxOperandFormTest : public OperandFormTest {};

TEST_F(SandboxOperandFormTest, HasIncludeUseIsPP62) {
    for (const char* cond : {"__has_include('f.iec')", "__has_include (<s.iec>)",
                             "(* __has_include(x) *) 1"}) {
        std::string out;
        Msgs msgs;
        run(std::string("{#if ") + cond + "}Y{#else}N{#endif}", out, msgs);
        ASSERT_FALSE(msgs.empty()) << cond;
        EXPECT_EQ(kAt + "error: PP62: Directive is restricted in sandbox mode; '__has_include'",
                  msgs[0])
            << cond << " -> " << lit(msgs);
    }
}

TEST_F(SandboxOperandFormTest, DefinedHasIncludeIsNotPP62) {
    check({
        {"{#if defined(__has_include)}Y{#else}N{#endif}", "N", {}},
        {"{#if defined __has_include}Y{#else}N{#endif}", "N", {}},
        {"{#ifdef __has_include}Y{#else}N{#endif}", "N", {}},
    });
}

#endif // JIEPP_SANDBOX
