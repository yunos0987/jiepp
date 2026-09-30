# coding: utf-8
"""Perf test case: {#undef} directive volume."""
import io

ns = (1024, 2048, 4096)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        o.write(f"{{#define O{i} {i}}}\n")
    for i in range(n):
        o.write(f"{{#undef O{i}}}\n")
