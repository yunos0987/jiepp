# coding: utf-8
"""Perf test case: {#if} conditional expression complexity."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    o.write(f"{{#if {'+'.join(str(i) for i in range(n))}}};{{#endif}}")
