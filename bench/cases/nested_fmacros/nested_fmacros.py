# coding: utf-8
"""Perf test case: macro-expansion chain depth (nested definitions)."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define F0(a) a}\n")
    for i in range(n):
        o.write(f"{{#define F{i + 1}(a) F{i}(a)}}\n")
    o.write(f"F{n}(0);\n")
