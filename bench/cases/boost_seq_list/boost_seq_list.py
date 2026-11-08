# coding: utf-8
"""Perf test case: BOOST_PP sequence and list algorithms on long inputs.

Long macro arguments passed down many levels: SEQ_FOR_EACH, SEQ_TRANSFORM and
LIST_FOLD_LEFT copy and re-expand the whole data structure at each step.
"""
import io
from pathlib import Path

ns = (32, 64, 128)

_CORPUS = (Path(__file__).resolve().parents[3] / "tests" / "jiepp" / "input").as_posix().replace("$", "$$")


def file(o: io.IOBase, n: int) -> None:
    seq = "".join(f"(e{i})" for i in range(n))
    o.write(f"{{#syspath '{_CORPUS}'}}\n")
    o.write("{#include <boost/preprocessor/library.hpp>}\n")
    o.write("{#define DECL(r, t, e) e: t;}\n")
    o.write("{#define WRAP(s, d, e) BOOST_PP_CAT(d, e)}\n")
    o.write("{#define JOIN(d, st, e) BOOST_PP_CAT(st, e)}\n")
    o.write(f"{{#define SEQ {seq}}}\n")
    o.write("BOOST_PP_SEQ_FOR_EACH(DECL, int, SEQ)\n")
    o.write("BOOST_PP_SEQ_SIZE(BOOST_PP_SEQ_TRANSFORM(WRAP, p_, SEQ))\n")
    o.write("BOOST_PP_LIST_FOLD_LEFT(JOIN, x, BOOST_PP_SEQ_TO_LIST(SEQ))\n")
    # Fails the run (not just a fast, unexpanded one) if the sequence broke.
    o.write(f"__assert(BOOST_PP_LIST_SIZE(BOOST_PP_SEQ_TO_LIST(SEQ)) = {n}, "
            "BOOST_PP_SEQ_TO_LIST did not expand)\n")
