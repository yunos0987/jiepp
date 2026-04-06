# Jiepp 仕様書 / Jiepp Specification

本文書は Jiepp プリプロセッサの完全な仕様をユーザー向けに記述します。

This document provides the complete specification of the Jiepp preprocessor for end users.

---

## 1. 概要 / Overview

**Jiepp** は IEC 61131-3 向けのプリプロセッサです。C プリプロセッサ (cpp) と同等のマクロ展開・条件コンパイル・ファイルインクルードを IEC 61131-3 テキストに対して提供します。

Jiepp is a preprocessor for IEC 61131-3. It provides C-preprocessor-equivalent macro expansion, conditional compilation, and file inclusion for IEC 61131-3 text.

### 入力 / Input

- エンコーディング: UTF-8 (BOM 有り・無し双方対応)
- 改行コード: LF, CRLF, CR
- ファイル拡張子: 任意（推奨: `.iec`）

### 出力 / Output

- プリプロセス済みの IEC 61131-3 テキストを stdout またはファイルに出力します
- プラグマ行: 行番号追跡用の `(*{#:N file}*)` (annotated) または `{#:N file}` (standard) 形式で挿入されます

---

## 2. ディレクティブ構文 / Directive Syntax

### 基本形 / Basic Form

```
{#directive_name arguments}
```

- `{` と `#` の間にスペースは **許可されません**
- 引数は閉じ `}` まで続きます
- 複数行に跨ることが可能です（閉じ `}` が次の行にあってもよい）

### 複数行ディレクティブ / Multi-line Directives

ディレクティブは閉じ `}` が見つかるまで複数行に跨ることができます。特別な行継続文字は不要です。

```
{#define LONG_MACRO(a, b, c)
    a + b + c}
```

IEC 61131-3 の `$` エスケープ文字を行末に置いて次の行と連結することもできます:

```
{#define LONG_MACRO(a, b, c) $
    a + b + c}
```

### ディレクティブ名のエイリアス / Directive Name Aliases

一部のディレクティブはスネークケースとケバブケースの両方をサポートします:

| 正式名 | エイリアス |
|--------|----------|
| `set_line` | `set-line`, `line` |
| `max_include_depth` | `max-include-depth` |
| `max_expansion_depth` | `max-expansion-depth` |
| `max_if_nesting` | `max-if-nesting` |
| `pp_output_pragma_style` | `pp-output-pragma-style` |

### コメント / Comments

Jiepp は以下のコメント形式を認識し保持します:

| 形式 | 記述 |
|------|------|
| `(* ... *)` | IEC 61131-3 ブロックコメント（ネスト不可） |
| `/* ... */` | IEC 61131-3 ブロックコメント（ネスト不可） |
| `// ...` | IEC 61131-3 行コメント |

`--remove-comments` / `-nC` オプション指定時はコメントが除去されます。

---

## 3. マクロ定義 / Macro Definitions

### 3.1 オブジェクトマクロ / Object Macros

```
{#define NAME replacement}
```

`NAME` が出現するとき、`replacement` のトークン列に置換されます。

```
{#define PI 3.14159}
{#define GRAVITY 9.81}
area := PI * r * r;         (* → 3.14159 * r * r; *)
```

### 3.2 関数マクロ / Function Macros

```
{#define NAME(params) replacement}
```

- `NAME` と `(` の間にスペースがあると、オブジェクトマクロとして扱われます
- パラメータはカンマ区切り

```
{#define DOUBLE(x) (x) + (x)}
{#define ADD(a, b) (a) + (b)}

result := DOUBLE(5);     (* → (5) + (5) *)
result := ADD(x, y);     (* → (x) + (y) *)
```

### 3.3 可変長引数 / Variadic Macros

最後のパラメータに `...` を指定すると可変長引数マクロになります:

```
{#define PRINT(...) print(__VA_ARGS__)}
{#define COUNT(...) __VA_ARGC__}
{#define FIRST(a, ...) a}
{#define REST(a, ...) __VA_ARGS__}
```

| 特殊マクロ | 説明 |
|-----------|------|
| `__VA_ARGS__` | 可変長引数全体に展開される |
| `__VA_ARGC__` | 可変長引数の個数（整数）に展開される |

```
PRINT(1, 2, 3);      (* → print(1,2,3); *)
n := COUNT(a, b, c);  (* → 3 *)
```

### 3.4 マクロ定義解除 / Undefine

```
{#undef NAME}
```

`NAME` の定義を解除します。未定義の名前に対する `{#undef}` はエラーになりません。
`defined` の再定義・定義解除は `OPERATION_NOT_ALLOWED` エラーとなります。

### 3.5 マクロ再定義 / Redefinition

同名のマクロを異なる内容で再定義すると `MACRO_REDEFINED` 警告が発行されます。同一内容の再定義は警告なしで受け入れられます。

### 3.6 再帰防止 / Recursion Prevention

C プリプロセッサと同様の "paint-blue" アルゴリズムを使用します。展開中のマクロはマーク（hide-set）され、再帰的な展開を防止します。再帰はエラーにはならず、展開済みマクロの名前はそのままテキストとして出力されます。

---

## 4. 文字列化とトークン連結 / Stringize & Token Paste

### 4.1 文字列化演算子 `@` / Stringize Operator

