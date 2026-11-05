#!/usr/bin/env python3
# coding: utf-8
"""Convert Boost.Preprocessor headers from C preprocessor syntax to jiepp syntax.

Regenerates tests/jiepp/input/boost/preprocessor from an upstream checkout:

    python tools/convert_boost_pp.py <upstream>/include/boost/preprocessor \
        tests/jiepp/input/boost/preprocessor

--check compares instead of writing and exits 1 on a difference, which verifies
a tree was produced by these rules from that upstream version.

The rewrite is purely textual and line based, so a converted header keeps the
upstream line numbers except where a backslash continuation is folded:
  - "\\<newline>" becomes /*n*/ (a directive stays on one line)
  - "# directive" becomes {# directive}; a "#" line that is not a directive,
    and a "#pragma" (which is MSVC specific), becomes a // comment
  - in a directive: || && ! stay as they are; ~ | == != become not or = <>,
    0x1F becomes 16#1F, the integer suffixes U and L are dropped, ## becomes @@,
    #param becomes @param, and ":" in {#error} becomes "$:"
  - a line starting with "//" (upstream comments out directives this way) gets
    the same rewrite

Standard library only.
"""
import argparse
import re
import sys
from pathlib import Path

_DIRECTIVE = re.compile(
    r"\s*#\s*(if|ifdef|ifndef|elif|else|endif|define|undef|include|error|warning|pragma|line)\b"
)


def _rewrite_operators(body):
    body = body.replace("||", "\0").replace("&&", "\1")
    body = body.replace("!=", " <> ").replace("==", " = ")
    body = body.replace("~", " not ").replace("|", " or ")
    body = body.replace("\0", "||").replace("\1", "&&")
    body = re.sub(r"(?<!\w)0x([0-9a-fA-F]+)(?:UL|LU|U|L)?\b", r"16#\1", body)
    body = re.sub(r"(?<!\w)(\d+)(?:UL|LU|U|L)\b", r"\1", body)
    body = body.replace("##", "@@")
    return re.sub(r"(?<![\w@])#(?=\w)", "@", body)


def convert_text(text):
    out = []
    for line in text.replace("\r\n", "\n").replace("\\\n", "/*n*/").split("\n"):
        if line.startswith("//"):
            out.append(_rewrite_operators(line))
            continue
        if not line.lstrip().startswith("#"):
            out.append(line)
            continue
        match = _DIRECTIVE.match(line)
        if match is None:
            out.append("// " + line)
            continue
        hash_pos = line.index("#")
        body = line[hash_pos + 1:]
        keyword = match.group(1)
        if keyword == "pragma":
            out.append("//{#" + body.strip().replace(":", "$:") + "}")
            continue
        body = _rewrite_operators(body)
        if keyword == "error":
            body = body.replace(":", "$:")
        out.append(line[:hash_pos] + "{#" + body + "}")
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("source", type=Path, help="upstream include/boost/preprocessor directory")
    parser.add_argument("dest", type=Path, help="directory of the converted headers")
    parser.add_argument("--check", action="store_true", help="compare only; exit 1 on a difference")
    args = parser.parse_args(argv)

    if not args.source.is_dir():
        print(f"ERROR: source directory not found: {args.source}", file=sys.stderr)
        return 1

    differing = []
    source_files = sorted(args.source.rglob("*.hpp"))
    if not source_files:
        print(f"ERROR: no *.hpp under {args.source}", file=sys.stderr)
        return 1
    for src in source_files:
        rel = src.relative_to(args.source)
        converted = convert_text(src.read_bytes().decode("latin-1")).encode("latin-1")
        dst = args.dest / rel
        if dst.is_file() and dst.read_bytes() == converted:
            continue
        differing.append(rel.as_posix())
        if not args.check:
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(converted)

    expected = {src.relative_to(args.source).as_posix() for src in source_files}
    extra = sorted(
        p.relative_to(args.dest).as_posix() for p in args.dest.rglob("*.hpp")
        if p.relative_to(args.dest).as_posix() not in expected
    ) if args.dest.is_dir() else []

    for rel in differing:
        print(f"{'differs' if args.check else 'wrote'}: {rel}")
    for rel in extra:
        print(f"not in source: {rel}")
    print(f"{len(source_files)} source files, {len(differing)} "
          f"{'differ' if args.check else 'written'}, {len(extra)} not in source")
    return 1 if args.check and (differing or extra) else 0


if __name__ == "__main__":
    sys.exit(main())
