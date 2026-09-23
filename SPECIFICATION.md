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
- 連続する空行は既定で最大 7 行まで出力されます。8 行以上連続する場合、その区間は 1 行の行番号プラグマ（マーカー）に置き換えられます（gcc/clang と同じ閾値）。上限は `--max-blank-lines N` オプションまたはソースコード内の `{#max_blank_lines N}` ディレクティブ（§11）で変更でき、`0` を指定すると圧縮を無効化します（マーカーの意味は §9、オプション詳細は §13 を参照）。`-P` 指定時は行マーカーと共に空行も全て除去されます
- Windows では stdout はバイナリモードで開かれるため、リダイレクトした stdout と `-o FILE` の出力はバイト単位で一致します（改行コードは LF のまま変換されません）
- `-o` で指定したファイルは、プリプロセス処理全体が成功した後にのみ作成・上書きされます。入力ファイルと同じパスを `-o` に指定しても入力内容は失われません（クラッシュ・エラー終了時もファイルは変更されません）

---

## 2. ディレクティブ構文 / Directive Syntax

### 基本形 / Basic Form

```
{#directive_name arguments}
```

- `{` と `#` の間にスペース・タブ・改行（またはそれらの混在）を挟むと、ディレクティブとしては扱われず、通常の IEC プラグマとして扱われます（挟んだ空白はそのまま出力に保持されます）。この場合 `WHITESPACE_BEFORE_DIRECTIVE` (`PP28`) 警告が発行されます。コメント（例: `{(*c*)#define ...}`）を挟んだ場合も同様に通常のプラグマとして扱われますが、この場合は警告が発行されません。`(*{`, `/*{`, `//{` の各開始形式でも同じ規則が適用されます
- 引数は閉じ `}` まで続きます
- 複数行に跨ることが可能です（閉じ `}` が次の行にあってもよい）

### 複数行ディレクティブ / Multi-line Directives

ディレクティブは閉じ `}` が見つかるまで複数行に跨ることができます。特別な行継続文字は不要です。

ディレクティブ本体内の生の改行（LF / CRLF / CR）は、通常のプラグマと同じ規則で単なる空白として扱われます。直前の文字がすでに空白でなければ 1 個のスペースが挿入され、直前がすでに空白（スペース・タブ・別の改行など）ならスペースは追加されません。マクロ名と `(` の間に改行を挟んだ場合も §3.2 の「スペースを挟むとオブジェクトマクロになる」規則がそのまま適用されるため、`{#define F<改行>(a) a}` は `F` を関数マクロではなくオブジェクトマクロとして定義します（値は `(a) a`）。

```
{#define LONG_MACRO(a, b, c)
    a + b + c}
LONG_MACRO(1, 2, 3)          (* → 1 + 2 + 3 *)
```

IEC 61131-3 の `$` エスケープ文字を行末に置いて次の行と連結することもできます。`$` + 改行は上記の空白化規則の対象外で、スペースを挿入せずにそのまま連結されます:

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

### 隣接文字列リテラルの結合 / Adjacent String Literal Merging

同じ引用符（`'...'` 同士、または `"..."` 同士）のリテラルが、空白のみを挟んで隣接している場合、字句解析時に 1 個のリテラルトークンへ自動的に結合されます（連結演算子は不要です）。コメントを挟むと結合されません。異なる引用符同士（`'...'` と `"..."`）も結合されません。

```
'ab'   'cd'         (* → 'abcd'   と等価に結合される *)
'ab' (*x*) 'cd'      (* → コメントに阻まれ結合されない *)
"ab" 'cd'           (* → 引用符が異なるため結合されない *)
```

### 引数の引用符 / Argument Quoting

ディレクティブの引数における引用符の扱いはディレクティブごとに異なります。

| ディレクティブ | 受理する形式 | 備考 |
|--------------|-------------|------|
| `{#include}` / `{#sinclude}` | `'file'` / `"file"` / `<file>` | `<file>` はシステムパス限定（§7.3） |
| `{#syspath}` | `'dir'` / `"dir"` | `<dir>` も同じ意味で受理される。相対パスはディレクティブを含むファイルのディレクトリが基準（§7.4）。出力への再展開はソースに書かれたパスのまま、常に `'...'` 形式 |
| `{#line}` とそのエイリアス | `'file'` / `"file"` | `<file>` は不可（`INVALID_SETLINE_OPERAND`、`PP41`）。`$` エスケープはデコードされる（§9） |
| `__has_include` | `'file'` / `"file"` / `<file>` | |
| `{#error}` / `{#warning}` / `{#info}` / `{#severe}` | 引用符処理なし | 引用符も他の文字と同様にメッセージの一部として扱われる。引用符なしでも構わない（§8） |
| `{#string}` / `{#wstring}` | — | 出力は常に `'...'` / `"..."` |

---

## 3. マクロ定義 / Macro Definitions

マクロ名は大文字・小文字を区別します（C プリプロセッサと同じ）。IEC 61131-3 の識別子は本来大文字・小文字を区別しませんが、Jiepp は `{#define}` / `{#ifdef}` / 展開のすべてで一貫して大文字・小文字を区別する現在の仕様を意図的に採用しています。

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

引数を区切るカンマは `(...)` だけでなく `[...]` の内側でも保護されます（Jiepp 拡張。C プリプロセッサは丸括弧のみをネスト対象とします、§17）:

```
{#define F(x) x}
F(arr[1,2]);     (* → arr[1,2]; 配列添字内のカンマは引数区切りと見なされない *)
```

#### 関数マクロ呼び出しの引数内ディレクティブ / Directives Inside a Function-Macro Call's Argument List

