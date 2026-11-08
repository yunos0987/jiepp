# coding: utf-8
"""Perf test case: including all of Boost.Preprocessor, then re-including it.

The first include lexes the headers and runs their {#define}s and skipped
groups; each later one goes through the include guards only (path resolution,
directive dispatch). tests/jiepp/input holds the converted Boost headers.
"""
import io
from pathlib import Path

ns = (1, 8, 64)

_CORPUS = (Path(__file__).resolve().parents[3] / "tests" / "jiepp" / "input").as_posix().replace("$", "$$")


def file(o: io.IOBase, n: int) -> None:
    o.write(f"{{#syspath '{_CORPUS}'}}\n")
    for _ in range(n):
        o.write("{#include <boost/preprocessor/library.hpp>}\n")
    # Fails the run (not just a fast, unexpanded one) if the library broke.
    o.write("__assert(BOOST_PP_ADD(2, 3) = 5, BOOST_PP_ADD did not expand)\n")
