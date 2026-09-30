# coding: utf-8
"""Perf test case: object-macro expansion volume in an expression."""
import io

ns = (1024, 2048, 4096)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define M 2}\n")
    o.write("program Main\n")
    o.write("{st}\n")
    o.write(f"{'+'.join(['M'] * n)};\n")
    o.write("{end}\n")
    o.write("end_program\n")