マクロ呼び出しの引数を収集している最中に現れたディレクティブは、パラメータの出現ごとに（マクロ本体展開時に）再実行されるのではなく、収集時に一度だけ実行され、引数トークン列からは除去されます。

```
{#define F(x) [x] [x]};F(
{#warning side-effect}
Y)   (* → [Y] [Y]; 警告は1回だけ発行される（2回ではない） *)
```

制御ディレクティブ（`{#if}` / `{#elif}` / `{#else}` / `{#endif}` / `{#ifdef}` / `{#ifndef}`）は、パラメータの出現回数によって実行回数が変わってしまう（0回、または複数回）ため、引数リスト内での使用は `OPERATION_NOT_ALLOWED` (`PP02`) エラーで拒否されます。

同様に、出力を直接生成するディレクティブ（`{#include}` / `{#sinclude}` / `{#string}` / `{#wstring}` / `{#token}` / `{#line}` / `{#syspath}`）も引数リスト内では拒否されます。これらのディレクティブの出力は、収集中のマクロ呼び出し自身の展開結果より先に出力ストリームへ押し出されてしまい、出力順序を破壊するためです:

```
{#define F(x) [x]};F(
{#include 'other.iec'}
Y)   (* → OPERATION_NOT_ALLOWED: 'include' inside macro argument: ... *)
```

一方、状態のみを変更するディレクティブ（`{#define}` / `{#undef}` / `{#warning}` / `{#error}` / `{#info}` / `{#severe}` / `{#ignore}` / `{#pragma once}` / `{#nop}` / `{#max_*}` 制限ディレクティブ / `{#pp_output_pragma_style}`）は、そのハンドラが出力トークンを一切生成しないため、引数リスト内でも許可されます。`-dD` 指定時は、引数リスト内の `{#define}`/`{#undef}` もマクロ呼び出し自身の展開結果より先にインライン出力される点に注意してください。

`OPERATION_NOT_ALLOWED` (`PP02`) は SEVERE 分類のため、`{#ignore PP02}` を指定しても抑制されません（`{#ignore}` は SEVERE コードには効果がありません、§10）。上記の制御ディレクティブ・出力ディレクティブの拒否は、`{#ignore}` によって無効化できない不変条件です。

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
| `__VA_OPT__(tokens)` | `__VA_ARGS__` が空なら何も展開しない。空でなければ `tokens` に展開される |

`__VA_ARGC__` は単独で本体に現れる場合だけでなく、`@@`（トークン連結、§4.3）や `@`（文字列化、§4.1）の直接のオペランドとして現れる場合も、常に個数（整数）に展開されます。可変長引数そのもののテキストが貼り付け・文字列化されることはありません:

```
{#define T(...) X @@ __VA_ARGC__}
T(a, b);              (* → X2; ("a,b" ではなく引数の個数) *)
T();                  (* → X0; *)

{#define S(...) @__VA_ARGC__}
S(a, b);              (* → '2'; *)
```

```
PRINT(1, 2, 3);      (* → print(1,2,3); *)
n := COUNT(a, b, c);  (* → 3 *)
```

`__VA_ARGS__` が空のとき、それを `@@`（トークン連結、§4.3）の右辺に置くと C17 のプレースマーカー規則が適用されます。直前のカンマは削除されず、そのまま残ります:

```
{#define LOG(fmt, ...) g(fmt, @@ __VA_ARGS__)}
LOG(x);   (* → g(x,); カンマが残る（GNU の comma-swallowing 拡張とは異なる） *)
```

末尾のカンマを削除したい場合は `__VA_OPT__`（§3.4）を使用してください。

### 3.4 `__VA_OPT__` — 条件付き可変長引数展開

```
{#define LOG(fmt, ...) write_log(fmt __VA_OPT__(,) __VA_ARGS__)}
LOG('error');          (* → write_log('error') — 末尾コンマなし *)
LOG('code=%d', 42);    (* → write_log('code=%d', 42) *)
```

- `__VA_OPT__` は可変長引数マクロ（`...` を持つマクロ）の本体内でのみ使用可能
- `__VA_ARGS__` が空のとき → `__VA_OPT__(tokens)` は何も生成しない
- `__VA_ARGS__` が非空のとき → `__VA_OPT__(tokens)` は `tokens` に展開される
- `@@`（トークン連結）および `@`（文字列化）の引数としても使用可能:
  - `prefix @@ __VA_OPT__(suffix)` — `__VA_ARGS__` 空のとき `prefix`、非空のとき `prefixsuffix`
  - `@__VA_OPT__(__VA_ARGS__)` — `__VA_ARGS__` 空のとき `''`、非空のとき文字列化した値
- `__VA_OPT__` のネストはエラー（PP37）

### 3.5 マクロ定義解除 / Undefine

```
{#undef NAME}
```

`NAME` の定義を解除します。未定義の名前に対する `{#undef}` はエラーになりません。
`defined` の再定義・定義解除は `OPERATION_NOT_ALLOWED` エラーとなります。

### 3.6 マクロ再定義 / Redefinition

同名のマクロを異なる内容で再定義すると `MACRO_REDEFINED` 警告が発行されます。同一内容の再定義は警告なしで受け入れられます。

「同一内容」の判定は C17 6.10.3p2 に従い、トークン間の空白の**有無**のみを比較し、空白の**量**（スペースの個数や改行の有無）は無視します。たとえば `{#define X 1 + 2}` の後に `{#define X 1  +  2}`（スペース2個）と再定義しても警告は発行されませんが、`{#define Y 1+2}` の後に `{#define Y 1 + 2}`（空白の有無が変わる）と再定義すると `MACRO_REDEFINED` が発行されます。

### 3.7 再帰防止 / Recursion Prevention

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
| LF（改行） | `$n` |
| CR（復帰） | `$r` |
| タブ | `$t` |
| フォームフィード | `$p` |

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

