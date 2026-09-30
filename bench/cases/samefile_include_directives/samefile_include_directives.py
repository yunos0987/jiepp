# coding: utf-8
"""Perf test case: include-graph shape with the same file included repeatedly."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    o.write("\n".join("{#include './samefile.dat'}" for _ in range(n)))
