# coding: utf-8
"""Perf test case: repeated object-macro redefinition."""
import io

ns = (1024, 2048, 4096)


def file(o: io.IOBase, n: int) -> None:
    o.write("\n".join("{#define O 157}" for _ in range(n)))
