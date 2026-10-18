#include "test_helper.hpp"

#include "util/path.hpp"

// P2-3: in normal builds every diagnostic shows control characters as $XX
// (LF as $n, CR as $r) instead of writing the raw bytes, so one diagnostic is
// one line (except a message body, which keeps its own LF/CR). Issue::happen()
// is the single place that does this. Sandbox builds strip instead (SPEC 14,
// SandboxDiagnosticStripTest) and are not touched.
#ifndef JIEPP_SANDBOX

class DiagnosticEscapeTest : public JieppTest {};

namespace {

// The single diagnostic in ms, or a "**FAIL**" marker.
std::string only_message(const std::vector<std::string>& ms) {
    if (ms.size() != 1)
        return "**FAIL** num of messages: " + std::to_string(ms.size());
    return ms[0];
}

}  // namespace

// PP41: {#line} decodes $hh in its operand and quotes it in the diagnostic.
TEST_F(DiagnosticEscapeTest, OperandControlBytesEscaped) {
    Issue::ContinueMode guard({});
    pp("{#line abc$1B[31mX$7FY}");
    const std::string m = only_message(messages());
    EXPECT_NE(std::string::npos,
              m.find("PP41: Invalid operand for 'set_line'; 'abc$1B[31mX$7FY'")) << m;
    EXPECT_EQ(std::string::npos, m.find('\x1B')) << m;
    EXPECT_EQ(std::string::npos, m.find('\x7F')) << m;
}

// PP38: the macro name is followed by a raw DEL.
TEST_F(DiagnosticEscapeTest, DefineDelEscaped) {
    Issue::ContinueMode guard({});
    pp("{#define A\x7F" "b 1}");
    const std::string m = only_message(messages());
    EXPECT_NE(std::string::npos, m.find("PP38: ")) << m;
    EXPECT_NE(std::string::npos, m.find("; 'A$7Fb 1'")) << m;
    EXPECT_EQ(std::string::npos, m.find('\x7F')) << m;
}

// PP20 quotes the unclosed comment, which contains a raw LF.
TEST_F(DiagnosticEscapeTest, UnclosedCommentLineBreakEscaped) {
    Issue::ContinueMode guard({});
    pp("(* unclosed\nnext line\n");
    const std::string m = only_message(messages());
    EXPECT_NE(std::string::npos, m.find("PP20: ")) << m;
    EXPECT_NE(std::string::npos, m.find("unclosed$nnext line$n")) << m;
}

// PP11 quotes the include path; $n in the source is a real LF in the path.
TEST_F(DiagnosticEscapeTest, IncludePathLineBreakEscaped) {
    Issue::ContinueMode guard({});
    pp("{#include 'no$nsuch.iec'}");
    const std::string m = only_message(messages());
    EXPECT_NE(std::string::npos, m.find("PP11: ")) << m;
    EXPECT_NE(std::string::npos, m.find("'no$nsuch.iec'")) << m;
}

// The message codes keep the line break the message asks for ($n); any other
// control character in the message is escaped.
TEST_F(DiagnosticEscapeTest, MessageKeepsLineBreakEscapesOthers) {
    Issue::ContinueMode guard({});
    pp("{#error a$1Bb$nc}");
    const auto ms = messages();
    ASSERT_EQ(2u, ms.size());
    EXPECT_NE(std::string::npos, ms[0].find("PP91: 'a$1Bb")) << ms[0];
    EXPECT_EQ(0u, ms[1].find("c'")) << ms[1];
}

TEST_F(DiagnosticEscapeTest, C1PairEscapedInText) {
    Issue::ContinueMode guard({});
    pp("{#line a$C2$9Bb}");
    const std::string m = only_message(messages());
    EXPECT_NE(std::string::npos,
              m.find("PP41: Invalid operand for 'set_line'; 'a$C2$9Bb'")) << m;
    EXPECT_EQ(std::string::npos, m.find("\xC2\x9B")) << m;
}

