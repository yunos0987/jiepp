# Contributing to Jiepp

Thank you for your interest in contributing!

## Build & Test

Ensure you have all [prerequisites](README.md#ビルド前提--prerequisites) installed, including **Flex** and **Bison** (see README for installation commands).

**Windows (PowerShell):**

```powershell
git clone https://github.com/yunos0987/jiepp.git
cd jiepp
git submodule update --init --recursive
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
cmake --workflow --preset windows-clang-ninja-debug
```

**Linux / WSL:**

```bash
git clone https://github.com/yunos0987/jiepp.git
cd jiepp
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --workflow --preset linux-makefiles-debug
```

See [README.md](README.md) for detailed build instructions.

### VSCode でのワークフロー / VSCode Workflow

CMake Tools 拡張を使用している場合、CMakePresets.json が自動検出されます。ステータスバーからプリセットを選択すると、configure・build・test を GUI から実行できます。開発時は `windows-clang-ninja-debug` (Windows) または `linux-makefiles-debug` (Linux/WSL) をお勧めします。

## Pull Requests

1. Fork the repository and create a feature branch.
2. Make your changes with clear, minimal commits.
3. Ensure all tests pass:
   - Windows: `ctest --preset windows-clang-ninja-debug`
   - Linux: `ctest --preset linux-makefiles-debug`
4. Open a pull request with a description of your changes.

## Code Style

- C++23.
- Follow existing naming conventions: `snake_case` for functions/variables, `PascalCase` for classes.
- Getter+setter pairs use `get_*`/`set_*`; read-only getters omit the `get_` prefix.

## Adding Tests

Tests live in `tests/` and use Google Test. Group related tests in the appropriate subdirectory (`loader/`, `macro/`, `core/`, `constfold/`, `env/`, `jiepp/`, `support/`).

## サンプルの更新 / Updating Samples

`iec_61131-3/samples/` 配下の `.iec` ファイルを変更した場合、`.piec` ファイルを再生成する必要があります。また、ドキュメント内でのサンプル引用と `.piec` 出力が一致することを確認してください。

**再生成手順:**

1. リポジトリルートから jiepp を実行：
   ```powershell
   .\build\windows-clang-ninja-debug\jiepp.exe iec_61131-3/samples/example.iec -o iec_61131-3/samples/example.piec
   ```

2. `-I iec_61131-3/samples/lib` が必要な場合は追加：
   ```powershell
   .\build\windows-clang-ninja-debug\jiepp.exe iec_61131-3/samples/include.iec -o iec_61131-3/samples/include.piec -I iec_61131-3/samples/lib
   ```

3. 変更後、テストスイートが pass することを確認し、ドキュメント（SPECIFICATION.md 等）での参照が正確であることを確認してください。

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
