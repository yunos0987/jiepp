#include "test_helper.hpp"

// =====================================================================
// Sandbox mode tests — only active when JIEPP_SANDBOX is defined.
// In a non-sandbox build these tests compile but are empty.
// =====================================================================

#ifdef JIEPP_SANDBOX

class SandboxDirectiveTest : public JieppTest {};

// ---- Blocked directives produce SANDBOX_RESTRICTED_DIRECTIVE ----

TEST_F(SandboxDirectiveTest, IncludeBlocked) {
    EXPECT_THROW(pp("{#include 'dummy.iec'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, SincludeBlocked) {
    EXPECT_THROW(pp("{#sinclude 'dummy.iec'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, SyspathBlocked) {
    EXPECT_THROW(pp("{#syspath '/tmp'}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxIncludeDepthBlocked) {
    EXPECT_THROW(pp("{#max_include_depth 10}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, IgnoreBlocked) {
    EXPECT_THROW(pp("{#ignore PP41}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, HasIncludeBlocked) {
    EXPECT_THROW(pp("{#if __has_include('dummy.iec')}YES{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

// ---- R4 follow-up U8: PP62 only for an actual __has_include(...) use ----

// X1: __has_include reads as undefined in sandbox mode, so {#ifdef}/
// {#ifndef} do not need the filesystem probe and are not PP62.
TEST_F(SandboxDirectiveTest, HasIncludeUndefinedForIfdefIfndef) {
    {
        auto r = pp("{#ifdef __has_include}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#ifndef __has_include}y{#else}n{#endif}");
        EXPECT_EQ("y", r);
        EXPECT_TRUE(empty());
    }
}

// X2: likewise 'defined(__has_include)'/'defined __has_include' are feature
// tests, not uses, so they are not PP62 either.
TEST_F(SandboxDirectiveTest, HasIncludeUndefinedForDefined) {
    {
        auto r = pp("{#if defined(__has_include)}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
    {
        auto r = pp("{#if defined __has_include}y{#else}n{#endif}");
        EXPECT_EQ("n", r);
        EXPECT_TRUE(empty());
    }
}

// X3: a user macro whose name happens to end in "__has_include" is not the
// operator and must not be blocked.
TEST_F(SandboxDirectiveTest, KeywordSuffixMacroNotBlocked) {
    auto r = pp("{#define weird__has_include(x) 99}"
                "{#if weird__has_include('foo') = 99}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, r.find("YES"));
    EXPECT_EQ(std::string::npos, r.find("NO"));
    EXPECT_TRUE(empty());
}

// X4: regression -- an actual __has_include(...) use is still PP62.
TEST_F(SandboxDirectiveTest, HasIncludeCallStillBlocked) {
    EXPECT_THROW(pp("{#if __has_include ('dummy.iec')}YES{#endif}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxExpansionDepthBlocked) {
    EXPECT_THROW(pp("{#max_expansion_depth 100}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxIfNestingBlocked) {
    EXPECT_THROW(pp("{#max_if_nesting 100}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

TEST_F(SandboxDirectiveTest, MaxBlankLinesBlocked) {
    EXPECT_THROW(pp("{#max_blank_lines 2}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

// ---- Allowed directives still work in sandbox ----

TEST_F(SandboxDirectiveTest, DefineAllowed) {
    auto result = pp("{#define N 42};N");
    EXPECT_EQ(";42", result);
    EXPECT_TRUE(empty());
}

TEST_F(SandboxDirectiveTest, UndefAllowed) {
    auto result = pp("{#define N 42}{#undef N};N");
    EXPECT_EQ(";N", result);
    EXPECT_TRUE(empty());
}

TEST_F(SandboxDirectiveTest, IfElseAllowed) {
    auto result = pp("{#if 1}YES{#else}NO{#endif}");
    EXPECT_NE(std::string::npos, result.find("YES"));
    EXPECT_EQ(std::string::npos, result.find("NO"));
    EXPECT_TRUE(empty());
}

TEST_F(SandboxDirectiveTest, ErrorAllowed) {
    EXPECT_THROW(pp("{#error test message}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
}

TEST_F(SandboxDirectiveTest, WarningAllowed) {
    pp("{#warning test message}");
    EXPECT_EQ(Issue::Code::WARNING_MESSAGE, code());
}

// R1 (SPEC 12.9/14): __error/__assert need no filesystem access, so they are
// allowed; their message goes through the same report_message() as
// {#error}, so control characters are stripped the same way.
TEST_F(SandboxDirectiveTest, BuiltinErrorAllowedAndStripsControlChars) {
    Issue::ContinueMode guard({});
    EXPECT_EQ("a  b", pp("a __error(x\x01y\x1b" "z) b"));
    EXPECT_EQ("", pp("__assert(0, p\x01q)"));
    auto ms = messages();
    ASSERT_EQ(2u, ms.size());
    EXPECT_NE(std::string::npos, ms[0].find("PP91: 'xyz'")) << ms[0];
    EXPECT_NE(std::string::npos, ms[1].find("PP91: 'pq'")) << ms[1];
}

// B3 (SPEC 14): besides C0 controls, the strip removes DEL (0x7F) and the
// UTF-8 form of the C1 controls U+0080-U+009F (C2 80 - C2 9F, e.g. U+009B
// CSI), for {#error} and __error alike. U+007E, U+00A0 and other multibyte
// text (whose continuation bytes include 0x80-0x9F) are kept; '\t' is kept
// and an LF from $n still splits the message onto two lines.
TEST_F(SandboxDirectiveTest, MessageStripsDelAndC1Controls) {
    Issue::ContinueMode guard({});
    // DEL, U+0080, U+009B, U+009F removed; U+007E '~', U+00A0 and
    // "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" (Japanese text, whose bytes
    // include 0x97, 0x9C and 0x9E as continuation bytes) kept.
    const std::string body =
        "a\x7F" "b\xC2\x80" "c\xC2\x9B" "31m" "d\xC2\x9F" "e~f\xC2\xA0" "g"
        "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const std::string kept =
        "ab" "c31m" "de~f\xC2\xA0" "g" "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    {
        SCOPED_TRACE("{#error}");
        pp("{#error " + body + "}");
        auto ms = messages();
        ASSERT_EQ(1u, ms.size());
        EXPECT_NE(std::string::npos, ms[0].find("PP91: '" + kept + "'")) << ms[0];
    }
    {
        SCOPED_TRACE("__error");
        EXPECT_EQ("x  y", pp("x __error(" + body + ") y"));
        auto ms = messages();
        ASSERT_EQ(1u, ms.size());
        EXPECT_NE(std::string::npos, ms[0].find("PP91: '" + kept + "'")) << ms[0];
    }
    {
        // $hh escapes decode to the same bytes, so they are stripped too.
        SCOPED_TRACE("{#error} $7F $C2$9B $C2$A0");
        pp("{#error a$7Fb$C2$9Bc$C2$A0d}");
        auto ms = messages();
        ASSERT_EQ(1u, ms.size());
        EXPECT_NE(std::string::npos, ms[0].find("PP91: 'abc\xC2\xA0" "d'")) << ms[0];
    }
    {
        // A raw tab is kept; $t decodes to a real tab (kept) and $n to a
        // real LF (kept, so the message spans two lines).
        SCOPED_TRACE("{#error} tab, $t, $n");
        pp("{#error p\tq$tr$nu}");
        auto ms = messages();
        ASSERT_EQ(2u, ms.size());
        EXPECT_NE(std::string::npos, ms[0].find("PP91: 'p\tq\tr")) << ms[0];
        EXPECT_EQ(0u, ms[1].find("u'")) << ms[1];
    }
    {
        // __error does not decode $ escapes, and a raw CR / LF in the
        // arguments is whitespace that is folded into one space before the
        // message is built; a raw tab is kept.
        SCOPED_TRACE("__error tab, CR, LF");
        pp("__error(p\tq\rr\ns)");
        auto ms = messages();
        ASSERT_EQ(1u, ms.size());
        EXPECT_NE(std::string::npos, ms[0].find("PP91: 'p\tq r s'")) << ms[0];
    }
}

namespace {

// The context of the single PP91 message in ms (between "PP91: '" and the
// last quote), or "**FAIL**" when there is not exactly one such message.
std::string pp91_context(const std::vector<std::string>& ms) {
    if (ms.size() != 1)
        return "**FAIL** num of messages: " + std::to_string(ms.size());
    const std::string head = "PP91: '";
    auto b = ms[0].find(head);
    auto e = ms[0].rfind('\'');
    if (b == std::string::npos || e == std::string::npos || e < b + head.size())
        return "**FAIL** " + ms[0];
    b += head.size();
    return ms[0].substr(b, e - b);
}

bool is_forbidden_c0_or_del(unsigned char c) {
    return (c < 0x20 && c != '\n' && c != '\r' && c != '\t') || c == 0x7F;
}

// Reference model of the sandbox strip: remove the forbidden single bytes,
// then remove C2 80 - C2 9F pairs, repeating until nothing changes.
std::string strip_to_fixed_point(std::string s) {
    for (;;) {
        std::string t;
        for (char ch : s)
            if (!is_forbidden_c0_or_del(static_cast<unsigned char>(ch)))
                t.push_back(ch);
        std::string u;
        for (std::size_t i = 0; i < t.size(); ++i) {
            const auto c = static_cast<unsigned char>(t[i]);
            if (c == 0xC2 && i + 1 < t.size()) {
                const auto d = static_cast<unsigned char>(t[i + 1]);
                if (d >= 0x80 && d <= 0x9F) {
                    ++i;
                    continue;
                }
            }
            u.push_back(t[i]);
        }
        if (u == s)
            return u;
        s = std::move(u);
    }
}

// True when s contains no C0 control other than LF/CR/tab, no DEL and no
// C2 80 - C2 9F pair.
bool free_of_controls(const std::string& s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (is_forbidden_c0_or_del(c))
            return false;
        if (c == 0xC2 && i + 1 < s.size()) {
            const auto d = static_cast<unsigned char>(s[i + 1]);
            if (d >= 0x80 && d <= 0x9F)
                return false;
        }
    }
    return true;
}

}  // namespace

// B3 retry: the strip decides on the resulting text, so removing a byte
// between C2 and 0x80-0x9F (or removing an inner C2 xx pair) must not leave
// a C2 80 - C2 9F pair behind. A lone 0x80-0x9F not preceded by a kept C2,
// a trailing C2 and C2 followed by another byte are kept.
TEST_F(SandboxDirectiveTest, MessageStripLeavesNoReassembledC1Pair) {
    Issue::ContinueMode guard({});
    struct Case { const char* name; std::string in; std::string out; };
    const std::vector<Case> cases = {
        {"C2 01 9B", "P\xC2\x01\x9BQ", "PQ"},
        {"C2 7F 9B", "P\xC2\x7F\x9BQ", "PQ"},
        {"C2 C2 80 9B", "P\xC2\xC2\x80\x9BQ", "PQ"},
        {"C2 C2 9B", "P\xC2\xC2\x9BQ", "P\xC2Q"},
        {"trailing C2", "PQ\xC2", "PQ\xC2"},
        {"C2 41", "P\xC2" "AQ", "P\xC2" "AQ"},
        {"lone 9B", "P\x9BQ", "P\x9BQ"},
        {"C2 A0 9B", "P\xC2\xA0\x9BQ", "P\xC2\xA0\x9BQ"},
    };
    for (const auto& k : cases) {
        {
            SCOPED_TRACE(std::string("{#error} ") + k.name);
            pp("{#error " + k.in + "}");
            EXPECT_EQ(k.out, pp91_context(messages()));
        }
        {
            SCOPED_TRACE(std::string("__error ") + k.name);
            pp("__error(" + k.in + ")");
            EXPECT_EQ(k.out, pp91_context(messages()));
        }
    }

    // Property check: every string of length 1..3 over an alphabet of the
    // relevant bytes, wrapped in P..Q, matches the fixed-point reference
    // model and contains no forbidden byte or pair, through both channels.
    const std::string alphabet = "a\x01\x7F\xC2\x80\x9B\x9F\xA0";
    std::vector<std::string> inputs = {""};
    std::vector<std::string> all;
    for (int len = 1; len <= 3; ++len) {
        std::vector<std::string> next;
        for (const auto& s : inputs)
            for (char ch : alphabet)
                next.push_back(s + ch);
        all.insert(all.end(), next.begin(), next.end());
        inputs = std::move(next);
    }
    int failures = 0;
    for (const auto& s : all) {
        const std::string in = "P" + s + "Q";
        const std::string want = strip_to_fixed_point(in);
        pp("{#error " + in + "}");
        const std::string got_directive = pp91_context(messages());
        pp("__error(" + in + ")");
        const std::string got_builtin = pp91_context(messages());
        if (got_directive != want || got_builtin != want ||
            !free_of_controls(got_directive) || !free_of_controls(got_builtin)) {
            if (++failures <= 5) {
                std::ostringstream hex;
                for (char ch : in)
                    hex << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(static_cast<unsigned char>(ch)) << ' ';
                ADD_FAILURE() << "input bytes: " << hex.str();
            }
        }
    }
    EXPECT_EQ(0, failures) << "of " << all.size() << " generated inputs";
}

// R1 (E25): __has_include(...) in an __assert expression is PP62, then the
// evaluator reports the call text that stays in the expression (PP52), the
// same pair as {#if __has_include('x')}; no PP91 is stacked on top.
TEST_F(SandboxDirectiveTest, AssertHasIncludeIsPP62AndPP52) {
    Issue::ContinueMode guard({});
    EXPECT_EQ(";", pp("__assert(__has_include('dummy.iec'), m);"));
    auto cs = codes();
    ASSERT_EQ(2u, cs.size());
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, cs[0]);
    EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, cs[1]);
}

// ---- Expansion-step cap (PP64) applies in sandbox builds too ----

class SandboxExpansionStepsTest : public JieppTest {};

TEST_F(SandboxExpansionStepsTest, DefaultLimitIs2To24) {
    Env env = setup();
    EXPECT_EQ(16777216u, env.get_max_expansion_steps());
}

TEST_F(SandboxExpansionStepsTest, LowerLimitStopsBomb) {
    Env env = setup();
    env.set_max_expansion_steps(262144);
    EXPECT_EQ(262144u, env.get_max_expansion_steps());
    std::string src = "{#define D0 1}\n";
    for (int i = 1; i <= 30; ++i)
        src += "{#define D" + std::to_string(i) + " D" + std::to_string(i - 1) +
               " + D" + std::to_string(i - 1) + "}\n";
    src += "D30\n";
    EXPECT_THROW(pp(src, env), Issue::Exception);
    EXPECT_EQ(Issue::Code::MAX_EXPANSION_STEPS_EXCEEDED, code());
}

TEST_F(SandboxExpansionStepsTest, IgnoreStillBlockedAndNoDirective) {
    // {#ignore} is a sandbox-restricted directive, so it cannot be used to
    // suppress PP64 either.
    EXPECT_THROW(pp("{#ignore PP64}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::SANDBOX_RESTRICTED_DIRECTIVE, code());
}

// ---- Info disclosure prevention ----

class SandboxInfoLeakTest : public JieppTest {};

TEST_F(SandboxInfoLeakTest, TimestampEmpty) {
    auto result = pp("__TIMESTAMP__");
    EXPECT_NE(std::string::npos, result.find("''"));
    EXPECT_TRUE(empty());
}

// __BASE_FILE__ and __FILE_NAME__ are not blanked in sandbox builds: they
// behave exactly like a non-sandbox build and agree with __FILE__.
TEST_F(SandboxInfoLeakTest, FileNameMacrosNotBlanked) {
    EXPECT_EQ("'<unknown location>'", pp("__FILE__"));
    EXPECT_EQ("'<unknown location>'", pp("__BASE_FILE__"));
    EXPECT_EQ("'<unknown location>'", pp("__FILE_NAME__"));
    EXPECT_TRUE(empty());
}

TEST_F(SandboxInfoLeakTest, SetlineIgnoresFilepath) {
    // {#setline} with a filepath arg should not change the filename in errors
    EXPECT_THROW(pp("{#line 10 'secret/path.iec'}{#error test}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ERROR_MESSAGE, code());
    // The error output should NOT contain the secret path
    for (const auto& msg : messages()) {
        EXPECT_EQ(std::string::npos, msg.find("secret/path.iec"))
            << "filepath should be ignored in sandbox: " << msg;
    }
}

// ---- Allowed macros still work in sandbox ----

TEST_F(SandboxInfoLeakTest, CounterWorks) {
    auto result = pp("__COUNTER__;__COUNTER__");
    // __COUNTER__ produces Token::ANY "0", "1", ...
    EXPECT_NE(std::string::npos, result.find("0"));
    EXPECT_NE(std::string::npos, result.find("1"));
    EXPECT_TRUE(empty());
}

// R2 (N18): named counters need no filesystem access, so they work.
TEST_F(SandboxInfoLeakTest, NamedCounterWorks) {
    EXPECT_EQ("0 1 2 5", pp("__counter(a) __counter(a) __counter_peek(a) __counter_set(a, 5)__counter(a)"));
    EXPECT_TRUE(empty());
}

TEST_F(SandboxInfoLeakTest, DateTimeWork) {
    // __DATE__ and __TIME__ should still produce non-empty strings
    auto result_d = pp("__DATE__");
    auto result_t = pp("__TIME__");
    EXPECT_FALSE(result_d.empty());
    EXPECT_FALSE(result_t.empty());
}

// ---- Control characters are stripped from every diagnostic (SPEC 14) ----

// The strip that message diagnostics (PP90-PP93) get is applied centrally in
// Issue::happen(), to the context text and to the file path of every
// diagnostic, so operand diagnostics that quote user text cannot carry ESC,
// DEL or a UTF-8 C1 control to the log either.
class SandboxDiagnosticStripTest : public JieppTest {};

namespace {

// The single diagnostic in ms, or a "**FAIL**" marker.
std::string only_message(const std::vector<std::string>& ms) {
    if (ms.size() != 1)
        return "**FAIL** num of messages: " + std::to_string(ms.size());
    return ms[0];
}

}  // namespace

// PP41: {#line} decodes $hh in its operand and quotes it in the diagnostic.
TEST_F(SandboxDiagnosticStripTest, SetLineOperandStripped) {
    Issue::ContinueMode guard({});
    pp("{#line abc$1B[31mX$7FY}");
    const std::string m = only_message(messages());
    EXPECT_NE(std::string::npos,
              m.find("PP41: Invalid operand for 'set_line'; 'abc[31mXY'")) << m;
    EXPECT_EQ(std::string::npos, m.find('\x1B')) << m;
    EXPECT_EQ(std::string::npos, m.find('\x7F')) << m;
}

// PP38: the macro name is followed by a DEL (raw and as $7F), and the
// warning quotes the definition text.
TEST_F(SandboxDiagnosticStripTest, DefineMissingWhitespaceStripped) {
    {
        SCOPED_TRACE("$7F");
        pp("{#define A$7Fb 1}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos, m.find("PP38: ")) << m;
        EXPECT_NE(std::string::npos, m.find("; 'Ab 1'")) << m;
        EXPECT_EQ(std::string::npos, m.find('\x7F')) << m;
    }
    {
        SCOPED_TRACE("raw DEL");
        pp("{#define A\x7F" "b 1}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos, m.find("PP38: ")) << m;
        EXPECT_NE(std::string::npos, m.find("; 'Ab 1'")) << m;
        EXPECT_EQ(std::string::npos, m.find('\x7F')) << m;
    }
}

// Other diagnostics quoting user text: the directive name (PP46), the
// expression (PP52) and the directive text (PP49).
TEST_F(SandboxDiagnosticStripTest, OtherOperandDiagnosticsStripped) {
    Issue::ContinueMode guard({});
    {
        SCOPED_TRACE("PP46");
        pp("{#foo\x1B[31m}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos, m.find("PP46: Invalid directive name; 'foo[31m'")) << m;
    }
    {
        SCOPED_TRACE("PP52");
        pp("{#if 1 \x1B}{#endif}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos, m.find("PP52: Invalid expression; '1 '")) << m;
    }
    {
        SCOPED_TRACE("PP49");
        pp("{#ifdef A \x1B\xC2\x9B}{#endif}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos, m.find("PP49: ")) << m;
        EXPECT_NE(std::string::npos, m.find("'{#ifdef A }'")) << m;
    }
}

// The C1 pair test is made on the resulting text in operand diagnostics
// too (same rule as MessageStripLeavesNoReassembledC1Pair), and U+00A0 and
// Japanese text are kept.
TEST_F(SandboxDiagnosticStripTest, OperandStripLeavesNoReassembledC1Pair) {
    Issue::ContinueMode guard({});
    struct Case { const char* name; std::string in; std::string out; };
    const std::vector<Case> cases = {
        {"C2 01 9B", "a$C2$01$9Bb", "ab"},
        {"C2 7F 9B", "a$C2$7F$9Bb", "ab"},
        {"C2 C2 80 9B", "a$C2$C2$80$9Bb", "ab"},
        {"C2 C2 9B", "a$C2$C2$9Bb", "a\xC2" "b"},
        {"C2 A0, Japanese", "a$C2$A0b\xE6\x97\xA5", "a\xC2\xA0" "b\xE6\x97\xA5"},
    };
    for (const auto& k : cases) {
        SCOPED_TRACE(k.name);
        pp("{#line " + k.in + "}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos,
                  m.find("PP41: Invalid operand for 'set_line'; '" + k.out + "'")) << m;
    }
}

// What a library caller receives: the diagnostic text written to the stream
// it installed with Issue::set_output(), both the context and the file path
// on the location stack stripped, and an Issue::Exception that carries only
// the code (what() is the code name, never user text).
TEST_F(SandboxDiagnosticStripTest, LibraryCallerReceivesStrippedText) {
    std::ostringstream own;
    Issue::set_output(own);
    {
        Issue::LineGuard g(7, std::string("dir\x1B[31m/a\x7F\xC2\x9B.iec"));
        try {
            Issue::happen(Issue::Code::INVALID_EXPRESSION, "x\x1By\xC2\x01\x9Bz\x7F");
            ADD_FAILURE() << "expected Issue::Exception";
        } catch (const Issue::Exception& e) {
            EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, e.code);
            EXPECT_STREQ("INVALID_EXPRESSION", e.what());
        }
        // A message diagnostic through preprocess() under the same path.
        EXPECT_THROW(pp("{#error m$1Bn}"), Issue::Exception);
    }
    Issue::set_output(output());
    std::istringstream i(own.str());
    std::vector<std::string> ms;
    for (std::string line; std::getline(i, line);)
        ms.push_back(PlainTextMessage::strip_source_location(line));
    ASSERT_EQ(2u, ms.size());
    EXPECT_EQ("dir[31m/a.iec:7.0: error: PP52: Invalid expression; 'xyz'", ms[0]);
    EXPECT_EQ(0u, ms[1].find("dir[31m/a.iec:")) << ms[1];
    EXPECT_NE(std::string::npos, ms[1].find("PP91: 'mn'")) << ms[1];
}

// ---- LF/CR become spaces outside the message text (SPEC 14) ----

// A $0A / $0D in a {#line} operand decodes to a real LF / CR, which the
// PP41 handler already re-escapes to "$n" / "$r" (Util::escape_line_breaks)
// before Issue::happen() sees it: the diagnostic stays on one line.
TEST_F(SandboxDiagnosticStripTest, SetLineOperandStaysOneLine) {
    Issue::ContinueMode guard({});
    struct Case { const char* name; std::string in; std::string out; };
    const std::vector<Case> cases = {
        {"$0A", "abc$0Adef", "abc$ndef"},
        {"$0D", "abc$0Ddef", "abc$rdef"},
        {"$0D$0A", "abc$0D$0Adef", "abc$r$ndef"},
    };
    for (const auto& k : cases) {
        SCOPED_TRACE(k.name);
        pp("{#line " + k.in + "}");
        const std::string m = only_message(messages());
        EXPECT_NE(std::string::npos,
                  m.find("PP41: Invalid operand for 'set_line'; '" + k.out + "'")) << m;
        EXPECT_EQ(std::string::npos, m.find('\r')) << m;
    }
}

// A real LF / CR that reaches Issue::happen() in the context of a
// non-message diagnostic becomes one space each (CR LF two spaces), so the
// context cannot forge a second diagnostic line; the message codes
// PP90-PP93 keep theirs.
TEST_F(SandboxDiagnosticStripTest, ContextLineBreaksReplaced) {
    std::ostringstream own;
    Issue::set_output(own);
    Issue::ContinueMode guard({});
    {
        Issue::LineGuard g(3, std::string("a.iec"));
        Issue::happen(Issue::Code::INVALID_EXPRESSION,
                      "x\njiepp: error: PP01: y\rz\r\nw");
        Issue::happen(Issue::Code::WARNING_MESSAGE, "m\nn");
    }
    Issue::set_output(output());
    std::istringstream i(own.str());
    std::vector<std::string> ms;
    for (std::string line; std::getline(i, line);)
        ms.push_back(PlainTextMessage::strip_source_location(line));
    ASSERT_EQ(3u, ms.size()) << own.str();
    EXPECT_EQ("a.iec:3.0: error: PP52: Invalid expression; "
              "'x jiepp: error: PP01: y z  w'", ms[0]);
    EXPECT_EQ("a.iec:3.0: warning: PP92: 'm", ms[1]);
    EXPECT_EQ(0u, ms[2].find("n'")) << ms[2];
}

// A file path is never legitimately multi-line: LF/CR in a library-supplied
// path become spaces for an ordinary diagnostic and for the message
// diagnostics ({#error}, __assert) alike, while the line break that the
// message text itself asks for with $n is kept.
TEST_F(SandboxDiagnosticStripTest, PathLineBreaksReplacedMessageLineBreakKept) {
    std::ostringstream own;
    Issue::set_output(own);
    Issue::ContinueMode guard({});
    {
        Issue::LineGuard g(7, std::string("dir\nx\r\ny\r.iec"));
        Issue::happen(Issue::Code::INVALID_EXPRESSION, "e");
        pp("{#error a$nb}");
        pp("__assert(0, c)");
    }
    Issue::set_output(output());
    const std::string raw = own.str();
    EXPECT_EQ(std::string::npos, raw.find('\r')) << raw;
    std::istringstream i(raw);
    std::vector<std::string> ms;
    for (std::string line; std::getline(i, line);)
        ms.push_back(PlainTextMessage::strip_source_location(line));
    ASSERT_EQ(4u, ms.size()) << raw;
    EXPECT_EQ("dir x  y .iec:7.0: error: PP52: Invalid expression; 'e'", ms[0]);
    EXPECT_EQ(0u, ms[1].find("dir x  y .iec:")) << ms[1];
    EXPECT_NE(std::string::npos, ms[1].find("PP91: 'a")) << ms[1];
    EXPECT_EQ(0u, ms[2].find("b'")) << ms[2];
    EXPECT_EQ(0u, ms[3].find("dir x  y .iec:")) << ms[3];
    EXPECT_NE(std::string::npos, ms[3].find("PP91: 'c'")) << ms[3];
}

// The message text of PP90-PP93 keeps its $n line break: {#error a$nb}
// still spans two lines.
TEST_F(SandboxDiagnosticStripTest, MessageLineBreakKept) {
    Issue::ContinueMode guard({});
    pp("{#error a$nb}");
    auto ms = messages();
    ASSERT_EQ(2u, ms.size());
    EXPECT_NE(std::string::npos, ms[0].find("PP91: 'a")) << ms[0];
    EXPECT_EQ("b'", ms[1].substr(0, 2)) << ms[1];
}

#endif  // JIEPP_SANDBOX
