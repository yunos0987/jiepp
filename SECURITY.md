# セキュリティポリシー / Security Policy

**English summary:** Please report security vulnerabilities privately through GitHub's private vulnerability reporting (the "Report a vulnerability" button in the repository's Security tab). Do not open a public issue for a vulnerability. Security fixes are made for the latest release and for the `develop` branch. The rest of this document is in Japanese.

## 脆弱性の報告方法

脆弱性は、公開の Issue や Pull Request では報告しないでください。GitHub の非公開の脆弱性報告（private vulnerability reporting）を使ってください。

1. リポジトリの **Security** タブを開く
2. **Report a vulnerability** を押す
3. 下記の内容を書いて送る

報告は管理者だけに届きます。修正が公開されるまで、報告内容は公開しないでください。

## 対象バージョン

セキュリティ修正の対象は、最新のリリースと `develop` ブランチです。古いリリースへの修正の取り込み（バックポート）は予定していません。現在のバージョンは [`VERSION`](VERSION) と [`vcpkg.json`](vcpkg.json) の `version`（現時点では 0.8.0）で確認できます。

## 報告に含めてほしい内容

- 影響を受けるバージョン（`jiepp --version` の出力、または commit ハッシュ）
- 使った OS、ビルド方法（preset 名、`JIEPP_SANDBOX` の有無）
- 問題を再現できる最小の入力ファイルとコマンドライン
- 起きたこと（クラッシュ、ハング、メモリの過大な消費、意図しないファイルの読み取りなど）と、想定していた動作
- 分かる場合は、原因の見立てや修正案

## 対象範囲

次のような問題を、セキュリティ上の問題として扱います。

- 信頼できない入力を jiepp で処理したときの、クラッシュ・メモリ破壊・無限ループ・過大なメモリ消費
- サンドボックスビルド（`JIEPP_SANDBOX=ON`、[`SPECIFICATION.md` §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode)）で、無効化されているはずのファイルシステムアクセスができてしまうこと

通常ビルドの jiepp は、`{#include}` などで任意のファイルを読めます。これは仕様です。Web サーバーなどで信頼できない入力を処理する場合は、サンドボックスビルドを使ってください。

### サンドボックスについての注意

サンドボックスビルドは、ファイルシステムにアクセスするディレクティブを無効にします。ただし、jiepp に渡したファイルパスを隠す機能はありません。`__FILE__` や診断メッセージには、渡したパスがそのまま表示されます。秘密にしたいパスは jiepp に渡さないでください。

### 資源の上限についての注意

メモリ使用量は入力サイズに比例するとは限らず、入力よりはるかに大きくなることがあります。実測では、`a$n` を並べた 60 MB の入力で 6.3 GB、`{#define F(x) x}` の後に `F(` を 12500 個、`1`、`)` を 12500 個並べた 37.5 KB の入力で約 1.05 GB を使いました（`PP60` で止まるまで）。

信頼できない入力を受け付ける場合は、次をすべて行ってください（[`SPECIFICATION.md` §14](SPECIFICATION.md#14-サンドボックスモード--sandbox-mode) の運用要件）。

- 呼び出し側で入力サイズを制限する
- 上限を既定値より小さくする。例: `--max-expansion-steps 1048576`、`--max-expansion-depth 16`（既定値はそれぞれ 16777216 と 256）。上の入れ子の入力は、前者で 100000 段の場合に 1.88 GB から 158 MB、後者で 12500 段の場合に約 1.05 GB から 75 MB になりました
- OS の機能（`ulimit`、cgroup、Windows のジョブオブジェクトなど）でメモリと実行時間を制限する
- 終了コードが 0 以外のときは、標準出力を使わずに捨てる。上限などで中止した場合も、それまでの出力が書き出されている

### 対象外

- jiepp が出力した変換結果を、利用者が別のツールで処理するときに起きる問題
- 通常ビルドで、利用者自身が指定した入力やオプションによってファイルが読み書きされること
