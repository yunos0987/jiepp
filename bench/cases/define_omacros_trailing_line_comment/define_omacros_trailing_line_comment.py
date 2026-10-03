# coding: utf-8
"""Perf test case: object-macro definition table growth, each body ending in
a '//' comment that reaches no real end-of-line.

Guards line_comments_to_block_comments() (directive_handlers.cpp, added by
commit 7aea13a), which handle_define() runs once per {#define} to rewrite a
trailing "//"/"//!" comment token into a block comment so it cannot silently
swallow the rest of an output line once the macro is used. Compare against
the plain define_omacros case to isolate this rewrite's per-definition cost
from ordinary macro-table growth.
"""
import io

ns = (32768, 65536, 131072)


def file(o: io.IOBase, n: int) -> None:
    o.write("\n".join(f"{{#define O{i} {i} // trailing comment {i}}}" for i in range(n)))