// The file path of the location follows the same rule: C0 and DEL are
// escaped, and the UTF-8 C1 pair is escaped only when narrow paths are UTF-8
// (Util::narrow_paths_are_utf8()); in an ANSI code page (this test binary on
// Windows) the pair stays, because C2 followed by 0x80-0x9F is ordinary text
// there (CP932 C2 82 A0 = U+FF82 U+3042). The context of a file code (PP11)
// is the path itself and follows the path rule.
TEST_F(DiagnosticEscapeTest, PathC1RuleFollowsNarrowPathEncoding) {
    std::ostringstream own;
    Issue::set_output(own);
    Issue::ContinueMode guard({});
    {
        Issue::LineGuard g(3, std::string("d\x01" "ir/\xC2\x82\xA0\x7F\xC2\x9B.iec"));
        Issue::happen(Issue::Code::INVALID_EXPRESSION, "\xC2\x9B" "e");
        Issue::happen(Issue::Code::FILE_NOT_FOUND, "\xC2\x82\xA0\x1B\xC2\x9B.iec");
    }
    Issue::set_output(output());
    std::istringstream i(own.str());
    std::vector<std::string> ms;
    for (std::string line; std::getline(i, line);)
        ms.push_back(PlainTextMessage::strip_source_location(line));
    ASSERT_EQ(2u, ms.size()) << own.str();
    const bool utf8_paths = Util::narrow_paths_are_utf8();
    // With UTF-8 paths C2 82 is itself a C1 pair ($C2$82); in an ANSI code
    // page it is ordinary text (CP932 U+FF82 U+3042 = C2 82 A0) and stays.
    const std::string path = utf8_paths ? "d$01ir/$C2$82\xA0$7F$C2$9B.iec"
                                        : "d$01ir/\xC2\x82\xA0$7F\xC2\x9B.iec";
    const std::string file_ctx = utf8_paths ? "$C2$82\xA0$1B$C2$9B.iec"
                                            : "\xC2\x82\xA0$1B\xC2\x9B.iec";
    // The context of PP52 is input text and uses the full rule everywhere.
    EXPECT_EQ(path + ":3.0: error: PP52: Invalid expression; '$C2$9B" "e'", ms[0]);
    EXPECT_EQ(0u, ms[1].find(path + ":3.0: error: PP11: ")) << ms[1];
    EXPECT_NE(std::string::npos, ms[1].find("; '" + file_ctx + "'")) << ms[1];
}

// What a library caller receives on the stream it installed with
// Issue::set_output(): the escaped text, for both the context and the path on
// the location stack.
TEST_F(DiagnosticEscapeTest, LibraryStreamGetsEscapedText) {
    std::ostringstream own;
    Issue::set_output(own);
    {
        Issue::LineGuard g(7, std::string("dir\x1B[31m/a\nb.iec"));
        try {
            Issue::happen(Issue::Code::INVALID_EXPRESSION, "x\x1By\r\nz\x7F");
            ADD_FAILURE() << "expected Issue::Exception";
        } catch (const Issue::Exception& e) {
            EXPECT_EQ(Issue::Code::INVALID_EXPRESSION, e.code);
        }
    }
    Issue::set_output(output());
    std::istringstream i(own.str());
    std::vector<std::string> ms;
    for (std::string line; std::getline(i, line);)
        ms.push_back(PlainTextMessage::strip_source_location(line));
    ASSERT_EQ(1u, ms.size()) << own.str();
    EXPECT_EQ("dir$1B[31m/a$nb.iec:7.0: error: PP52: Invalid expression; 'x$1By$r$nz$7F'", ms[0]);
}

// A diagnostic that carries no control character is unchanged, including
// '$' itself (it is not escaped, so existing output stays byte for byte).
TEST_F(DiagnosticEscapeTest, PlainDiagnosticUnchanged) {
    std::ostringstream own;
    Issue::set_output(own);
    {
        Issue::LineGuard g(2, std::string("plain/a.iec"));
        try {
            Issue::happen(Issue::Code::INVALID_EXPRESSION, "a$1Bb\tc");
        } catch (const Issue::Exception&) {
        }
    }
    Issue::set_output(output());
    EXPECT_NE(std::string::npos,
              PlainTextMessage::strip_source_location(own.str())
                  .find("plain/a.iec:2.0: error: PP52: Invalid expression; 'a$1Bb\tc'"))
        << own.str();
}

#endif  // JIEPP_SANDBOX