関数マクロ本体内で `@param` と記述すると、引数を IEC 61131-3 文字列リテラルに変換します。C の `#arg` に相当します。

```
{#define STR(x) @x}
s := STR(Hello World);   (* → 'Hello World' *)
```

IEC 61131-3 の特殊文字は自動的にエスケープされます:

| 文字 | エスケープ |
|------|----------|
| `$` | `$$` |
| `'` | `$27` |
| `"` | `$22` |
| LF | `$0a` |
| CR | `$0d` |

### 4.2 文字列化ディレクティブ `{#string}` / `{##}` / Stringize Directive

ディレクティブ形式の文字列化です。引数をマクロ展開してから文字列化します。

```
{#string expr}
{## expr}
```

```
s := {## 123 + 456};   (* → '123 + 456' *)
s := {## __LINE__};     (* → 展開された行番号の文字列 *)
```

ワイド文字列版 `{#wstring expr}` は `"..."` 形式の WSTRING リテラルを生成します。

### 4.3 トークン連結演算子 `@@` / Token Paste Operator

関数マクロ本体内で `@@` を使用すると、隣接するトークンを連結します。C の `##` に相当します。

```
{#define VAR_NAME(prefix, n) prefix@@_@@n}
VAR_NAME(sensor, 1)   (* → sensor_1 *)
```

### 4.4 トークン化ディレクティブ `{#token}` / Tokenize Directive

`{#token expr}` / `{#@@ expr}` / `{### expr}` は、引数を再トークン化してマクロ展開し出力します。文字列からトークン列を動的に生成する場合に使用します。

```
{#define A hello}
{#define B world}
{#token A B}   (* → hello world *)
```

---

## 5. 条件コンパイル / Conditional Compilation

### 5.1 基本構文 / Basic Syntax

```
{#if EXPR}
  ...
{#elif EXPR}
  ...
{#else}
  ...
{#endif}
```

```
{#ifdef NAME}     (* NAME が定義されていれば真 *)
{#ifndef NAME}    (* NAME が未定義なら真 *)
```

### 5.2 `defined` 演算子 / `defined` Operator

```
{#if defined(NAME)}
{#if defined(NAME1) \and\ defined(NAME2)}
```

`defined` は `{#if}` / `{#elif}` 式内でのみ使用可能です。

### 5.3 未定義の識別子 / Undefined Identifiers

`{#if}` 式内で未定義の識別子は `0` に評価されます（C プリプロセッサと同じセマンティクス）。

---

## 6. 式評価 / Expression Evaluation (`{#if}` / `{#elif}`)

### 6.1 サポートするリテラル / Supported Literals

| 種別 | 例 | 説明 |
|------|-----|------|
| 整数 | `42`, `-1`, `0` | 10進整数 |
| 浮動小数点 | `3.14`, `-1.5e10` | IEEE 754 倍精度 |
| 真偽値 | `TRUE`, `FALSE`, `true`, `false` | ブール値 |
| ビット列 | `BYTE#16#ff`, `WORD#2#1010`, `DWORD#8#77` | IEC 61131-3 ビット列リテラル (BYTE/WORD/DWORD/LWORD) |
| N進数 | `16#ff`, `2#1010`, `8#77` | 2〜36進数リテラル |

### 6.2 演算子の優先順位 / Operator Precedence

低い順（上が低優先度、下が高優先度）:

