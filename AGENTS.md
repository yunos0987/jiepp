# jiepp Build Rules for AI Agents

## Build

- Always use `cmake --preset windows-clang-ninja-debug` (or `windows-clang-ninja-release`) to build on Windows. Direct `cmake ..` is prohibited (the `CMakeLists.txt` has a preset enforcement check at the top).
- On Linux/WSL, use `cmake --preset linux-makefiles` (or `linux-makefiles-release`). The preset specifies `clang++` as the compiler.
- For sandbox mode (web server deployment): append `-DJIEPP_SANDBOX=ON` to any preset:
  ```powershell
  # Windows sandbox (development/testing)
  cmake --preset windows-clang-ninja-debug -DJIEPP_SANDBOX=ON && cmake --build --preset windows-clang-ninja-debug
  ```
  ```bash
  # Linux sandbox + portable static binary
  cmake --preset linux-portable-release -DJIEPP_SANDBOX=ON && cmake --build --preset linux-portable-release
  ```
- For server deployment (portable binary with no GLIBC version requirements), use `linux-portable-release`:
  - Plain: `cmake --workflow --preset linux-portable-release`
  - With sandbox: `cmake --preset linux-portable-release -DJIEPP_SANDBOX=ON && cmake --build --preset linux-portable-release`
- Run tests with: `ctest --preset windows-clang-ninja-debug` (Windows) or `ctest --preset linux-makefiles` (Linux)
- When delegating a build task, **always explicitly state** the preset in your prompt — omitting it causes the agent to fall back to `cmake ..`, which may trigger a FATAL eror or use the wrong compiler.

## Code Rules

- Do NOT remove `[[noreturn]]` attributes. They are required to inform the compiler that control never returns. If an "unused attribute" warning appears, keep the attribute and add `throw std::logic_error("unreachable");` immediately after the call.
- After adding `FLEX_TARGET` / `BISON_TARGET` in CMakeLists.txt, always add `ADD_FLEX_BISON_DEPENDENCY(SCANNER PARSER)`. Omitting it causes the scanner to reference a stale parser header and breaks the build.

## Sample Regeneration

- Regenerate sample `piec` files from the **project root** directory:
  ```powershell
  .\build\windows-clang-ninja-debug\jiepp.exe iec_61131-3/samples/xxx.iec -o iec_61131-3/samples/xxx.piec
  ```
- For samples that use `{#syspath 'lib'}` or `{#sinclude}`, the `-I iec_61131-3/samples/lib` flag is **required** to resolve include paths:
  ```powershell
  .\build\windows-clang-ninja-debug\jiepp.exe iec_61131-3/samples/include.iec -o iec_61131-3/samples/include.piec -I iec_61131-3/samples/lib
  ```
