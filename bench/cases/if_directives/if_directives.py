# coding: utf-8
"""Perf test case: {#if} directive volume."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        o.write(f"{{#if {i}}}\n")
        o.write("//\n")
        o.write("{#endif}\n")
