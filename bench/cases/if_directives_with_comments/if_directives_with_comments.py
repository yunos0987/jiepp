# coding: utf-8
"""Perf test case: {#if} directive volume with comments woven through the
directive name and the condition text.

Guards the comment-as-whitespace treatment in {#if}/{#elif} conditions
(commit d8fa97f), the comment allowed right after "{#" and right after the
directive name (commit cd74b20), and the new expand_operand_text() path
(preprocessor.cpp) that macro-expands a directive operand without the
blank-line-compaction pass that preprocess_text() runs.
"""
import io

ns = (16384, 32768, 65536)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        # A comment right after "{#", a block comment inside the condition,
        # and a trailing "//" comment that must not swallow the directive's
        # closing "}" (the directive's raw text is delimited structurally,
        # before any comment scanning runs on it).
        o.write(f"{{#(* c *)if {i} = {i} /* c2 */ // trailing comment}}\n")
        o.write("yes;\n")
        o.write("{#endif}\n")
