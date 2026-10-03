# coding: utf-8
"""Perf test case: function-macro definition table growth."""
import io

ns = (2048, 4096, 8192)


def file(o: io.IOBase, n: int) -> None:
    o.write("\n".join(f"{{#define C{i}(a,b,c) a+b+c}}" for i in range(n)))