#### プレースマーカー規則 / Placemarker Rule

`@@` の左辺または右辺となる仮引数に対応する実引数が空の場合、C17 6.10.3.3p2 の「プレースマーカー」規則が適用されます。プレースマーカーは「何もないトークン」として扱われ、連結相手だけが残ります。

```
{#define F(a,b) a@@b}    F(x,);    (* → x;   （bが空 → プレースマーカー） *)
{#define F(a,b) z a@@b}  F(,y);    (* → z y; （aが空 → プレースマーカー。zとの間の空白は保持される） *)
{#define F(a,b) a@@b}    F(,);     (* → ;    （両辺が空 → プレースマーカー同士の連結は空） *)
{#define r(x,y) x @@ y}  r(4,);    (* → 4 *)
                         r(,5);    (* → 5 *)
                         r(,);     (* → （空） *)
```

3 個以上の連結が連鎖する場合、中間のプレースマーカーは両側の非空トークンを橋渡しします:

```
{#define F(a,b,c) a@@b@@c}  F(x,,y);   (* → xy; *)
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
| 整数 | `42`, `-1`, `0`, `1_000_000` | 10進整数（桁区切り `_` 使用可） |
| 浮動小数点 | `3.14`, `-1.5e10`, `1_234.567_8` | IEEE 754 倍精度（桁区切り `_` 使用可） |
| 真偽値 | `TRUE`, `FALSE`, `true`, `false` | ブール値 |
| ビット列 | `BYTE#16#ff`, `WORD#2#1010`, `DWORD#8#77` | IEC 61131-3 ビット列リテラル (BYTE/WORD/DWORD/LWORD) |
| N進数 | `16#ff`, `2#1010`, `8#77` | 2〜36進数リテラル（桁区切り `_` 使用可） |

IEC 61131-3 の桁区切り文字 `_` は、10進整数・浮動小数点・N進数リテラルの数字の間に挿入できます（先頭・末尾は不可）。評価前に取り除かれるため、`1_000` と `1000` は同じ値に評価されます。

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

候補パスがディレクトリである場合はファイルとして存在しないものとして扱われ、次の候補の検索が継続されます。全候補を検索してもファイルが見つからず、いずれかの候補がディレクトリだった場合は `FILE_NOT_FOUND` (`PP11`) の代わりに `INCLUDE_TARGET_IS_DIRECTORY` (`PP14`) エラーになります。

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

相対パス（`dir`）は、`{#syspath}` ディレクティブ**そのものを含むファイル**のディレクトリを基準に解決されます。`{#include 'file'}` の相対パス解決と同じ規則です（§7.1、§7.9）。

- 絶対パスはそのまま使用されます（`{#include}` と同様）
- ディレクティブを処理している時点で物理的な「現在のファイル」が存在しない場合（文字列入力・標準入力からの読み込みなど）は、プロセスの CWD（カレントディレクトリ）を基準にします
- `{#syspath ''}`（空文字列）は「宣言元ファイル自身のディレクトリ」を意味します
- `{#line}` によるファイル名の書き換えは診断メッセージ上の表示パスのみに影響し、`{#syspath}` の基準ディレクトリには影響しません
- マクロ展開後にパスが確定する場合（例: `{#syspath MY_LIB_PATH}`）も、基準は展開結果ではなく `{#syspath}` ディレクティブを実際に含むファイルのディレクトリです
- 出力に再展開される `(*{syspath:'...'}*)` プラグマは、解決後の絶対パスではなく、ソースに書かれたパス文字列をそのまま再出力します（§9「行マーカーの意味」と同様、情報提供目的のコメントです）

### 7.5 `__has_include` — インクルード存在チェック

```
{#if __has_include('file.iec')}
  {#include 'file.iec'}
{#endif}
```

指定ファイルがインクルードパスに存在すれば `1`、なければ `0` に評価されます。`"file"` 形式は `{#include}` 検索順序、`<file>` 形式は `{#sinclude}` 検索順序を使用します。候補がディレクトリである場合もファイルとして存在しないため `0` に評価されます。

### 7.6 `{#pragma once}` — 重複インクルード防止

```
{#pragma once}
```

ファイルの先頭付近に記述することで、そのファイルが複数回インクルードされても、2回目以降はスキップされます。

- 判定は **正規化パス（canonical path）** で行われます。シンボリックリンクやジャンクションを解決し、Windows ではファイル名の大文字・小文字表記も正規化するため、異なるパス表記（`../a.iec` や `./dir/../a.iec`、Windows での大文字・小文字違いのスペリング等）でインクルードされても同じファイルとして扱われます
- ダイアモンドインクルード（A→B, A→C, B→D, C→D の依存関係で D が重複する場合）も正しく防止されます
- `{#pragma once}` は `{#sinclude}` / `{#include <...>}` でインクルードされたファイルにも有効です

```
(* header.iec *)
{#pragma once}
VAR CONSTANT MAX_SIZE : INT := 100; END_VAR
```

```
(* main.iec *)
{#include 'header.iec'}
{#include 'header.iec'}   (* 2回目はスキップされる *)
```

**マクロガードとの比較**:

| 方法 | 利点 | 欠点 |
|------|------|------|
| `{#pragma once}` | 簡潔。ファイルパスベースで確実 | エディタがコピーしたファイルを同一視する場合あり |
| マクロガード | ポータブル | 記述が冗長。同名マクロとの衝突リスク |

### 7.7 マクロベースのインクルードガード / Macro-Based Include Guards

`{#pragma once}` の代替として、マクロを使ったインクルードガードも使用できます:

