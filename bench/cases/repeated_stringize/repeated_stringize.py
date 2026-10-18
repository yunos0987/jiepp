# coding: utf-8
"""Perf test case: a parameter stringized many times in one body, with an
argument made of n comments.

Guards commit 456703f (subst: make __VA_OPT__ and repeated @ linear in
argument size), which stringizes a parameter once per invocation
(ArgExpansionMemo::stringized). Before it, each @x rebuilt and rescanned
the whole argument, so k occurrences of @x against an n-token argument
cost O(k * n) (@x x8000 with a 40000-comment argument took 7.6 s). The
comments vanish from the stringized text ('a b'), so the output stays
small and the time measures the stringizing, not the output size.
The @__VA_ARGS__ form is exercised too.
"""
import io

ns = (16384, 32768, 65536)

_K = 512


def file(o: io.IOBase, n: int) -> None:
    arg = "a " + "(**)" * n + " b"
    o.write("{#define S(x) " + "@x " * _K + "}\n")
    o.write("{#define V(...) " + "@__VA_ARGS__ " * _K + "}\n")
    o.write(f"S({arg});\n")
    o.write(f"V({arg});\n")
