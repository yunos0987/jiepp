# coding: utf-8
"""Perf test case: BOOST_PP variadic and tuple helpers on wide argument lists.

__VA_ARGS__ handling, argument counting and overload selection. The largest
size is BOOST_PP_LIMIT_VARIADIC (64): more arguments do not expand.
"""
import io
from pathlib import Path

ns = (16, 32, 64)

_CORPUS = (Path(__file__).resolve().parents[3] / "tests" / "jiepp" / "input").as_posix().replace("$", "$$")


def file(o: io.IOBase, n: int) -> None:
    args = ", ".join(f"v{i}" for i in range(n))
    o.write(f"{{#syspath '{_CORPUS}'}}\n")
    o.write("{#include <boost/preprocessor/library.hpp>}\n")
    for i in range(0, n, max(1, n // 16)):
        o.write(f"BOOST_PP_VARIADIC_ELEM({i}, {args})\n")
        o.write(f"BOOST_PP_TUPLE_ELEM({i}, ({args}))\n")
    o.write(f"BOOST_PP_VARIADIC_SIZE({args})\n")
    o.write(f"BOOST_PP_SEQ_SIZE(BOOST_PP_VARIADIC_TO_SEQ({args}))\n")
    o.write(f"BOOST_PP_LIST_SIZE(BOOST_PP_TUPLE_TO_LIST(({args})))\n")
    # Fails the run (not just a fast, unexpanded one) past the limit.
    o.write(f"__assert(BOOST_PP_VARIADIC_SIZE({args}) = {n}, BOOST_PP_VARIADIC_SIZE did not expand)\n")
