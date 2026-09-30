# coding: utf-8
"""Perf test case: a single function macro with a very wide parameter list.

Guards the O(k) duplicate-parameter check added by commit 2905bc2 (a
std::unordered_set<std::string_view> replacing an O(k^2) nested scan over
std::vector<std::string>) and the strict parameter-list parsing it runs
alongside. define_fmacro_args spreads k across n *separate* macros (macro i
has i parameters, so no single macro ever gets very wide) and so never
isolates this check's per-macro cost; this case uses one macro with n
distinct parameters and one call with n arguments, matching the commit's own
benchmark shape ("one macro with 16384 parameters").
"""
import io

ns = (65536, 131072, 262144)


def file(o: io.IOBase, n: int) -> None:
    params = [f"a{i}" for i in range(n)]
    o.write(f"{{#define F({','.join(params)}) {'+'.join(params)}}}\n")
    args = [str(i) for i in range(n)]
    o.write(f"F({','.join(args)});\n")
