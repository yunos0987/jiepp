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

The operators are rewritten one by one, so a directive whose C grouping
jiepp would not share (SPECIFICATION.md 6.2: jiepp binds & | ^ tighter than
+ - << >> and the comparisons, all at one level, and its ! yields Bool) would
silently change meaning. Such #if/#elif conditions and #define bodies --
an unparenthesized binary & | ^ next to a comparison, + - << >> or a
different bit operator, or a ! operand compared without parentheses -- are
reported as "precedence:" lines on stderr with their upstream line number,
and the exit code is 1 (the files are still written). Check the converted
directive in such a case and add a conversion rule if needed.

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


_COND_TOKEN = re.compile(r"\d\w*|[A-Za-z_]\w*|&&|\|\||==|!=|<=|>=|<<|>>|\S")
_BIT_OPS = {"&", "|", "^"}
_REGROUPED_WITH_BIT = {"==", "!=", "<", ">", "<=", ">=", "+", "-", "<<", ">>"}
_COMPARISONS = {"==", "!=", "<", ">", "<=", ">="}
# Lowest-precedence separators, grouped alike in C and jiepp.
_SPLITTERS = {"&&", "||", "?", ":", ","}
_EXPR_DIRECTIVE = re.compile(r"\s*#\s*(?:if|elif|define\s+\w+(?:\([^)]*\))?)(?=\W|$)(.*)")


def _segment_hazard(binary_ops, unary_not):
    bits = binary_ops & _BIT_OPS
    if bits and (len(bits) > 1 or binary_ops & _REGROUPED_WITH_BIT):
        return "unparenthesized bit operator mixed with " + " ".join(
            sorted((binary_ops - bits) | (bits if len(bits) > 1 else set())))
    if unary_not and binary_ops & _COMPARISONS:
        return "'!' operand compared without parentheses"
    return None


def precedence_hazard(expr):
    """Why C and jiepp would group the C expression `expr` differently, or
    None. Each parenthesized group and each operand of && || ? : , is one
    segment; a + - & after an operand is binary, otherwise unary."""
    segments = [[set(), False]]
    prev_is_operand = False
    for tok in _COND_TOKEN.findall(expr):
        seg = segments[-1]
        if tok == "(":
            segments.append([set(), False])
            prev_is_operand = False
        elif tok == ")":
            if len(segments) > 1:
                reason = _segment_hazard(*segments.pop())
                if reason:
                    return reason
            prev_is_operand = True
        elif tok in _SPLITTERS:
            reason = _segment_hazard(*seg)
            if reason:
                return reason
            segments[-1] = [set(), False]
            prev_is_operand = False
        elif tok[0].isalnum() or tok[0] == "_":
            prev_is_operand = True
        else:
            if prev_is_operand:
                seg[0].add(tok)
            elif tok == "!":
                seg[1] = True
            prev_is_operand = False
    for seg in reversed(segments):
        reason = _segment_hazard(*seg)
        if reason:
            return reason
    return None


def precedence_hazards(text):
    """(upstream line number, reason, line) for each #if/#elif condition or
    #define body in the C source `text` that precedence_hazard() flags."""
    # Join backslash continuations; a logical line keeps its first line number.
    logical = []
    for lineno, line in enumerate(text.replace("\r\n", "\n").split("\n"), 1):
        if logical and logical[-1][1].endswith("\\"):
            logical[-1] = (logical[-1][0], logical[-1][1][:-1] + " " + line)
        else:
            logical.append((lineno, line))
    hazards = []
    for start, line in logical:
        match = _EXPR_DIRECTIVE.match(line)
        if match is None:
            continue
        expr = re.sub(r"//.*", "", re.sub(r"/\*.*?\*/", " ", match.group(1)))
        reason = precedence_hazard(expr.replace("#", " "))
        if reason:
            hazards.append((start, reason, line.strip()))
    return hazards


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
    hazard_count = 0
    source_files = sorted(args.source.rglob("*.hpp"))
    if not source_files:
        print(f"ERROR: no *.hpp under {args.source}", file=sys.stderr)
        return 1
    for src in source_files:
        rel = src.relative_to(args.source)
        source_text = src.read_bytes().decode("latin-1")
        for lineno, reason, line in precedence_hazards(source_text):
            hazard_count += 1
            print(f"precedence: {rel.as_posix()}:{lineno}: {reason}: {line}", file=sys.stderr)
        converted = convert_text(source_text).encode("latin-1")
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
          f"{'differ' if args.check else 'written'}, {len(extra)} not in source, "
          f"{hazard_count} precedence hazards")
    return 1 if hazard_count or (args.check and (differing or extra)) else 0


if __name__ == "__main__":
    sys.exit(main())
