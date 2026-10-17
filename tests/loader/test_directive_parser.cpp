#include "test_helper.hpp"
#include "loader/directive_parser.hpp"

#include <cctype>
#include <chrono>
#include <optional>
#include <string_view>

using P = std::pair<std::string, std::string>;

class DirectiveParserTest : public JieppTest {};

TEST_F(DirectiveParserTest, Simple) {
    const std::pair<const char*, P> cases[] = {
        {"{#id:abc}", {"id", "abc"}},
        {"{#id abc}", {"id", "abc"}},
        {"{#id:}", {"id", ""}},
        {"{#id }", {"id", ""}},
        {"{#id}", {"id", ""}},
        {"{#include   \"file\"}", {"include", "\"file\""}},
        {"{#include   :   \"file\"   }", {"include", "\"file\"   "}},
        {"{#include\"file\"}", {"include", "\"file\""}},
        {"{#include \"file   \"}", {"include", "\"file   \""}},
        {"{#include   'file'}", {"include", "'file'"}},
        {"{#include   :   'file'   }", {"include", "'file'   "}},
        {"{#include'file'}", {"include", "'file'"}},
        {"{#include 'file   '}", {"include", "'file   '"}},
        {"{#include   <file>}", {"include", "<file>"}},
        {"{#include   :   <file>   }", {"include", "<file>   "}},
        {"{#include<file>}", {"include", "<file>"}},
        {"{#include <file   >}", {"include", "<file   >"}},
        {"{#  id\t:\t }", {"id", ""}},
        {"{#  id\t \t }", {"id", ""}},
        {"{#id: \ta  }", {"id", "a  "}},
        {"{#id  \ta  }", {"id", "a  "}},
        {"{#\nid\n: \n\na\n\n}", {"id", "a\n\n"}},
        {"{#\nid\n  \n\na\n\n}", {"id", "a\n\n"}},
        {"{#id:{:}}", {"id", "{:}"}},
        {"{#id {:}}", {"id", "{:}"}},
        {"{#id;}", {"id", ";"}},
    };

    for (const auto& [input, expected] : cases) {
        EXPECT_EQ(expected, parse_directive(input)) << input;
    }
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, Nop) {
    EXPECT_EQ(P("", ""), parse_directive("{#}"));
    EXPECT_EQ(P("", ""), parse_directive("{#   }"));
    EXPECT_EQ(P("", ""), parse_directive("{#:}"));
    EXPECT_EQ(P("", ""), parse_directive("{#   :}"));
    EXPECT_EQ(P("", ""), parse_directive("{#:   }"));
    EXPECT_EQ(P("", ""), parse_directive("{#   :   }"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, Colon) {
    const std::pair<const char*, P> cases[] = {
        {"{#id:a:b}", {"id", "a:b"}},
        {"{#id :a:b}", {"id", "a:b"}},
        {"{#id: a:b}", {"id", "a:b"}},
        {"{#id : a:b}", {"id", "a:b"}},
        {"{#id:a:b:c}", {"id", "a:b:c"}},
        {"{#id a:b:c}", {"id", "a:b:c"}},
        {"{#id:a :b:c}", {"id", "a :b:c"}},
        {"{#id a :b:c}", {"id", "a :b:c"}},
        {"{#id:a: b:c}", {"id", "a: b:c"}},
        {"{#id a: b:c}", {"id", "a: b:c"}},
        {"{#id:a : b:c}", {"id", "a : b:c"}},
        {"{#id a : b:c}", {"id", "a : b:c"}},
        {"{#id: id: b:c}", {"id", "id: b:c"}},
        {"{#id  id: b:c}", {"id", "id: b:c"}},
        {"{#id::}", {"id", ":"}},
        {"{#id :}", {"id", ""}},
    };

    for (const auto& [input, expected] : cases) {
        EXPECT_EQ(expected, parse_directive(input)) << input;
    }
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, EscapeError) {
    const char* cases[] = {
        "{#id $0X}",
        "{#id $X0}",
        "{#id $0}",
        "{#id $}",
        "{#$i$d:abc}",
        "{#$i$d abc}",
        "{#id:$i$d}",
        "{#id $i$d}",
    };
    for (const auto* input : cases) {
        EXPECT_THROW(parse_directive(input), Issue::Exception);
        EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, code()) << input;
    }
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, Escape) {
    const std::pair<const char*, P> cases[] = {
        {R"({#id$::a})", {"id:", "a"}},
        {R"({#id$ :a})", {"id ", "a"}},
        {R"({#id$:abc})", {"id:abc", ""}},
        {R"({#id$ abc})", {"id abc", ""}},
        {R"({#$49D:${$:$}$$})", {"ID", "{:}$"}},
        {R"({#$49D ${$:$}$$})", {"ID", "{:}$"}},
        {R"({#id:$:${$}$49$ $$})", {"id", ":{}I $"}},
        {R"({#id $:${$}$49$ $$})", {"id", ":{}I $"}},
    };

    for (const auto& [input, expected] : cases) {
        EXPECT_EQ(expected, parse_directive(input)) << input;
    }
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, DecodeText) {
    EXPECT_EQ("\n",   decode_directive_text("$n"));
    EXPECT_EQ("\r",   decode_directive_text("$r"));
    EXPECT_EQ("\t",   decode_directive_text("$t"));
    EXPECT_EQ("{",    decode_directive_text("${"));
    EXPECT_EQ("}",    decode_directive_text("$}"));
    EXPECT_EQ("$",    decode_directive_text("$$"));
    EXPECT_EQ(":",    decode_directive_text("$:"));
    EXPECT_EQ(" ",    decode_directive_text("$ "));
    EXPECT_EQ("I",    decode_directive_text("$49"));
    EXPECT_EQ(":",    decode_directive_text("$3a"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, DecodeTextAllEscapes) {
    EXPECT_EQ("'",    decode_directive_text("$'"));
    EXPECT_EQ("\"",   decode_directive_text("$\""));
    EXPECT_EQ("\n",   decode_directive_text("$l"));
    EXPECT_EQ("\n",   decode_directive_text("$L"));
    EXPECT_EQ("\f",   decode_directive_text("$p"));
    EXPECT_EQ("\f",   decode_directive_text("$P"));
    EXPECT_EQ("\n",   decode_directive_text("$N"));
    EXPECT_EQ("\r",   decode_directive_text("$R"));
    EXPECT_EQ("\t",   decode_directive_text("$T"));
    EXPECT_EQ("J",    decode_directive_text("$4A"));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, DecodeTextInvalidEscapes) {
    // An invalid escape is kept literally, and PP21 is raised once per
    // decoded text however many invalid escapes it has.
    Issue::remove_blocking(Issue::Code::INVALID_ESCAPE_SEQUENCE);
    const std::pair<const char*, const char*> cases[] = {
        {"$x", "$x"}, {"$4", "$4"}, {"$4G", "$4G"}, {"$", "$"},
        {"a$qb$qc", "a$qb$qc"}, {"$4$n", "$4\n"}, {"$4$$", "$4$"},
        {"$4$41", "$4A"}, {"x$q$3a", "x$q:"},
    };
    for (const auto& [input, expected] : cases) {
        EXPECT_EQ(expected, decode_directive_text(input)) << input;
        EXPECT_EQ(std::vector<Issue::Code>{Issue::Code::INVALID_ESCAPE_SEQUENCE}, codes()) << input;
    }
}

TEST_F(DirectiveParserTest, DecodeTextFlagOverload) {
    // The 2-argument overload never raises a diagnostic; it just reports
    // whether an invalid escape was kept via *has_invalid.
    bool bad = false;
    EXPECT_EQ("a$qb", decode_directive_text("a$qb", &bad));
    EXPECT_TRUE(bad);
    bad = false;
    EXPECT_EQ("a:b", decode_directive_text("a$:b", &bad));
    EXPECT_FALSE(bad);
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, EncodeText) {
    EXPECT_EQ("$n",  encode_directive_text("\n"));
    EXPECT_EQ("$r",  encode_directive_text("\r"));
    EXPECT_EQ("$t",  encode_directive_text("\t"));
    EXPECT_EQ("${",  encode_directive_text("{"));
    EXPECT_EQ("$}",  encode_directive_text("}"));
    EXPECT_EQ("$$",  encode_directive_text("$"));
    EXPECT_EQ("$:",  encode_directive_text(":"));
    EXPECT_TRUE(empty());
}

// UD2/UD3: the 2-arg overload never raises PP21 itself; it just reports the
// raw (undecoded) text of the first invalid escape -- key preferred over
// value -- so a caller can raise it at most once, and only if/when the
// directive turns out to matter.
TEST_F(DirectiveParserTest, ParseDirectiveInvalidEscapeFlagOverload) {
    std::optional<std::string> bad;
    EXPECT_EQ(P("def$qine", "A$q"), parse_directive("{#def$qine A$q}", bad));
    ASSERT_TRUE(bad.has_value());
    EXPECT_EQ("def$qine", *bad);
    EXPECT_TRUE(empty());

    // Only the value is invalid: bad holds the value's raw text.
    bad.reset();
    EXPECT_EQ(P("define", "A x$q y"), parse_directive("{#define A x$q y}", bad));
    ASSERT_TRUE(bad.has_value());
    EXPECT_EQ("A x$q y", *bad);
    EXPECT_TRUE(empty());

    // No invalid escape at all: bad stays unset.
    bad.reset();
    EXPECT_EQ(P("id", "abc"), parse_directive("{#id abc}", bad));
    EXPECT_FALSE(bad.has_value());
    EXPECT_TRUE(empty());
}



// U8 (B'): inside a string literal written with raw quotes, IEC 61131-3
// string escapes are kept as written and only the directive-only escapes
// ${ $} $: $<space> are decoded; outside strings, in comments, and in a
// string opened by an escaped quote ($' / $27), decoding is unchanged.
TEST_F(DirectiveParserTest, DecodeTextKeepsIecEscapesInRawStrings) {
    const std::pair<const char*, const char*> cases[] = {
        {R"('it$'s')", R"('it$'s')"},
        {R"('a$nb')", R"('a$nb')"},
        {R"('a$$b')", R"('a$$b')"},
        {R"('a$41b')", R"('a$41b')"},
        {R"("a$0041b$"c")", R"("a$0041b$"c")"},
        {R"('a$}b${c$:d$ e')", R"('a}b{c:d e')"},
        {R"(x$n'a$nb'$ny)", "x\n'a$nb'\ny"},
        {R"('a$qb')", R"('a$qb')"},
        {R"('a$'b)", R"('a'b)"},     // never closes: decoded as before
        {R"('x//y$nz')", R"('x//y$nz')"},
        {R"((* it's *) a$nb)", "(* it's *) a\nb"},
        {R"(/* 'a */ 'b$nc')", R"(/* 'a */ 'b$nc')"},
        {R"(// it's $n'a$nb')", "// it's \n'a$nb'"},
        {R"($'it$$$'s$')", R"('it$'s')"},
        {R"($'a$$b$' 'c$nd')", R"('a$b' 'c$nd')"},
        {R"($27ab$27 'c$nd')", R"('ab' 'c$nd')"},
        {R"($'(*$' 'a$nb')", R"('(*' 'a$nb')"},
        // A quote whose literal does not close (an apostrophe in prose) is
        // not a string literal: the text after it decodes as before.
        {R"(don't$nstop)", "don't\nstop"},
    };
    for (const auto& [input, expected] : cases) {
        bool bad = false;
        EXPECT_EQ(expected, decode_directive_text(input, &bad)) << input;
        EXPECT_FALSE(bad) << input;
    }
    bool bad = false;
    EXPECT_EQ(R"('a$qb' c$qd)", decode_directive_text(R"('a$qb' c$qd)", &bad));
    EXPECT_TRUE(bad);
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, StringLiteralEnd) {
    EXPECT_EQ(7u, string_literal_end(R"('it$'s' x)", 0));
    EXPECT_EQ(5u, string_literal_end(R"(x 'a' y)", 2));
    EXPECT_EQ(6u, string_literal_end(R"("a$"b")", 0));
    EXPECT_EQ(std::string_view::npos, string_literal_end(R"('a$')", 0));
    EXPECT_EQ(std::string_view::npos, string_literal_end(R"('a)", 0));
    EXPECT_EQ(std::string_view::npos, string_literal_end("'a\nb'", 0));
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, DecodePathLiteral) {
    EXPECT_EQ("a$b.iec", decode_path_literal(R"('a$$b.iec')"));
    EXPECT_EQ("it's.iec", decode_path_literal(R"('it$'s.iec')"));
    EXPECT_EQ("ab.iec", decode_path_literal(R"('a$62.iec')"));
    EXPECT_EQ(R"(a"b)", decode_path_literal(R"("a$"b")"));
    EXPECT_EQ("a}b", decode_path_literal(R"('a}b')"));
    EXPECT_TRUE(empty());
    EXPECT_THROW(decode_path_literal(R"('f$q.iec')"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_ESCAPE_SEQUENCE, code());
}

// ---------------------------------------------------------------------------
// Directive-key comment detection (A4).
//
// The key scan in parse_directive() ends the name at whitespace or at a
// comment. The reference below is a verbatim copy of the pre-fix recognizer
// (skip_directive_ws_and_comments(), which looks for the closing "*)" / "*/"
// with a forward find() at every character) and of the pre-fix key loop. The
// production code finds the closers once per directive instead, and must
// agree with the reference on every string.
// ---------------------------------------------------------------------------
namespace {

bool ref_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::size_t ref_skip_ws(std::string_view t, std::size_t pos) {
    while (pos < t.size() && ref_ws(static_cast<unsigned char>(t[pos])))
        ++pos;
    return pos;
}

std::size_t ref_skip_ws_and_comments(std::string_view text, std::size_t pos) {
    for (;;) {
        pos = ref_skip_ws(text, pos);
        if (pos + 1 >= text.size())
            return pos;
        const char c = text[pos], d = text[pos + 1];
        const bool doc = pos + 2 < text.size() && text[pos + 2] == '!';
        if ((c == '(' || c == '/') && d == '*' && !doc) {
            const std::size_t e = text.find(c == '(' ? "*)" : "*/", pos + 2);
            if (e == std::string_view::npos)
                return pos;
            pos = e + 2;
            continue;
        }
        if (c == '/' && d == '/' && !doc)
            return text.size();
        return pos;
    }
}

bool ref_hex(unsigned char c) { return std::isxdigit(c) != 0; }

// Returns the raw (key, value) split of the pre-fix parse_directive().
std::pair<std::string, std::string> ref_split(std::string_view inner) {
    std::size_t pos = ref_skip_ws_and_comments(inner, 0);
    const std::size_t key_start = pos;
    while (pos < inner.size()) {
        unsigned char c = static_cast<unsigned char>(inner[pos]);
        if (c == '$') {
            if (pos + 1 >= inner.size())
                break;
            if (pos + 2 < inner.size()
                && ref_hex(static_cast<unsigned char>(inner[pos + 1]))
                && ref_hex(static_cast<unsigned char>(inner[pos + 2]))) {
                pos += 3;
            } else {
                pos += 2;
            }
            continue;
        }
        if (ref_skip_ws_and_comments(inner, pos) != pos)
            break;
        if (c == ':' || c == ';' || c == '\'' || c == '"' || c == '<' || ref_ws(c))
            break;
        ++pos;
    }
    std::string_view raw_key = inner.substr(key_start, pos - key_start);
    std::string_view raw_rem = inner.substr(pos);
    std::size_t value_pos = ref_skip_ws(raw_rem, 0);
    if (value_pos < raw_rem.size() && raw_rem[value_pos] == ':') {
        ++value_pos;
        value_pos = ref_skip_ws(raw_rem, value_pos);
    }
    return {std::string(raw_key), std::string(raw_rem.substr(value_pos))};
}

} // namespace

TEST_F(DirectiveParserTest, KeyCommentDetectionMatchesReference_Guard) {
    // Every string over the alphabet up to a bounded length: 9^6 = 531441 in
    // Release, 9^5 = 59049 in Debug. The alphabet has both comment openers
    // and closers, the document-comment '!', whitespace, '$' and a hex digit
    // (so "$0" forms an escape), and a plain letter.
    static const char kAlphabet[] = {'(', '*', ')', '/', '!', 'a', ' ', '$', '0'};
    constexpr int kBase = 9;
#ifdef NDEBUG
    constexpr int kMaxLen = 6;
#else
    constexpr int kMaxLen = 5;
#endif
    std::size_t checked = 0, mismatches = 0;
    for (int len = 0; len <= kMaxLen; ++len) {
        std::size_t total = 1;
        for (int i = 0; i < len; ++i)
            total *= kBase;
        std::string s(static_cast<std::size_t>(len), ' ');
        for (std::size_t n = 0; n < total; ++n) {
            std::size_t v = n;
            for (int i = 0; i < len; ++i) {
                s[static_cast<std::size_t>(i)] = kAlphabet[v % kBase];
                v /= kBase;
            }
            const auto [raw_key, raw_value] = ref_split(s);
            bool key_bad = false, value_bad = false;
            const std::string want_key = decode_directive_text(raw_key, &key_bad);
            const std::string want_value = decode_directive_text(raw_value, &value_bad);
            std::optional<std::string> want_invalid;
            if (key_bad)
                want_invalid = raw_key;
            else if (value_bad)
                want_invalid = raw_value;

            std::optional<std::string> got_invalid;
            const auto got = parse_directive("{#" + s + "}", got_invalid);
            ++checked;
            if (got.first != want_key || got.second != want_value || got_invalid != want_invalid) {
                if (++mismatches <= 5)
                    ADD_FAILURE() << "mismatch for body [" << s << "]";
            }
        }
    }
    EXPECT_EQ(0u, mismatches) << "of " << checked;
    EXPECT_TRUE(empty());
}

TEST_F(DirectiveParserTest, MixedClosersInKey_Guard) {
    // A "(*" is closed only by "*)", a "/*" only by "*/".
    EXPECT_EQ((P{"a", "(*b/*c*)"}), parse_directive("{#a(*b/*c*)}"));
    EXPECT_EQ((P{"a/*b", "(*c*)"}), parse_directive("{#a/*b(*c*)}"));
    EXPECT_EQ((P{"ab(*cd", "x"}), parse_directive("{#ab(*cd x}"));
    EXPECT_TRUE(empty());
}

#ifdef NDEBUG
TEST_F(DirectiveParserTest, UnclosedOpenersInKeyAreLinear_TimingGuard) {
    // Before the fix an unclosed "(*" / "/*" made every character of the key
    // search the rest of the text for the closer: quadratic. Measured before
    // the fix (Release, measured on the development machine; load-dependent):
    // "(*" x 400000 took 49.9 s and "/*x" x 200000 took 32.0 s. After: well
    // under 0.1 s.
    const auto timed = [](const std::string& body, std::string& key) {
        const auto t0 = std::chrono::steady_clock::now();
        std::optional<std::string> invalid;
        key = parse_directive("{#" + body + "}", invalid).first;
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    std::string body, key;
    for (int i = 0; i < 400000; ++i)
        body += "(*";
    EXPECT_LT(timed(body, key), 3.0);
    EXPECT_EQ(body, key);

    body.clear();
    for (int i = 0; i < 200000; ++i)
        body += "/*x";
    EXPECT_LT(timed(body, key), 3.0);
    EXPECT_EQ(body, key);
    EXPECT_TRUE(empty());
}
#endif