```
{#ifndef MYHEADER_INCLUDED}
{#define MYHEADER_INCLUDED 1}
  (* ヘッダ内容 *)
{#endif}
```

### 7.8 インクルード深度制限 / Include Depth Limit

デフォルト: **100**。`--max-include-depth N` オプションまたは `{#max_include_depth N}` ディレクティブで変更可能。超過すると `MAX_INCLUDE_DEPTH_EXCEEDED` エラー。

> **注意**: Jiepp は循環インクルードを即座に検出する仕組みを備えていません。代わりに、深度制限に到達した際に PP12 エラーで処理を停止することにより、無限ループを防止しています。実質的に、循環インクルードはこの深度制限により検出・防止されます。

### 7.9 相対パスの基準 / Relative Path Base

各インクルード関連機能が相対パスをどこ基準に解決するかを示します。「gcc/clang」列は、対応する gcc/clang の挙動と一致するかどうかを示します。

| 対象 | 基準 | gcc/clang |
|------|------|-----------|
| `{#include 'file'}` / `{#include "file"}` | 最も内側の現在のファイルのディレクトリ → 見つからなければ `-I` / `{#syspath}`（追加順） | 一致 |
| `<file>` / `{#sinclude 'file'}` | `-I` / `{#syspath}` のみ（現在のファイルのディレクトリは検索しない） | 一致 |
| `-I dir` | プロセス起動時の CWD（カレントディレクトリ） | 一致 |
| `-include FILE` | プロセスの CWD を優先し、見つからなければ `-I` / `{#syspath}` を検索（`{#include 'file'}` と同じ検索順） | 一致 |
| `__has_include(...)` | 対応する `{#include}` の形式（`'file'`/`"file"` または `<file>`）と同じ検索順序（§7.5） | 一致（`'file'` 形式を受理する点は拡張） |
| 標準入力 (`-`) | プロセスの CWD | 一致 |
| `{#syspath 'dir'}` | ディレクティブを**含むファイル**のディレクトリ（`{#include 'file'}` と同じ規則）。現在のファイルが無い場合（文字列入力・標準入力）はプロセスの CWD | 対応する概念なし。`{#include}` と同じ相対パス規則を踏襲 |

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
- 上記の例にある引用符（`'...'`）は慣例的な記述であり、実際には取り除かれません。引用符も含めたメッセージ全体がそのまま扱われます（§2「引数の引用符」参照）

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
- ファイルパスを `<...>` 形式（アングルブラケット）で指定することはできません。指定すると `INVALID_SETLINE_OPERAND` (`PP41`) エラー
- 第2引数の後に余分なトークンがあると `INVALID_SETLINE_OPERAND` (`PP41`) エラー（第2引数までで構文が終了している必要がある）
- ファイルパスにスペースを含めることができます（クォートで囲まれているため区切り文字として扱われません）
- ファイルパス内の `$` エスケープ（§4.1 参照）はデコードされます
- 出力される行マーカーおよび `__FILE__` の値は、第2引数をシングルクォートとダブルクォートのどちらで指定した場合でも、常にシングルクォート `'...'`（IEC エスケープ適用済み）で再エンコードされます

行番号変更は `__LINE__` マクロと、出力のプラグマ行に反映されます。

### 行マーカーの意味 / Line Marker Semantics

出力される `(*{#:N 'file'}*)` (annotated) / `{#:N 'file'}` (standard) は「行カウンタが N になった」ことを表します。続く行の行番号は N+1 です。

gcc の `# N "file"` は逆に「次の行が行番号 N である」ことを表すため、両者は 1 だけ異なります: `jiepp の N == gcc の N − 1`（§17 参照）。

例（§1 の空行圧縮を伴う）:

```
(*{#:0 'blank_run.iec'}*)
A;
(*{#:13 'blank_run.iec'}*)
B 14;
```

1行目のマーカーはカウンタが 0 であることを示し、次の `A;` は行番号 1 です。3行目のマーカーはカウンタが 13 であることを示し、次の `B 14;` は行番号 14 です（`__LINE__` の値と一致）。`A;` と `B 14;` の間にあった 12 行の空行（インデント付き `{#define}` 行が展開で消えた結果の空行）は、既定の圧縮閾値（8 行以上）を超えたため 1 行のマーカーに置き換えられています。

空行圧縮は内容を持つ行の行番号を変更しません。マーカーは行カウンタを実際の物理行番号に再同期させるだけです。

---

## 10. エラー抑制 / Issue Suppression

### `{#ignore}`

```
{#ignore PP41}
```

指定したエラーコード（`PP` + 2桁番号）の診断メッセージを抑制します。エラーコード値は §16 のエラーコード一覧を参照してください。

オペランドは1ディレクティブにつき正確に1個の `PPnn`（`PP` の後に数字ちょうど2桁）でなければなりません。それ以外（コード番号の欠如、3桁以上、数字以外の文字混入、複数コードの列挙、小文字の `pp` 等）は `INVALID_IGNORE_OPERAND` (`PP42`) エラーになります。末尾のコメントは他の制限ディレクティブと同様に無視されます（例: `{#ignore PP41 (* コメント *)}` は有効）。正しい形式の `PPnn` であれば、未割当・廃止番号（例: PP03。§16 参照）も受理し、効果はありません。

構造的エラーコード（`PP24`〜`PP27`: 未終了の `{#if}`・位置エラーの `{#elif}`/`{#else}`/`{#endif}`）を `{#ignore}` で抑制した場合でも、条件コンパイル処理自体が不安定になることはありません。診断メッセージは抑制されますが、対応する構文異常（不整合な `{#endif}` によるスタック破損等）は内部で安全に無視され、抑制対象の行以降のコンテンツも通常どおり出力されます。

---

## 11. ランタイム制限ディレクティブ / Runtime Limit Directives

