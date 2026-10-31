# coding: utf-8
"""Perf test case: many __has_include probes in one __assert expression.

Guards the linear-time __has_include scan in expand_ctrl. Before it, the
token pre-scan of an __assert or __counter_set operand erased each
resolved call from the middle of the token vector and rescanned for each
call's closing parenthesis, so n probes cost O(n^2) (80000 probes took
204 s, Release, Windows). After it, the operand is scanned once. Every
probe names a missing file next to the input, so the time also includes
one filesystem lookup per probe, as in the {#if} path.
HasIncludeTest.FlatProbes_TimingGuard only fails above 3 s; this case
tracks the steady-state cost.
"""
import io

ns = (2048, 4096, 8192)


def file(o: io.IOBase, n: int) -> None:
    o.write("__assert(" + "__has_include('nx_has_include_probe.iec') + " * n + "1, m);\n")
