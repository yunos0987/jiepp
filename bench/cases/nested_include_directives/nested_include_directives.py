# coding: utf-8
"""Perf test case: deep include-chain shape."""
import io
from pathlib import Path

ns = (16, 32, 64)


def generate(testid: str, dirpath: Path) -> None:
    for i in range(max(ns)):
        with open(dirpath.joinpath(f'perftest.{testid}.{i}.ttxt'), 'w') as f:
            if i != 0:
                f.write(f"{{#include 'perftest.{testid}.{i - 1}.ttxt'}}\n")
    for n in ns:
        with open(dirpath.joinpath(f'perftest.{testid}.{n}.gtxt'), 'w') as f:
            f.write(f"{{#include 'perftest.{testid}.{n - 1}.ttxt'}}\n")
