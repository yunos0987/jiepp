# coding: utf-8
"""Perf test case: function-macro expansion chain (task_slug hideset-linear, E3-1).

{#define F0(a) F1(a)}...{#define Fn(a) a}F0(x); -- the function-macro
version of object_macro_chain: each level also rebuilds the (HS(T) n
HS(')')) u {T} intersection, which was a fresh std::set<std::string> copy
per level (quadratic with a larger constant than the object-macro chain).
"""
import io

ns = (16384, 32768, 65536)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        o.write(f"{{#define F{i}(a) F{i + 1}(a)}}")
    o.write(f"{{#define F{n}(a) a}}")
    o.write("F0(x);\n")
