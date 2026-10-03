# coding: utf-8
"""Perf test case: wide include-graph shape."""
import io
from pathlib import Path

ns = (128, 256, 512)


def generate(testid: str, dirpath: Path) -> None:
    for i in range(max(ns)):
        with open(dirpath.joinpath(f'perftest.{testid}.{i}.ttxt'), 'w') as f:
            f.write("\n")
    for n in ns:
        with open(dirpath.joinpath(f'perftest.{testid}.{n}.gtxt'), 'w') as f:
            for l in range(n):
                f.write(f"{{#include 'perftest.{testid}.{l}.ttxt'}}\n")
