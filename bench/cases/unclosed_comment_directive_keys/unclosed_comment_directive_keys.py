# coding: utf-8
"""Perf test case: directive keys made of unclosed comment openers.

Guards commit b993870 (directive_parser: find comment closers once per
directive key scan). parse_directive() decides at every character of a
directive name whether a comment starts there; before the fix an unclosed
"(*" or "/*" made it search to the end of the directive for the closer at
every character, so a key of n openers cost O(n^2) (100K "(*" took 3.7 s,
200K 13.2 s). The fix finds the last closers once.

The keys are not valid directive names (PP45). {#ignore PP45} keeps the
run at exit code 0, as perftest.py requires, without skipping the key scan
that is being measured. DirectiveParserTest.
UnclosedOpenersInKeyAreLinear_TimingGuard only fails above 3 s; this case
tracks the steady-state cost.
"""
import io

ns = (65536, 131072, 262144)


def file(o: io.IOBase, n: int) -> None:
    o.write("{#ignore PP45}\n")
    o.write("{#" + "(*" * n + "}\n")
    o.write("{#" + "/*x" * (n // 2) + "}\n")
    o.write("done;\n")
