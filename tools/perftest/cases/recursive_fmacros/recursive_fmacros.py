# coding: utf-8
"""Perf test case: macro-expansion chain depth (recursive-style definitions)."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define F0(a) a}\n")
    o.write("\n".join(f"{{#define F{i + 1}(a) F{i}(a)}}" for i in range(n)))
    o.write(f"F{n}(0);\n")
