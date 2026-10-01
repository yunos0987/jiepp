#!/usr/bin/env python3
# coding: utf-8
"""bench/perftest.py -- jiepp preprocessor performance test harness.

Ported from jiecc's tools/perftest/{perftest.py,perftest_maker.py,
perftest_config.py,stats_perftest.py}. Unlike the jiecc version, this
harness invokes jiepp.exe directly (no jiecc Python driver dependency) and
keeps every generated artifact under a caller-supplied output directory
instead of the source tree.

Subcommands
-----------
make       Generate perf test case input files from the *.py generators
           under --cases-src into --out (the output directory is cleared
           first).
run        Run jiepp against every generated case under --cases, --repeat
           times each, and write a wide TSV of per-run/min/median/mean/
           stdev timings (milliseconds) under --results.
selfcheck  Fast make+run over two small cases (repeat=1); validates the
           resulting TSV shape. Intended for `ctest -L perf`
           (perftest_smoke).

Standard library only. No dependency on the sibling jiecc repository.

Generator module contract (kept identical to jiecc's, for easy re-porting):
    ns = (n1, n2, n3)                  # tuple of case sizes
    def file(o, n): ...                # write one case of size n to stream o
or:
    ns = (n1, n2, n3)
    def generate(testid, dirpath): ... # write the whole case directory itself

Timings include jiepp.exe process-spawn overhead (a wall-clock timer wraps
the whole subprocess.run call), matching jiecc's historical timing
semantics, which also included process-spawn cost.
"""
import argparse
import importlib.util
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
import types
from datetime import datetime
from pathlib import Path

# Never litter the source tree with __pycache__ when loading case generators
# via spec_from_file_location.
sys.dont_write_bytecode = True

_FILE_CONTRACT_EXT = "gxxx"
_RUNNABLE_EXTS = (".gxxx", ".gtxt", ".txt")

_JIEPP_FIXED_FLAGS = (
    "--max-include-depth", "1000000",
    "--max-expansion-depth", "1000000",
    "--max-if-nesting", "1000000",
    "--recursion-limit", "4096",
)

_SELFCHECK_CASES = ("glues", "if_directive_plus_const_exprs")


def _load_case_module(py_filepath: Path) -> types.ModuleType:
    spec = importlib.util.spec_from_file_location(py_filepath.stem, py_filepath)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _discover_case_dirs(cases_src: Path):
    """Yield (case_name, case_dir, generator_py) for every subdirectory of
    cases_src that contains a <name>.py generator module."""
    for entry in sorted(cases_src.iterdir()):
        if not entry.is_dir():
            continue
        generator = entry / f"{entry.name}.py"
        if generator.is_file():
            yield entry.name, entry, generator


def _report_unknown_cases(case_filter, available) -> bool:
    """Print an error and return True if case_filter names cases that are not
    in 'available'; a typo must not silently benchmark fewer cases."""
    unknown = sorted(case_filter - set(available))
    if unknown:
        print(f"ERROR: unknown perf test case(s): {', '.join(unknown)}", file=sys.stderr)
        return True
    return False


def cmd_make(args) -> int:
    cases_src = Path(args.cases_src)
    out_dir = Path(args.out)
    if not cases_src.is_dir():
        print(f"ERROR: --cases-src not found: {cases_src}", file=sys.stderr)
        return 1

    case_filter = set(args.case) if args.case else None
    if case_filter is not None:
        available = {case_name for case_name, _, _ in _discover_case_dirs(cases_src)}
        if _report_unknown_cases(case_filter, available):
            return 1

    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    made = 0
    for case_name, case_dir, generator_py in _discover_case_dirs(cases_src):
        if case_filter is not None and case_name not in case_filter:
            continue

        case_out = out_dir / case_name
        case_out.mkdir(parents=True, exist_ok=True)

        # Copy fixed (non-generator) data files, e.g. samefile.dat.
        for fixed in sorted(case_dir.iterdir()):
            if fixed.is_file() and fixed.name != generator_py.name:
                shutil.copy2(fixed, case_out / fixed.name)

        module = _load_case_module(generator_py)
        if not hasattr(module, "ns"):
            print(f"ERROR: case '{case_name}' has no 'ns' tuple", file=sys.stderr)
            return 1
        ns = list(module.ns)
        if args.mode == "greedy":
            ns = [max(ns)]
        elif args.mode == "greedy2":
            ns = [min(ns)]
        module.ns = ns

        if hasattr(module, "generate"):
            module.generate(case_name, case_out)
        elif hasattr(module, "file"):
            for n in ns:
                target = case_out / f"perftest.{case_name}.{n}.{_FILE_CONTRACT_EXT}"
                with open(target, "w", encoding="utf-8", newline="\n") as f:
                    module.file(f, n)
        else:
            print(f"ERROR: case '{case_name}' has neither 'file' nor 'generate'", file=sys.stderr)
            return 1
        made += 1

    if made == 0:
        print("ERROR: no perf test cases matched (0 cases generated)", file=sys.stderr)
        return 1

    print(f"Generated {made} perf test case(s) under {out_dir}")
    return 0


