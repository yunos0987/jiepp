# coding: utf-8
"""Perf test case: a variadic macro body with many __VA_OPT__ occurrences,
called with many variable arguments and with none.

Guards commit 456703f (subst: make __VA_OPT__ and repeated @ linear in
argument size). Before it, every __VA_OPT__ occurrence copied all variable
arguments just to test emptiness and copied its own content even when it
was skipped, so k occurrences against n arguments cost O(k * n)
(k = 1600, n = 40000 took 8.9 s). After it, the emptiness test stops at
the first token and skipped content is never copied: the cost is linear
in the input. SubstCostTest.*_TimingGuard only fails above 3 s; this case
tracks the steady-state cost.
"""
import io

ns = (8192, 16384, 32768)

_K = 512


def file(o: io.IOBase, n: int) -> None:
    content = " ".join(f"z{i}" for i in range(64))
    o.write("{#define M(...) " + "__VA_OPT__(y)" * _K + "}\n")
    o.write("{#define E(...) " + f"__VA_OPT__({content}) " * 8 + "}\n")
    o.write("M(" + ",".join(f"a{i}" for i in range(n)) + ");\n")
    for _ in range(n // 8):
        o.write("E();\n")
