# coding: utf-8
"""Perf test case: function-macro arity growth."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        args = [f'a{j}' for j in range(i)]
        o.write(f"{{#define F{i}({','.join(args)}) {'+'.join(args)}}}\n")
