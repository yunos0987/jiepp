# coding: utf-8
"""Perf test case: __has_include calls nested inside malformed ones.

Guards the linear-time __has_include scan in expand_ctrl. Before it,
every candidate call in an __assert operand had its whole text rebuilt
and parsed, so n nested calls cost O(n^2) (16000 took 9.5 s, Release,
Windows); nested <...> paths sharing one '>' were cubic. After it, every
keyword is parsed once. The expression is malformed (PP52) and the
assertion fails (PP91); both are ignored so that jiepp exits 0, as
perftest.py requires.
HasIncludeTest.NestedMalformedCalls_TimingGuard only fails above 3 s; this
case tracks the steady-state cost.
"""
import io

ns = (2048, 4096, 8192)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#ignore PP52}{#ignore PP91}\n")
    o.write("__assert(" + "__has_include(" * n + "1" + ")" * n + ", m);\n")
