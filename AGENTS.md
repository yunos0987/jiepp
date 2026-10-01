# jiepp Build Rules for AI Agents

## Build

- Always configure with an explicit preset from `CMakePresets.json`: `windows-clang-ninja-debug` / `windows-clang-ninja-release` on Windows; `linux-makefiles-debug` / `linux-makefiles-release` on Linux/WSL; `linux-portable-release` for a fully static, GLIBC/GLIBCXX-version-independent server build. `windows-clang-ninja` and `linux-makefiles` are aliases for the two `-debug` presets (mentioned here once; do not treat them as separate build configurations elsewhere).
- Direct `cmake ..` (no `--preset`) is prohibited. On Windows this only emits a CMake `message(WARNING ...)` — not a fatal error — but it silently drops the preset's `CMAKE_TOOLCHAIN_FILE` (vcpkg) and `CMAKE_CXX_COMPILER=clang++` settings, so the configure can still "succeed" while linking against the wrong toolchain or failing to find GoogleTest. When delegating a build task, **always explicitly state** the preset in your prompt — omitting it causes the agent to fall back to `cmake ..`.
- `cmake --workflow --preset <name>` runs configure + build + test in one step. To run the steps individually: `cmake --preset <name>`, then `cmake --build --preset <name>`, then `ctest --preset <name>`.
- Sandbox mode (web server deployment; disables filesystem-access directives): append `-DJIEPP_SANDBOX=ON` to any preset's configure step, e.g. `cmake --preset windows-clang-ninja-debug -DJIEPP_SANDBOX=ON && cmake --build --preset windows-clang-ninja-debug`.

## Test

- Run with `ctest --preset <name>`, using the same preset name you configured with.
- `BoostPreprocessorIntegration` (`tests/jiepp/test_jiepp_command.cpp`) is wrapped in `#ifdef NDEBUG`, so it is a no-op under any `-debug` preset (it passes trivially without running its body). Run a `*-release` ctest (`windows-clang-ninja-release`, `linux-makefiles-release`, or `linux-portable-release`) at least once before finishing any task that could affect preprocessing behavior — a Debug-only ctest pass does not exercise this test.
- Sandbox-only tests (`tests/core/test_sandbox.cpp`, `#ifdef JIEPP_SANDBOX`) only run in a `-DJIEPP_SANDBOX=ON` build. Conversely, many ordinary tests are *expected* to fail in a sandbox build, since filesystem-access directives are intentionally disabled there — do not treat those failures as regressions when testing sandbox mode specifically.
- The `tests/jiepp/` end-to-end tests write actual output under `build/<preset>/e2e-actual/`, not into the source tree, so a Debug and a Release ctest no longer race on the same files and can run concurrently.

## Benchmarking

- `cmake --build --preset <release-preset> --target bench` builds `jiepp`, generates perf test case inputs from `bench/cases/*/*.py`, and runs them, writing a TSV under `build/<preset>/perftest/results/stats.<timestamp>.tsv`.
- Configure-time cache vars: `JIEPP_PERF_REPEAT` (samples per case, default 10), `JIEPP_PERF_MODE` (`all`|`greedy`|`greedy2`, default `all`), `JIEPP_PERF_RESULTS_DIR` (default `<build>/perftest/results`), `JIEPP_PERF_CASES` (semicolon-separated case names to restrict to, default empty = all cases).
- `ctest --preset <name> -L perf` runs only `perftest_smoke`, a fast 2-case selfcheck; it is not part of a plain `ctest` run's normal case count and is not part of `ALL`/CI otherwise.
- Benchmarking a Debug build still runs but prints a `WARNING` and is not representative of real performance — always use a Release preset for numbers you intend to report.

## Code Rules

- Do NOT remove `[[noreturn]]` attributes. They are required to inform the compiler that control never returns. If an "unused attribute" warning appears, keep the attribute and add `throw std::logic_error("unreachable");` immediately after the call.
- After adding `FLEX_TARGET` / `BISON_TARGET` in CMakeLists.txt, always add `ADD_FLEX_BISON_DEPENDENCY(SCANNER PARSER)`. Omitting it causes the scanner to reference a stale parser header and breaks the build.
- Line endings follow `.gitattributes`: most source/build/doc files are LF; `*.bat`/`*.cmd` are CRLF; `*.iec`/`*.piec`/`*.jiec` sample and golden files are `-text` (intentionally exempt) — do not "fix" their line endings.

## Sample and Test-Golden Regeneration

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the `iec_61131-3/samples/*.piec` sample regeneration procedure and the `tests/jiepp/output/` test-golden update procedure — do not duplicate those commands here. For preprocessor behavior questions, see [`SPECIFICATION.md`](SPECIFICATION.md); for internals, see [`ARCHITECTURE.md`](ARCHITECTURE.md).
