# coding: utf-8
"""Perf test case: a guarded header included again and again.

Only the first include does real work; the rest measure path resolution and
the guard. The header sits in a nested directory so that canonicalising the
path has several components to look at.
"""
from pathlib import Path

ns = (1024, 2048, 4096)


def generate(testid: str, dirpath: Path) -> None:
    subdir = dirpath / "a" / "b" / "c" / "d"
    subdir.mkdir(parents=True, exist_ok=True)
    (subdir / "guarded.ttxt").write_text(
        "{#ifndef GUARDED}\n{#define GUARDED}\nguarded;\n{#endif}\n", encoding="utf-8", newline="\n")
    for n in ns:
        with open(dirpath / f"perftest.{testid}.{n}.gtxt", "w", encoding="utf-8", newline="\n") as f:
            for _ in range(n):
                f.write("{#include 'a/b/c/d/guarded.ttxt'}\n")
