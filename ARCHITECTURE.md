# Architecture / アーキテクチャ

## English Summary

Jiepp is a cross-platform (Windows / Linux+WSL) IEC 61131-3 preprocessor written in C++23, built via CMake presets with vcpkg-managed dependencies (GoogleTest). It handles `#define`, `#include`, `#if`/`#elif`/`#else`/`#endif`, function-like macros with variadic arguments, constant folding, and `#pragma`-style output, across seven tiers — **Loader**, **Macro**, **Core**, **ConstFold**, **Env**, **Util**, **CLI** — detailed below in Japanese. Build and run commands live in [`README.md`](README.md) and [`CONTRIBUTING.md`](CONTRIBUTING.md); this document covers internal structure only.

---

本書は Jiepp の内部構造を開発者向けに説明する。ビルド・実行コマンドは [`README.md`](README.md) と [`CONTRIBUTING.md`](CONTRIBUTING.md) を参照（重複記載しない）。

## プラットフォームとビルド

Jiepp は **Windows** と **Linux / WSL** に対応し、CMake プリセットがコンパイラ・ビルドシステムの差を吸収する。

| プラットフォーム | コンパイラ | ビルドシステム | 主なプリセット |
|---|---|---|---|
| Windows | Clang (`clang++`) | Ninja | `windows-clang-ninja-debug`, `windows-clang-ninja-release`（サンドボックス: `windows-clang-ninja-sandbox-debug`, `windows-clang-ninja-sandbox-release`） |
| Linux / WSL | Clang (`clang++`) | Make | `linux-makefiles-debug`, `linux-makefiles-release`（サンドボックス: `linux-makefiles-sandbox-debug`, `linux-makefiles-sandbox-release`） |
| Linux（サーバー配布） | Clang (`clang++`) | Make | `linux-portable-release`（完全静的リンク）（サンドボックス: `linux-portable-sandbox-release`） |

