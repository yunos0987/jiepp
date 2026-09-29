# coding: utf-8
"""Perf test case: object/function macros whose bodies embed '$n' newlines,
expanded many times across lines, each use followed by __LINE__.

Guards the "output-only newline" mechanism (Token::newline()/num_of_lines,
and expand.cpp's advance_lineno() plus its sum_num_of_lines/sum_uncounted
bookkeeping) that marks a macro-body newline as belonging to the *output*
stream without it having consumed a line of *input* -- the same mechanism
commit aa73ca5 (this session's "core: expand directive operands and pragma
bodies without blank-line compaction") relies on to keep an operand's
embedded newlines from being spliced with a line marker. Every use of M/F
below re-triggers this line-marker resync path and then reads it back out
via __LINE__, so a regression that miscounts embedded newlines shows up as
both a perf outlier and (were this a correctness test) a wrong __LINE__
value.
"""
import io

ns = (16384, 32768, 65536)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#define M a$n$nb}\n")
    o.write("{#define F(x) x$na$nb}\n")
    for i in range(n):
        o.write("M __LINE__;\n")
        o.write("F(1) __LINE__;\n")
