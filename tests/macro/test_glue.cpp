#include "test_helper.hpp"

// ---- @@ (token pasting) tests ported from Python test_pp_glue.py ----
// Internal _glue() API is tested here via function macro expansion.

class GlueTest : public JieppTest {};

TEST_F(GlueTest, Regular) {
    EXPECT_EQ(";ab;",         pp("{#define F(a,b) a@@b};F(a,b);"));
    EXPECT_EQ(";abcABC;",     pp("{#define F(a,b) a@@b};F(abc,ABC);"));
    EXPECT_EQ(";1234;",       pp("{#define F(a,b) a@@b};F(12,34);"));
    // Typed numeric literals form Token::ANY after pasting
    EXPECT_EQ(";16#216#5;",   pp("{#define F(a,b) a@@b};F(16#2,16#5);"));
    EXPECT_EQ(";16#ab16#cd;", pp("{#define F(a,b) a@@b};F(16#ab,16#cd);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, Type) {
    EXPECT_EQ(";ab;",  pp("{#define F(a,b) a@@b};F(a,b);"));
    EXPECT_EQ(";a23;", pp("{#define F(a,b) a@@b};F(a,23);"));
    EXPECT_EQ(";23a;", pp("{#define F(a,b) a@@b};F(23,a);"));
    EXPECT_EQ(";2345;",pp("{#define F(a,b) a@@b};F(23,45);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, Whitespace) {
    // Last real token of left arg is pasted with first real token of right arg.
    // Surrounding whitespace in args is preserved in the output.
    EXPECT_EQ(";a bc;", pp("{#define F(a,b) a@@b};F(a b ,c);"));
    EXPECT_EQ(";a bc;", pp("{#define F(a,b) a@@b};F(  a b,c);"));
    EXPECT_EQ(";ab c;", pp("{#define F(a,b) a@@b};F(a,b c  );"));
    EXPECT_EQ(";ab c;", pp("{#define F(a,b) a@@b};F(a,  b c);"));
    EXPECT_EQ(";a b cA B C;", pp("{#define F(a,b) a@@b};F(  a b c  ,    A B C     );"));
    EXPECT_EQ(";aA;",   pp("{#define F(a,b) a@@b};F(  a  ,    A     );"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, WhitespaceOnly) {
    // Whitespace-only args are treated as empty. Both operands are then
    // placemarkers (C17 6.10.3.3p2), so pasting them yields nothing.
    EXPECT_EQ(";;", pp("{#define F(a,b) a@@b};F(  ,   );"));
    EXPECT_EQ(";;", pp("{#define F(a,b) a@@b};F(,);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, Error) {
    // Pasting identifier or number with special tokens (@, @@, string literal) is invalid.
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(xyz,@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(xyz,@@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(xyz,'');"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@,xyz);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@@,xyz);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F('',xyz);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(123,@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(123,@@);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(123,'');"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@,123);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F(@@,123);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_THROW(pp("{#define F(a,b) a@@b};F('',123);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, code());
    EXPECT_TRUE(empty());
}

// ---- @@ (token pasting) in function macro bodies ----

TEST_F(GlueTest, PasteInFuncMacro) {
    EXPECT_EQ(";xyzw;",          pp("{#define F(a, b) a@@b};F(xy,zw);"));
    EXPECT_EQ(";xyzw;",          pp("{#define F(a, b) a@@b};F(  xy  ,  zw  );"));
    EXPECT_EQ(";A  B  CD  E  F;",pp("{#define F(a, b) a@@b};F(A  B  C,D  E  F);"));
    EXPECT_EQ(";a bc d;",        pp("{#define F(a, b) a@@b};F( a b , c d );"));
    EXPECT_EQ(";xy+-zw;",        pp("{#define F(a, b) a+@@-b};F(xy,zw);"));
    EXPECT_EQ(";xyzw;xyzw;xyzw;",
              pp("{#define F(a, b) a  @@b;a@@  b;a  @@  b};F(xy,zw);"));
    EXPECT_EQ(";a#b;", pp("{#define F(a, b) a@@b};F(a#, b);"));
    EXPECT_EQ(";a#b;", pp("{#define F(a, b) a@@b};F(a, #b);"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, PasteInFuncMacroNoArgs) {
    EXPECT_EQ(";ab;",       pp("{#define F() a@@b};F();"));
    EXPECT_EQ(";abc;",      pp("{#define F() a@@b@@c};F();"));
    EXPECT_EQ(";abcd;",     pp("{#define F() a@@b@@c@@d};F();"));
    EXPECT_EQ(";abcd efg hi;",
              pp("{#define F() a@@b@@c@@d e@@f@@g h@@i};F();"));
    EXPECT_TRUE(empty());
}

// NOTE: C17 6.10.3.3p2 placemarker rule: an empty macro argument adjacent to
// @@ is a placemarker, an invisible token that participates in pasting
// without contributing any text. Pasting two placemarkers together yields
// another placemarker (i.e. nothing), not the parameter's own name.
TEST_F(GlueTest, PasteEmptyArgs) {
    EXPECT_EQ(";;", pp("{#define F(a,b) a@@b};F(,);"));
    EXPECT_EQ(";;", pp("{#define F(a,b) a@@b};F(  ,);"));
    EXPECT_EQ(";;", pp("{#define F(a,b) a@@b};F(,  );"));
    EXPECT_EQ(";;", pp("{#define F(a,b) a@@b};F(  ,  );"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, PlacemarkerRightOperand) {
    // Non-empty left operand pasted with a placemarker right operand: the
    // left operand's own text is preserved unchanged (not "xb").
    EXPECT_EQ(";x;", pp("{#define F(a,b) a@@b};F(x,);"));
    EXPECT_EQ(";x z;", pp("{#define F(a,b) a@@b z};F(x,);"));
}

TEST_F(GlueTest, PlacemarkerLeftOperand) {
    // Placemarker left operand pasted with a non-empty right operand: the
    // right operand's own text is preserved unchanged, and surrounding
    // whitespace belonging to the macro body (not the paste itself) survives.
    EXPECT_EQ(";z y;", pp("{#define F(a,b) z a@@b};F(,y);"));
}

TEST_F(GlueTest, PlacemarkerChain) {
    // A chain of pastes where the middle operand is a placemarker: b's
    // placemarker leaves a's value untouched, which is then pasted with c.
    EXPECT_EQ(";xy;", pp("{#define F(a,b,c) a@@b@@c};F(x,,y);"));
}

TEST_F(GlueTest, PlacemarkerCStandardExample3) {
    // C17 6.10.3.5 EXAMPLE 3 style check: #define r(x,y) x @@ y
    EXPECT_EQ(";4;", pp("{#define r(x,y) x @@ y};r(4,);"));
    EXPECT_EQ(";5;", pp("{#define r(x,y) x @@ y};r(,5);"));
    EXPECT_EQ(";;",  pp("{#define r(x,y) x @@ y};r(,);"));
}

TEST_F(GlueTest, CommaPasteEmptyVaArgs) {
    // The classic pre-__VA_OPT__ GNU idiom `, @@ __VA_ARGS__` with an empty
    // __VA_ARGS__ must degrade gracefully to the C placemarker rule (the
    // comma is retained) instead of aborting with INVALID_TOKEN_PASTING.
    // This diverges from the GNU extension, which deletes the comma; use
    // __VA_OPT__ for that behaviour (see D1 in the audit plan).
    EXPECT_NO_THROW(
        EXPECT_EQ(";g(x,);", pp("{#define LOG(fmt, ...) g(fmt, @@ __VA_ARGS__)};LOG(x);")));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, PasteVaArgc) {
    // F16: __VA_ARGC__ as a @@ (paste) raw operand must substitute the
    // variadic-argument *count*, not the raw __VA_ARGS__ text -- the paste
    // right-hand-operand resolution used to fall through to selecting the
    // raw actual-argument tokens (the same bypass as the pre-fix VA_ARGC
    // path used by the stringize test below).
    EXPECT_EQ(";X2;", pp("{#define T(...) X @@ __VA_ARGC__};T(a,b);"));
    EXPECT_EQ(";X0;", pp("{#define T(...) X @@ __VA_ARGC__};T();"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, StringizeVaArgc) {
    // F16: __VA_ARGC__ as a @ (stringize) raw operand must also substitute
    // the count, not the stringized raw __VA_ARGS__ text.
    EXPECT_EQ(";'2';", pp("{#define T(...) @__VA_ARGC__};T(a,b);"));
    EXPECT_EQ(";'0';", pp("{#define T(...) @__VA_ARGC__};T();"));
    EXPECT_TRUE(empty());
}

// NOTE: C++ impl: bracket token arguments cause ARGUMENT_COUNT_MISMATCH, not
// INVALID_TOKEN_PASTING, when token pasting would form invalid results.
TEST_F(GlueTest, PasteErrors) {
    EXPECT_THROW(pp("{#define F(a,b) b@@a};F(],[);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_THROW(pp("{#define F(a,b) b@@a};F(]]],[[[);"), Issue::Exception);
    EXPECT_EQ(Issue::Code::ARGUMENT_COUNT_MISMATCH, code());
    EXPECT_TRUE(empty());
}

// ---- @@ with newlines in arguments ----

TEST_F(GlueTest, PasteLines) {
    // Fact: @@ with newlines in arguments concatenates tokens, newlines are consumed
    EXPECT_EQ(";x yz  w\n\n\n;4;x  yz   w\n\n\n\n\n;9",
              pp("{#define F(a, b) a@@b};F(x\ny,z\n\nw);__LINE__;F(x\n\ny,z\n\n\nw);__LINE__"));
    EXPECT_TRUE(empty());
}



// ---- bounded PP32 diagnostics ----

// A failed paste keeps both operands, so the left operand does not grow with
// the number of failures: each of the n failing pastes below has the same
// large identifier on the left (x) and a string on the right (y). Each
// message still echoes at most a fixed-size piece of each operand, so the
// total size is linear in n.
TEST_F(GlueTest, InvalidPasteDiagnosticsAreLinear) {
    constexpr int n = 2000;
    const std::string big(1000, 'q');
    std::string body = "x@@y";
    for (int i = 1; i < n; ++i)
        body += " x@@y";
    const std::string def = "{#define A(x,y) " + body + "}\n";
    {
        Issue::ContinueMode guard({});
        auto env = setup();
        pp(def + "A(" + big + ",'')\n", env);
    }
    auto msgs = messages();
    // Every one of the n pastes fails.
    EXPECT_GE(msgs.size(), static_cast<std::size_t>(n));
    std::size_t total = 0;
    for (const auto& m : msgs) {
        EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, PlainTextMessage::parse_code(m));
        total += m.size();
        // The 1000-byte left operand is cut to 64 bytes plus "...".
        EXPECT_LT(m.size(), 400u);
    }
    // An echo of the whole left operand would be about n*1000 = 2 MB here.
    EXPECT_LT(total, static_cast<std::size_t>(n) * 400u);
}

// ---- a failed paste keeps both operands as separate tokens ----
//
// C leaves the result of an invalid paste undefined; gcc and clang report an
// error and keep the two tokens apart, and so does jiepp: the left operand is
// not changed and the right operand is pushed after it. A paste is valid only
// when both operands have the same token type and that type is not a
// structural token ( ) , [ ].

namespace {

// Runs `src` as the jiepp command does (continue mode: PP32 and PP34 are
// counted, processing goes on) and returns the output; the diagnostics are
// left for messages().
std::string pp_continue(const std::string& src) {
    Issue::ContinueMode guard({});
    auto env = setup();
    return pp(src, env);
}

const std::string kPasteDefs = "{#define ID(a, b) a b}{#define G(a, b) <a|b>}";

std::string paste_diag(const std::string& context) {
    return "<unknown location>:1.0: error: PP32: Invalid token pasting (@@); '" + context + "'";
}

} // namespace

TEST_F(GlueTest, FailedPasteKeepsRightOperandAfterClosingParen) {
    // Row 1: ID(a, b) @@ y -- ')' @@ 'y' used to become one RP and eat 'y'.
    EXPECT_EQ("a by", pp_continue(kPasteDefs + "{#define Q ID(a, b) @@ y}Q"));
    EXPECT_EQ(std::vector<std::string>{paste_diag(") @@ y")}, messages());
}

TEST_F(GlueTest, FailedPasteIdentifierThenParenStillCallsMacro) {
    // Row 2: G @@ ( -- G stays an identifier, so G(1, 2) is called on rescan.
    EXPECT_EQ("<1|2>", pp_continue(kPasteDefs + "{#define T(m) m @@ (1, 2)}T(G)"));
    EXPECT_EQ(std::vector<std::string>{paste_diag("G @@ (")}, messages());
}

TEST_F(GlueTest, FailedPasteClosingParenThenOpenParen) {
    // Row 3: ')' @@ '(' -- both kept, the '(' is not eaten.
    EXPECT_EQ("a b(q)", pp_continue(kPasteDefs + "{#define Q ID(a, b) @@ (q)}Q"));
    EXPECT_EQ(std::vector<std::string>{paste_diag(") @@ (")}, messages());
}

TEST_F(GlueTest, FailedPasteOpenParenThenMacroCall) {
    // Row 4: '(' @@ ID(a, b) -- ID is still called.
    EXPECT_EQ("(a b)", pp_continue(kPasteDefs + "{#define Q ( @@ ID(a, b))}Q"));
    EXPECT_EQ(std::vector<std::string>{paste_diag("( @@ ID")}, messages());
}

TEST_F(GlueTest, FailedPasteCommaVaArgsKeepsArgument) {
    // Row 5: `, @@ __VA_ARGS__` with a non-empty __VA_ARGS__ is PP32 (unlike
    // the GNU extension in gcc/clang, which gives no diagnostic; intended
    // difference, SPEC 4.3) and now keeps the argument.
    EXPECT_EQ("<x|y>", pp_continue(kPasteDefs + "{#define LOG(fmt, ...) G(fmt, @@ __VA_ARGS__)}LOG(x, y)"));
    EXPECT_EQ(std::vector<std::string>{paste_diag(", @@ y")}, messages());
}

TEST_F(GlueTest, FailedPasteClosingParenThenClosingParenIsNowPP32) {
    // Row 6: ')' @@ ')' used to be one RP without a diagnostic (a ')' vanished).
    EXPECT_EQ("a b) tail", pp_continue(kPasteDefs + "{#define Q ID(a, b) @@ ) tail}Q"));
    EXPECT_EQ(std::vector<std::string>{paste_diag(") @@ )")}, messages());
}

TEST_F(GlueTest, FailedPasteCommaThenCommaMakesThreeArguments) {
    // Row 7: ',' @@ ',' used to be one SEP without a diagnostic (a comma vanished).
    // Now the call has three arguments: PP32, then PP34, and ID is not expanded.
    EXPECT_EQ("ID\ntail", pp_continue(kPasteDefs + "{#define Q ID(a , @@ , b)}Q\ntail"));
    EXPECT_EQ((std::vector<std::string>{
                  paste_diag(", @@ ,"),
                  "<unknown location>:1.0: error: PP34: Argument count mismatch; 'ID: expected 2, got 3'"}),
              messages());
}

TEST_F(GlueTest, FailedPasteOpenParenThenOpenParenUnclosedCall) {
    // Row 8: '(' @@ '(' used to be one LP without a diagnostic. Now the call
    // `ID ( ( a, b)` is not closed: PP32, then PP34 (missing closing
    // parenthesis); the rest of the file ("tail") is consumed by the search
    // for the ')' and is not output (only the line break remains).
    EXPECT_EQ("\n", pp_continue(kPasteDefs + "{#define Q ID ( @@ ( a, b)}Q\ntail"));
    EXPECT_EQ((std::vector<std::string>{
                  paste_diag("( @@ ("),
                  "<unknown location>:1.0: error: PP34: Argument count mismatch; 'ID: missing closing parenthesis'"}),
              messages());
}

TEST_F(GlueTest, StructuralTokenPastesAreAllPP32) {
    // Same-type pastes of the five structural tokens are not valid pastes.
    // The macro body is `L @@ R` with L and R being the same token; both stay.
    struct Case { const char* tok; const char* name; };
    for (const Case& c : {Case{"(", "("}, Case{")", ")"}, Case{",", ","},
                          Case{"[", "["}, Case{"]", "]"}}) {
        const std::string ctx = std::string(c.tok) + " @@ " + c.tok;
        const std::string src = std::string("{#define Q a ") + c.tok + " @@ " + c.tok + " b}Q";
        // Output is a, the two kept tokens, b (spacing as in the body).
        std::string out = pp_continue(src);
        EXPECT_NE(std::string::npos, out.find(std::string(c.tok) + c.tok)) << c.name << ": " << out;
        auto msgs = messages();
        ASSERT_FALSE(msgs.empty()) << c.name;
        EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, PlainTextMessage::parse_code(msgs.front()))
            << c.name;
        EXPECT_NE(std::string::npos, msgs.front().find("'" + ctx + "'")) << c.name << ": " << msgs.front();
    }
}

TEST_F(GlueTest, IgnoredPP32StillKeepsBothOperands) {
    // {#ignore PP32}: no diagnostic, and the operands are still not merged.
    EXPECT_EQ("a by", pp_continue(kPasteDefs + "{#ignore PP32}{#define Q ID(a, b) @@ y}Q"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, ValidPastesAreUnchanged) {
    // Same non-structural types still merge (ANY @@ ANY, STRING @@ STRING).
    EXPECT_EQ("xy", pp_continue("{#define Q x @@ y}Q"));
    EXPECT_EQ("'a''b'", pp_continue("{#define Q 'a' @@ 'b'}Q"));
    EXPECT_TRUE(empty());
}

TEST_F(GlueTest, InvalidPasteDiagnosticsBoundLargeOperands) {
    // A single huge operand pasted repeatedly must not be echoed whole.
    const std::string big(100000, 'z');
    std::string body = "@x";
    for (int i = 0; i < 50; ++i)
        body += "@@y";
    {
        Issue::ContinueMode guard({});
        auto env = setup();
        pp("{#define A(x,y) " + body + "}\nA(a," + big + ")\n", env);
    }
    auto msgs = messages();
    ASSERT_FALSE(msgs.empty());
    for (const auto& m : msgs)
        EXPECT_LT(m.size(), 400u);
    EXPECT_NE(std::string::npos, msgs.front().find("..."));
}

namespace {

// True iff s is well-formed UTF-8 (no stray continuation byte, no truncated
// sequence); enough for the 2-3 byte characters used below.
bool is_valid_utf8(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3
                      : (c >> 3) == 0x1E ? 4 : 0;
        if (n == 0 || i + n > s.size())
            return false;
        for (std::size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80)
                return false;
        i += n;
    }
    return true;
}

} // namespace

// The echo cut must land on a UTF-8 character boundary on both sides: the kept
// tail of the left operand and the kept head of the right operand.
TEST_F(GlueTest, InvalidPasteDiagnosticsKeepUtf8Valid) {
    std::string l = "'a", r = "\"a";
    for (int i = 0; i < 40; ++i) {
        l += "あ"; // U+3042
        r += "あ";
    }
    // Two ASCII bytes after the multibyte run put the 64-byte tail cut inside a
    // character; the right operand's head cut (64 = 2 + 20*3 + 2) does too.
    l += "a'";
    r += "\"";
    {
        Issue::ContinueMode guard({});
        auto env = setup();
        pp("{#define A(x,y) @x@@y}\nA(" + l + "," + r + ")\n", env);
    }
    auto msgs = messages();
    ASSERT_FALSE(msgs.empty());
    bool saw_cut = false;
    for (const auto& m : msgs) {
        EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, PlainTextMessage::parse_code(m));
        EXPECT_TRUE(is_valid_utf8(m)) << m;
        EXPECT_LT(m.size(), 400u);
        saw_cut = saw_cut || m.find("...") != std::string::npos;
    }
    EXPECT_TRUE(saw_cut);
}
