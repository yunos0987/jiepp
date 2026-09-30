# coding: utf-8
"""Perf test case: deeply nested macro call syntax."""
import io

ns = (128, 256, 512)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define I(a) (a)+1}\n")
    o.write("program Main\n")
    o.write("{st}\n")
    o.write(f"{''.join(['I('] * n)}0{''.join([')'] * n)};\n")
    o.write("{end}\n")
    o.write("end_program\n")
