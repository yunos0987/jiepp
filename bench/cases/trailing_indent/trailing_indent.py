# coding: utf-8
"""Perf test case: long indentation after a compacted blank-line run.

Guards the linear-time collection of the trailing indentation in
line_compaction. Before it, each whitespace token was prepended to the
string collected so far, so n tokens cost O(n^2) (800000 tokens took 12.9 s,
Release, Windows). The ten blank lines exceed the default --max-blank-lines,
and the n empty-macro uses leave n separate whitespace tokens before `y`.
BlankLinesTest.LongTrailingIndentationRunIsLinear_TimingGuard only fails
above 3 s; this case tracks the steady-state cost.
"""
import io

ns = (50000, 100000, 200000)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define S}\nx\n" + "\n" * 10 + "S " * n + "y\n")
