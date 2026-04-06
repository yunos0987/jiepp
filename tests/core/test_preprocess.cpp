#include "test_helper.hpp"

class PreprocessTest : public JieppTest {};

// ---- test_simple ----

TEST_F(PreprocessTest, Simple) {
    // Fact: plain IEC ST source passes through with {pragmas} wrapped in (*{...}*)
    EXPECT_EQ(
        "program Main\nvar\n    v: int;\nend_var\n(*{st}*)\nv := 1 + 2;\n(*{end}*)\nend_program\n\nprogram Sub end_program",
        pp("program Main\nvar\n    v: int;\nend_var\n{st}\nv := 1 + 2;\n{end}\nend_program\n\nprogram Sub end_program")
    );
    EXPECT_TRUE(empty());
}

// ---- test_bom ----

TEST_F(PreprocessTest, Bom) {
    // UTF-8 BOM (U+FEFF) at the beginning of input is stripped
    const std::string bom = "\xEF\xBB\xBF";
    EXPECT_EQ("", pp(bom));

    const std::string bom_program = bom + "program Main\n        end_program";
    EXPECT_EQ("program Main\n        end_program", pp(bom_program));

    // irregular: BOM in the middle of content passes through
    const std::string irregular = bom + " " + bom;
    EXPECT_EQ(std::string(" ") + bom, pp(irregular));

    EXPECT_TRUE(empty());
}

// ---- test_lexer_error ----

