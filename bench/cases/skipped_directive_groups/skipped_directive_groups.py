# coding: utf-8
"""Perf test case: long directives inside groups that are skipped.

The cost of a directive that does nothing: compiler-specific branches of a
portable header, here with a long replacement list that is never used.
"""
import io

ns = (4096, 16384, 65536)


def file(o: io.IOBase, n: int) -> None:
    body = " ".join(f"tok{i}" for i in range(32))
    o.write("{#if 0}\n")
    for i in range(n):
        o.write(f"{{#define SKIPPED_{i} {body}}}\n")
    o.write("{#else}\nkept;\n{#endif}\n")
