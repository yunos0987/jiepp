# coding: utf-8
"""Perf test case: object-macro definition table growth."""
import io

ns = (2048, 4096, 8192)


def file(o: io.IOBase, n: int) -> None:
    o.write("\n".join(f"{{#define O{i} {i}}}" for i in range(n)))
