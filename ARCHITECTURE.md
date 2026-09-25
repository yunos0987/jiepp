# Architecture / アーキテクチャ

## English Summary

Jiepp is a cross-platform (Windows / Linux+WSL) IEC 61131-3 preprocessor written in C++23, built via CMake presets with vcpkg-managed dependencies (GoogleTest). It handles `#define`, `#include`, `#if`/`#elif`/`#else`/`#endif`, function-like macros with variadic arguments, constant folding, and `#pragma`-style output, across seven tiers — **Loader**, **Macro**, **Core**, **ConstFold**, **Env**, **Util**, **CLI** — detailed below in Japanese. Build and run commands live in [`README.md`](README.md) and [`CONTRIBUTING.md`](CONTRIBUTING.md); this document covers internal structure only.

---

本書は Jiepp の内部構造を開発者向けに説明する。ビルド・実行コマンドは [`README.md`](README.md) と [`CONTRIBUTING.md`](CONTRIBUTING.md) を参照（重複記載しない）。

## プラットフォームとビルド

Jiepp は **Windows** と **Linux / WSL** に対応し、CMake プリセットがコンパイラ・ビルドシステムの差を吸収する。

| プラットフォーム | コンパイラ | ビルドシステム | 主なプリセット |
|---|---|---|---|
| Windows | Clang (`clang++`) | Ninja | `windows-clang-ninja-debug`, `windows-clang-ninja-release` |
| Linux / WSL | Clang (`clang++`) | Make | `linux-makefiles-debug`, `linux-makefiles-release` |
| Linux（サーバー配布） | Clang (`clang++`) | Make | `linux-portable-release`（完全静的リンク） |

