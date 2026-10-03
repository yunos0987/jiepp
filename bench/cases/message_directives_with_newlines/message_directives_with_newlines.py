# coding: utf-8
"""Perf test case: {#info} directive volume with $n-newline-heavy message
text.

Guards handle_message() (directive_handlers.cpp), which now calls
expand_operand_text() instead of preprocess_text() to macro-expand a
message's text: unlike preprocess_text(), expand_operand_text() runs no
blank-line-compaction pass, so a message packed with many $n newlines (as in
OperandExpansionTest.MessageTextIsNotCompacted) must stay linear in the
number of directives rather than paying compaction's per-line cost on text
that was never meant to be compacted.
"""
import io

ns = (8192, 16384, 32768)

_TEN_NEWLINES = "$n" * 10


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        o.write(f"{{#info a{i}{_TEN_NEWLINES}b{i}}}\n")
