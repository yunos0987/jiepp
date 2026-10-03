#include "test_helper.hpp"

class SetlineTest : public JieppTest {};

// ---- #line basic ----

TEST_F(SetlineTest, Regular) {
    // C1/U1(a): {#line N} means gcc's "the *next* line is N". The marker
    // pragma it emits therefore encodes N-1 (the marker form's own "counter
    // = M, next line = M+1" semantics, unchanged -- see MarkerForm below).
    // A same-line __LINE__ (no newline yet) reflects the *current* line,
    // i.e. the counter's new value, N-1, not N.
    EXPECT_EQ("(*{#:9}*)9;",   pp("{#line 10}__LINE__;"));
    EXPECT_EQ("(*{#:9}*)\n10;", pp("{#line 10}\n__LINE__;"));

    // //-comment containing setline is processed
    EXPECT_EQ("(*{#:19}*)\n20;", pp("// {#line 20}\n__LINE__;"));

    // Multiple setlines: line count advances correctly
    EXPECT_EQ("\n2;\n(*{#:9}*)\n10;\n(*{#:19}*)\n20;\n",
        pp("\n__LINE__;\n{#line 10}\n__LINE__;\n// {#line 20}\n__LINE__;\n"));
    EXPECT_TRUE(empty());

    // With filename: pragma includes filename, Error records show file:line
    EXPECT_EQ("(*{#:9 'a.iec'}*)9;",   pp("{#line 10 'a.iec'}__LINE__;"));
    EXPECT_EQ("(*{#:9 'a.iec'}*)\n10;", pp("{#line 10 'a.iec'}\n__LINE__;"));

    pp("{#line 10 'a.iec'}\n{#info}\n{#line 20 'b.iec'}\n{#info}");
    auto recs = messages();
    ASSERT_EQ(2u, recs.size());
    EXPECT_EQ("a.iec:10.0: info: PP93: ''", recs[0]);
    EXPECT_EQ("b.iec:20.0: info: PP93: ''", recs[1]);
}

TEST_F(SetlineTest, MarkerForm) {
    // C2/U1(a): the nameless marker form ({#:N}) keeps its original meaning
    // unchanged: "set the line counter to N" (this line becomes N, so the
    // *next* line is N+1) -- the mirror image of the named form above.
    EXPECT_EQ("(*{#:10}*)10;",   pp("{#:10}__LINE__;"));
    EXPECT_EQ("(*{#:10}*)\n11;", pp("{#:10}\n__LINE__;"));
    EXPECT_TRUE(empty());
}

// ---- #line syntax ----