ソースコード内からプリプロセッサの動作制限を設定できます。CLI オプションの `--` 接頭辞付きバージョンも同等の機能を提供します。

| ディレクティブ | CLI オプション | デフォルト | 説明 |
|--------------|--------------|-----------|------|
| `{#max_include_depth N}` | `--max-include-depth N` | 100 | インクルード最大ネスト深度 |
| `{#max_expansion_depth N}` | `--max-expansion-depth N` | 256 | マクロ展開深度上限 |
| `{#max_if_nesting N}` | `--max-if-nesting N` | 256 | 条件分岐ネスト深度上限 |
| `{#max_blank_lines N}` / `{#max-blank-lines N}` | `--max-blank-lines N` | 7 | 連続空行の圧縮閾値（`0` で圧縮を無効化、§1・§9） |
| `{#pp_output_pragma_style STYLE}` | `--pp-output-pragma-style STYLE` | `annotated` | プラグマ出力スタイル |

CLI オプションで設定した値はソースコード内のディレクティブで上書きできません（ロックされます）。

`{#max_blank_lines}` には他の制限ディレクティブと異なる非自明な意味論があります: 空行圧縮は展開が完了した**後**に一度だけ行われ、その時点の値（＝ファイル内で最後に実行された `{#max_blank_lines}` の値）が出力全体に適用されます。したがって、ファイルの先頭付近にある空行区間であっても、ファイル後方の `{#max_blank_lines}` によって圧縮挙動が決まります（`{#max_if_nesting}` のように展開中に逐次適用される制限とは異なります）。

### プラグマ出力スタイル / Pragma Output Style

| 値 | 出力例 | 説明 |
|----|--------|------|
| `annotated` | `(*{#:100 file.iec}*)` | IEC 61131-3 コメントで囲まれ、処理系に影響しない |
| `standard` | `{#:100 file.iec}` | プラグマ形式で出力される |

`{#pp_output_pragma_style}` ディレクティブに `annotated` / `standard` 以外の値を指定した場合、`INVALID_PRAGMA_STYLE_OPERAND` (`PP48`) 警告が発行され、その指定は無視されます（現在のスタイルを維持したまま処理を継続）。CLI の `--pp-output-pragma-style` に不正な値を指定した場合は `INVALID_OPTION_VALUE` (`PP71`) エラーで即座に終了します。

ディレクティブのオペランドは（他の `{#max_*}` 制限ディレクティブと同様に）トークン化された上でコメント・空白が取り除かれるため、末尾にコメントが付いていても値として認識されます（例: `{#pp-output-pragma-style standard (* コメント *)}` は `standard` として扱われます）。トークン化後に残る非空白トークンが1個ちょうどでなく、かつそれが `standard`/`annotated` のいずれかと一致しない場合は不正な値として扱われます。

> **注意（既知の制限）**: 8 行以上の空行区間が圧縮されるとき（§1・§9）、挿入される圧縮マーカーのスタイルは直近に出力された行マーカー（ファイル/インクルード境界のマーカー等）のスタイルを引き継ぎます。`{#pp_output_pragma_style}` によるスタイル切り替え自体は行マーカーを生成しないため、切り替え直後・次の行マーカーが現れる前に 8 行以上の空行が生じた場合、そこに挿入される圧縮マーカーは切り替え「前」のスタイルで出力されます。

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

