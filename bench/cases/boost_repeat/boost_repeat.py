# coding: utf-8
"""Perf test case: BOOST_PP_REPEAT generating declarations, as a real user does.

Nested REPEAT with BOOST_PP_ADD/BOOST_PP_CAT in the body, like
tests/jiepp/input/boost.iec: deep macro recursion with long identifiers.
"""
import io
from pathlib import Path

ns = (8, 16, 32)

_CORPUS = (Path(__file__).resolve().parents[3] / "tests" / "jiepp" / "input").as_posix().replace("$", "$$")


def file(o: io.IOBase, n: int) -> None:
    o.write(f"{{#syspath '{_CORPUS}'}}\n")
    o.write("{#include <boost/preprocessor/library.hpp>}\n")
    o.write("{#define PARAM(z, i, t) BOOST_PP_CAT(in, BOOST_PP_ADD(i, 1)): t;}\n")
    o.write("{#define FUNC(z, i, t) function BOOST_PP_CAT(f_, i): t "
            "var_input BOOST_PP_REPEAT(BOOST_PP_ADD(i, 2), PARAM, t) end_var end_function}\n")
    o.write(f"BOOST_PP_REPEAT({n}, FUNC, int)\n")
    # Fails the run (not just a fast, unexpanded one) if the repetition broke.
    o.write("{#define ITEM(z, i, t) (i)}\n")
    o.write(f"__assert(BOOST_PP_SEQ_SIZE(BOOST_PP_REPEAT({n}, ITEM, _)) = {n}, "
            "BOOST_PP_REPEAT did not expand)\n")
