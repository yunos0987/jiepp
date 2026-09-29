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


