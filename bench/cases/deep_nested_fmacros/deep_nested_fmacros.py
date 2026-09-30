# coding: utf-8
"""Perf test case: deeply nested function-macro calls, deeper than
iterate_fmacros.

Deeply nested calls such as `I(I(...I(0)...))` take time proportional to the
square of the nesting depth (Prosser-style pre-expansion re-reads each
level's expanded argument, in gcc/clang too). iterate_fmacros already
exercises this up to n=512 (its own ns) -- commit ba2b275's hide-set-sharing
fix and commit 5b45aa7's "avoid token copies when expanding macro arguments"
were both measured against it up to n=1024. This case goes deeper (1024 /
1536 / 2048) to keep stressing that quadratic shape as depth grows past
those measurements, and every level's expand() call also pays the
stack-exhaustion check added by commits 0fc0253/fd84b53/22a733e (an 8 MiB
default stack budget, ExpansionDepthGuard checked at every expand() entry).
Must finish with rc 0: --recursion-limit 4096 (perftest's fixed flags) stays
well above 2048's call depth, so PP63 (STACK_EXHAUSTED) must not fire.
"""
import io

ns = (1024, 1536, 2048)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define I(a) (a)+1}\n")
    o.write("program Main\n")
    o.write("{st}\n")
    o.write(f"{''.join(['I('] * n)}0{''.join([')'] * n)};\n")
    o.write("{end}\n")
    o.write("end_program\n")