関数マクロの呼び出しが複数行にまたがる場合、その置換本体内または引数内に現れる `__LINE__` は、呼び出しの閉じ括弧 `)` がある行番号に評価されます（gcc/clang と同じ挙動）。呼び出しより後ろに書かれた `__LINE__` の評価には影響しません。

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
| `__DWORD_MASK__` | `dword#16#ffff_ffff` |
| `__LWORD_MASK__` | `lword#16#ffff_ffff_ffff_ffff` |

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
| `__INT8_TYPE` | `sint` |
| `__UINT8_TYPE` | `usint` |
| `__BITS8_TYPE` | `byte` |
| `__INT16_TYPE` | `int` |
| `__UINT16_TYPE` | `uint` |
| `__BITS16_TYPE` | `word` |
| `__INT32_TYPE` | `dint` |
| `__UINT32_TYPE` | `udint` |
| `__BITS32_TYPE` | `dword` |
| `__INT64_TYPE` | `lint` |
| `__UINT64_TYPE` | `ulint` |
| `__BITS64_TYPE` | `lword` |

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
| `-o PATH` | 出力先ファイルパス。`-` は標準出力を意味する（gcc/clang と同様。文字通り `-` という名前のファイルは作成されない） | stdout |
| `-D NAME[=VALUE]` | マクロ定義。`-D NAME` は `NAME=1` と同等。値・名前を丸ごと省略した場合（末尾の裸の `-D`、または `-D ""` のように空文字列を値として渡した場合のいずれも）は `MISSING_OPTION_VALUE` エラー（`PP72`）。`-D=VALUE` のようにマクロ名だけが空の場合は `INVALID_MACRO_DEF` エラー（`PP73`） | — |
| `-U NAME` | マクロ定義の取り消し（`-D` の後に適用）。値を省略した場合（末尾の裸の `-U`、または `-U ""` のように空文字列を値として渡した場合のいずれも）は `MISSING_OPTION_VALUE` エラー（`PP72`、`-D` の裸指定と同じコード） | — |
| `-I PATH` | インクルード検索パスの追加。`PATH` は宣言元ファイルではなく、常に**プロセス起動時の CWD（カレントディレクトリ）** を基準に解決される（`{#syspath}` ディレクティブとは異なる。§7.4、§7.9） | — |
| `-include FILE` | 入力ファイルの前に強制インクルード | — |
| `-P` | 行マーカー出力を抑制（`(*{#:...}*)` / `{#:...}` を出力しない）。連続空行もすべて除去される | off |
| `-w` | 警告メッセージの抑制（エラーは出力） | off |
| `-Werror` | 警告をエラーに昇格 | off |
| `-M` | Makefile 依存関係ルールを出力（全インクルード）。**プリプロセス結果は出力されない**（依存関係ルールのみ）。`-MF` を伴わない場合、ルールの出力先は `-o`（未指定時は stdout）。詳細は後述「依存関係ファイルの Make エスケープ」参照 | off |
| `-MM` | `-M` と同様だがシステムインクルードを除外。**プリプロセス結果は出力されない** | off |
| `-MD` | プリプロセス出力と同時に依存関係ファイル（`.d`）を自動生成。`-M`/`-MM` と異なりプリプロセス結果も出力される | off |
| `-MMD` | `-MD` と同様だがシステムインクルードを除外 | off |
| `-MF FILE` | 依存関係ルールの出力先ファイル。書き込みは `-o` を開く前に行われるため、この書き込みが失敗しても `-o` は一切変更されない（後述） | stdout |
| `-MT TARGET` | 依存関係ルールのターゲット名（`-MF` は出力先のみ変更し、ターゲット名には影響しない）。**指定した文字列はそのまま（エスケープなし）でルール先頭に出力される**（後述） | 入力ファイル名（拡張子を `.output` に変更、Make エスケープ適用済み） |
| `--max-include-depth N` | インクルード深度上限 | 100 |
| `--max-expansion-depth N` | マクロ展開深度上限 | 256 |
| `--max-if-nesting N` | 条件分岐ネスト深度上限 | 256 |
| `--max-blank-lines N` | 圧縮せず出力する連続空行の最大数（`0` で圧縮を無効化） | 7 |
| `--pp-output-pragma-style STYLE` | プラグマ出力スタイル (`annotated` / `standard`)。不正な値は `INVALID_OPTION_VALUE` (`PP71`) エラー | `annotated` |
| `--remove-comments` / `-nC` | コメントを除去 | off |
| `-dM` | 定義されたマクロの一覧を出力（プリプロセス結果は出力しない） | off |
| `-dD` | プリプロセス出力に `{#define}` / `{#undef}` 行をインライン挿入（`-dM` の処理中版） | off |
| `--silent` | 全ての診断メッセージを抑制 | off |
| `--recursion-limit N` | OS スタックサイズの設定 (N × 約 8KB) | システムデフォルト |
| `--disppath PATH` | 診断メッセージ・行番号プラグマに表示するパスを、実際の入力ファイルパスと切り離して上書きする（内部・テスト専用。`--help` には表示されない） | 実際の入力ファイルパス |
| `--` | オプションの終端（以降は全てファイルパスとして扱う） | — |
| `--help` / `-h` | ヘルプメッセージの表示 | — |
| `--version` | バージョン番号の表示 | — |

未知のオプションはエラーとして処理され、即座に終了します（gcc 互換）。

`-o` で指定したファイルは、プリプロセス処理全体（マクロ展開・ファイルインクルード）が成功した後にのみ作成・上書きされます（§1 参照）。したがって `-o` に入力ファイルと同じパスを指定しても安全で、処理が途中でエラー終了した場合も `-o` の対象ファイルは変更されません。この保証は `-MF`/`-MD`/`-MMD` による依存関係ファイルの書き込み失敗にも及びます（依存関係ファイルは `-o` を開く前に書き込まれるため）。詳細は次項を参照してください。

### 依存関係ファイルの Make エスケープ / Dependency-File Make Escaping

`-M`/`-MM`/`-MD`/`-MMD` が出力する Makefile 依存関係ルールでは、ターゲット名・前提条件（依存先ファイル）のパスに含まれる次の文字を GNU Make の構文に従ってエスケープします（gcc/clang の `mkdeps` munge() と同じ規則）。

