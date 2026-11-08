# coding: utf-8
"""Perf test case: BOOST_PP_LOCAL_ITERATE.

local.hpp is included once and holds a chain of {#if BOOST_PP_LOCAL_C(i)}
groups, so this mixes {#if} evaluation and skipped groups with the
expansion of BOOST_PP_LOCAL_MACRO for each index.
"""
import io
from pathlib import Path

ns = (16, 64, 256)

_CORPUS = (Path(__file__).resolve().parents[3] / "tests" / "jiepp" / "input").as_posix().replace("$", "$$")


def file(o: io.IOBase, n: int) -> None:
    o.write(f"{{#syspath '{_CORPUS}'}}\n")
    o.write("{#include <boost/preprocessor/library.hpp>}\n")
    o.write("{#define BOOST_PP_LOCAL_MACRO(i) BOOST_PP_CAT(item_, i): int := __counter(it);}\n")
    o.write(f"{{#define BOOST_PP_LOCAL_LIMITS (0, {n - 1})}}\n")
    o.write("{#include BOOST_PP_LOCAL_ITERATE()}\n")
    # Fails the run (not just a fast, unexpanded one) if the iteration broke.
    o.write(f"__assert(__counter_peek(it) = {n}, BOOST_PP_LOCAL_ITERATE did not iterate)\n")
