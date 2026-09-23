# coding: utf-8
"""Perf test case: token-paste (##) chains."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    args = [f"a{i}" for i in range(n)]
    o.write(f"{{#define F({','.join(args)}) {'@@'.join(args)}}}\n")
    params = [f"{i % 2}" for i in range(n)]
    o.write(f"F(c{','.join(params)});\n")
