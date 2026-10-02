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

// Repeated invalid pastes keep concatenating, so echoing the accumulated left
// operand made the diagnostics quadratic in the input. Each message now echoes
// at most a fixed-size piece of each operand.
TEST_F(GlueTest, InvalidPasteDiagnosticsAreLinear) {
    constexpr int n = 2000;
    std::string body = "@x";
    for (int i = 0; i < n; ++i)
        body += "@@y";
    const std::string def = "{#define A(x,y) " + body + "}\n";
    {
        Issue::ContinueMode guard({});
        auto env = setup();
        pp(def + "A(a,bcdefghij)\n", env);
    }
    auto msgs = messages();
    EXPECT_GE(msgs.size(), static_cast<std::size_t>(n) / 2);
    std::size_t total = 0;
    for (const auto& m : msgs) {
        EXPECT_EQ(Issue::Code::INVALID_TOKEN_PASTING, PlainTextMessage::parse_code(m));
        total += m.size();
        EXPECT_LT(m.size(), 400u);
    }
    // A quadratic echo would be on the order of n*n*9/2 = 18 MB here.
    EXPECT_LT(total, static_cast<std::size_t>(n) * 400u);
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