configure/build/test コマンドは [`README.md`](README.md#ビルドとテスト--build--test) と [`CONTRIBUTING.md`](CONTRIBUTING.md#build--test) を参照。サンドボックスビルド（`*-sandbox-*` プリセット、`JIEPP_SANDBOX=ON`）は「[サンドボックスモード](#サンドボックスモード-jiepp_sandbox)」を参照。

### 依存管理・言語標準・コンパイラ要件

外部依存は [vcpkg](https://github.com/microsoft/vcpkg) で管理する。vcpkg は git サブモジュールで、`vcpkg.json` が依存パッケージ（Google Test）を宣言する。`CMakePresets.json` の `CMAKE_TOOLCHAIN_FILE` が vcpkg ツールチェーンを自動設定する。

- **C++23** (`-std=c++23`)
- Clang 17 以上（Windows / Linux 共通、公式プリセット）。Linux では GCC 13 以上でも可だが、プリセット外の手動設定が必要
- Release ビルドは ThinLTO (`-O3 -flto=thin`) 有効
- `linux-portable-release` は `-static` で libc/libstdc++/libgcc を静的リンク
- 防御的ビルドオプション（`CMakeLists.txt` の `jiepp_harden_target`。`jiepp_lib` / `jiepp` / `jiepp_test` に適用）: Linux は `-fstack-protector-strong`、最適化ビルド（Release 系）のみ `-D_FORTIFY_SOURCE=2`、リンク時 `-z relro -z now`（フル RELRO）。PIE は動的リンクではツールチェーン既定のままで、`linux-portable-release`（完全静的）は非 PIE。Windows は Control Flow Guard（コンパイル `-Xclang -cfguard`、リンク `/guard:cf`）。ASLR（`/DYNAMICBASE`・`/HIGHENTROPYVA`）と DEP（`/NXCOMPAT`）は lld-link の既定で有効

## モジュール構成

実装は **Loader / Macro / Core / ConstFold / Env / Util / CLI** の 7 層。`src/` はこの単位でディレクトリが分かれる。

| ディレクトリ | 主なファイル | 役割 |
|---|---|---|
| `jiepp/` | `main.cpp`, `jiepp.hpp/cpp`, `option.hpp/cpp` | CLI: 引数解析・前処理実行・入出力制御 |
| `env/` | `env.hpp/cpp`, `issue.hpp/cpp`, `issue_message.hpp/cpp`, `issue_codes.def` | `Env` 状態管理と診断出力 |
| `loader/` | `lexer.hpp/cpp`, `token.hpp/cpp`, `hideset.hpp/cpp`, `directive_parser.hpp/cpp`, `directive_token.cpp`, `loader.hpp/cpp` | トークン化・ディレクティブ解析 |
| `core/` | `preprocessor.hpp/cpp`, `directive_handlers.cpp`, `expand.cpp`, `expand_ctrl.cpp`, `expand_subst.cpp`, `builtin_function_macros.cpp`, `line_compaction.hpp/cpp` | ディレクティブ処理・マクロ展開本体（`expand_helpers.hpp`, `preprocessor_internal.hpp`, `builtin_macros.def` も含む） |
| `macro/` | `macro.hpp/cpp`, `macro_builtin.cpp` | `Macro` クラスと組み込みオブジェクトマクロ |
| `constfold/` | `constfold.hpp/cpp`, `constfold_internal.hpp`, `constfold.l`, `constfold.y` | `#if` / `#elif` 式の定数畳み込み |
| `util/` | `path.hpp/cpp`, `text.hpp/cpp`, `iec_61131-3.hpp/cpp`, `stack_guard.hpp/cpp` | `Util` 名前空間の低レベルヘルパー: パス正規化 (`absolute_path()`, `canonical_path()`)、トリム (`ltrim_view()`/`rtrim_view()`/`trim_view()`)、IEC 文字列エンコード (`encode_iec_string()`)、スタック枯渇ガード（`note_stack_base()`/`stack_nearly_exhausted()`。スレッド入口で基点を記録し、`expand()` 入口と `subst_va_opt_content()`（`subst()` の自己再帰の直前）で残量を検査する） |
| `bench/` | `perftest.py`, `cases/<name>/<name>.py` | 性能測定ハーネス（`src/` 外）。CMake `bench` ターゲットが `jiepp` を直接起動して計測（詳細は CONTRIBUTING.md「性能測定 / Benchmark」） |

`Loader::fullpath()` はインクルードパス解決結果を `Util::canonical_path()`（symlink 解決・OS 正規化）で正規化する。この正規化済みパスは `Loader::tokens()` のトークンキャッシュキーと `-M`/`-MM` 依存関係の重複排除に使う同一性キー。`{#pragma once}` の処理済みファイル集合は、ファイルの実体を表す `Util::file_id`（Windows はボリューム + ファイル ID、POSIX は `st_dev` + `st_ino`）をキーにする（`src/env/file_context.cpp`。ファイルでない入力はパス文字列で代用する）。

### env/ の内部構成

`Env`（`env/env.hpp`）はプリプロセッサ全体の状態を保持し、3 つの Mixin 基底クラスの多重継承で構成される。

- `Symtab`（`symtab.hpp/cpp`）— マクロシンボルテーブル。`undef()`/再定義で置き換えられた `Macro` は `retired_` に退避し破棄しない（取得済み `Macro*` を有効に保つため。1 回の実行内の退避でリークではない）
- `FileContext`（`file_context.hpp/cpp`）— インクルードスタック・検索パス・行番号・依存関係追跡・`once_files_`（`{#pragma once}` の処理済みパスセット）。行番号は `LineNo`（`env/lineno.hpp`、64 ビット格納・32 ビットラップ、`Issue::LocationEntry`/`Token::line_pragma`/`LineMarker` も同じ型）
- `Param`（`param.hpp/cpp`, `param_constants.hpp`）— リミット値・プラグマスタイル・トークンキャッシュ・`dd_mode_`（`-dD` フラグ）・入力サイズの上限と読んだバイト数（`max_input_bytes_`・`input_bytes_`、`charge_input_bytes()`、`PP67`。`preprocess()` ごとに `reset_input_bytes()`）・名前付きカウンタの値（`counters_`、`counter_value()`/`counter_slot()`。SPEC §12.10）。カウンタはマクロとは別の名前空間で、`Env` の寿命の間残る（`preprocess()` ごとにはリセットしない。`-include` のファイルと主入力で共有）。値をマクロのオブジェクトではなく `Env` に置くので、`{#undef __counter}` しても値は消えない

診断出力（`Issue`, `IssueMessage`）も `env/` が担当（詳細は「[エラー処理 (Issue)](#エラー処理-issue)」）。

### loader/ の内部構成

字句解析ヘルパー: `lexer_comment.cpp`, `lexer_pragma.cpp`, `lexer_literal.cpp`, `lexer_helpers.hpp/cpp`。`.def` マクロ定義も含む。

- `token.hpp` の `DirectiveToken` はディレクティブをビットマスクで分類する。`MASK_OUTPUT` は自身のハンドラが `ots`（出力トークン列）へ直接書き込むディレクティブ（`{#include}` 等）を示し、マクロ引数収集中に見つかると囲むマクロ展開より先に出力してしまうため拒否される
- 未知のディレクティブ名（`DirectiveToken::name_to_kind()` が解決できないキー）は `directive_token.cpp` の `classify_unknown_directive()` が PP45 (`UNKNOWN_DIRECTIVE`, ERROR) / PP46 (`INVALID_DIRECTIVE_NAME`, ERROR) に振り分けるが、実際に診断を出すのは `core/expand.cpp` の `dispatch_directive()` 1 箇所のみ（有効なコード範囲内のときだけ）
- `{` と `#` の間に空白・改行・コメントがあると通常のプラグマとして扱われる。空白/改行なら `PP28`（`WHITESPACE_BEFORE_DIRECTIVE`）警告（`lexer_pragma.cpp`）
- 閉じていない文字列リテラルは `tokenize()` が `Token::unterminated` を立て（後続リテラルと結合しない）、`PP29`（`UNTERMINATED_STRING_LITERAL`）警告は core がトークンを処理するとき（`expand()` 主ループ・関数マクロ引数収集・`preprocessor_detail::lex_operand()`）に 1 回だけ出してフラグを下ろす。無効なグループとメッセージディレクティブでは出さない
- `preprocessor_detail::lex_operand()`（ディレクティブのオペランド・プラグマ本体・マクロが作ったテキストの再字句解析）は `iec3_relex()` を使う。このとき字句解析器は自分の行カウンタで位置を積まず（`LexOptions::at_current_location`、`lexer_helpers.hpp` の `report_lex_issue()`）、`PP20`・`PP22`・`PP23` をその時点の `Issue` の位置（処理中のディレクティブ/プラグマの位置）に出す。ソースファイルの字句解析（`iec3_tokens()` / `iec3_tokens_from_string()`）は従来どおり自分の行カウンタで位置を積む。両者は `lexer.cpp` の `lex_text()` を共有する
- 入力の読み込み（`loader.hpp/cpp`）: プリプロセッサが読むソースは、すべて上限付きの読み込み関数を通る。ファイル（主入力、`-include`、`{#include}`/`{#sinclude}`）は `Loader::tokens()` のキャッシュに無いときだけ `Loader::read_file()` で読む。OS を問わず `std::FILE`（Windows は `_fsopen(path, "rb", _SH_DENYNO)`、ほかは `std::fopen(path, "rb")`、`std::fread`、`std::ferror`、`std::fclose`）で読む。MSVC STL の `std::ifstream` は読み取りエラーを EOF としか伝えないためで、`_fsopen` は MSVC の `ifstream` が内部で使う `_Fiopen` と同じ呼び出し（狭い文字列のパス、`_SH_DENYNO`）。標準入力（`jiepp.cpp`）とライブラリの `preprocess()` のストリームは `Loader::read_stream()` で `rdbuf()->sgetn` を `sgetn` が 0 を返すまで呼んで読む（ストリームの状態は見ない。try で囲むのは `sgetn` の呼び出しだけで、`std::bad_alloc` はそのまま伝わる）。どちらも 64 KiB（`Loader::READ_CHUNK`）ずつ読んで `Param::charge_input_bytes()` に数え（`--max-input-bytes`、`PP67`）、読み取りエラーは `PP10`（ファイルはパス、標準入力は `<stdin>`、ライブラリのストリームは `<input>`）。標準入力では `std::ferror(stdin)` も調べる（libstdc++ の `std::cin` は stdio と同期していて、読み取りエラーで badbit を立てないため）。ストリームバッファがエラーを入力の終わりとしてしか伝えない場合（呼び出し元が渡した MSVC の `std::ifstream` など）は検出できない。`iec3_tokens(std::istream&)` は上限なしの読み込みのまま残り、これらの読み込み経路では使わない
- `Token`（`token.hpp`）はトークン 1 個を表す構造体（`text`, `kind` 等のフィールド）
- `Token::relexed_pragma` は、プラグマ本体の再字句解析（`core/expand.cpp` の `expand_pragma_token()` → `preprocessor_detail::expand_operand_tokens()`）で字句解析器が作った PRAGMA トークンに立つ印で、テキストは字句解析器が作ったままのものに限る。印の付いたトークンの本体を再字句解析するときは、`read_pragma_body()` が本体の中の生の `{` に対する `PP22` だけを出さない（外側の段で同じ文字を報告済みのため。入れ子の `{` N 個で N(N+1)/2 行だった報告が線形になる）。閉じていないプラグマの `PP22`・閉じ記号の不一致・`PP23` は常に出す。テキストを書き換える `expand_pragma_token()` の出力と `@@` 連結（`glue_tokens()`）の結果では印を下ろす。印を下ろすのは連結に成功したときだけで、連結に失敗した（`PP32`）ときは左辺の字句を書き換えず（印・テキスト・ハイドセットとも）、右辺の字句を別の字句としてその後ろに残すので、どちらの印も変わらない（失敗の条件は異なる字句の種類、または `(` `)` `,` `[` `]` どうし。§4.3）
- Prosser の hide set（`Token::hs`）は `hideset.hpp/cpp` の `HideSet` が実装する。中身はインターン済みマクロ名 id 上の永続的（immutable、構造共有）ビットマップ基数トライで、値はポインタ 1 個のハンドル（コピーは参照カウントの加算のみ。カウントは atomic ではないため、HideSet の値をスレッド間で共有しない）。名前→id のテーブルはプロセス全体で 1 つ・一度登録した名前は解放されず・`Issue` 同様スレッドセーフではない

### core/ の内部構成

`preprocessor.hpp/cpp` が主要 API を提供する。

- `setup()` — 組み込みマクロ・型範囲/マスクマクロ・バージョンマクロを登録した `Env` を作る。CLI (`jiepp_command()`) の呼び出しエントリポイント。`predefine_macros`（`-D`）は `preprocessor_detail::handle_define()` を通して `{#define NAME VALUE}` と同じ検証・再定義警告付きで登録する（§3, R4/C4）。`Issue::initialize()`（CLI 専用、`main()` が呼ぶ）を別途呼ばなくても、`setup()` が `Issue::ensure_location_stack()` で `Issue::loc_stack_` の土台エントリを自分で用意するため、`jiepp_lib` を直接リンクするライブラリ利用者は `setup()` + `preprocess()`/`preprocess_text()` だけで安全に呼べる（B1）
- `apply_undef_option()` — `-U NAME` を `preprocessor_detail::handle_undef()` を通して `{#undef NAME}` と同じ検証で処理する（R4/C4）
- `expand()`（ファイルパス版オーバーロード） — `Loader::fullpath()`/`Loader::tokens()` で対象ファイルを解決・読み込み、トークン版 `expand()` に委譲（利用元は「データフロー」参照）
- `expand()`（トークン列版オーバーロード） — Prosser のアルゴリズムに基づく展開の主ループ。トークン列を受けマクロ展開・ディレクティブ処理して出力トークン列を返す
- `expand_detail::subst()`（`expand_subst.cpp`） — マクロ本体への引数の置換・`@`（文字列化）・`@@`（連結）・`__VA_OPT__` を処理する。本体はコピーせず `std::span` で受け、マクロが定義時に持つ `VaOptSpan` 表（「macro/ の内部構成」）の対応する部分範囲と組で渡す。`__VA_OPT__(…)` の中身は本体の部分範囲として再帰的に置換し、コピーも再走査もしない。可変長引数が空かどうかは `va_args_empty()` が O(1) で判定する（`select_arg()` と同じ結合処理 `join_va_args()` を最初のトークンで打ち切る。区切りの規則は 1 か所だけにある）。`@@` の被演算子かどうかは隣のトークンが `GLUE` かどうかだけで決める（`normalize_glue()` が `GLUE` の隣の空白を必ず除くため。Debug ビルドでは定義時に表明する）。ユーザー定義オブジェクトマクロの本体は参照で渡し、使うたびにコピーしない。2 回以上書かれた仮引数を `@` で文字列化した結果は、呼び出しごとのメモ `ArgExpansionMemo::stringized` に保存し、1 回の呼び出しで 1 回だけ文字列化する（`@__VA_ARGC__` は可変長引数と同じスロットを指すため、キャッシュを引く前に判定する）。`__VA_OPT__` の扱いは仮引数の有無で決まる。仮引数のあるマクロ（可変長引数でないものを含む）では、可変長引数が空でなければ中身を置換し、空か可変長引数がなければ捨てる。仮引数のないマクロ（`formal_params` が空: `F()`・オブジェクト形式マクロ・組み込みの置換）では普通の識別子として出力し、`@` の直後でも `PP31` を出さずに `@` を残す（clang と同じ）
- `preprocess()` — ストリーム/ファイルパス向け簡易 API。CLI は使わず、主に `tests/test_helper.hpp` が利用
- `preprocess_text()` — 文字列を `preprocess()` に通してマクロ展開後の文字列を返す公開 API。空行圧縮（`compact_blank_lines()`）込みで、ディレクティブのオペランド再展開には使わない
- `preprocessor_detail::expand_operand_tokens()`/`expand_operand_text()` — ディレクティブのオペランド（`{#if}`/`{#elif}` の条件式、`{#line}`/`{#include}`/`{#sinclude}`/`{#syspath}` のオペランド、メッセージ本文）と通常のプラグマ本体をマクロ展開する。空行圧縮を一切行わない点が `preprocess_text()` との違いで、圧縮は最終出力に対する後処理であり、オペランドのテキスト自体に行マーカーが混入するのを防ぐ（詳細は「データフロー」）。`{#string}`/`{#wstring}` だけは `expand_operand_tokens()` の結果を `expand_operand_text()`（単純連結）ではなく `expand_detail::stringize_text()`（文字列化の空白圧縮規則、`expand_subst.cpp`、§4.1）に通す
- `dump_macros()` — `env` のユーザー定義マクロを `{#define ...}` 形式で書き出す。`-dM` の実体
- ビルトイン関数マクロ `__error`/`__assert`（SPEC §12.9）、`__counter`/`__counter_peek`/`__counter_set`（SPEC §12.10）と `__nosplit`（SPEC §12.11）— `expand()` の関数マクロ呼び出しの分岐を `FunctionMacro` と `BuiltinFunctionMacro` で共有する（`(` の検索、引数の収集、`[...]` のカンマ保護、引数内ディレクティブ、`PP34` の回復、行番号の計算は同じ）。最後の段だけが異なり、`FunctionMacro` は `subst()`、ビルトインは `invoke_builtin_call()`（`expand()` のフレームを大きくしないよう非インライン）から `invoke()` を呼ぶ。`invoke()` の実装は `builtin_function_macros.cpp`。メッセージは `report_message()`（`directive_handlers.cpp`）を通して報告し、`{#error}` などの `handle_message()` と同じ 1 か所で `Issue::happen()` を呼ぶ。`__nosplit` の `invoke()` は引数を `select_arg()` で `__VA_ARGS__` と同じく結合して `expand()` し、深さ 0（`(...)`/`[...]` の外）の `SEP` を `ANY` に変える（`SuspendDefined` は使わない）
- `expand_detail::eval_cond_value_tokens()`（`expand_ctrl.cpp`）— `__assert` の条件式と `__counter_set` の値をトークンのまま `{#if}` の規則で評価する。テキストに戻して `eval_cond()` に渡すと隠しセット（hide set）が失われ、`{#define N __assert(N, m)}` の `N` が `PP60` まで再展開されるため、トークンのまま `expand()` してから `eval_const_value()` に渡す。`defined` 演算子の導入は `eval_cond_str()` と共通の `DefinedScope` で行う。`{#if}` の条件式の中で入れ子に呼ばれた場合は、外側の条件式の `operand_error` などを保存してから評価し、終わったら元に戻す。メッセージの展開中は `DefinedOperator::suspended` を立て、メッセージ中の `defined` をただの語として扱う

### macro/ の内部構成

`Macro` クラス（`macro.hpp/cpp`）はオブジェクト形式・関数形式マクロの定義を表現する（パラメータ一覧・本体トークン列を保持）。`BuiltinFunctionMacro`（`ErrorBuiltin`/`AssertBuiltin`/`CounterBuiltin`/`CounterSetBuiltin`/`NosplitBuiltin`）は関数マクロと同じ形で呼び出すビルトインで、引数を事前展開せず `invoke()` で結果を計算する。`FunctionMacro` から派生させないので、`define_directive_text()`/`-dM` には `__COUNTER__` と同じく出ない。`macro_builtin.cpp` は `__LINE__`/`__FILE__`/`__COUNTER__`/`__TIMESTAMP__`/`__BASE_FILE__`/`__FILE_NAME__` 等、値が動的に決まる組み込みオブジェクトマクロを実装する。`TimeStampMacro` はファイルシステムにアクセスするため `JIEPP_SANDBOX` ビルドでは空文字列を返すスタブに切り替わる（`BaseFileMacro`/`FileNameMacro` は `__FILE__` と同じ情報しか返さず、ファイルシステムにもアクセスしないため、サンドボックスでも通常ビルドと同じ値を返す）。

`FunctionMacro` と `UserDefinedObjectMacro` は、本体に `__VA_OPT__` がある場合に限り、正規化した本体と同じ長さの `VaOptSpan` 表を定義時に 1 回だけ作る（`compute_va_opt_spans()`、本体の長さに比例する時間）。各要素は自分の位置からの相対オフセットで、次の空白以外のトークン（`next`）、`__VA_OPT__` の `(` に対応する `)`（`close`、形が崩れていれば 0）、その中の入れ子の `__VA_OPT__` の数（`nested`。`)` が閉じていなければ本体の末尾まで数える。`(` がなければ 0）を持つ。相対オフセットなので、外側の `__VA_OPT__` の中身には表の同じ部分範囲をそのまま使える。表は本体から導かれる値なので `equal()`（再定義の同一判定）には含めない。`handle_define()` は関数形式マクロを作った直後にこの表を左から順に読み、誤った `__VA_OPT__`（`(` がない、`nested` が 0 でない、`close` が 0）を最初の 1 件だけ `PP37` で報告して定義全体を捨てる（本体を走査し直さない）。再定義の検査はその後なので、拒否された定義は既存の定義を置き換えず `PP35` も出さない。オブジェクト形式マクロは定義時には調べない。`handle_define()` は同じ関数の中で、`__VA_OPT__` のトークンごとの警告 `PP80` も出す。`ts` のうち警告済みの範囲を `warned` で持ち、マクロ名とパラメータリストの `__VA_OPT__`、可変長引数でないマクロとオブジェクト形式マクロの本体の `__VA_OPT__` を警告する。定義が拒否されるときは、clang がエラーの前後に警告する順序に合わせ、エラーを見つけたトークンまでを先に、行の残りを後に警告する（`PP37` では `malformed_va_opt()` が返す、エラーまでに読んだ `__VA_OPT__` の数で境目を決める）。可変長引数マクロの本体は警告しない。

### jiepp/ の内部構成

- `main.cpp` — `parse_args()` を呼び `jiepp_command()` を実行するエントリポイント
- `jiepp.hpp/cpp` — `int jiepp_command(const JieppOptions& opts)`: `JieppOptions` に基づく前処理の実行
- `option.hpp/cpp` — `JieppOptions` 構造体 + `DepMode` enum + `parse_args()` + `define_macro_option()`
- 出力段階の検査（`jiepp.cpp`）— `-o` と依存ファイルはクローズ後のストリームの状態を、stdout は `std::cout` の状態・`std::fflush(stdout)`・`std::ferror(stdout)` を確かめ、失敗は `PP10`（stdout は `<stdout>`）。中断・エラーありの経路でも stdout に出した後に同じ検査をし、出した内容を再び出すことはない。この実行が書き込み用に開いた出力ファイルは、失敗時にストリームを閉じ（結果は見ない）、`fs::is_regular_file(path, ec)` が真のときだけ `fs::remove(path, ec)` で消す（catch の中で動くので `std::error_code` 版だけを使う）。`--help`/`--version`（`option.cpp`）も stdout を確かめ、書けなければ終了コード 1

`main.cpp` は `main()` 本体に加え次を担う。

- スタック確保: 既定 8 MiB（clang の DesiredStackSize と同じ）、`--recursion-limit N` で N × 8 KiB（最小 1 MiB）。Windows は `CreateThread(..., STACK_SIZE_PARAM_IS_A_RESERVATION)` で予約したワーカースレッドで `jiepp_command()` を実行する。POSIX は `setrlimit(RLIMIT_STACK, ...)` でソフト上限だけを変えてから直接呼ぶ（指定なしのときは 8 MiB 未満の場合だけ引き上げる）
- Windows の標準出力バイナリモード化 (`_setmode(_fileno(stdout), _O_BINARY)`)。CRT 既定のテキストモード（`\n`→`\r\n`）を無効化し、リダイレクト出力を `-o FILE`（バイナリで開く）と一致させる
- Windows のアプリケーションマニフェスト `src/jiepp/jiepp.manifest`（`activeCodePage=UTF-8`）。`CMakeLists.txt` の `if(WIN32)` の中で `target_sources(jiepp PRIVATE …)` に足すと、CMake が lld-link に `/MANIFESTINPUT:` を渡し、リンカーが既定の `asInvoker` のマニフェストと合成して `RT_MANIFEST`（ID 1）として埋め込む（`.rc` は使わない。CMake の規則が `/MANIFEST:EMBED` を付けるので ID が重なる）。Windows 10 version 1903 以降では、これで `main(int, char**)` の `argv` と、`fs::path(std::string)`・`std::ifstream`/`std::ofstream` の名前・`generic_string()`・`fs::current_path()` などの narrow 文字列が UTF-8 になる。`src/` のコードは変えていない。`jiepp_test` には入れない: ライブラリを呼ぶインプロセスのテストは、一般の呼び出し元と同じ ANSI コードページ（ローカルは 932、CI は 1252）で動かす。1903 未満ではマニフェストが無視され、ANSI のままになる。コンソールのコードページは変わらない（表示は `chcp 65001` が要る）。確認は 2 段で行う: `llvm-readobj --coff-resources` で `MANIFEST (ID 24)` の下が `Name: (ID 1)` 1 個、生のバイト列で `activeCodePage`・`>UTF-8</activeCodePage>`・`asInvoker` を調べる（`llvm-readobj` の 16 進ダンプは文字列が行をまたぐので grep できない）
- 最上位の例外処理: `Issue::Exception` は診断済みのため黙って終了コード 1、他の `std::exception`/`catch (...)` は `jiepp: error: PP01: ...` を出して終了コード 1。ワーカースレッド関数 `jiepp_thread_func` も同じ 3 段の catch を持つ（スレッド境界を越えて C++ 例外を伝播できないため）

`JieppOptions`（`src/jiepp/option.hpp`）は CLI オプションを集約した構造体。主なフィールド（詳細はヘッダ参照）:

- 入出力: `input_filepaths`, `output_filepath`, `disppath`（表示用パス。既定値・stdin 時の挙動は [SPECIFICATION.md §13](SPECIFICATION.md#13-cli-リファレンス--cli-reference)）
- マクロ: `define_macros`(`-D`), `undef_macros`(`-U`), `include_filepaths`(`-include`), `syspaths`(`-I`)
- 上限値: `max_include_depth`, `max_expansion_depth`, `max_expansion_steps`(`--max-expansion-steps`、展開の仕事量の上限・`PP64`・ディレクティブなし), `max_include_count`(`--max-include-count`、実行した `{#include}`/`{#sinclude}` の回数の上限・`PP66`・ディレクティブなし。`Param::count_include()` を `handle_include()` が strip_path と NUL 検査の後、`expand()` の前に 1 回呼ぶ。`{#pragma once}` で飛ばすものも数える。カウンタは `preprocess()` ごとに `reset_include_count()`。CLI は Env 1 個で全体を数え、`-include` と `__has_include` は数えない), `max_input_bytes`(`--max-input-bytes`、読み込む入力の合計バイト数の上限・`PP67`・ディレクティブなし・既定 0 で無制限。`-include` を読む前に `Env::set_max_input_bytes()` へ設定する), `error_limit`(`--error-limit`、エラー件数の上限・`PP65`・ディレクティブなし。`Issue::error_limit_` に設定する), `max_if_nesting`, `max_blank_lines`, `recursion_limit`
- 依存関係: `dep_mode`(`-M`/`-MM`), `dep_file`(`-MF`), `dep_target`(`-MT`), `MD`/`MMD`(`-MD`/`-MMD`)
- 出力形式: `pp_output_pragma_style`, `no_line_markers`(`-P`), `dM`(`-dM`), `dD`(`-dD`), `remove_comments`(`-nC`)
- 診断: `silent`, `suppress_warnings`(`-w`), `werror`(`-Werror`), `warn_undef`(`-Wundef`。`jiepp_command()` が `-D`/`-U` を処理する前に `Issue::enable(UNDEFINED_IDENTIFIER)` を呼ぶ), `werror_undef`(`-Werror=undef`。同じ位置で `Issue::add_werror(UNDEFINED_IDENTIFIER)` を呼ぶ。`-Werror=` に続けられるのは `undef` だけ)

### constfold/ の内部構成

- `constfold.hpp` — 公開 API: `eval_const_expr()`（`#if` 式文字列を評価して真偽を 0/1 で返す）と `eval_const_value()`（型付きの値 `cf::ConstValue` と、失敗した最初の診断のコードを返す。`__assert` と `__counter_set` が使う）
- `constfold.cpp` — 両 API が共有する評価の実装。生成された parser/scanner の定義をヘッダ経由で利用。`eval_const_expr()` は `{#if}` 向けの従来の結果（構文エラー・型エラー・空の式は 0、それ以外は値の真偽）を変えない
- `constfold_internal.hpp` — 型定義（`ValueKind`, `BitKind`, `CfValue`, `bit_mask`）と失敗コードの記録 `cf_failure_code`/`cf_note_failure()`。失敗は、スキャナやグラマーのアクションの中で出る診断（不正な基数・桁、リテラルの桁あふれ、浮動小数点リテラルの 0 へのアンダーフロー、浮動小数点値の整数範囲外）でも、その `ISSUE()` の直前に記録する（構文解析そのものは成功するため、戻り値だけでは分からない）。0 による除算・剰余は失敗にしない。評価されない被演算子の深さ `cf_unevaluated_depth`（`constfold.y` で定義、`evaluate()` が評価ごとに 0 へ戻す）と、値に依存する診断を出す関数 `cf_value_diag(code, ctx, failure, loc)`（深さが 0 より大きければ何もせず、失敗の記録もしない。`std::source_location` を `Issue::happen()` へ渡すので Debug の `@file:line` は呼び出し側のアクションを指す）。これを使うのは、値に依存する 4 箇所だけ: `PP54`（`warn_undefined()`）、`constfold.y` の除算・剰余のゼロ除数（`failure=false`）、`cast_scalar_to_int()` の「実数が整数範囲外」（`failure=true`。失敗の記録は診断を出すときだけなので、評価されない被演算子の中では `__assert` / `__counter_set` の失敗にならない）。リテラルの誤り、単独の 2^63、`PP50`、`PP51` / `PP52` の構文エラーは、評価されない被演算子の中でも従来どおり出る（値に依存しない）
- `constfold.l` — Flex ソース → ビルド時に `constfold_scanner.cpp` を生成。型キーワード（`BOOL` など）は `yytext` の綴りのまま `cflval.text` に持つ（`-Wundef` が書いたとおりの綴りを示すため）。`constfold.y` のキャストは大文字にした複製と比較する
- `constfold.y` — Bison ソース → ビルド時に `constfold_parser.cpp` を生成。`\and\` / `\or\` の規則は、演算子と右の被演算子の間に型付きの途中アクション（`<bool>{ … }`）を置く。左の被演算子は右の最初のトークンより前に完全に簡約されるので、そのアクションが左の値から「右は評価されない」を決めて `cf_unevaluated_depth` を上げ、最後の簡約で下げる。投げないので、実数の左（真偽値にできない）は評価される側として扱い、最後の簡約が従来どおり `PP50` を出す。`%expect 0` は変わらない。`CF_IDENT` と、`#` を伴わない `CF_TYPE_KW` が `0` になる箇所で `PP54` を出す

対応する演算子・優先順位は [`SPECIFICATION.md` §6.2](SPECIFICATION.md#62-演算子の優先順位--operator-precedence)、型規則は [§6.3](SPECIFICATION.md#63-型の規則--type-rules) を参照。生成された `constfold_scanner.cpp`/`constfold_parser.cpp` はリポジトリに存在せず、CMakeLists.txt の `FLEX_TARGET`/`BISON_TARGET` がビルド時に生成し `JIEPP_SOURCES` に別の翻訳単位として追加する。

`__has_builtin(NAME)` は `expand()`（`expand.cpp`）の専用の分岐で 1/0 にする。過渡的な `defined` 演算子が有効で `suspended` でない間（`{#if}`/`{#elif}`、`__assert` の条件式、`__counter_set` の値）だけ働き、ハイドセット検査とマクロ検索より前に判定するので、ユーザーの `{#define __has_builtin …}` で置き換えられない。本体は `expand_ctrl.cpp` の `expand_has_builtin()`（`[[gnu::noinline]]`。`expand()` のフレームを大きくしないため）で、被演算子はマクロ展開せずトークンのまま読み、誤りは `defined` と同じく `DefinedOperator::operand_error`/`operand_error_code` に最初の 1 件だけ記録する（`eval_cond()`/`eval_cond_value_tokens()` が条件式全体を失敗として扱う）。値は `has_builtin()`（同ファイル）が決め、「ビルトインか」の判定は `macro.hpp` の `is_builtin_macro()` で、`{#undef}`/`{#define}` の `PP35`（`directive_handlers.cpp`）と共有する。シンボル表に項目がない演算子 `__has_builtin`/`__has_include` の `PP35` は、`directive_handlers.cpp` の `is_builtin_operator_name()` が判定する。

`__has_include` は `expand_ctrl.cpp` で 1/0 に解決される。`{#if}` / `{#elif}` の条件式では `resolve_has_include()` がマクロ展開前に raw 文字列レベルで解決する。ビルトイン関数マクロの `__assert` の条件式と `__counter_set` の値（`eval_cond_value_tokens()`）でも、マクロ展開前のオペランドのトークン列で解決する（`{#if}` と同じで、引数はマクロ展開しない）。`__has_include` トークンの後に（空白・コメントを挟んで）`(` が続くものが候補で、その `(` に対応する `)` までのトークンを連結したテキスト（コメントは空白 1 個）が `resolve_has_include()` と同じ規則でちょうど 1 つの呼び出しなら、`1`/`0` のトークンに置き換える。1 回の呼び出しの読み取りは両方の経路で `parse_has_include_call()` を共有し、`<path>` の終わりは `>` の文字を探す（文字列リテラルや `>=` の中の `>` でも終わる）。トークン列では連結・括弧の対応・キーワードの読み取りをオペランド全体で 1 回ずつ行う（`HasIncludeTokenScan`）。トークン列の経路は左から右へ走査し、解決した呼び出しはその範囲全体を消費する（その `<path>` の中のビルトインの名前はパスの文字）。解決しなかった呼び出しの後では、次のトークンから続ける。まだビルトインである `__assert`・`__error`・`__counter`・`__counter_peek`・`__counter_set` の呼び出しに走査が行き当たると（解決した呼び出しの中は除く）、`nested_builtin_call_end()` がその範囲（`expand()` の引数収集と同じ `(`/`[`・`)`/`]` の深さの規則で、`)` がなければオペランドの終わりまで）を測り、中を走査せずそのまま写す（サンドボックスの `PP62` の検出でも同じ）。呼び出しの引数は内側の呼び出しが自分の規則で扱うためで、名前は `expand()` と同じ認識（ハイドセットになく、`env.lookup()` が元のビルトインで、空白を挟んで `(` が続く）で、`defined`/`__has_builtin` の被演算子は除く。マクロの展開が作る呼び出しは対象外（展開前のトークンだけを見る）。`{#if}` のテキストの経路はこの除外をしない。引用符のパスに不正なエスケープがあると、従来どおり、そのリテラルを含む候補ごとに `PP21` を出す。それを含む候補のテキストだけをもう一度たどるので、そうしたリテラルがなければ処理時間はオペランドの長さに比例する。`"path"` 形式は INCLUDE 検索、`<path>` 形式は SINCLUDE 検索を使用。

## エラー処理 (Issue)

`Issue`（`src/env/issue.hpp/cpp`）が診断出力と例外送出を一元管理する。

- 診断は `Issue::happen(code, context)` で発生させる。`ISSUE(CODE, ...)` は `Issue::happen(Issue::Code::CODE, ...)` の短縮形で、呼び出し位置の `std::source_location` を自動で渡す（Debug ビルドは末尾に `@file:line` を付ける。`issue_message.cpp` の `#ifndef NDEBUG` 分岐）。`[[noreturn]]` な `Issue::fatal()`／`FATAL()` は `happen(Code::FATAL, ...)` の後 `throw std::logic_error("unreachable")` する
- サンドボックスビルド（`JIEPP_SANDBOX`）では、`happen()` が `IssueMessage::message()` に渡す直前に、context とファイルパスから制御文字を取り除き（`Util::strip_control_chars()`、`util/text.hpp`）、続けてファイルパスと、メッセージ系コード（`PP90`〜`PP93`）以外の context の LF・CR を空白に置き換える（`Util::line_breaks_to_spaces()`。規則は [SPECIFICATION.md §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode)）。ナロー文字列のパスが UTF-8 かどうかは `Util::narrow_paths_are_utf8()`（`util/path.hpp`。Linux は常に true、Windows は `GetACP() == CP_UTF8 || ___lc_codepage_func() == CP_UTF8` を呼ぶたびに評価し、保持しない）で決める。UTF-8 でない Windows のプロセス（1903 未満で動く `jiepp.exe`、ANSI コードページのライブラリ呼び出し元）では、位置のファイルパスと `PP10`/`PP11`/`PP12`/`PP14` の context を C1 の組を残すモード（`C1Pairs::Keep`）で取り除く。UTF-8 なら（マニフェストがある `jiepp.exe`、UTF-8 ロケールを設定した呼び出し元）Linux と同じく組も取り除く。メッセージ系（`report_message()`）もオペランドの診断も同じ 1 か所を通る。通常ビルドでは取り除かず、同じ 1 か所で `Util::escape_control_chars()`（`util/text.hpp`）を通して `$XX`（LF は `$n`、CR は `$r`、UTF-8 の C1 の組は `$C2$XX`）で表す（メッセージ系 `PP90`〜`PP93` の context だけ LF・CR を残す）。C1 の組をパスで処理するかどうか（`path_c1`、`path_code`）はサンドボックスと共有する。ビルドごとに違うのは除去かエスケープかだけで、描画点は 1 か所。`main.cpp` の最後の手段の `PP01`（`e.what()`）も通常ビルドでは同じ関数を通す
- 例外送出の有無は `continue_mode_` フラグで 2 通り。既定（`false`、`preprocess()`/`expand()` 等が依存）は `SEVERE` と `blockings_`（既定で全 `ERROR`/`SEVERE`）内の `ERROR`、`-Werror` 格上げの `WARNING` で常に `Issue::Exception` を送出する。継続モード（`true`。`Issue::ContinueMode` RAII ガード、`jiepp_command()` のみ使用）は中断コード（`SEVERE` + `continue_abort_codes_`。`jiepp_command()` は PP10〜14, 60, 61 を渡す）だけが例外を送出し、他は `error_count_` を加算して続行する。`{#ignore}`（`add_ignoring()`）の抑制は `Issue::is_ignored()` で判定し、`SEVERE` と PP10〜14（`issue.cpp` の `is_unignorable_file_code()`）には効かない。PP10〜14 を抑制すると、インクルードできなかったファイルが黙って空になったり `-o`・`-MF` を開けない失敗が終了コード 0 で隠れたりするため（PP60・PP61 は上限を外す用途で抑制可能のまま）。件数・`--silent`/`{#ignore}`・終了コードと出し分けは [SPECIFICATION.md §16](SPECIFICATION.md#16-エラーコード一覧--issue-code-reference) を参照
- エラー件数の上限（`PP65`、`--error-limit`）: 公開 static の `Issue::error_limit_`（0 = 無制限。`initialize()` が 0 に戻し、CLI の `jiepp_command()` だけが `DEFAULT_ERROR_LIMIT`（`env/param_constants.hpp`。通常 1000、サンドボックス 100）か `--error-limit` の値を設定する）。`happen()` は重大度と昇格を決めた後、出力より前に判定する。`continue_mode_` で、`error_limit_ > 0`、`severity == ERROR`（`-Werror` で昇格した警告を含む）、SEVERE でなく中断コードでもなく、`error_count_ >= error_limit_` なら、そのエラーは出さず（数えず）に `PP65` を同じ位置で出して `Exception(TOO_MANY_ERRORS)` を送出する。`PP65` は SEVERE なのでこの判定には入らず、再帰しない（O(1)）。`error_limit_` は `continue_mode_` のときだけ効くので、ライブラリの利用者が `ContinueMode` なしで値を設定しても何も起きない
- 既定で無効のコード: `Issue::is_default_off(code)`（今は `UNDEFINED_IDENTIFIER`＝`PP54` だけ）に当たるコードは、`Issue::enable(code)` が `enabled_` に入れるまで、`happen()` が `{#ignore}` の判定の直後に何もせず戻る（表示も `error_count_` への加算も例外もない）。`initialize()` は `enabled_` を空にし、`ensure_location_stack()`（`setup()` から呼ぶ）は触らない。`Issue::is_enabled(code)` が問い合わせ。`PP54` の文面は `issue_message.cpp` の `PlainTextMessage::message()` の特別扱いで、補足情報を先頭に置く（`'FOO' is not defined, evaluates to 0`）。`-Werror` による昇格は有効なコードにだけ効く
- コード単位の昇格: `Issue::add_werror(code)` が `werror_codes_` に入れ、同時に `enabled_` へも入れる（有効化を兼ねる）。`happen()` は `is_warning(code) && (werror_ || werror_codes_.count(code))` のとき昇格扱いにする（表示は `error`、`error_count_` に加算、継続モードでなければ例外、`-w` では隠れない）。`initialize()` は `werror_codes_` も空にし、`ensure_location_stack()` は触らない。`werror_` / `enabled_` と同じくプロセス全体の状態。CLI で使うのは `-Werror=undef` だけ（`werror_undef`）
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
2. `jiepp_command()` が `Env` を構築し、`Issue::ContinueMode` ガードの下で `-include` の展開とトップレベル入力の `expand()`（ファイルパス版。標準入力ならトークン列版）を呼ぶ。入力の読み込みは上限付き（`Loader::read_file()` / `Loader::read_stream()`。上記「loader/ の内部構成」）
3. `core/expand.cpp` がトークン列を走査する。ディレクティブは `directive_parser` → `directive_handlers` が、`#include`/`#sinclude` は `handle_include()`（`directive_handlers.cpp`。再帰的に `expand()` を呼ぶ）が、`#if` 条件式は `constfold` が（`__has_include` は事前解決）処理する。ディレクティブのオペランド（`#include` のパス、`#if` の条件式文字列、`{#error}`/`{#warning}` のメッセージ文字列）は `preprocessor_detail::expand_operand_text()` が 1 往復再展開する（内部で `expand()` を呼ぶだけで空行圧縮はしない。使用元: `directive_handlers.cpp`/`expand.cpp`/`expand_ctrl.cpp`）。マクロ参照は `Env` のシンボルテーブルで展開する
4. `-include`/トップレベル入力の展開が中断コード（`SEVERE` または PP10〜14, 60, 61）で止まった場合、および中断せず終えても `Issue::error_count_ >= 1` の場合の、標準出力・`-o`・依存ファイルの出し分けと終了コードは [SPECIFICATION.md §16](SPECIFICATION.md#16-エラーコード一覧--issue-code-reference) を参照（`jiepp_command()`/`src/jiepp/jiepp.cpp` が実施。中断時はそれまでの `ots` に空行圧縮をかけたうえで判定する）
5. 中断も未処理エラーもなければ `jiepp::compact_blank_lines()`（`core/line_compaction.cpp`）が `ots` 全体に後処理として 1 回走り、8 行以上連続する空行を行マーカー 1 行に圧縮する（`-P` 指定時は空行を全除去。上限は `--max-blank-lines`、既定 7）。`Token::output_only_lines`（マクロ展開が出力する `$n` 等の改行、`token.hpp`）が立った改行はソースの行として数えない。それにより出力がソースより先に進んだ場合、次のソース行の先頭に再同期用の行マーカーを 1 行挿入する（この再同期は空行圧縮とは独立で、`--max-blank-lines 0` でも行う。`-P` では行わない）
6. 出力トークン列をテキスト化して書き出す（`output_filepath` 指定時はファイルへ、なければ stdout へ）。依存ファイル（`-MF`/`-MD`/`-MMD` 自動命名）は `-o` を開く前に書く。書いた後、ファイルはクローズ、stdout はフラッシュして結果を確かめ、失敗は `PP10`（[SPECIFICATION.md §16](SPECIFICATION.md#16-エラーコード一覧--issue-code-reference)）

## サンドボックスモード (`JIEPP_SANDBOX`)

Web サーバーで信頼できない入力を処理する際のコンパイル時セキュリティモード。無効化ディレクティブ・情報の扱い（`__TIMESTAMP__` を空にする等。渡したパスは隠さない）・ランタイム制限は [`SPECIFICATION.md` §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode)、ビルドコマンドは [`README.md`](README.md#サンドボックスモード--sandbox-mode) を参照。

設計方針:

- **コンパイル時フラグ**: `#ifdef JIEPP_SANDBOX` で分岐。ランタイムオーバーヘッドなし
- ファイルシステムにアクセスする組み込みマクロ `__TIMESTAMP__`（実装は `TimeStampMacro`、`macro/macro_builtin.cpp`）は空文字列を返すスタブに切り替わる。`__BASE_FILE__`/`__FILE_NAME__` は `__FILE__` や診断メッセージと同じパス情報しか返さないため、通常ビルドと同じ値を返す（空にしない）
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