TEST_F(SetlineTest, Syntax) {
    // Valid: with colon separator (C1: named form -> marker N-1, next line N)
    EXPECT_EQ("(*{#:9 'a.iec'}*)\n(*{st}*)10;(*{end}*)",
        pp("{#line  :  10  'a.iec'  }\n{st}__LINE__;{end}"));
    // Valid: without colon
    EXPECT_EQ("(*{#:9 'a.iec'}*)\n(*{st}*)10;(*{end}*)",
        pp("{#line  10  'a.iec'  }\n{st}__LINE__;{end}"));
    // Valid: line number only, with colon
    EXPECT_EQ("(*{#:9}*)\n(*{st}*)10;(*{end}*)",
        pp("{#line  :  10  }\n{st}__LINE__;{end}"));
    // Valid: line number only, without colon
    EXPECT_EQ("(*{#:9}*)\n(*{st}*)10;(*{end}*)",
        pp("{#line  10  }\n{st}__LINE__;{end}"));
    EXPECT_TRUE(empty());

    // C3: signed operands are rejected -- {#line} takes an unsigned decimal
    // digit string only, unlike std::istream >> int (the previous parser).
    EXPECT_THROW(pp("{#line +5}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line -5}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#:+5}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#:-5}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    // C3: trailing non-digit/non-quoted-path garbage glued to the number.
    EXPECT_THROW(pp("{#line 1x}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_TRUE(empty());

    // item a (rev2): {#line} operand range is 0..4294967295 -- gcc and
    // clang both accept 0 and every value up to 2^32 - 1 (only -pedantic
    // warns); above that is PP41 (clang; gcc instead warns and wraps).
    // This overturns the 1..2147483647 range introduced by commit 2cb1cc6
    // ("{#line 0}" used to be PP41).
    EXPECT_NE(std::string::npos, pp("{#line 0}").find("(*{#:4294967295}*)"));
    EXPECT_NE(std::string::npos, pp("{#line 2147483648}").find("(*{#:2147483647}*)"));
    EXPECT_NE(std::string::npos, pp("{#line 2147483647}").find("(*{#:2147483646}*)"));
    EXPECT_NE(std::string::npos, pp("{#line 4294967295}").find("(*{#:4294967294}*)"));
    EXPECT_THROW(pp("{#line 4294967296}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_TRUE(empty());

    // item a (rev2): the marker form now shares that same range,
    // 0..4294967295 -- no longer "one less than the named form"; the named
    // form's emitted N-1 marker is always valid instead because it wraps
    // mod 2^32 (wrap_lineno(), see lineno.hpp), including for N == 0.
    EXPECT_NE(std::string::npos, pp("{#:0}").find("(*{#:0}*)"));
    EXPECT_NE(std::string::npos, pp("{#:2147483647}").find("(*{#:2147483647}*)"));
    EXPECT_NE(std::string::npos, pp("{#:4294967295}").find("(*{#:4294967295}*)"));
    EXPECT_THROW(pp("{#:4294967296}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_TRUE(empty());

    // Invalid: no line number (named form keeps reporting PP41: gcc requires
    // #line to have an operand, and there is no "empty named directive").
    EXPECT_THROW(pp("{#line:}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line:  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line:  \n  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    EXPECT_THROW(pp("{#line  \n  }"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    // C4: a nameless directive with no operand ({#:...} or bare {#...}) is
    // gcc's "empty directive" -- a no-op: no output, no exception raised,
    // no diagnostic issued.
    EXPECT_EQ("", pp("{#:}"));
    EXPECT_EQ("", pp("{#:  }"));
    // An embedded raw newline inside the directive still contributes to the
    // output's newline count (as it does for every directive, e.g. {#nop}),
    // even though the directive itself is a no-op.
    EXPECT_EQ("\n", pp("{#:  \n  }"));
    EXPECT_EQ("", pp("{#}"));
    EXPECT_EQ("", pp("{#  }"));
    EXPECT_EQ("\n", pp("{#  \n  }"));

    EXPECT_TRUE(empty());
}

// ---- #line / #: counter wrap (item a) ----

TEST_F(SetlineTest, CounterWrapsMod2To32) {
    // item a: the running line counter is 64-bit storage (LineNo, see
    // lineno.hpp) but wraps mod 2^32, like gcc/clang: after 4294967295 the
    // next line is 0.
    EXPECT_EQ("(*{#:4294967294}*)\nx 4294967295;\ny 0;",
        pp("{#:4294967294}\nx __LINE__;\ny __LINE__;"));
    EXPECT_TRUE(empty());

    // A named {#line 0} sets the *next* physical line to 0 (gcc/clang): the
    // effective counter wraps from new_lineno - 1 == -1 to 4294967295, and
    // the emitted marker reads back the same way.
    EXPECT_EQ("(*{#:4294967295}*)\nx 0;",
        pp("{#line 0}\nx __LINE__;"));
    EXPECT_TRUE(empty());

    // {#line 4294967295}: the next line is 4294967295, then wraps to 0.
    EXPECT_EQ("(*{#:4294967294}*)\nx 4294967295;\ny 0;",
        pp("{#line 4294967295}\nx __LINE__;\ny __LINE__;"));
    EXPECT_TRUE(empty());

    // {#line 2147483647}: __LINE__ advances past INT32_MAX with no
    // signed-overflow UB, now that the counter's storage is 64-bit.
    EXPECT_EQ("(*{#:2147483646}*)\nb 2147483647;\nc 2147483648;",
        pp("{#line 2147483647}\nb __LINE__;\nc __LINE__;"));
    EXPECT_TRUE(empty());
}

TEST_F(SetlineTest, DiagnosticLineNumberPrintedUnsigned) {
    // item a: diagnostics print the (wrapped, non-negative) line number as
    // a plain unsigned decimal (clang style), never negative -- unlike the
    // pre-a int32 counter, which wrapped into negative values via signed
    // overflow (UB) past 2147483647.
    pp("{#line 2147483647}\n\n{#warning x}");
    EXPECT_EQ("<unknown location>:2147483648.0: warning: PP92: 'x'", message());
}

TEST_F(SetlineTest, MarkerRoundTripsThroughReprocessing) {
    // item a: re-feeding jiepp's own output back into jiepp reproduces the
    // same output byte-for-byte, even across the wrap boundary -- the
    // blank-line-compaction marker computed above the 2^32 line-count
    // wraparound is itself a valid, re-parseable marker (unchanged from the
    // pre-a "marker value == named-form N - 1" identity, see §9).
    std::string blanks(13, '\n'); // 1 to end "b __LINE__;", 12 blank lines
    std::string input = "{#line 2147483647}\nb __LINE__;" + blanks + "c __LINE__;\n";
    std::string out1 = pp(input);
    EXPECT_NE(std::string::npos, out1.find("(*{#:2147483659}*)"));
    EXPECT_NE(std::string::npos, out1.find("b 2147483647;"));
    EXPECT_NE(std::string::npos, out1.find("c 2147483660;"));
    EXPECT_EQ(out1, pp(out1));
    EXPECT_TRUE(empty());
}

// ---- #line on same line (oneline) ----

TEST_F(SetlineTest, Oneline) {
    // Multiple setlines on the same line. C1: no newline separates the
    // directive from the same-line __LINE__ query, so the query still sees
    // the *current* line, i.e. the running counter's new value (N-1), not
    // the "next line" value (N) that a newline would have advanced it to.
    EXPECT_EQ("(*{#:9}*)9;(*{#:19}*)19;",
        pp("{#line 10}{#info}__LINE__;{#line 20}{#info}__LINE__;"));
    const auto first_oneline = codes();
    ASSERT_EQ(2u, first_oneline.size());
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, first_oneline[0]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, first_oneline[1]);
}

TEST_F(SetlineTest, OnelineCComment) {
    // Via C-style comments (no space between /* and directive)
    EXPECT_EQ("(*{#:9}*)9;(*{#:19}*)19;",
        pp("/*{#line 10}*/{#info}__LINE__;/*{#line 20}*/{#info}__LINE__;"));
}

TEST_F(SetlineTest, OnelinePascalComment) {
    // Via Pascal-style comments (no space between (* and directive)
    EXPECT_EQ("(*{#:9}*)9;(*{#:19}*)19;",
        pp("(*{#line 10}*){#info}__LINE__;(*{#line 20}*){#info}__LINE__;"));
    const auto pascal_oneline = codes();
    ASSERT_EQ(2u, pascal_oneline.size());
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, pascal_oneline[0]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, pascal_oneline[1]);
}

// ---- #line complex ----

TEST_F(SetlineTest, Complex) {
    // Mix of setlines with comments and line advances
    // Note: Pascal/C block comments with leading space hide directives inside
    EXPECT_EQ(
        "\n2;\n(*{#:99}*)99;\n(*\n\n*) (*{#:199}*)199;\n(*{#:299}*) //\n300;\n",
        pp(
            "\n"
            "{#info}__LINE__;\n"
            "{#line 100}{#info}__LINE__;\n"
            "(*\n"
            "\n"
            "*) {#line 200}{#info}__LINE__;\n"
            "{#line 300} //\n"
            "{#info}__LINE__;\n"
        )
    );
    const auto complex_codes = codes();
    ASSERT_EQ(4u, complex_codes.size());
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[0]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[1]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[2]);
    EXPECT_EQ(Issue::Code::INFO_MESSAGE, complex_codes[3]);
}

// ---- #line filepath ----

TEST_F(SetlineTest, Filepath) {
    // Plain filename (no quotes) → error
    EXPECT_THROW(pp("{#line 2 a.iec}__FILE__;"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    // Trailing spaces trimmed but still unquoted → error
    EXPECT_THROW(pp("{#line 2   a.iec  }__FILE__;"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
    
    // Single-quoted filename (C1: named form -> marker/location N-1 = 1)
    EXPECT_EQ("(*{#:1 'a.iec'}*)'a.iec';", pp("{#line 2 'a.iec'}__FILE__;"));
    // Double-quoted filename
    EXPECT_EQ("(*{#:1 'a.iec'}*)'a.iec';", pp("{#line 2 \"a.iec\"}__FILE__;"));

    // $$ in unquoted filename → error
    EXPECT_THROW(pp("{#line 2 a$$.iec}__FILE__;"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());

    // $$ in single-quoted filename
    EXPECT_EQ("(*{#:1 'a$$.iec'}*)'a$$.iec';", pp("{#line 2 'a$$.iec'}__FILE__;"));
    // $$ in double-quoted filename
    EXPECT_EQ("(*{#:1 'a$$.iec'}*)'a$$.iec';", pp("{#line 2 \"a$$.iec\"}__FILE__;"));
    EXPECT_TRUE(empty()) << "unexpected errors";

    // Error records reflect the setline filepath
    pp("{#line 2 'a.iec'}{#info}");
    EXPECT_EQ("a.iec:1.0: info: PP93: ''", message());

    pp("{#line 2 'a$$.iec'}{#info}");
    EXPECT_EQ("a$.iec:1.0: info: PP93: ''", message());
}

// ---- #line filepath (B2 fixes) ----

TEST_F(SetlineTest, SetLineFilenameWithSpaces) {
    // Fact: the whole remainder after the line number is read (not just one
    // whitespace-delimited word), so a quoted filename may contain spaces.
    EXPECT_EQ("(*{#:1 'my file.iec'}*)'my file.iec';",
        pp("{#line 2 'my file.iec'}__FILE__;"));
    EXPECT_EQ("(*{#:1 'my file.iec'}*)'my file.iec';",
        pp("{#line 2 \"my file.iec\"}__FILE__;"));
    EXPECT_TRUE(empty());
}

TEST_F(SetlineTest, SetLineTrailingGarbageRejected) {
    // Fact: extra words after a quoted filename are rejected, not silently ignored.
    EXPECT_THROW(pp("{#line 2 'a.iec' junk}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
}

TEST_F(SetlineTest, SetLineAngleBracketRejected) {
    // Fact: a syspath-only '<...>' filename is not accepted by {#line}.
    EXPECT_THROW(pp("{#line 2 <a.iec>}"), Issue::Exception);
    EXPECT_EQ(Issue::Code::INVALID_SETLINE_OPERAND, code());
}