| 優先度 | 演算子 | 説明 |
|--------|--------|------|
| 1 | `\or\`, `or` | 論理 OR |
| 2 | `\and\`, `and` | 論理 AND |
| 3 | `\xor\`, `xor` | 論理 XOR |
| 4 | `\not\`, `not` | 論理 NOT（単項） |
| 5 | `=`, `<>`, `<`, `<=`, `>`, `>=` | 比較 |
| 6 | `<<`, `>>` | ビットシフト |
| 7 | `+`, `-` | 加減算 |
| 8 | `or`, `xor`, `and`, `&` | ビット演算 |
| 9 | `*`, `/`, `mod` | 乗除算・剰余 |
| 10 | `+`, `-`, `not` | 単項演算子 |
| 11 | リテラル, `(...)`, `defined()`, `__has_include()` | 基本要素 |

> **注意**: `or`, `and`, `xor` はコンテキストにより論理演算またはビット演算として解釈されます。
> エスケープ形式 `\or\`, `\and\`, `\xor\`, `\not\` は常に論理演算として解釈されます。

### 6.3 型の昇格 / Type Promotion

- 整数と浮動小数点の混合演算: 整数が浮動小数点に昇格
- ゼロ除算: エラー (`EXPR_TYPE_ERROR`)
- 整数オーバーフロー: 64ビット範囲で演算

---

## 7. ファイルインクルード / File Inclusion

### 7.1 `{#include}` — 相対パス優先

```
{#include 'file.iec'}
{#include "file.iec"}
```

検索順序:
1. 現在のファイルのディレクトリ
2. `-I` オプションまたは `{#syspath}` で追加されたパス（追加順）

### 7.2 `{#sinclude}` — システムパス限定

```
{#sinclude 'file.iec'}
```

検索順序:
1. `-I` オプションまたは `{#syspath}` で追加されたパスのみ（現在ディレクトリは検索しない）

### 7.3 `{#include <file>}` — アングルブラケット形式

```
{#include <file.iec>}
```

`{#sinclude 'file.iec'}` と同等です。

### 7.4 `{#syspath}` — インクルードパス追加

```
{#syspath 'dir'}
```

インクルード検索パスを動的に追加します。CLI の `-I dir` と同等です。

### 7.5 `__has_include` — インクルード存在チェック

```
{#if __has_include('file.iec')}
  {#include 'file.iec'}
{#endif}
```

指定ファイルがインクルードパスに存在すれば `1`、なければ `0` に評価されます。`"file"` 形式は `{#include}` 検索順序、`<file>` 形式は `{#sinclude}` 検索順序を使用します。

### 7.6 インクルードガード / Include Guards

Jiepp にはプラグマベースのインクルードガード（`#pragma once` 等）はありません。C と同様のマクロベースのガードを使用してください:

```
{#ifndef MYHEADER_INCLUDED}
{#define MYHEADER_INCLUDED 1}
  (* ヘッダ内容 *)
{#endif}
```

### 7.7 インクルード深度制限 / Include Depth Limit

デフォルト: **100**。`--max-include-depth N` オプションまたは `{#max_include_depth N}` ディレクティブで変更可能。超過すると `MAX_INCLUDE_DEPTH_EXCEEDED` エラー。

> **注意**: Jiepp は循環インクルードを即座に検出する仕組みを備えていません。代わりに、深度制限に到達した際に PP12 エラーで処理を停止することにより、無限ループを防止しています。実質的に、循環インクルードはこの深度制限により検出・防止されます。

---

## 8. メッセージディレクティブ / Message Directives

```
{#error 'message'}     (* エラーを発行して処理を中断 *)
{#warning 'message'}   (* 警告を発行 *)
{#info 'message'}      (* 情報メッセージを発行 *)
{#severe 'message'}    (* 致命的エラーを発行して処理を中断 *)
```

- `{#error}` と `{#severe}` は処理を中断します（例外を送出）
- `{#warning}` と `{#info}` は処理を継続します
- メッセージ引数はマクロ展開されます

---

## 9. 行番号制御 / Line Number Control

### `{#line}` / `{#set_line}` / `{#set-line}`

```
{#line 100}
{#set_line 200 'virtual.iec'}
{#set_line 300 "virtual.iec"}
```

- 第1引数: 新しい行番号
- 第2引数 (省略可): 新しいファイルパス（シングルクォート `'...'` またはダブルクォート `"..."` で囲む。**クォートは必須**。クォートなしは `INVALID_SETLINE_OPERAND` エラー）

行番号変更は `__LINE__` マクロと、出力のプラグマ行に反映されます。

---

## 10. エラー抑制 / Issue Suppression

### `{#ignore}`

```
{#ignore PP41}
```

指定したエラーコード（`PP` + 2桁番号）の診断メッセージを抑制します。エラーコード値は §16 のエラーコード一覧を参照してください。

---

## 11. ランタイム制限ディレクティブ / Runtime Limit Directives

ソースコード内からプリプロセッサの動作制限を設定できます。CLI オプションの `--` 接頭辞付きバージョンも同等の機能を提供します。

| ディレクティブ | CLI オプション | デフォルト | 説明 |
|--------------|--------------|-----------|------|
| `{#max_include_depth N}` | `--max-include-depth N` | 100 | インクルード最大ネスト深度 |
| `{#max_expansion_depth N}` | `--max-expansion-depth N` | 256 | マクロ展開深度上限 |
| `{#max_if_nesting N}` | `--max-if-nesting N` | 256 | 条件分岐ネスト深度上限 |
| `{#pp_output_pragma_style STYLE}` | `--pp-output-pragma-style STYLE` | `annotated` | プラグマ出力スタイル |

CLI オプションで設定した値はソースコード内のディレクティブで上書きできません（ロックされます）。

### プラグマ出力スタイル / Pragma Output Style

| 値 | 出力例 | 説明 |
|----|--------|------|
| `annotated` | `(*{#:100 file.iec}*)` | IEC 61131-3 コメントで囲まれ、処理系に影響しない |
| `standard` | `{#:100 file.iec}` | プラグマ形式で出力される |

### `{#nop}` — 無操作 / No Operation

```
{#nop}
{#}
```

何も行わないディレクティブです。空のディレクティブ `{#}` と同等です。

---

## 12. ビルトインマクロ / Built-in Macros

### 12.1 ファイル・行情報 / File & Line Information

| マクロ | 型 | 説明 |
|-------|----|------|
| `__FILE__` | 文字列 | 現在処理中のファイルパス（相対パス） |
| `__FILE_NAME__` | 文字列 | 現在のファイル名（パスなし） |
| `__BASE_FILE__` | 文字列 | トップレベルファイルパス（最初に処理を開始したファイル） |
| `__LINE__` | 整数 | 現在の行番号 |
| `__INCLUDE_LEVEL__` | 整数 | インクルードネスト深度（0起点。トップレベルファイルは 0） |
| `__COUNTER__` | 整数 | 参照ごとにインクリメントされるカウンタ（0起点） |

### 12.2 日時情報 / Date & Time Information

| マクロ | 型 | 説明 |
|-------|----|------|
| `__DATE__` | 文字列 | 処理開始日 (`'Mmm dd yyyy'` 形式) |
| `__TIME__` | 文字列 | 処理開始時刻 (`'hh:mm:ss'` 形式) |
| `__TIMESTAMP__` | 文字列 | 現在のファイルの最終更新日時 |

### 12.3 プリプロセッサ識別 / Preprocessor Identification

| マクロ | 型 | 説明 |
|-------|----|------|
| `_JIEPP` | 真偽値 | 常に `true`。jiepp で処理されていることを示す |
| `_JIEPP_VERSION` | 文字列 | バージョン文字列 (例: `'1.0.0'`) |
| `_JIEPP_VER` | 整数 | メジャー×100 + マイナー (例: `100`) |
| `_JIEPP_FULL_VER` | 整数 | メジャー×10000 + マイナー×100 + パッチ (例: `10000`) |

### 12.4 IEC 61131-3 型範囲マクロ / IEC 61131-3 Type Range Macros

| マクロ | 値 |
|-------|----|
| `__SINT_MIN__` / `__SINT_MAX__` | `-128` / `127` |
| `__INT_MIN__` / `__INT_MAX__` | `-32768` / `32767` |
| `__DINT_MIN__` / `__DINT_MAX__` | `-2147483648` / `2147483647` |
| `__LINT_MIN__` / `__LINT_MAX__` | `-9223372036854775808` / `9223372036854775807` |
| `__USINT_MIN__` / `__USINT_MAX__` | `0` / `255` |
| `__UINT_MIN__` / `__UINT_MAX__` | `0` / `65535` |
| `__UDINT_MIN__` / `__UDINT_MAX__` | `0` / `4294967295` |
| `__ULINT_MIN__` / `__ULINT_MAX__` | `0` / `18446744073709551615` |

### 12.5 ビットマスクマクロ / Bitmask Macros

| マクロ | 値 |
|-------|----|
| `__BYTE_MASK__` | `byte#16#ff` |
| `__WORD_MASK__` | `word#16#ffff` |
| `__DWORD_MASK__` | `dword#16#ffffffff` |
| `__LWORD_MASK__` | `lword#16#ffffffffffffffff` |

### 12.6 時間・日付型範囲マクロ / Time & Date Range Macros

| マクロ | 値 |
|-------|----|
| `__LTIME_MIN__` / `__LTIME_MAX__` | `ltime#-9223372036854775808ns` / `ltime#9223372036854775807ns` |
| `__LTIME_MIN_IN_NS__` / `__LTIME_MAX_IN_NS__` | `-9223372036854775808` / `9223372036854775807` |
| `__LDATE_MIN__` / `__LDATE_MAX__` | `ldate#1970-01-01` / `ldate#2554-07-21` |
| `__LTOD_MIN__` / `__LTOD_MAX__` | `ltod#1970-01-01` / `ltod#2554-07-21` |
| `__LDT_MIN__` / `__LDT_MAX__` | `ldt#1970-01-01-00:00:00` / `ldt#2554-07-21-23:34:33.709551615` |

### 12.7 型エイリアスマクロ / Type Alias Macros

| マクロ | 値 |
|-------|----|
| `__INT8_TYPE` / `__UINT8_TYPE` | `sint` / `usint` |
| `__INT16_TYPE` / `__UINT16_TYPE` | `int` / `uint` |
| `__INT32_TYPE` / `__UINT32_TYPE` | `dint` / `udint` |
| `__INT64_TYPE` / `__UINT64_TYPE` | `lint` / `ulint` |
| `__BITS8_TYPE` / `__BITS16_TYPE` | `byte` / `word` |
| `__BITS32_TYPE` / `__BITS64_TYPE` | `dword` / `lword` |

### 12.8 条件式専用 / Conditional Expression Only

| マクロ | 説明 |
|-------|----|
| `defined(NAME)` | `{#if}` / `{#elif}` 式内でのみ使用可能。NAME が定義されていれば 1、そうでなければ 0 |
| `__has_include('file')` | `{#if}` 式内でファイルの存在を確認 (§7.5 参照) |

---

## 13. CLI リファレンス / CLI Reference

### 基本構文 / Basic Syntax

```
jiepp [filepath] [options]
```

- `filepath` 省略時: 標準入力から読み取り
- `-`: 明示的に標準入力を指定

### オプション一覧 / Option Reference

| オプション | 説明 | デフォルト |
|-----------|------|-----------|
| `-o PATH` | 出力先ファイルパス | stdout |
| `-D NAME[=VALUE]` | マクロ定義。`-D NAME` は `NAME=1` と同等 | — |
| `-U NAME` | マクロ定義の取り消し（`-D` の後に適用） | — |
| `-I PATH` | インクルード検索パスの追加 | — |
| `-include FILE` | 入力ファイルの前に強制インクルード | — |
| `-w` | 警告メッセージの抑制（エラーは出力） | off |
| `-Werror` | 警告をエラーに昇格 | off |
| `-M` | Makefile 依存関係ルールを出力（全インクルード） | off |
| `-MM` | `-M` と同様だがシステムインクルードを除外 | off |
| `-MF FILE` | 依存関係ルールの出力先ファイル | stdout |
| `-MT TARGET` | 依存関係ルールのターゲット名 | 入力ファイル名.o |
| `--max-include-depth N` | インクルード深度上限 | 100 |
| `--max-expansion-depth N` | マクロ展開深度上限 | 256 |
| `--max-if-nesting N` | 条件分岐ネスト深度上限 | 256 |
| `--pp-output-pragma-style STYLE` | プラグマ出力スタイル (`annotated` / `standard`) | `annotated` |
| `--remove-comments` / `-nC` | コメントを除去 | off |
| `-dM` | 定義されたマクロの一覧を出力 | off |
| `--silent` | 全ての診断メッセージを抑制 | off |
| `--recursion-limit N` | OS スタックサイズの設定 (N × 約 8KB) | システムデフォルト |
| `--` | オプションの終端（以降は全てファイルパスとして扱う） | — |
| `--help` / `-h` | ヘルプメッセージの表示 | — |
| `--version` | バージョン番号の表示 | — |

未知のオプションはエラーとして処理され、即座に終了します（gcc 互換）。

### 使用例 / Usage Examples

```bash
# ファイルをプリプロセスして stdout に出力
jiepp input.iec

# 出力先ファイルを指定
jiepp input.iec -o output.iec

# マクロを定義してプリプロセス
jiepp -D TARGET_OMRON -D VERSION=2 input.iec

# マクロ定義の取り消し
jiepp -D DEBUG -U DEBUG input.iec

# インクルードパスを追加
jiepp -I lib/ -I common/ input.iec

# ファイルの前にヘッダを強制インクルード
jiepp -include config.iec input.iec

# 標準入力から処理
echo '{#define X 42}
VAR x : INT := X; END_VAR' | jiepp -

# マクロ一覧を出力
jiepp -dM input.iec

# コメントを除去
jiepp -nC input.iec -o clean.iec

# 警告を抑制
jiepp -w input.iec

# 警告をエラーに昇格
jiepp -Werror input.iec

# Makefile 依存関係ルールを生成
jiepp -M -MF deps.d input.iec

# ハイフンで始まるファイル名を処理
jiepp -- -unusual-name.iec
```

---

## 14. サンドボックスモード / Sandbox Mode

信頼できない入力を処理する場合（Web サーバー等）のセキュリティモードです。コンパイル時フラグ `-DJIEPP_SANDBOX=ON` で有効化します。

### ビルド / Build

**通常ビルド (動的リンク):**

```bash
cmake --preset linux-makefiles-release -DJIEPP_SANDBOX=ON
cmake --build --preset linux-makefiles-release
```

**サーバー配布向け (完全静的リンク + サンドボックス):**

GLIBC バージョンが異なる古いサーバーに配布する場合は `linux-portable-release` プリセットと組み合わせます。

```bash
cmake --preset linux-portable-release -DJIEPP_SANDBOX=ON
cmake --build --preset linux-portable-release
# 成果物: build/linux-portable-release/jiepp
```

### 制限事項 / Restrictions

サンドボックスモードでは以下のディレクティブが無効化されます:

| ディレクティブ | 理由 |
|--------------|------|
| `{#include}` | ファイルシステムアクセスの防止 |
| `{#sinclude}` | ファイルシステムアクセスの防止 |
| `{#syspath}` | ファイルシステムアクセスの防止 |
| `{#max_include_depth}` | 制限緩和の防止 |
| `{#ignore}` | エラー抑制の防止 |
| `__has_include` | ファイルシステム探査の防止 |

### 情報漏洩防止 / Information Leak Prevention

| 対策 | 詳細 |
|------|------|
| `__TIMESTAMP__` → `''` | ファイルシステム stat の防止 |
| `{#set_line}` ファイルパス引数無視 | パス偽装の防止 |
| `{#error}`/`{#warning}` 制御文字除去 | ログインジェクションの防止 |

### 運用要件 / Operational Requirements

- **Process-per-request 必須**: flex/bison のグローバル状態のため、リクエストごとにプロセスを起動する必要があります
- 入出力サイズ制限は呼び出し元（CGI 等）で管理してください

---

## 15. ディレクティブ一覧 / Directive Reference

| ディレクティブ | 参照 | 説明 |
|--------------|------|------|
| `{#define NAME ...}` | §3.1 | オブジェクトマクロ定義 |
| `{#define NAME(args) ...}` | §3.2 | 関数マクロ定義 |
| `{#undef NAME}` | §3.4 | マクロ定義解除 |
| `{#include 'file'}` | §7.1 | ファイルインクルード（相対パス優先） |
| `{#include <file>}` | §7.3 | ファイルインクルード（システムパス限定） |
| `{#sinclude 'file'}` | §7.2 | システムパス限定インクルード |
| `{#syspath 'dir'}` | §7.4 | インクルード検索パス追加 |
| `{#if EXPR}` | §5.1 | 条件コンパイル開始 |
| `{#elif EXPR}` | §5.1 | 条件分岐 |
| `{#else}` | §5.1 | 条件分岐（上記すべて偽のとき） |
| `{#endif}` | §5.1 | 条件コンパイル終了 |
| `{#ifdef NAME}` | §5.1 | マクロ定義チェック |
| `{#ifndef NAME}` | §5.1 | マクロ未定義チェック |
| `{#error 'msg'}` | §8 | エラーメッセージ発行 |
| `{#warning 'msg'}` | §8 | 警告メッセージ発行 |
| `{#info 'msg'}` | §8 | 情報メッセージ発行 |
| `{#severe 'msg'}` | §8 | 致命的エラー発行 |
| `{## expr}` / `{#string expr}` | §4.2 | 文字列化ディレクティブ |
| `{#wstring expr}` | §4.2 | ワイド文字列化ディレクティブ |
| `{#@@ expr}` / `{### expr}` / `{#token expr}` | §4.4 | トークン化ディレクティブ |
| `{#line N ['file']}` / `{#set_line ...}` | §9 | 行番号設定 |
| `{#ignore CODE}` | §10 | エラーコード抑制 |
| `{#max_include_depth N}` | §11 | インクルード深度制限設定 |
| `{#max_expansion_depth N}` | §11 | マクロ展開深度制限設定 |
| `{#max_if_nesting N}` | §11 | 条件分岐ネスト深度制限設定 |
| `{#pp_output_pragma_style STYLE}` | §11 | プラグマ出力スタイル設定 |
| `{#nop}` / `{#}` | §11 | 無操作 |

---

## 16. エラーコード一覧 / Issue Code Reference

エラーコードは `PP` プレフィックス + 2桁の番号（例: `PP30`）で表示されます。十の位でカテゴリをグループ化しています。

### 出力形式 / Output Format

プリプロセス時のエラー・警告メッセージは以下の形式で出力されます:

```
filepath:line.col: severity: PPxx: message
```

- `filepath` — ソースファイルのパス（`{#line}` / `{#set_line}` による変更後の値）
- `line` — 行番号
- `col` — 列番号（常に `0`）
- `severity` — `error` / `warning` / `info` のいずれか（SEVERE は `error`）
- `PPxx` — エラーコード（2桁ゼロパディング）
- `message` — エラーメッセージ

CLI オプションのエラーは `jiepp:` をファイルパスの代わりに使用します:

```
<unknown location>: error: PP70: Unknown command-line option; "--foo".
```

### カテゴリ / Categories

| 範囲 | カテゴリ |
|------|---------|
| 01-09 | System/Fatal |
| 10-19 | File/IO |
| 20-29 | Lexical/Syntax |
| 30-39 | Macro |
| 40-49 | Directive/Operand |
| 50-59 | Expression |
| 60-69 | Runtime/Limit |
| 70-79 | CLI/Option |
| 80-89 | （予約） |
| 90-99 | Message passthrough |

### 重大度 / Severity

| レベル | 説明 |
|--------|------|
| SEVERE | 致命的エラー。処理を即座に中断する |
| ERROR | エラー。デフォルトでは処理を中断する。`{#ignore}` で続行モードに変更可能。エラーカウントを増加させる |
| WARNING | 警告。処理を続行する。`-Werror` 指定時はエラーに昇格する |
| INFO | 情報メッセージ。処理に影響しない |

### エラーコード一覧 / Error Codes

| コード | コード名 | 重大度 | メッセージ | 説明 |
|--------|----------|--------|-----------|------|
| PP01 | `FATAL` | SEVERE | A fatal error occurred. | 内部致命的エラー |
| PP02 | `OPERATION_NOT_ALLOWED` | SEVERE | Operation not allowed | 禁止された操作（`defined` の再定義等） |
| PP03 | `RECURSION_LIMIT_EXCEEDED` | SEVERE | Failed to set recursion limit | OS スタックサイズ設定の失敗 |
| PP04 | `PARAMETER_VALUE_OVERFLOW` | SEVERE | Parameter value exceeds maximum (2^24) | パラメータ値が上限（2^24）を超過 |
| PP10 | `FILE_ERROR` | ERROR | An error occurred with the file | ファイル操作エラー |
| PP11 | `FILE_NOT_FOUND` | ERROR | No such file or directory | ファイルが見つからない |
| PP12 | `MAX_INCLUDE_DEPTH_EXCEEDED` | ERROR | Maximum include depth exceeded | インクルード深度上限超過 |
| PP13 | `INVALID_COMMAND` | ERROR | Invalid command | 無効なコマンド |
| PP20 | `UNCLOSED_COMMENT` | ERROR | Unclosed comment | コメントが閉じられていない |
| PP21 | `INVALID_ESCAPE_SEQUENCE` | ERROR | Invalid escape sequence | 不正なエスケープシーケンス |
| PP22 | `INVALID_PRAGMA_SYNTAX` | ERROR | Invalid syntax for pragma | プラグマの構文エラー |
| PP23 | `INVALID_PP_SYNTAX` | ERROR | Invalid preprocessor syntax | プリプロセッサの構文エラー（汎用） |
| PP24 | `UNTERMINATED_CONDITIONAL` | ERROR | Unterminated conditional directive | `{#if}` が閉じられていない |
| PP25 | `ELIF_ERROR` | ERROR | Unexpected elif directive | `{#elif}` の位置エラー |
| PP26 | `ELSE_ERROR` | ERROR | Unexpected else directive | `{#else}` の位置エラー |
| PP27 | `ENDIF_ERROR` | ERROR | Unexpected endif directive | `{#endif}` の位置エラー |
| PP30 | `INVALID_DEFINE_SYNTAX` | ERROR | Invalid define syntax | `{#define}` の構文エラー |
| PP31 | `INVALID_STRINGIZING` | ERROR | Invalid stringizing (@) | 不正な文字列化演算子 |
| PP32 | `INVALID_TOKEN_PASTING` | ERROR | Invalid token pasting (@@) | 不正なトークン連結演算子 |
| PP33 | `INVALID_VARIADIC_PLACEMENT` | ERROR | '...' must be the last parameter | 可変長引数が最後のパラメータでない |
| PP34 | `ARGUMENT_COUNT_MISMATCH` | ERROR | Argument count mismatch | 関数マクロの引数数不一致 |
| PP35 | `MACRO_REDEFINED` | WARNING | Macro redefined | マクロの再定義警告 |
| PP36 | `DUPLICATE_MACRO_PARAMETER` | ERROR | Duplicate macro parameter name | マクロパラメータ名の重複 |
| PP40 | `INVALID_DEFINED_OPERAND` | ERROR | Invalid operand for 'defined' | `defined` の不正なオペランド |
| PP41 | `INVALID_SETLINE_OPERAND` | ERROR | Invalid operand for 'set_line' | `{#set_line}` の不正なオペランド |
| PP42 | `INVALID_IGNORE_OPERAND` | ERROR | Invalid operand for 'ignore' | `{#ignore}` の不正なオペランド |
| PP43 | `INVALID_LIMIT_OPERAND` | ERROR | Invalid operand for limit directive | 制限ディレクティブの不正なオペランド |
| PP44 | `INVALID_PARAMETER_VALUE` | ERROR | Invalid parameter value | パラメータ値が不正（0以下等） |
| PP45 | `UNKNOWN_DIRECTIVE` | WARNING | Unknown directive | 未知のディレクティブ |
| PP46 | `INVALID_DIRECTIVE_NAME` | ERROR | Invalid directive name | 不正なディレクティブ名（先頭が数字等） |
| PP50 | `EXPR_TYPE_ERROR` | ERROR | Type error in expression | 式中の型エラー（ゼロ除算等） |
| PP51 | `MISSING_EXPRESSION` | ERROR | Missing expression | `{#if}` に式がない |
| PP52 | `INVALID_EXPRESSION` | ERROR | Invalid expression | 不正な式 |
| PP60 | `MAX_EXPANSION_DEPTH_EXCEEDED` | ERROR | Maximum expansion depth exceeded | マクロ展開深度上限超過 |
| PP62 | `MAX_IF_NESTING_EXCEEDED` | ERROR | Maximum conditional nesting depth exceeded | 条件分岐ネスト深度上限超過 |
| PP63 | `SANDBOX_RESTRICTED_DIRECTIVE` | ERROR | Directive is restricted in sandbox mode | サンドボックスモードで禁止されたディレクティブ |
| PP70 | `UNKNOWN_OPTION` | ERROR | Unknown command-line option | 未知のコマンドラインオプション |
| PP71 | `INVALID_OPTION_VALUE` | ERROR | Invalid option value | オプション値が不正（正の整数でない等） |
| PP72 | `MISSING_OPTION_VALUE` | ERROR | Option requires a value | オプションに値が指定されていない |
| PP73 | `INVALID_MACRO_DEF` | ERROR | Invalid macro definition | マクロ定義が不正（`-D` の引数が空等） |
| PP74 | `RECURSION_LIMIT_RANGE` | ERROR | Recursion limit value out of range | `--recursion-limit` 値が上限を超過 |
| PP75 | `THREAD_CREATE_FAILED` | ERROR | Failed to create worker thread | ワーカースレッドの作成失敗（Windows） |
| PP76 | `STACK_LIMIT_FAILED` | ERROR | Failed to set stack limit | スタックサイズの設定失敗（POSIX） |
| PP90 | `SEVERE_MESSAGE` | SEVERE | #severe | `{#severe}` ディレクティブによる致命的エラー |
| PP91 | `ERROR_MESSAGE` | ERROR | #error | `{#error}` ディレクティブによるエラー |
| PP92 | `WARNING_MESSAGE` | WARNING | #warning | `{#warning}` ディレクティブによる警告 |
| PP93 | `INFO_MESSAGE` | INFO | #info | `{#info}` ディレクティブによる情報 |

---

## 17. C プリプロセッサとの対応 / C Preprocessor Correspondence

Jiepp は C プリプロセッサ (cpp) の概念を IEC 61131-3 に適応させています。以下は対応表です。

| C プリプロセッサ | Jiepp | 備考 |
|-----------------|-------|------|
| `#define` | `{#define}` | 同等 |
| `#undef` | `{#undef}` | 同等 |
| `#include "file"` | `{#include 'file'}` | IEC 61131-3 では `'` が文字列デリミタ |
| `#include <file>` | `{#sinclude 'file'}` / `{#include <file>}` | システムパス限定 |
| `#if` / `#elif` / `#else` / `#endif` | `{#if}` / `{#elif}` / `{#else}` / `{#endif}` | 同等 |
| `#ifdef` / `#ifndef` | `{#ifdef}` / `{#ifndef}` | 同等 |
| `#error` / `#warning` | `{#error}` / `{#warning}` | 同等 |
| `#line N "file"` | `{#line N 'file'}` | 同等 |
| `#arg` (stringize) | `@arg` | IEC 61131-3 では `#` が別の意味を持つため |
| `##` (token paste) | `@@` | 同上 |
| `__VA_ARGS__` | `__VA_ARGS__` | 同等 |
| — | `__VA_ARGC__` | Jiepp 拡張: 可変長引数の個数 |
| `defined(NAME)` | `defined(NAME)` | 同等 |
| `__has_include(...)` | `__has_include(...)` | 同等 |
| `&&`, `\|\|`, `!` | `\and\`, `\or\`, `\not\` | IEC 61131-3 論理演算子形式 |
| — | `{#info}` / `{#severe}` | Jiepp 拡張 |
| — | `{#ignore CODE}` | Jiepp 拡張 |
| — | `{#nop}` | Jiepp 拡張 |
| — | `{#max_*}` 制限ディレクティブ | Jiepp 拡張 |
| — | `{#wstring}` | Jiepp 拡張: ワイド文字列化 |

---

## 付録 A. サンプル / Appendix A: Examples

### A.1 オブジェクトマクロ / Object Macro

**入力:**
```
{#define PI 3.14159}
{#define GRAVITY 9.81}
area := PI * r * r;
force := mass * GRAVITY;
```

**出力:**
```
area := 3.14159 * r * r;
force := mass * 9.81;
```

### A.2 関数マクロとトークン連結 / Function Macro with Token Paste

**入力:**
```
{#define DECL_ADD(T)
	function add_@@T: T
	var_input
		in1, in2: T;
	end_var
	${st$}
	add_@@T := in1 + in2;
	${end$}
	end_function
}
DECL_ADD(int);
DECL_ADD(lreal);
```

**出力:**
```
function add_int: int	var_input		in1, in2: int;	end_var	(*{st}*)	add_int := in1 + in2;	(*{end}*)	end_function;
function add_lreal: lreal	var_input		in1, in2: lreal;	end_var	(*{st}*)	add_lreal := in1 + in2;	(*{end}*)	end_function;
```

### A.3 プラットフォーム切替 / Platform Switching

**入力:**
```
{#define TARGET_OMRON 1}

{#if defined(TARGET_OMRON)}
  {#define FB_TIMER TON}
  {#define MAX_IO 2048}
{#elif defined(TARGET_MITSUBISHI)}
  {#define FB_TIMER Timer_100ms}
  {#define MAX_IO 1024}
{#else}
  {#error 'Unknown target platform.'}
{#endif}

timer1: FB_TIMER;
io_count := MAX_IO;
```

**出力:**
```
timer1: TON;
io_count := 2048;
```

### A.4 可変長ログマクロ / Variadic Logging Macro

**入力:**
```
{#define LOG_PREFIX(level) @level}
{#define LOG_INFO(...) log_message(LOG_PREFIX(INFO), __VA_ARGS__)}
{#define LOG_WARN(...) log_message(LOG_PREFIX(WARN), __VA_ARGS__)}

LOG_INFO('System started');
LOG_WARN('Low memory', ' threshold=', {## 1024});
```

**出力:**
```
log_message('INFO', 'System started');
log_message('WARN', 'Low memory',' threshold=','1024');
```

---

## 付録 B. 文法概要 / Appendix B: Grammar Summary

```
directive     := '{#' directive-name arg '}'
directive-name := 'define' | 'D' | 'undef' | 'include' | 'sinclude'
               | 'if' | 'elif' | 'else' | 'endif' | 'ifdef' | 'ifndef'
               | 'error' | 'warning' | 'info' | 'severe'
               | 'line' | 'set_line' | 'set-line'
               | 'ignore' | 'nop' | '' | 'syspath' | 'I'
               | 'string' | '#' | 'wstring'
               | 'token' | '@@' | '##'
               | 'max_include_depth' | 'max-include-depth'
               | 'max_expansion_depth' | 'max-expansion-depth'
               | 'max_if_nesting' | 'max-if-nesting'
               | 'pp_output_pragma_style' | 'pp-output-pragma-style'
arg           := <text until closing '}'>

setline-arg   := INTEGER
               | INTEGER ws quoted-string

quoted-string := "'" <chars> "'" | '"' <chars> '"'

define-body   := NAME ws replacement
               | NAME '(' params ')' ws replacement
params        := NAME (',' NAME)* (',' '...')?
               | '...'
               | <empty>

if-expr       := or-expr
or-expr       := and-expr ('\or\' and-expr)*
and-expr      := xor-expr ('\and\' xor-expr)*
xor-expr      := not-expr ('\xor\' not-expr)*
not-expr      := '\not\' not-expr | cmp-expr
cmp-expr      := shift-expr (('=' | '<>' | '<' | '<=' | '>' | '>=') shift-expr)*
shift-expr    := add-expr (('<<' | '>>') add-expr)*
add-expr      := bit-expr (('+' | '-') bit-expr)*
bit-expr      := mul-expr (('or' | 'xor' | 'and' | '&') mul-expr)*
mul-expr      := unary-expr (('*' | '/' | 'mod') unary-expr)*
unary-expr    := ('+' | '-' | 'not') unary-expr | primary
primary       := INTEGER | FLOAT | BOOL | BITSTRING
               | 'defined' '(' NAME ')'
               | '__has_include' '(' path ')'
               | '(' if-expr ')'
```
