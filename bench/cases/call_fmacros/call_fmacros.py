# coding: utf-8
"""Perf test case: function-macro call volume."""
import io

ns = (1024, 2048, 4096)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define F(a,b,c) a+b+c}\n")
    o.write("program Main\n")
    o.write("{st}\n")
    o.write("\n".join(f"F({i},{i + 1},{i + 2});" for i in range(n)))
    o.write("{end}\n")
    o.write("end_program\n")