configure/build/test コマンドは [`README.md`](README.md#ビルドとテスト--build--test) と [`CONTRIBUTING.md`](CONTRIBUTING.md#build--test) を参照。サンドボックスビルド (`-DJIEPP_SANDBOX=ON`) は「[サンドボックスモード](#サンドボックスモード-jiepp_sandbox)」を参照。

### 依存管理・言語標準・コンパイラ要件

外部依存は [vcpkg](https://github.com/microsoft/vcpkg) で管理する。vcpkg は git サブモジュールで、`vcpkg.json` が依存パッケージ（Google Test）を宣言する。`CMakePresets.json` の `CMAKE_TOOLCHAIN_FILE` が vcpkg ツールチェーンを自動設定する。

- **C++23** (`-std=c++23`)
- Clang 17 以上（Windows / Linux 共通、公式プリセット）。Linux では GCC 13 以上でも可だが、プリセット外の手動設定が必要
- Release ビルドは ThinLTO (`-O3 -flto=thin`) 有効
- `linux-portable-release` は `-static` で libc/libstdc++/libgcc を静的リンク

## モジュール構成

実装は **Loader / Macro / Core / ConstFold / Env / Util / CLI** の 7 層。`src/` はこの単位でディレクトリが分かれる。

| ディレクトリ | 主なファイル | 役割 |
|---|---|---|
| `jiepp/` | `main.cpp`, `jiepp.hpp/cpp`, `option.hpp/cpp` | CLI: 引数解析・前処理実行・入出力制御 |
| `env/` | `env.hpp/cpp`, `issue.hpp/cpp`, `issue_message.hpp/cpp`, `issue_codes.def` | `Env` 状態管理と診断出力 |
| `loader/` | `lexer.hpp/cpp`, `token.hpp/cpp`, `directive_parser.hpp/cpp`, `directive_token.cpp`, `loader.hpp/cpp` | トークン化・ディレクティブ解析 |
| `core/` | `preprocessor.hpp/cpp`, `directive_handlers.cpp`, `expand.cpp`, `expand_ctrl.cpp`, `expand_subst.cpp`, `line_compaction.hpp/cpp` | ディレクティブ処理・マクロ展開本体（`expand_helpers.hpp`, `preprocessor_internal.hpp`, `builtin_macros.def` も含む） |
| `macro/` | `macro.hpp/cpp`, `macro_builtin.cpp` | `Macro` クラスと組み込みオブジェクトマクロ |
| `constfold/` | `constfold.hpp/cpp`, `constfold_internal.hpp`, `constfold.l`, `constfold.y` | `#if` / `#elif` 式の定数畳み込み |
| `util/` | `path.hpp/cpp`, `text.hpp/cpp`, `iec_61131-3.hpp/cpp` | `Util` 名前空間の低レベルヘルパー: パス正規化 (`absolute_path()`, `canonical_path()`)、トリム (`ltrim_view()`/`rtrim_view()`/`trim_view()`)、IEC 文字列エンコード (`encode_iec_string()`) |
| `tools/perftest/` | `perftest.py`, `cases/<name>/<name>.py` | 性能測定ハーネス（`src/` 外）。CMake `profiling` ターゲットが `jiepp` を直接起動して計測（詳細は CONTRIBUTING.md「性能測定 / Profiling」） |

`Loader::fullpath()` はインクルードパス解決結果を `Util::canonical_path()`（symlink 解決・OS 正規化）で正規化する。この正規化済みパスは `{#pragma once}` の処理済みファイル集合・`Loader::tokens()` のトークンキャッシュキー・`-M`/`-MM` 依存関係の重複排除に共通で使う同一性キー。

### env/ の内部構成

`Env`（`env/env.hpp`）はプリプロセッサ全体の状態を保持し、3 つの Mixin 基底クラスの多重継承で構成される。

- `Symtab`（`symtab.hpp/cpp`）— マクロシンボルテーブル。`undef()`/再定義で置き換えられた `Macro` は `retired_` に退避し破棄しない（取得済み `Macro*` を有効に保つため。1 回の実行内の退避でリークではない）
- `FileContext`（`file_context.hpp/cpp`）— インクルードスタック・検索パス・行番号・依存関係追跡・`once_files_`（`{#pragma once}` の処理済みパスセット）。行番号は `LineNo`（`env/lineno.hpp`、64 ビット格納・32 ビットラップ、`Issue::LocationEntry`/`Token::line_pragma`/`LineMarker` も同じ型）
- `Param`（`param.hpp/cpp`, `param_constants.hpp`）— リミット値・プラグマスタイル・トークンキャッシュ・`dd_mode_`（`-dD` フラグ）

診断出力（`Issue`, `IssueMessage`）も `env/` が担当（詳細は「[エラー処理 (Issue)](#エラー処理-issue)」）。

### loader/ の内部構成

字句解析ヘルパー: `lexer_comment.cpp`, `lexer_pragma.cpp`, `lexer_literal.cpp`, `lexer_helpers.hpp/cpp`。`.def` マクロ定義も含む。

- `token.hpp` の `DirectiveToken` はディレクティブをビットマスクで分類する。`MASK_OUTPUT` は自身のハンドラが `ots`（出力トークン列）へ直接書き込むディレクティブ（`{#include}` 等）を示し、マクロ引数収集中に見つかると囲むマクロ展開より先に出力してしまうため拒否される
- 未知のディレクティブ名（`DirectiveToken::name_to_kind()` が解決できないキー）は `directive_token.cpp` の `classify_unknown_directive()` が PP45 (`UNKNOWN_DIRECTIVE`, ERROR) / PP46 (`INVALID_DIRECTIVE_NAME`, ERROR) に振り分けるが、実際に診断を出すのは `core/expand.cpp` の `dispatch_directive()` 1 箇所のみ（有効なコード範囲内のときだけ）
- `{` と `#` の間に空白・改行・コメントがあると通常のプラグマとして扱われる。空白/改行なら `PP28`（`WHITESPACE_BEFORE_DIRECTIVE`）警告（`lexer_pragma.cpp`）
- `Token`（`token.hpp`）はトークン 1 個を表す構造体（`text`, `kind` 等のフィールド）

### core/ の内部構成

`preprocessor.hpp/cpp` が主要 API を提供する。

- `setup()` — 組み込みマクロ・型範囲/マスクマクロ・バージョンマクロを登録した `Env` を作る。CLI (`jiepp_command()`) の呼び出しエントリポイント
- `expand()`（ファイルパス版オーバーロード） — `Loader::fullpath()`/`Loader::tokens()` で対象ファイルを解決・読み込み、トークン版 `expand()` に委譲（利用元は「データフロー」参照）
- `expand()`（トークン列版オーバーロード） — Prosser のアルゴリズムに基づく展開の主ループ。トークン列を受けマクロ展開・ディレクティブ処理して出力トークン列を返す
- `preprocess()` — ストリーム/ファイルパス向け簡易 API。CLI は使わず、主に `tests/test_helper.hpp` が利用
- `preprocess_text()` — 文字列を `preprocess()` に通してマクロ展開後の文字列を返す。ディレクティブのオペランド再展開に使う（詳細は「データフロー」）
- `dump_macros()` — `env` のユーザー定義マクロを `{#define ...}` 形式で書き出す。`-dM` の実体

### macro/ の内部構成

`Macro` クラス（`macro.hpp/cpp`）はオブジェクト形式・関数形式マクロの定義を表現する（パラメータ一覧・本体トークン列を保持）。`macro_builtin.cpp` は `__LINE__`/`__FILE__`/`__COUNTER__`/`__TIMESTAMP__`/`__BASE_FILE__`/`__FILE_NAME__` 等、値が動的に決まる組み込みオブジェクトマクロを実装する。`TimeStampMacro`/`BaseFileMacro`/`FileNameMacro` はファイルシステムにアクセスするため `JIEPP_SANDBOX` ビルドでは空文字列を返すスタブに切り替わる。

### jiepp/ の内部構成

- `main.cpp` — `parse_args()` を呼び `jiepp_command()` を実行するエントリポイント
- `jiepp.hpp/cpp` — `int jiepp_command(const JieppOptions& opts)`: `JieppOptions` に基づく前処理の実行
- `option.hpp/cpp` — `JieppOptions` 構造体 + `DepMode` enum + `parse_args()` + `define_macro_option()`

`main.cpp` は `main()` 本体に加え次を担う。

- `--recursion-limit` のスタック確保: Windows は `CreateThread()` にサイズを渡しワーカースレッドで `jiepp_command()` を実行、POSIX は `setrlimit(RLIMIT_STACK, ...)` 後に直接呼ぶ
- Windows の標準出力バイナリモード化 (`_setmode(_fileno(stdout), _O_BINARY)`)。CRT 既定のテキストモード（`\n`→`\r\n`）を無効化し、リダイレクト出力を `-o FILE`（バイナリで開く）と一致させる
- 最上位の例外処理: `Issue::Exception` は診断済みのため黙って終了コード 1、他の `std::exception`/`catch (...)` は `jiepp: error: PP01: ...` を出して終了コード 1。ワーカースレッド関数 `jiepp_thread_func` も同じ 3 段の catch を持つ（スレッド境界を越えて C++ 例外を伝播できないため）

`JieppOptions`（`src/jiepp/option.hpp`）は CLI オプションを集約した構造体。主なフィールド（詳細はヘッダ参照）:

- 入出力: `input_filepaths`, `output_filepath`, `disppath`（表示用パス。既定値・stdin 時の挙動は [SPECIFICATION.md §13](SPECIFICATION.md#13-cli-リファレンス--cli-reference)）
- マクロ: `define_macros`(`-D`), `undef_macros`(`-U`), `include_filepaths`(`-include`), `syspaths`(`-I`)
- 上限値: `max_include_depth`, `max_expansion_depth`, `max_if_nesting`, `max_blank_lines`, `recursion_limit`
- 依存関係: `dep_mode`(`-M`/`-MM`), `dep_file`(`-MF`), `dep_target`(`-MT`), `MD`/`MMD`(`-MD`/`-MMD`)
- 出力形式: `pp_output_pragma_style`, `no_line_markers`(`-P`), `dM`(`-dM`), `dD`(`-dD`), `remove_comments`(`-nC`)
- 診断: `silent`, `suppress_warnings`(`-w`), `werror`(`-Werror`)

### constfold/ の内部構成

- `constfold.hpp` — 公開 API: `eval_const_expr()`（`#if` 式文字列を評価して int64_t 値を返す）
- `constfold.cpp` — `eval_const_expr` 実装。生成された parser/scanner の定義をヘッダ経由で利用
- `constfold_internal.hpp` — 型定義（`ValueKind`, `BitKind`, `CfValue`, `bit_mask`）
- `constfold.l` — Flex ソース → ビルド時に `constfold_scanner.cpp` を生成
- `constfold.y` — Bison ソース → ビルド時に `constfold_parser.cpp` を生成

対応する演算子・優先順位は [`SPECIFICATION.md` §6.2](SPECIFICATION.md#62-演算子の優先順位--operator-precedence)、型規則は [§6.3](SPECIFICATION.md#63-型の規則--type-rules) を参照。生成された `constfold_scanner.cpp`/`constfold_parser.cpp` はリポジトリに存在せず、CMakeLists.txt の `FLEX_TARGET`/`BISON_TARGET` がビルド時に生成し `JIEPP_SOURCES` に別の翻訳単位として追加する。

`__has_include` は `expand_ctrl.cpp` の `resolve_has_include()` でマクロ展開前に raw 文字列レベルで 1/0 に解決される。`"path"` 形式は INCLUDE 検索、`<path>` 形式は SINCLUDE 検索を使用。

## エラー処理 (Issue)

`Issue`（`src/env/issue.hpp/cpp`）が診断出力と例外送出を一元管理する。

- 診断は `Issue::happen(code, context)` で発生させる。`ISSUE(CODE, ...)` は `Issue::happen(Issue::Code::CODE, ...)` の短縮形で、呼び出し位置の `std::source_location` を自動で渡す（Debug ビルドは末尾に `@file:line` を付ける。`issue_message.cpp` の `#ifndef NDEBUG` 分岐）。`[[noreturn]]` な `Issue::fatal()`／`FATAL()` は `happen(Code::FATAL, ...)` の後 `throw std::logic_error("unreachable")` する
- 例外送出の有無は `continue_mode_` フラグで 2 通り。既定（`false`、`preprocess()`/`expand()` 等が依存）は `SEVERE` と `blockings_`（既定で全 `ERROR`/`SEVERE`）内の `ERROR`、`-Werror` 格上げの `WARNING` で常に `Issue::Exception` を送出する。継続モード（`true`。`Issue::ContinueMode` RAII ガード、`jiepp_command()` のみ使用）は中断コード（`SEVERE` + `continue_abort_codes_`。`jiepp_command()` は PP10〜14, 60, 61 を渡す）だけが例外を送出し、他は `error_count_` を加算して続行する。件数・`--silent`/`{#ignore}`・終了コードと出し分けは [SPECIFICATION.md §16](SPECIFICATION.md#16-エラーコード一覧--issue-code-reference) を参照
- `Issue::CliMode` という別の RAII ガードは、ソースファイルがまだスタックに積まれていない CLI 段階（`main()`／`parse_args()`／`jiepp_command()` 冒頭）の診断を `Issue::CLI_LOCATION`（`"jiepp"`）で「`jiepp: error: PPxx: message`」形式（`line.column` なし）に出す。文字列入力 API の `"<unknown location>:N.0"` 形式とは別物で両者は独立して共存する
- ファイル処理に入る際の `Issue::loc_stack_`／`FileContext` の include スタックへの push/pop は、`Issue::LineGuard` と `FileContext::FileScope` の 2 つの RAII ガードで対になっている（`core/expand.cpp` の `expand()` ファイルインクルード版・`jiepp/jiepp.cpp` の標準入力分岐）。処理中に例外（ライブラリモードで `Issue::Exception` が送出される場合）が飛んでも両スタックは必ず呼び出し前の深さに戻る

エラーコード定義（Issue Code X-macro）: `src/env/issue_codes.def` に全エラーコードを 4 フィールドの X-macro で定義する。

```cpp
// JIEPP_ISSUE_CODE(name, id, severity, message)
JIEPP_ISSUE_CODE(FILE_NOT_FOUND, 11, ERROR, "No such file or directory")
```

`Issue::Code`/`Issue::Severity` enum を生成する。ID は `PP` + 2桁ゼロパディング（例: `PP11`）。番号帯によるカテゴリ分け・コード一覧は [SPECIFICATION.md §16](SPECIFICATION.md#16-エラーコード一覧--issue-code-reference) を参照。

## 組み込みマクロの責務分離

GCC/cpp 同様、組み込みマクロの定義責務をプリプロセッサとコンパイラで分離する。

| 責務 | マクロ例 | 定義元 | GCC での対応 |
|---|---|---|---|
| プリプロセッサ識別 | `_JIEPP` | jiepp 自身 (`builtin_macros.def`) | `__GNUC__` — cpp 自身が定義 |
| プリプロセッサのバージョン | `_JIEPP_VER`, `_JIEPP_FULL_VER`, `_JIEPP_VERSION` | jiepp 自身（`core/preprocessor.cpp` の `setup()` が CMake のプロジェクトバージョンから生成） | — |
| ターゲット型情報 | `__SINT_MIN__`, `__UINT_MAX__` 等 | jiepp 自身 (`builtin_macros.def`) | `__SIZEOF_INT__` 等 — cpp が定義 |
| コンパイラ識別 | `_JIECC`, `_JIECC_VER` 等 | jiecc が `-D` で渡す | gcc が cpp を呼ぶ際に渡すフラグ |
| ベンダー固有 | `_OMRON` 等 | 呼び出し元が `-D` で渡す | ユーザー定義 (`-D`) |

**原則**: jiepp は「自分が何者か」だけを知り、「誰に呼ばれたか」は呼び出し元が教える。

## データフロー

入力（ファイル or stdin）→ `lexer` → `core::expand()`（ディレクティブ処理・マクロ展開）→ `line_compaction`（空行圧縮）→ 出力（ファイル or stdout）。処理の流れ:

1. `jiepp/main.cpp` が `parse_args()` で引数を解析して `jiepp_command()` を呼ぶ（スタック確保・標準出力のバイナリモード化は「jiepp/」節参照）
2. `jiepp_command()` が `Env` を構築し、`Issue::ContinueMode` ガードの下で `-include` の展開とトップレベル入力の `expand()`（ファイルパス版。標準入力ならトークン列版）を呼ぶ
3. `core/expand.cpp` がトークン列を走査する。ディレクティブは `directive_parser` → `directive_handlers` が、`#include`/`#sinclude` は `handle_include()`（`directive_handlers.cpp`。再帰的に `expand()` を呼ぶ）が、`#if` 条件式は `constfold` が（`__has_include` は事前解決）処理する。ディレクティブのオペランド（`#include` のパス、`#if` の条件式文字列、`{#error}`/`{#warning}` のメッセージ文字列）は `preprocess_text()` が 1 往復再展開する（内部で `expand()` を呼ぶ。使用元: `directive_handlers.cpp`/`expand.cpp`/`expand_ctrl.cpp`）。マクロ参照は `Env` のシンボルテーブルで展開する
4. `-include`/トップレベル入力の展開が中断コード（`SEVERE` または PP10〜14, 60, 61）で止まった場合、および中断せず終えても `Issue::error_count_ >= 1` の場合の、標準出力・`-o`・依存ファイルの出し分けと終了コードは [SPECIFICATION.md §16](SPECIFICATION.md#16-エラーコード一覧--issue-code-reference) を参照（`jiepp_command()`/`src/jiepp/jiepp.cpp` が実施。中断時はそれまでの `ots` に空行圧縮をかけたうえで判定する）
5. 中断も未処理エラーもなければ `jiepp::compact_blank_lines()`（`core/line_compaction.cpp`）が `ots` 全体に後処理として 1 回走り、8 行以上連続する空行を行マーカー 1 行に圧縮する（`-P` 指定時は空行を全除去。上限は `--max-blank-lines`、既定 7）
6. 出力トークン列をテキスト化して書き出す（`output_filepath` 指定時はファイルへ、なければ stdout へ）。依存ファイル（`-MF`/`-MD`/`-MMD` 自動命名）は `-o` を開く前に書く

## サンドボックスモード (`JIEPP_SANDBOX`)

Web サーバーで信頼できない入力を処理する際のコンパイル時セキュリティモード。無効化ディレクティブ・情報漏洩防止・ランタイム制限は [`SPECIFICATION.md` §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode)、ビルドコマンドは [`README.md`](README.md#サンドボックスモード--sandbox-mode) を参照。

設計方針:

- **コンパイル時フラグ**: `#ifdef JIEPP_SANDBOX` で分岐。ランタイムオーバーヘッドなし
- ファイルシステムにアクセスする組み込みマクロ（`__TIMESTAMP__`/`__BASE_FILE__`/`__FILE_NAME__`。実装は `TimeStampMacro`/`BaseFileMacro`/`FileNameMacro`、`macro/macro_builtin.cpp`）は空文字列を返すスタブに切り替わる
- 運用要件（process-per-request 必須の理由・入出力サイズ制限の管理元）は [SPECIFICATION.md §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode) の「運用要件」参照

### テスト構成

`tests/` 配下は `src/` と同じモジュール構成:

| ディレクトリ | 内容 |
|---|---|
| `loader/` | 字句解析・トークン化・ディレクティブ解析のテスト |
| `macro/` | マクロ定義・展開のテスト |
| `core/` | プリプロセッサ統合テスト・サンドボックステスト |
| `constfold/` | 定数式評価のテスト |
| `env/` | 環境設定・エラー処理・ロバスト性制限のテスト |
| `jiepp/` | CLI 入出力・エンドツーエンドテスト |
| `util/` | `Util` 名前空間（トリム・パス正規化・IEC 文字列エンコード等）のテスト |

共有ヘルパー（`run_e2e()` 等の e2e アサーション補助）は `tests/test_helper.hpp`（専用サブディレクトリなし）。

- `tests/env/test_robustness.cpp` — 常時有効なロバスト性制限のテスト
- `tests/core/test_sandbox.cpp` — サンドボックス固有テスト（`#ifdef JIEPP_SANDBOX` で囲まれ、通常ビルドではスキップ）
