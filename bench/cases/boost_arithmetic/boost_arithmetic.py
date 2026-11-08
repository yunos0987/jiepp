# coding: utf-8
"""Perf test case: BOOST_PP arithmetic and comparison through BOOST_PP_WHILE.

MUL and DIV iterate WHILE with a tuple state, so each result is many nested
function-macro calls whose hide sets grow with the depth.
"""
import io
from pathlib import Path

ns = (8, 16, 32)

_CORPUS = (Path(__file__).resolve().parents[3] / "tests" / "jiepp" / "input").as_posix().replace("$", "$$")


def file(o: io.IOBase, n: int) -> None:
    o.write(f"{{#syspath '{_CORPUS}'}}\n")
    o.write("{#include <boost/preprocessor/library.hpp>}\n")
    for i in range(1, n + 1):
        o.write(f"BOOST_PP_MUL({i}, 3) BOOST_PP_DIV({i * 3}, 2) "
                f"BOOST_PP_MOD({i}, 7) BOOST_PP_GREATER({i}, {n // 2})\n")
    # Fails the run (not just a fast, unexpanded one) if the arithmetic broke.
    o.write(f"__assert(BOOST_PP_DIV({n * 3}, 2) = {n * 3 // 2}, BOOST_PP_DIV did not expand)\n")
