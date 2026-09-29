# coding: utf-8
"""Perf test case: {#define} directive volume with dense '$' escapes and an
escaped-quote run, each processed (defined) once.

Guards decode_directive_text() (directive_parser.cpp) and read_pragma_body()
(lexer_pragma.cpp), whose per-character loop this session's commits
6513289 (keep an invalid '$' escape literally instead of emptying the whole
operand), 4eaf950 (let a pending hex digit survive a '$'-newline
continuation) and 834cfa4 (parse_directive()'s 2-arg overload, gating PP21 to
once per reachable directive) all touch. Each directive body below packs in
one of every *valid* escape kind so as not to trip PP21 itself (kept out of
scope here; the three commits above are about not corrupting the operand
when an escape *is* invalid, not about the decode loop's steady-state cost):
$n (newline), $27 (hex-encoded "'"), $$ (literal "$"), ${/$} (literal braces,
consumed as a single 2-char escape before the raw-brace/closing-brace scan
in read_pragma_body() ever sees them), $41/$42 (hex A/B), and a $'...$' run
enclosing a "//" that must stay inside LineCommentTracker's String state
(lexer_pragma.cpp) -- i.e. not be mistaken for a comment start, the same
string-vs-comment distinction tokenize() makes for ordinary source text.

Also carries a raw-quoted string literal ('it$'s a$nb $$') so that
DirectiveLexState's raw-string tracking (directive_parser.hpp) -- which
keeps IEC 61131-3 string escapes as written inside '...'/"..." instead of
decoding them -- is exercised at the same volume, not just the escapes
that stay outside of quotes.
"""
import io

ns = (16384, 32768, 65536)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        body = (
            "a$n"
            " b$27c$$d"
            "$" + "{" + "e$" + "}" + "f"
            " $'g//h$'"
            " $41$42"
            " 'it$'s a$nb $$'"
            " // trailing comment " + str(i)
        )
        o.write("{#define E" + str(i) + " " + body + "}\n")
