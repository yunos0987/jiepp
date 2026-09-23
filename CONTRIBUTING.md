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

正確なプリセット名・コマンド・テストの注意点（Release ctest が必須なケース、サンドボックスビルド等）は [AGENTS.md](AGENTS.md) を参照してください。詳細なビルド手順は [README.md](README.md) にもあります。

### VSCode でのワークフロー / VSCode Workflow

CMake Tools 拡張を使用している場合、CMakePresets.json が自動検出されます。ステータスバーからプリセットを選択すると、configure・build・test を GUI から実行できます。開発時は `windows-clang-ninja-debug` (Windows) または `linux-makefiles-debug` (Linux/WSL) をお勧めします。

## Pull Requests

1. Fork the repository and create a feature branch.
2. Make your changes with clear, minimal commits.
3. Ensure all tests pass:
   - Windows: `ctest --preset windows-clang-ninja-debug`
   - Linux: `ctest --preset linux-makefiles-debug`
4. If your change could affect preprocessing behavior, also run a Release ctest at least once (`ctest --preset windows-clang-ninja-release` or `ctest --preset linux-makefiles-release`) — `BoostPreprocessorIntegration` is `#ifdef NDEBUG`-gated and does not run under a `-debug` preset (see [AGENTS.md](AGENTS.md#test)).
5. Open a pull request with a description of your changes.

## Code Style

- C++23.
- Follow existing naming conventions: `snake_case` for functions/variables, `PascalCase` for classes.
- Getter+setter pairs use `get_*`/`set_*`; read-only getters omit the `get_` prefix.

## Adding Tests

Tests live in `tests/` and use Google Test. Group related tests in the appropriate subdirectory (`constfold/`, `core/`, `env/`, `jiepp/`, `loader/`, `macro/`, `util/`). The shared e2e/assertion helpers live directly at `tests/test_helper.hpp` (not in its own subdirectory).

## サンプルの更新 / Updating Samples

`iec_61131-3/samples/` 配下の `.iec` ファイルを変更した場合、`.piec` ファイルを再生成する必要があります。また、ドキュメント内でのサンプル引用と `.piec` 出力が一致することを確認してください。

**推奨手順（スクリプト一括再生成）:**

`tools/pp_iec61131-3_samples.ps1` がサンプル再生成の公式手順です。`iec_61131-3/samples/*.iec` を自動列挙し（`_` 始まりのファイルは除外）、`build/windows-clang-ninja-debug/jiepp.exe`（無ければ release ビルド）を使って各サンプルを `.piec` へ再生成します。`{#syspath 'lib'}` を使う `include` / `syspath` サンプルには `-I iec_61131-3/samples/lib` を自動付与します。

```powershell
# 全サンプルを再生成
powershell .\tools\pp_iec61131-3_samples.ps1

# 特定のサンプルのみ再生成
powershell .\tools\pp_iec61131-3_samples.ps1 example
```

**手動での個別再生成:**

リポジトリルートから jiepp を直接実行することもできます：

```powershell
.\build\windows-clang-ninja-debug\jiepp.exe iec_61131-3/samples/example.iec -o iec_61131-3/samples/example.piec
```

`{#syspath 'lib'}` または `{#sinclude}` を使うサンプルには `-I iec_61131-3/samples/lib` を追加します：

```powershell
.\build\windows-clang-ninja-debug\jiepp.exe iec_61131-3/samples/include.iec -o iec_61131-3/samples/include.piec -I iec_61131-3/samples/lib
```

再生成後は、テストスイートが pass することを確認し、ドキュメント（SPECIFICATION.md 等）での参照が正確であることを確認してください。

## テスト期待値（ゴールデン）の更新 / Updating Test Goldens

上記のサンプル `.piec` とは別に、`tests/jiepp/` の E2E テスト（`run_e2e()`、`tests/jiepp/test_jiepp_command.cpp`）は独自のゴールデンファイル一式を持ちます。意図的な仕様変更でこれらの期待値を更新する必要がある場合は、以下の手順に従ってください。

1. `run_e2e()` は実行のたびに実際の出力を `tests/jiepp/input/<testid>.piec` / `.log` / `.d`（いずれも `.gitignore` 対象で追跡外）へ書き込み、`tests/jiepp/output/<testid>.piec` / `.log` / `.d` の期待値と比較します（`.log` / `.d` は期待値ファイルが存在する場合のみ比較されます）。
2. **必ず Release プリセットでビルドしたバイナリでテストを実行してください**（例: `ctest --preset windows-clang-ninja-release --gtest_filter` 相当のフィルタ、または `jiepp_test.exe --gtest_filter=JieppCommandTest.<TestName>` を Release ビルドで直接実行）。Debug ビルドではメッセージ末尾に `@<ソースファイル>:<行番号>`（ローカルの絶対パスを含む）が付与されます（`src/env/issue_message.cpp` の `#ifndef NDEBUG` 分岐）。テスト内の比較自体はこの付与部分を両辺で除去してから行うため Debug でもテストは通りますが、ここで生成した `tests/jiepp/input/<testid>.log` をそのままコミット用ゴールデンにコピーすると、ビルド環境のローカル絶対パスがリポジトリに残ってしまいます。既存のゴールデンは全て Release ビルドで生成されています。
3. 対象テストを実行後、`tests/jiepp/input/<testid>.piec`（変更があれば `.log` / `.d` も）を `tests/jiepp/output/<testid>.piec` 等と diff し、意図した変更のみであることを確認します。
4. 問題なければ、`tests/jiepp/input/<testid>.*` を対応する `tests/jiepp/output/<testid>.*` へ上書きコピーします。
5. 再度テストを実行し、pass することを確認します。

## 性能測定 / Profiling

`tools/perftest/` は旧 jiecc 由来の性能測定ハーネスです（`tools/perftest/perftest.py`、ケース生成器 `tools/perftest/cases/<name>/<name>.py`）。標準ライブラリのみに依存し、`jiepp.exe` を直接起動します。

- **make フェーズ**: `perftest.py make` がケース生成器を実行し、ビルドディレクトリ配下（ソースツリー外）にテスト入力ファイル一式を生成します（実行のたびに出力先ディレクトリをクリアしてから生成）。
- **run フェーズ**: `perftest.py run` が各ケースに対して `jiepp.exe` を `--repeat` 回実行し、実行時間（プロセス起動オーバーヘッドを含む壁時計時間。旧 jiecc の計測方式を踏襲）を TSV に記録します。
- CMake ターゲット: `perf_make`（ケース生成のみ）と `profiling`（`perf_make` + `jiepp` に依存し、生成とベンチ実行の両方を行う）。どちらも `ALL` ビルドには含まれず、CI でも実行されません（`perftest_smoke` という高速な自己診断テストのみ `ctest -L perf` で実行されます）。
  ```powershell
  cmake --build --preset windows-clang-ninja-release --target profiling
  ```
- 設定用キャッシュ変数: `JIEPP_PERF_REPEAT`（既定 10）、`JIEPP_PERF_MODE`（`all`|`greedy`|`greedy2`、既定 `all`）、`JIEPP_PERF_RESULTS_DIR`（既定 `<build>/perftest/results`）、`JIEPP_PERF_CASES`（セミコロン区切りのケース名リストで対象を絞り込み、既定は空 = 全ケース）。
- TSV 形式: `build/<preset>/perftest/results/stats.<YYYYmmdd_HHMMSS>.tsv`。ヘッダは `case name`, `time_1 [ms]` … `time_N [ms]`, `min [ms]`, `median [ms]`, `mean [ms]`, `stdev [ms]`（小数点以下 3 桁）。
- 計測値は必ず Release プリセットで取得してください。Debug ビルドでも動作しますが `WARNING` が出力され、数値は代表値になりません。

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