TEST_F(PreprocessTest, LexerError) {
    // Fact: pragmas with mismatched terminators in IEC block comments throw INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("(*{ #line: 2 }*/ }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("(*{ #line  2 }*/ }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: bare '}' (without *)) as closer in IEC block comment pragmas throws INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("(*{ #line: 2 }   }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("(*{ #line  2 }   }*)"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: mismatched terminators in C block comment pragmas throw INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("/*{ #line: 2 }*) }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("/*{ #line: 2 }   }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("/*{ #line  2 }*) }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("/*{ #line  2 }   }*/"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    // Fact: spurious }*) or }*/ after bare-brace pragma body throws INVALID_PRAGMA_SYNTAX
    EXPECT_THROW(pp("{ #line: 2 }*) }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("{ #line: 2 }*/ }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("{ #line  2 }*) }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());
    EXPECT_THROW(pp("{ #line  2 }*/ }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_PRAGMA_SYNTAX, code());

    EXPECT_TRUE(empty());
}

// ---- test_linecomment ----

TEST_F(PreprocessTest, LineComment) {
    // Fact: line comments pass through unchanged
    EXPECT_EQ("// abc", pp("// abc"));
    EXPECT_EQ("// abc\n//\n//", pp("// abc\n//\n//"));
    EXPECT_EQ(
        "// abc\nprogram Main //\n    // abc\nend_program //",
        pp("// abc\nprogram Main //\n    // abc\nend_program //")
    );
    EXPECT_TRUE(empty());
}

// ---- test_blockcomment1 ----

TEST_F(PreprocessTest, BlockComment1) {
    // Fact: IEC block comments (* *) pass through unchanged
    EXPECT_EQ("(* abc *)", pp("(* abc *)"));
    EXPECT_EQ("(* abc */*)", pp("(* abc */*)"));
    EXPECT_EQ(
        "program (* abc *) Main\n(**)end_program(**)",
        pp("program (* abc *) Main\n(**)end_program(**)")
    );
    EXPECT_TRUE(empty());
}

// ---- test_blockcomment2 ----

TEST_F(PreprocessTest, BlockComment2) {
    // Fact: C block comments /* */ pass through unchanged
    EXPECT_EQ("/* abc */", pp("/* abc */"));
    EXPECT_EQ("/* abc *)*/", pp("/* abc *)*/"));
    EXPECT_EQ(
        "program /* abc */ Main\n/**/end_program/**/",
        pp("program /* abc */ Main\n/**/end_program/**/")
    );
    EXPECT_TRUE(empty());
}

// ---- test_ieccomment1 ----

TEST_F(PreprocessTest, IecComment1) {
    // Fact: IEC doc comments (** *) pass through unchanged
    EXPECT_EQ("(** abc *)", pp("(** abc *)"));
    EXPECT_EQ(
        "program (** abc *) Main\n(***)end_program(***)",
        pp("program (** abc *) Main\n(***)end_program(***)")
    );
    EXPECT_TRUE(empty());
}

// ---- test_ieccomment2 ----

TEST_F(PreprocessTest, IecComment2) {
    // Fact: C doc comments /** */ pass through unchanged
    EXPECT_EQ("/** abc */", pp("/** abc */"));
    EXPECT_EQ(
        "program /** abc */ Main\n/***/end_program/***/",
        pp("program /** abc */ Main\n/***/end_program/***/")
    );
    EXPECT_TRUE(empty());
}

// ---- test_pragma ----

TEST_F(PreprocessTest, Pragma) {
    // Fact: pragmas are wrapped in (*{...}*) regardless of their original form
    EXPECT_EQ("(*{a:0}*)",  pp("{a:0}"));
    EXPECT_EQ("(*{a 0}*)",  pp("{a 0}"));
    EXPECT_EQ("(*{a:0}*)",  pp("//{a:0}"));
    EXPECT_EQ("(*{a:0}*)",  pp("(*{a:0}*)"));
    EXPECT_EQ("(*{a:0}*)",  pp("/*{a:0}*/"));

    // Fact: leading space inside braces is stripped (Python: [ \t\f\v\xa0]* in pragma regex)
    EXPECT_EQ("(*{a : 0 }*)", pp("{ a : 0 }"));
    EXPECT_EQ("(*{a   0 }*)", pp("{ a   0 }"));

    // Fact: pragma in line comment strips the '//' prefix and leading space
    EXPECT_EQ("(*{a : 0 }*)", pp("// { a : 0 }"));

    // Fact: block comment with optional space before '{' is treated as a pragma
    EXPECT_EQ("(*{a : 0 }*)", pp("(* { a : 0 } *)"));
    EXPECT_EQ("(*{a : 0 }*)", pp("/* { a : 0 } */"));

    // Fact: single and double quotes in pragma values pass through unchanged in C++
    EXPECT_EQ("(*{doc: it's a test}*)", pp("{doc: it's a test}"));
    EXPECT_EQ("(*{doc: say \"hello\"}*)", pp("{doc: say \"hello\"}"));
    EXPECT_EQ("(*{doc: 'helloworld' }*)", pp("{doc: 'hello' 'world'}"));

    EXPECT_TRUE(empty());
}

// ---- test_pragma_lineno ----

TEST_F(PreprocessTest, PragmaLineno) {
    // Fact: $\n in a pragma acts as a line continuation; newlines are preserved as blank lines
    EXPECT_EQ("(*{id a}*)\n\n\n\n;5", pp("{id $\n$\na$\n$\n};__LINE__"));

    // Fact: bare newlines in pragma body behave like whitespace in the output.
    EXPECT_EQ("(*{id a }*)\n\n\n\n;5", pp("{id \n\na\n\n};__LINE__"));

    EXPECT_EQ("(*{id:a}*)\n\n\n\n;5", pp("{id:$\n$\na$\n$\n};__LINE__"));
    EXPECT_EQ("(*{id: a }*)\n\n\n\n;5", pp("{id:\n\na\n\n};__LINE__"));

    EXPECT_EQ("(*{id }*)\n;2", pp("{id $\n};__LINE__"));
    EXPECT_EQ("(*{id }*)\n;2", pp("{id \n};__LINE__"));

    EXPECT_EQ("(*{id:}*)\n;2", pp("{id:$\n};__LINE__"));
    EXPECT_EQ("(*{id: }*)\n;2", pp("{id:\n};__LINE__"));

    EXPECT_TRUE(empty());
}

// ---- __INCLUDE_LEVEL__ ----

TEST_F(PreprocessTest, IncludeLevel) {
    // At top level (no push_file), include_level is 0
    EXPECT_EQ("0", pp("__INCLUDE_LEVEL__"));
    EXPECT_TRUE(empty());
}

// ---- __BASE_FILE__ ----

TEST_F(PreprocessTest, BaseFile) {
    // With no file context, base_file is '<unknown location>'
    EXPECT_EQ("'<unknown location>'", pp("__BASE_FILE__"));
    EXPECT_TRUE(empty());
}

// ---- __FILE_NAME__ ----

TEST_F(PreprocessTest, FileName) {
    // With no file context, filename is empty -> '<unknown location>'
    EXPECT_EQ("'<unknown location>'", pp("__FILE_NAME__"));
    EXPECT_TRUE(empty());
}