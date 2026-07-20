# incbin Build Dependencies

> **実装保留中:** 本ファイルは検討中の設計方針を記録するものであり、現時点ではビルドスクリプト、`platformio.ini`、`WebAPI.cpp`の変更を行わない。実装開始前に方式と警告条件を再レビューする。

## 目的

`incbin`内のHTML/JavaScriptを変更したときに、対象ファイルを埋め込む`WebAPI.cpp`が自動的に再コンパイルされるようにする。

また、今後Web UIファイルを追加した際に`IMPORT_FILE()`への登録を忘れた場合、ビルド時に検出できるようにする。

## 現状と課題

`WebAPI.cpp`の`IMPORT_FILE()`は、インラインアセンブラの`.incbin`ディレクティブで`incbin`内のファイルを取り込んでいる。

`.incbin`のファイルパスはC/C++の通常のinclude依存解析では認識されないため、HTML/JavaScriptだけを変更しても`WebAPI.cpp`が再コンパイルされず、変更前の内容がファームウェアへ残る場合がある。

依存ファイル一覧をビルドスクリプト側へ手作業で重複定義すると、Web UIファイル追加時に一覧の更新を忘れる可能性が残る。

## 対象範囲

- `src/WebAPI.cpp`内の`IMPORT_FILE()`による埋め込み
- `incbin/*.html`
- `incbin/*.js`
- PlatformIOの追加ビルドスクリプト
- 全PlatformIOビルド環境で共通となる`extra_scripts`設定

Web APIのルーティング、MIME type、HTML/JavaScriptの内容、設定保存処理は本作業の対象外とする。

## 想定する主な変更ファイル

- `platformio.ini`
- 新規PlatformIOビルドスクリプト（配置先とファイル名は実装前に決定する）
- `src/WebAPI.cpp`
  - `IMPORT_FILE()`付近への保守コメントのみ
- `doc/fw_design.md`
- `doc/codex/steering/20260719-incbin-build-dependencies.md`

## 実装方針案

1. PlatformIOのPRE追加スクリプトからBuild Middlewareを登録する。
2. ビルドスクリプトで`src/WebAPI.cpp`を読み、文字列リテラルを指定した`IMPORT_FILE()`呼び出しから埋め込みファイル名を抽出する。
3. 抽出した各ファイルを、`WebAPI.cpp`から生成されるオブジェクトファイルの依存関係としてSConsへ登録する。
4. 埋め込みファイルの更新日時または内容が変化した場合、PlatformIOの通常の差分ビルドによって`WebAPI.cpp`を再コンパイルする。
5. `incbin`直下の`.html`と`.js`を走査し、`IMPORT_FILE()`から参照されていないファイルをビルド時に警告する。
6. `IMPORT_FILE()`から参照されているファイルが存在しない場合は、埋め込み不能であるためビルドエラーとする。
7. `WebAPI.cpp`の`IMPORT_FILE()`付近に、依存関係がビルドスクリプトによって生成されることと、未参照ファイルが警告対象になることを短いコメントで記載する。

## 検出ルール案

### 警告

次の条件を満たすファイルがある場合、ファイル名を含む警告を表示する。

- `incbin`直下に存在する
- 拡張子が`.html`または`.js`
- `WebAPI.cpp`の`IMPORT_FILE()`から参照されていない

警告例:

```text
Warning: incbin/settings.html is not referenced by IMPORT_FILE()
```

未参照ファイルを直ちにビルドエラーにはせず、開発途中で配置したファイルも扱えるよう警告に留める案とする。

### エラー

次の条件ではビルドを停止する。

- `IMPORT_FILE()`で指定されたファイルが`incbin`内に存在しない
- `IMPORT_FILE()`の記述を安全に解釈できず、依存関係を正しく登録できない

## 設計上の注意点

- 埋め込み対象の正は`WebAPI.cpp`の`IMPORT_FILE()`呼び出しとし、Python側に同じファイル一覧を手書きしない。
- マクロ定義そのものではなく、文字列リテラルを使用した呼び出しだけを抽出対象とする。
- コメントアウトされた`IMPORT_FILE()`を誤検出しない方法を検討する。単純な正規表現で十分か、軽量な前処理が必要かは実装前に確認する。
- ファイル名にサブディレクトリを許可するか、現在どおり`incbin`直下だけに限定するかを実装前に決定する。
- `.css`、画像、JSONなどを将来埋め込む場合に、警告対象拡張子を容易に追加できる構造とする。
- Build Middlewareの対象指定がPlatformIOの各環境で同じように`src/WebAPI.cpp`へ一致することを確認する。
- IDEのコード解析など、通常ビルド以外のIntegration Dumpでは不要な警告や副作用を発生させない。
- Web UIファイルにAPIキー、Wi-Fiパスワードなどの実データを追加しない。

## 実装前に再確認する事項

- 追加スクリプトの配置先と命名
- 未参照ファイルを警告に留めるか、CIではエラーに昇格させるか
- コメントアウトされた`IMPORT_FILE()`の扱い
- `incbin`サブディレクトリと追加拡張子をサポートするか
- Build Middlewareで依存関係を付与する対象ノードの指定方法

## 確認方法

1. 変更のない状態で連続ビルドし、`WebAPI.cpp`が不要に再コンパイルされないことを確認する。
2. `incbin/config.html`だけを変更し、`WebAPI.cpp`が自動的に再コンパイルされることを確認する。
3. `incbin/config.js`だけを変更した場合も同様に確認する。
4. テスト用HTML/JavaScriptを`incbin`へ追加し、`IMPORT_FILE()`未登録の警告が表示されることを確認する。
5. `IMPORT_FILE()`から存在しないファイルを参照し、分かりやすいビルドエラーになることを確認する。
6. `m5stack-core2-realtime`、`m5stack-cores3-realtime`、`m5stack-atoms3r-realtime`で動作を確認する。
7. 必要に応じてRealtime API以外の環境でも共通スクリプトが問題なく動作することを確認する。

## 戻し方

`platformio.ini`から追加スクリプトの登録を削除し、追加したビルドスクリプトと`WebAPI.cpp`の案内コメントを削除する。`IMPORT_FILE()`による現在の埋め込み方式自体は変更しないため、ファームウェア側の復元作業は不要。
