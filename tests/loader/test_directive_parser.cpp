#include "test_helper.hpp"
#include "loader/directive_parser.hpp"

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
