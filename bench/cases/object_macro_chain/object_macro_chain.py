# coding: utf-8
"""Perf test case: object-macro expansion chain (task_slug hideset-linear, E3-1).

{#define M0 M1}{#define M1 M2}...{#define Mn END}M0; -- before the
persistent-trie HideSet, every expansion level copied the whole hide set
(std::set<std::string>), making this O(n^2); it must now be O(n log n).
"""
import io

ns = (16384, 32768, 65536)


def file(o: io.IOBase, n: int) -> None:
    for i in range(n):
        o.write(f"{{#define M{i} M{i + 1}}}")
    o.write(f"{{#define M{n} END}}")
    o.write("M0;\n")
