# Jiepp

[![CI](https://github.com/yunos0987/jiepp/actions/workflows/ci.yml/badge.svg)](https://github.com/yunos0987/jiepp/actions/workflows/ci.yml)

**Jiepp** is a preprocessor for IEC 61131-3. It handles C-style directives (`{#define}`, `{#include}`, `{#if}`, …) plus IEC 61131-3 extensions such as `{#sinclude}` and the `@@` token-paste operator.

**Jiepp** は IEC 61131-3 向けのプリプロセッサです。C 系ディレクティブ（`{#define}`、`{#include}`、`{#if}` 等）に加え、IEC 61131-3 固有の拡張（`{#sinclude}` システムパス限定インクルード、`@@` トークン連結演算子など）を処理します。

## ビルド前提 / Prerequisites

| ツール / Tool | バージョン / Version |
|--------------|---------------------|
| Git | 2.x 以上 |
| CMake | 3.25 以上 |
| Clang (`clang++`) | 17 以上（C++23 対応） |
| Ninja | 1.11 以上 |
| Flex (`win_flex`) | 2.6 以上 |
| Bison (`win_bison`) | 3.8 以上 |

**Flex / Bison** は winget でインストールします:

```powershell
winget install WinFlexBison.win_flex_bison
```

インストール後は **新しい PowerShell セッションを開き直してから** cmake を実行してください（PATH への反映に必要）。cmake は winget のインストール先を自動検出します（PATH 更新なしでも動作）。

## クイックスタート / Quick Start

```powershell
git clone https://github.com/yunos0987/jiepp.git
cd jiepp
git submodule update --init --recursive
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
cmake --workflow --preset windows-clang-ninja-debug

echo '{#define X 42} VAR x : INT := X; END_VAR' | .\build\windows-clang-ninja-debug\jiepp.exe -
# Output (pragma line + expanded text):
# (*{#:0 '<stdin>'}*)
#  VAR x : INT := 42; END_VAR
```

## 対応ディレクティブ / Supported Directives

主なディレクティブの概要です。完全な一覧と詳細な仕様は [`SPECIFICATION.md`](SPECIFICATION.md) を参照してください。

| Directive | Description |
|-----------|-------------|
| `{#define NAME ...}` | マクロ定義 / Define a macro |
| `{#define NAME(args) ...}` | 関数マクロ / Function-like macro |
| `{#undef NAME}` | マクロ定義解除 / Undefine a macro |
| `{#include 'file'}` | ファイルインクルード / Include a file |
| `{#sinclude 'file'}` | システムパス限定インクルード / System-path-only include (like C `#include <...>`) |
| `{#if EXPR}` / `{#elif}` / `{#else}` / `{#endif}` | 条件コンパイル / Conditional compilation |
| `{#ifdef NAME}` / `{#ifndef NAME}` | マクロ存在チェック / Macro existence check |
| `{#error 'msg'}` / `{#warning 'msg'}` | エラー・警告出力 / Error/warning output |
| `@@` | トークン連結 / Token paste operator |
| `@arg` | 文字列化演算子 / Stringize operator |

### ビルトインマクロ / Built-in Macros

主なビルトインマクロです。完全な一覧は [`SPECIFICATION.md` §12](SPECIFICATION.md#12-ビルトインマクロ--built-in-macros) を参照してください。

| Macro | Description |
|-------|-------------|
| `__FILE__` | 現在のファイルパス / Current file path |
| `__LINE__` | 現在の行番号 / Current line number |
| `__COUNTER__` | インクリメントカウンタ / Auto-incrementing counter |
| `__has_include(...)` | インクルード存在チェック / Include file existence check |
| `_JIEPP` | jiepp 識別マクロ / jiepp identification |
| `__VA_ARGS__` | 可変長引数 / Variadic macro arguments |

## ビルドとテスト / Build & Test

configure・build・test を一括で実行します。ビルド成果物は `build/<preset-name>/` ディレクトリに出力されます。

**Debug:**

```powershell
cmake --workflow --preset windows-clang-ninja-debug
```

**Release:**

```powershell
cmake --workflow --preset windows-clang-ninja-release
```

個別に実行する場合:

```powershell
cmake --preset windows-clang-ninja-debug            # configure
cmake --build --preset windows-clang-ninja-debug    # build
ctest --preset windows-clang-ninja-debug            # test
```

`tests/` ディレクトリには loader・macro・core・constfold・env・jiepp・util 系のテストが含まれます。

Release プリセットでは clang の ThinLTO (`-O3 -flto=thin`) が有効になります（Windows は常時有効、Linux では `ld.lld` が見つかった場合のみ有効で、見つからない場合は configure 時に警告が出て ThinLTO なしでビルドされます）。

どのプリセットでも、攻撃への耐性を高める防御的ビルドオプションが付きます。Linux はスタックプロテクタとフル RELRO（Release 系ではさらに `_FORTIFY_SOURCE=2`）、Windows は Control Flow Guard です。詳細は [ARCHITECTURE.md](ARCHITECTURE.md#依存管理言語標準コンパイラ要件) を参照してください。

### 性能測定 / Benchmark

Release ビルドに `bench` ターゲットを用意しています。詳細は [CONTRIBUTING.md の性能測定](CONTRIBUTING.md#性能測定--benchmark) を参照してください。

```powershell
cmake --build --preset windows-clang-ninja-release --target bench
```

### VSCode での開発 / Development in VSCode

VSCode CMake Tools 拡張を使用している場合、`CMakePresets.json` で定義されたプリセットが自動検出されます。ステータスバーから `windows-clang-ninja-debug` を選択して、configure・build・test をワンクリックで実行できます。

## Windows バイナリの実行環境 / Windows Runtime Requirements

Windows でビルドした `jiepp.exe` を実行する場合、以下が必要になる場合があります：

- **Microsoft Visual C++ Redistributable 2015-2022 (x64)**  
  Windows 10 以降では UCRT（Universal C Runtime）がOS に統合済みのため、個別インストール不要な場合がほとんどです。ただし、古い環境や一部カスタム設定では Redistributable が必要になることがあります。  
  不足している場合は、[Microsoft の公式ページ](https://support.microsoft.com/en-us/help/2977003/) から 2015-2022 版をダウンロード・インストールしてください。

- **テスト実行 (`jiepp_test.exe`)**  
  Debug・Release いずれのビルドでも `gtest.dll` / `gtest_main.dll`（ビルド時に実行ファイルと同じディレクトリに配置されます）が必要です。Debug ビルドではさらに Visual C++ の Debug ランタイムが必要になります。通常、開発環境では自動的にインストール済みです。

## 使い方 / Usage

```bash
jiepp [filepath] [options]
```

`filepath` を省略すると標準入力を読みます。`-` も標準入力です。

```bash
jiepp input.iec
jiepp input.iec -o output.iec
jiepp -D NAME=VALUE -I include input.iec
cat input.iec | jiepp -
```

主な CLI オプション / CLI Options:

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `-o PATH` | 出力先ファイル | stdout |
| `-D NAME[=VALUE]` | マクロ定義 | — |
| `-U NAME` | マクロ定義取り消し | — |
| `-I PATH` | インクルードパス追加 | — |
| `-include FILE` | 入力前に強制インクルード | — |
| `-w` | 警告抑制 | off |
| `-Werror` | 警告→エラー昇格 | off |
| `-M` / `-MM` | Makefile 依存関係出力（プリプロセス結果は出力しない。`-MD`/`-MMD` は出力も残す） | off |
| `-nC` | コメント除去 | off |
| `-dM` | マクロ一覧出力 | off |
| `--max-expansion-steps N` | マクロ展開の仕事量の上限（`0` で無制限。超えると `PP64` で中断） | 16777216 |
| `--` | オプション終端 | — |
| `--help` / `-h` | ヘルプ表示 | — |

全オプションの詳細は [`SPECIFICATION.md` §13](SPECIFICATION.md#13-cli-リファレンス--cli-reference) を参照してください。

## サンプル再生成 / Sample Regeneration

`iec_61131-3/samples/` ディレクトリのサンプル `.piec` ファイルは、対応する `.iec` ファイルを jiepp で前処理した結果です。ソースを編集した場合は再生成が必要です。手順は [CONTRIBUTING.md のサンプルの更新](CONTRIBUTING.md#サンプルの更新--updating-samples) を参照してください。

## Linux / WSL でのビルド

### Prerequisites

| ツール / Tool | バージョン / Version |
|--------------|---------------------|
| Git | 2.x 以上 |
| CMake | 3.25 以上 |
| Clang (`clang++`) | 17 以上（C++23 対応）。GCC 13 以上でもビルド可能だがプリセット外 |
| Make | 任意 |
| Flex | 2.6 以上 |
| Bison | 3.8 以上 |
| `curl`, `zip`, `unzip`, `tar` | vcpkg の依存取得に必要 |

WSL (Ubuntu) では次のコマンドで揃えられます:

```bash
sudo apt-get install -y cmake make clang flex bison curl zip unzip tar
```

### ビルド

```bash
git clone https://github.com/yunos0987/jiepp.git
cd jiepp
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
cmake --workflow --preset linux-makefiles-debug
```

| preset 名 | ビルドモード | 備考 |
|----------|------------|------|
| `linux-makefiles-debug` | Debug | |
| `linux-makefiles-release` | Release | ThinLTO |
| `linux-portable-release` | Release | 完全静的リンク・サーバー配布向け |

`linux-makefiles` は `linux-makefiles-debug` の alias です。

### サンドボックスモード / Sandbox Mode

Web サーバー上で信頼できない入力を処理する場合は、サンドボックスビルドを使用します。ファイルシステムアクセスを行うディレクティブが無効化され、`__TIMESTAMP__` は空になります。ただし、渡したファイルパスを隠す機能はありません。`__FILE__` や診断メッセージにはパスがそのまま表示されるため、秘密にしたいパスは jiepp に渡さないでください。

**Linux / WSL:**

```bash
cmake --preset linux-makefiles-sandbox-release
cmake --build --preset linux-makefiles-sandbox-release
```

**Windows (開発・テスト用):**

```powershell
cmake --preset windows-clang-ninja-sandbox-debug
cmake --build --preset windows-clang-ninja-sandbox-debug
ctest --preset windows-clang-ninja-sandbox-debug
```

サンドボックス専用プリセット（`*-sandbox-debug` / `*-sandbox-release`）は独自のビルドディレクトリを持ち、`JIEPP_SANDBOX=ON` を設定します。通常のプリセットは `JIEPP_SANDBOX=OFF` を明示します。通常プリセットに `-DJIEPP_SANDBOX=ON` を付けるとキャッシュに値が残るため、使用しないでください。サンドボックス用 `ctest` は `Sandbox` / `RobustnessTest` のみを実行します（通常テストはサンドボックスビルドでは失敗が想定されます）。

詳細は [`SPECIFICATION.md` §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode) を参照してください。

### サーバーへのデプロイ / Server Deployment

ビルド環境より古い Linux（Ubuntu 20.04 以前等）に配布する場合、GLIBC バージョンの不一致で実行できないことがあります。`linux-portable-release` プリセットは完全静的リンクを行い、GLIBC/GLIBCXX バージョン依存のないバイナリを生成します。

```bash
# サンドボックス + 完全静的リンク
cmake --preset linux-portable-sandbox-release
cmake --build --preset linux-portable-sandbox-release
# 成果物: build/linux-portable-sandbox-release/jiepp
```

サーバー側では追加インストール不要です。生成されたバイナリをそのまま配置して使用できます。ただし、このバイナリを第三者へ配布する場合は、[静的リンクについて](#静的リンクについて--static-linking-notice)の手順が必要です。

## 参考 / References

- [`SPECIFICATION.md`](SPECIFICATION.md) — プリプロセッサの完全な仕様 / Complete preprocessor specification
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — 内部モジュール構成とデータフロー / Internal architecture
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — コントリビューションガイド / Contribution guide
- [`CMakePresets.json`](CMakePresets.json) — 公式 preset と workflow 定義 / Build presets
- [`vcpkg.json`](vcpkg.json) — 依存パッケージ定義 / Dependencies
- [`.gitmodules`](.gitmodules) — `vcpkg` サブモジュール設定
- [`LICENSE`](LICENSE) — MIT ライセンス / MIT License

バグ報告・機能要望は [GitHub Issues](https://github.com/yunos0987/jiepp/issues) で受け付けています。

脆弱性は公開の Issue に書かず、[SECURITY.md](SECURITY.md) の手順で非公開に報告してください。

## ライセンス / License

MIT ライセンス — 詳細は [LICENSE](LICENSE) を参照してください。

### 静的リンクについて / Static Linking Notice

`linux-portable-*` プリセットは `libstdc++`・`libgcc`・`glibc` を静的リンクします。

- **自分のサーバーでビルドして自分で使うだけの場合**、ライセンス上の追加義務は生じません。
- **ビルドしたバイナリを第三者へ配布する場合**、glibc (LGPL v2.1) の条件を満たす必要があります。バイナリに次を添えてください（手順は [`CONTRIBUTING.md`](CONTRIBUTING.md#静的バイナリの配布) を参照）。
  - glibc を LGPL の下で使用している旨の目立つ表示
  - LGPL v2.1 の全文と、glibc の著作権・ライセンス表示
  - 再リンクに必要な資料（対応する glibc のソースと、jiepp のソース・ビルド手順）の入手方法

| ライブラリ | ライセンス | 配布時の扱い |
|-----------|-----------|------|
| glibc (GNU C Library) | LGPL v2.1 | 上記の表示・全文・再リンク資料が必要。MIT であること自体は再リンク要件を満たしません |
| libstdc++, libgcc | GPL v3 + GCC Runtime Library Exception | 例外条項により追加義務なし |
| flex / bison が生成したコード | スケルトンに生成コード向け例外あり | 追加義務なし |

これは法的助言ではありません。配布の形態によっては、各ライセンスの原文を確認してください。

### テストデータ / Test Data

`tests/jiepp/input/boost/` 以下の Boost.Preprocessor ヘッダファイルは [Boost Software License 1.0 (BSL-1.0)](https://www.boost.org/LICENSE_1_0.txt) の下でライセンスされています。テストデータとしてのみ使用しています。ライセンス全文は [`tests/jiepp/input/boost/LICENSE_1_0.txt`](tests/jiepp/input/boost/LICENSE_1_0.txt) に同梱しています。また、これらから生成した `tests/jiepp/output/boost.piec` も同じライセンス (BSL-1.0) の下にあります。