def _discover_run_files(cases_dir: Path):
    """Yield (case_name, input_path) for every top-level jiepp input file
    under cases_dir. Only one level of case subdirectories is scanned, so
    include-only fixtures (e.g. *.ttxt, samefile.dat) are never run
    directly -- only the files produced by a case's 'file'/'generate'
    contract for its top-level ns sizes are."""
    for case_dir in sorted(p for p in cases_dir.iterdir() if p.is_dir()):
        for entry in sorted(case_dir.iterdir()):
            if entry.is_file() and entry.suffix in _RUNNABLE_EXTS:
                yield case_dir.name, entry


def _run_once(jiepp_exe: str, input_path: Path, out_dir: Path, timeout: float) -> float:
    out_dir.mkdir(parents=True, exist_ok=True)
    output_path = out_dir / f"{input_path.stem}.out"
    command = [jiepp_exe, input_path.name, "-o", str(output_path), *_JIEPP_FIXED_FLAGS]
    start = time.perf_counter()
    try:
        result = subprocess.run(
            command,
            cwd=str(input_path.parent),
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as exc:
        print(f"ERROR: jiepp timed out after {timeout}s on {input_path}", file=sys.stderr)
        if exc.stderr:
            print(exc.stderr, file=sys.stderr)
        raise
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    if result.returncode != 0:
        print(f"ERROR: jiepp exited {result.returncode} on {input_path}", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        raise RuntimeError(str(input_path))
    return elapsed_ms


def _sort_key(rel: str):
    m = re.match(r"^(.+)\.([0-9]+)\.[^.]+$", rel)
    if m:
        return (m.group(1), int(m.group(2)))
    return (rel, 0)


def cmd_run(args) -> int:
    if args.repeat < 1:
        print(f"ERROR: --repeat must be >= 1 (got {args.repeat})", file=sys.stderr)
        return 1

    jiepp_path = Path(args.jiepp)
    if not jiepp_path.is_file():
        print(f"ERROR: --jiepp not found: {jiepp_path}", file=sys.stderr)
        return 1
    if not os.access(jiepp_path, os.X_OK):
        print(f"ERROR: --jiepp is not executable: {jiepp_path}", file=sys.stderr)
        return 1
    # Resolve to an absolute path: _run_once() spawns jiepp with cwd set to
    # each case's own directory, so a relative --jiepp path would otherwise
    # fail to resolve once the working directory changes.
    jiepp_path = jiepp_path.resolve()

    cases_dir = Path(args.cases)
    if not cases_dir.is_dir():
        print(f"ERROR: --cases not found: {cases_dir}", file=sys.stderr)
        return 1
    # Resolve to an absolute path: the scratch output directory below is
    # derived from cases_dir's parent, and a relative --cases path would
    # otherwise point at the wrong place once compared across cwd changes.
    cases_dir = cases_dir.resolve()

    results_dir = Path(args.results)
    results_dir.mkdir(parents=True, exist_ok=True)
    # Resolve to an absolute path: --results is a user-facing cache variable
    # (JIEPP_PERF_RESULTS_DIR) that may be relative to an unrelated cwd, and
    # keeping it relative would make the TSV path below ambiguous.
    results_dir = results_dir.resolve()

    display_build_type = args.build_type if args.build_type else "unknown"
    is_release = (args.build_type == "Release")
    if not is_release:
        print(f"WARNING: benchmarking a {display_build_type} build; timings are not representative", file=sys.stderr)

    case_filter = set(args.case) if args.case else None
    if case_filter is not None:
        available = {case_name for case_name, _ in _discover_run_files(cases_dir)}
        if _report_unknown_cases(case_filter, available):
            return 1
    entries = [
        (case_name, input_path)
        for case_name, input_path in _discover_run_files(cases_dir)
        if case_filter is None or case_name in case_filter
    ]
    if not entries:
        print("ERROR: no perf test input files found (0 cases)", file=sys.stderr)
        return 1

    # Scratch output lives beside the cases directory, not beside --results:
    # --results may be an unrelated user-supplied cache directory (e.g.
    # JIEPP_PERF_RESULTS_DIR pointed at $HOME/results), and deleting
    # "<that dir>/../out" would recursively remove whatever happens to sit
    # next to it. cases_dir is always the perftest cases tree, so its parent
    # is a safe place to scope the scratch "out" directory to.
    out_dir = cases_dir.parent / "out"
    if out_dir.exists():
        shutil.rmtree(out_dir)

    samples = {}
    for case_name, input_path in entries:
        rel = f"{case_name}/{input_path.name}"
        case_out_dir = out_dir / case_name
        times = []
        for _ in range(args.repeat):
            try:
                times.append(_run_once(str(jiepp_path), input_path, case_out_dir, args.timeout))
            except (RuntimeError, subprocess.TimeoutExpired):
                return 1
        samples[rel] = times

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    tsv_path = results_dir / f"stats.{timestamp}.tsv"
    header = (
        ["case name"]
        + [f"time_{i + 1} [ms]" for i in range(args.repeat)]
        + ["min [ms]", "median [ms]", "mean [ms]", "stdev [ms]"]
    )
    with open(tsv_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(header) + "\n")
        for rel in sorted(samples, key=_sort_key):
            times = samples[rel]
            stdev = statistics.stdev(times) if len(times) > 1 else 0.0
            row = (
                [rel]
                + [f"{t:.3f}" for t in times]
                + [
                    f"{min(times):.3f}",
                    f"{statistics.median(times):.3f}",
                    f"{statistics.mean(times):.3f}",
                    f"{stdev:.3f}",
                ]
            )
            f.write("\t".join(row) + "\n")

    print(f"Wrote {tsv_path}")
    if not is_release:
        print(f"WARNING: benchmarking a {display_build_type} build; timings are not representative", file=sys.stderr)
    return 0


def cmd_selfcheck(args) -> int:
    work_dir = Path(args.work)
    cases_src = Path(__file__).resolve().parent / "cases"
    cases_out = work_dir / "cases"
    results_dir = work_dir / "results"

    make_rc = cmd_make(argparse.Namespace(
        cases_src=str(cases_src),
        out=str(cases_out),
        mode="greedy2",
        case=list(_SELFCHECK_CASES),
    ))
    if make_rc != 0:
        return make_rc

    run_rc = cmd_run(argparse.Namespace(
        jiepp=args.jiepp,
        cases=str(cases_out),
        results=str(results_dir),
        repeat=1,
        build_type=args.build_type,
        case=list(_SELFCHECK_CASES),
        timeout=60.0,
    ))
    if run_rc != 0:
        return run_rc

    tsv_files = sorted(results_dir.glob("stats.*.tsv"))
    if not tsv_files:
        print("ERROR: selfcheck produced no TSV", file=sys.stderr)
        return 1
    tsv_path = tsv_files[-1]
    with open(tsv_path, "r", encoding="utf-8") as f:
        lines = [line.rstrip("\n") for line in f]
    if len(lines) < 2:
        print(f"ERROR: selfcheck TSV has no data rows: {tsv_path}", file=sys.stderr)
        return 1

    expected_header = ["case name", "time_1 [ms]", "min [ms]", "median [ms]", "mean [ms]", "stdev [ms]"]
    header = lines[0].split("\t")
    if header != expected_header:
        print(f"ERROR: unexpected TSV header {header}", file=sys.stderr)
        return 1

    for line in lines[1:]:
        cells = line.split("\t")
        if len(cells) != len(expected_header):
            print(f"ERROR: malformed TSV row: {line}", file=sys.stderr)
            return 1
        for value in cells[1:]:
            try:
                float(value)
            except ValueError:
                print(f"ERROR: non-numeric TSV cell: {value}", file=sys.stderr)
                return 1

    if len(lines) - 1 != len(_SELFCHECK_CASES):
        print(f"ERROR: selfcheck expected {len(_SELFCHECK_CASES)} rows, got {len(lines) - 1}", file=sys.stderr)
        return 1

    print(f"selfcheck OK: {tsv_path}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="perftest.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    sub = parser.add_subparsers(dest="command", required=True)

    make_p = sub.add_parser("make", help="Generate perf test case inputs")
    make_p.add_argument("--cases-src", required=True, help="Directory containing <name>/<name>.py generators")
    make_p.add_argument("--out", required=True, help="Output directory (cleared first)")
    make_p.add_argument("--mode", choices=("all", "greedy", "greedy2"), default="all",
                         help="all=every ns size, greedy=max(ns) only, greedy2=min(ns) only")
    make_p.add_argument("--case", action="append", default=None, help="Restrict to this case name (repeatable)")
    make_p.set_defaults(func=cmd_make)

    run_p = sub.add_parser("run", help="Run jiepp over generated cases and write a TSV")
    run_p.add_argument("--jiepp", required=True, help="Path to jiepp executable")
    run_p.add_argument("--cases", required=True, help="Directory produced by 'make'")
    run_p.add_argument("--results", required=True, help="Directory to write stats.*.tsv into")
    run_p.add_argument("--repeat", type=int, default=10, help="Samples per case (default: 10)")
    run_p.add_argument("--build-type", default="unknown", help="CMAKE_BUILD_TYPE, for the non-Release warning")
    run_p.add_argument("--case", action="append", default=None, help="Restrict to this case name (repeatable)")
    run_p.add_argument("--timeout", type=float, default=600.0, help="Per-run timeout in seconds (default: 600)")
    run_p.set_defaults(func=cmd_run)

    selfcheck_p = sub.add_parser("selfcheck", help="Fast make+run smoke test (for ctest)")
    selfcheck_p.add_argument("--jiepp", required=True, help="Path to jiepp executable")
    selfcheck_p.add_argument("--work", required=True, help="Scratch directory for cases and results")
    selfcheck_p.add_argument("--build-type", default="unknown", help="CMAKE_BUILD_TYPE, for the non-Release warning")
    selfcheck_p.set_defaults(func=cmd_selfcheck)

    return parser


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