| 文字 | エスケープ後 |
|------|-------------|
| `$` | `$$` |
| `#` | `\#` |
| スペース | `\` + スペース（2 バイト: バックスラッシュ + 実際のスペース文字） |
| タブ | `\` + タブ（2 バイト: バックスラッシュ + 実際のタブ文字。`\t` という 2 文字の C 言語風表記ではない） |
| `:` | エスケープしない（Windows のドライブレター `C:/...` を壊さないため。gcc も同様にコロンは無条件） |

**`-MT TARGET` は上記のエスケープを一切適用せず、指定した文字列をそのままルール先頭に出力します**（gcc/clang の `-MT` と同じ動作）。エスケープ済みターゲットが必要な場合、gcc/clang には `-MQ` がありますが、jiepp は `-MQ` を実装していません。`-MT` を省略した場合に入力ファイル名（または `-o` のファイル名）から自動導出されるターゲット名は、上記の規則でエスケープされます。前提条件のパスは `-MT` の有無に関わらず常にエスケープされます。

**依存関係ファイルの書き込み順序（`-o` との関係）**: `-MF`（または `-MD`/`-MMD` による自動命名）で指定された依存関係ファイルは、プリプロセス処理全体の完了後・`-o` を開く前に書き込まれます。したがって依存関係ファイルの書き込みが失敗した場合（例: `-MF` が存在しないディレクトリを指す）、`-o` は一切開かれず、既存の `-o` ファイル（前回の成功結果など）はバイト単位で変更されません。逆に依存関係ファイルの書き込みには成功したものの、その後に `-o` を開く処理が失敗した場合（例: `-o` が存在しないディレクトリを指す）は、この実行が書き込んだ依存関係ファイルを削除します。これは、対応するプリプロセス結果を一切生成できなかった実行が、あたかも成功したかのような `.d` ファイルを残して Make ベースのビルドを誤解させることを防ぐためです（この実行が作成・上書きしていない、無関係な既存の `.d` ファイルは削除されません）。

依存関係のみモード（`-MD`/`-MMD` を伴わない `-M`/`-MM`）で `-MF` も指定していない場合、依存関係ルールは `-o`（未指定時は標準出力）にそのまま出力されます（プリプロセス結果自体は出力されません）。逆に依存関係のみモードで `-MF` を指定した場合、`-o` に書き込むべき内容が何も残らないため、`-o` は開かれず、作成も上書きもされません（gcc が `-M -MF` 実行時にコンパイラ本来の出力ファイルに一切触れないのと同じ考え方です）。

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

# プリプロセスと同時に依存関係ファイルを自動生成
jiepp -MD input.iec -o output.iec       # output.d が自動生成される
jiepp -MMD input.iec -o output.iec      # システムインクルードを除外

# 行マーカーを出力しない（-P）
jiepp -P input.iec -o clean.iec

# プリプロセス中のマクロ定義をインライン出力（-dD）
jiepp -dD input.iec

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
| `{#max_expansion_depth}` | 制限緩和の防止 |
| `{#max_if_nesting}` | 制限緩和の防止 |
| `{#max_blank_lines}` | 制限緩和の防止 |
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
| `{#pragma once}` | §7.6 | 重複インクルード防止 |
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
| `{#max_blank_lines N}` / `{#max-blank-lines N}` | §11 | 連続空行の圧縮閾値設定 |
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
| PP04 | `PARAMETER_VALUE_OVERFLOW` | SEVERE | Parameter value exceeds maximum (2^24) | パラメータ値が上限（2^24）を超過 |
| PP10 | `FILE_ERROR` | ERROR | An error occurred with the file | ファイル操作エラー |
| PP11 | `FILE_NOT_FOUND` | ERROR | No such file or directory | ファイルが見つからない |
| PP12 | `MAX_INCLUDE_DEPTH_EXCEEDED` | ERROR | Maximum include depth exceeded | インクルード深度上限超過 |
| PP13 | `INVALID_COMMAND` | ERROR | Invalid command | 無効なコマンド |
| PP14 | `INCLUDE_TARGET_IS_DIRECTORY` | ERROR | Include target is a directory | インクルード対象がディレクトリ |
| PP20 | `UNCLOSED_COMMENT` | ERROR | Unclosed comment | コメントが閉じられていない |
| PP21 | `INVALID_ESCAPE_SEQUENCE` | ERROR | Invalid escape sequence | 不正なエスケープシーケンス |
| PP22 | `INVALID_PRAGMA_SYNTAX` | ERROR | Invalid syntax for pragma | プラグマの構文エラー |
| PP23 | `INVALID_PP_SYNTAX` | ERROR | Invalid preprocessor syntax | プリプロセッサの構文エラー（汎用） |
| PP24 | `UNTERMINATED_CONDITIONAL` | ERROR | Unterminated conditional directive | `{#if}` が閉じられていない |
| PP25 | `ELIF_ERROR` | ERROR | Unexpected elif directive | `{#elif}` の位置エラー |
| PP26 | `ELSE_ERROR` | ERROR | Unexpected else directive | `{#else}` の位置エラー |
| PP27 | `ENDIF_ERROR` | ERROR | Unexpected endif directive | `{#endif}` の位置エラー |
| PP28 | `WHITESPACE_BEFORE_DIRECTIVE` | WARNING | Whitespace between '{' and '#'; treated as an ordinary pragma | `{` と `#` の間に空白があり、通常のプラグマとして扱われた（§2。コメントを挟んだ場合はこの警告は出ない） |
| PP30 | `INVALID_DEFINE_SYNTAX` | ERROR | Invalid define syntax | `{#define}` の構文エラー |
| PP31 | `INVALID_STRINGIZING` | ERROR | Invalid stringizing (@) | 不正な文字列化演算子 |
| PP32 | `INVALID_TOKEN_PASTING` | ERROR | Invalid token pasting (@@) | 不正なトークン連結演算子 |
| PP33 | `INVALID_VARIADIC_PLACEMENT` | ERROR | '...' must be the last parameter | 可変長引数が最後のパラメータでない |
| PP34 | `ARGUMENT_COUNT_MISMATCH` | ERROR | Argument count mismatch | 関数マクロの引数数不一致 |
| PP35 | `MACRO_REDEFINED` | WARNING | Macro redefined | マクロの再定義警告 |
| PP36 | `DUPLICATE_MACRO_PARAMETER` | ERROR | Duplicate macro parameter name | マクロパラメータ名の重複 |
| PP37 | `INVALID_VA_OPT` | ERROR | Invalid __VA_OPT__ | `__VA_OPT__` の不正な使用（非可変長マクロ内での使用・ネスト） |
| PP40 | `INVALID_DEFINED_OPERAND` | ERROR | Invalid operand for 'defined' | `defined` の不正なオペランド |
| PP41 | `INVALID_SETLINE_OPERAND` | ERROR | Invalid operand for 'set_line' | `{#set_line}` の不正なオペランド |
| PP42 | `INVALID_IGNORE_OPERAND` | ERROR | Invalid operand for 'ignore' | `{#ignore}` の不正なオペランド |
| PP43 | `INVALID_LIMIT_OPERAND` | ERROR | Invalid operand for limit directive | 制限ディレクティブの不正なオペランド |
| PP44 | `INVALID_PARAMETER_VALUE` | ERROR | Invalid parameter value | パラメータ値が不正（0以下等） |
| PP45 | `UNKNOWN_DIRECTIVE` | WARNING | Unknown directive | 未知のディレクティブ |
| PP46 | `INVALID_DIRECTIVE_NAME` | ERROR | Invalid directive name | 不正なディレクティブ名（先頭が数字等） |
| PP47 | `INVALID_PATH` | ERROR | Invalid path | 不正なパス |
| PP48 | `INVALID_PRAGMA_STYLE_OPERAND` | WARNING | Invalid operand for pragma style directive | `{#pp_output_pragma_style}` の不正なオペランド |
| PP50 | `EXPR_TYPE_ERROR` | ERROR | Type error in expression | 式中の型エラー（ゼロ除算等） |
| PP51 | `MISSING_EXPRESSION` | ERROR | Missing expression | `{#if}` に式がない |
| PP52 | `INVALID_EXPRESSION` | ERROR | Invalid expression | 不正な式 |
| PP60 | `MAX_EXPANSION_DEPTH_EXCEEDED` | ERROR | Maximum expansion depth exceeded | マクロ展開深度上限超過 |
| PP61 | `MAX_IF_NESTING_EXCEEDED` | ERROR | Maximum conditional nesting depth exceeded | 条件分岐ネスト深度上限超過 |
| PP62 | `SANDBOX_RESTRICTED_DIRECTIVE` | ERROR | Directive is restricted in sandbox mode | サンドボックスモードで禁止されたディレクティブ |
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

PP03 は欠番（廃止済み、再利用しない）。

---

## 17. C プリプロセッサとの対応 / C Preprocessor Correspondence

Jiepp は C プリプロセッサ (cpp) の概念を IEC 61131-3 に適応させています。以下は対応表です。

| C プリプロセッサ | Jiepp | 備考 |
|-----------------|-------|------|
| `#define` | `{#define}` | 同等 |
| `#undef` | `{#undef}` | 同等 |
| `#include "file"` | `{#include 'file'}` | IEC 61131-3 の文字列リテラルは `'...'`（STRING）と `"..."`（WSTRING）の2種類。Jiepp のパス引数はどちらも同じ意味で受け付ける（§2「引数の引用符」参照） |
| `#include <file>` |  `{#include <file>}` / `{#sinclude 'file'}` | 同等 |
| `-include FILE` | `-include FILE` | 同等。指定ファイルを主入力ファイルより前に処理する。相対パスは CWD を優先し、見つからなければ `-I` / `{#syspath}` を検索（§7.9） |
| `#if` / `#elif` / `#else` / `#endif` | `{#if}` / `{#elif}` / `{#else}` / `{#endif}` | 同等 |
| `#ifdef` / `#ifndef` | `{#ifdef}` / `{#ifndef}` | 同等 |
| `#error` / `#warning` | `{#error}` / `{#warning}` | 同等 |
| `#line N "file"` | `{#line N 'file'}` / `{#line N "file"}` | 両方のクォートを同じ意味で受理（エイリアス `set_line`/`set-line` も同等）。出力される行マーカーと `__FILE__` は常に `'...'`（IEC エスケープ適用済み）で再エンコードされる（§9） |
| `#arg` (stringize) | `@arg` | IEC 61131-3 では `#` が別の意味を持つため |
| `##` (token paste) | `@@` | 同上 |
| `#pragma once` | `{#pragma once}` | 同等。正規化パス（symlink 解決・大小文字正規化）ベースで判定 |
| `__VA_OPT__(tokens)` | `__VA_OPT__(tokens)` | 同等。`__VA_ARGS__` 空→なし、非空→tokens展開 |
| `__VA_ARGS__` | `__VA_ARGS__` | 同等。ただし空の `__VA_ARGS__` を `@@` の右辺に置いた場合、gcc/clang の GNU 拡張（comma-swallowing、直前のカンマを削除）ではなく C17 のプレースマーカー規則（カンマを保持）を採用（§3.3）。gcc 相当の挙動が必要な場合は `__VA_OPT__` を使用 |
| `-P` | `-P` | 同等。行マーカー出力を抑制。連続空行もすべて除去される |
| `-dD` | `-dD` | 同等。処理中マクロ定義をインライン出力 |
| （内部動作、専用オプションなし） | `--max-blank-lines N` | Jiepp 拡張: 連続空行を既定 7 行までに制限し、8 行以上の区間は行マーカー 1 行に圧縮する。gcc/clang も内部的に同じ閾値（8 行以上）で空行を 1 行の `# N "file"` に圧縮するが、jiepp のマーカー N は gcc の `# N` より 1 小さい（§9 参照） |
| `-MD` / `-MMD` | `-MD` / `-MMD` | 同等。依存ファイルを自動生成（-MMD はシステムインクルード除外） |
| — | `__VA_ARGC__` | Jiepp 拡張: 可変長引数の個数 |
| `defined(NAME)` | `defined(NAME)` | 同等 |
| `__has_include(...)` | `__has_include(...)` | 同等 |
| `&&`, `\|\|`, `!` | `\and\`, `\or\`, `\not\` | IEC 61131-3 論理演算子形式 |
| — | `{#info}` / `{#severe}` | Jiepp 拡張 |
| — | `{#ignore CODE}` | Jiepp 拡張 |
| — | `{#nop}` | Jiepp 拡張 |
| — | `{#max_*}` 制限ディレクティブ | Jiepp 拡張 |
| — | `{#wstring}` | Jiepp 拡張: ワイド文字列化 |
| `(...)` のみカンマ保護 | `(...)` および `[...]` の両方でカンマ保護 | Jiepp 拡張: 関数マクロ引数を分割するカンマは丸括弧に加え角括弧の内側でも保護される（例: `F(arr[1,2])`、§3.2） |

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
               | 'ignore' | 'nop' | '' | 'syspath' | 'I' | 'pragma'
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
