# coding: utf-8
"""Perf test case: a function-macro parameter spelled multiple times in the
body (task_slug arg-expand-once).

Before the fix, src/core/expand_subst.cpp's subst() re-expanded a formal
parameter's actual argument once PER OCCURRENCE of that parameter in the
body, instead of once per invocation (C17 6.10.3.1p1). A chain of n nested
single-use macros fed into a parameter spelled k times therefore cost
O(n * k) work for the chain's own expansion -- repeated independently for
each of the k occurrences -- on top of the O(n^2)-ish cost
deep_nested_fmacros already exercises for the chain itself. This case
isolates that extra, now-eliminated factor of k:

(a) I(I(...I(0)...)), n deep (I(a) (a)+1, a single-use parameter -- cheap
    by itself, like deep_nested_fmacros), fed into M(a) a a a a (a's
    parameter spelled 4 times): the chain must be expanded once and its
    result copied 4 times, not re-walked 4 times.
(b) a fixed-depth (14) doubling chain D(a) a a, D(D(...D(0)...)): each
    level doubles the occurrence count of a single-use-chain argument if
    never memoised (2**14 occurrences of the innermost expansion before
    the fix), independent of n -- an rc == 0 (termination, under
    perftest's fixed --recursion-limit) check more than a timing one,
    kept shallow enough to still finish (slowly) pre-fix for the
    old-vs-new baseline comparison in the design's perf gate.
"""
import io

ns = (512, 1024, 2048)

_D_DEPTH = 14


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define I(a) (a)+1}\n")
    o.write("{#define M(a) a a a a}\n")
    o.write("{#define D(a) a a}\n")
    o.write("program Main\n")
    o.write("{st}\n")
    o.write(f"M({''.join(['I('] * n)}0{''.join([')'] * n)});\n")
    o.write(f"{''.join(['D('] * _D_DEPTH)}0{''.join([')'] * _D_DEPTH)};\n")
    o.write("{end}\n")
    o.write("end_program\n")
