# Config Page MCP Settings

## 目的

Web UI の Config ページに任意設定用の `MCPs (Option)` タブを追加し、`SC_ExConfig.yaml` の `llm.mcpServers` を最大5件まで編集、保存できるようにする。
MCPサーバー設定は `REALTIME_API` ビルドでも利用される既存設定であり、今回そのWeb UI未対応部分を補う。

## 対象範囲

- `incbin/config.html` の4つ目のタブとMCPサーバー入力欄
- `incbin/config.js` のMCPサーバー設定の読込、入力値収集、画面側の検証
- `src/WebAPI.cpp` の `llm.mcpServers` のJSON応答と `SC_ExConfig.yaml` 生成
- `src/StackchanExConfig.h`、`src/StackchanExConfig.cpp` のMCPサーバー最大件数と安全な読込
- `doc/fw_design.md` の Config page、API、保存形式の設計記述

MCPサーバー設定の形式と既存のMCPクライアント処理は変更しない。

## 主な変更ファイル

- `incbin/config.html`
- `incbin/config.js`
- `src/WebAPI.cpp`
- `src/StackchanExConfig.h`
- `src/StackchanExConfig.cpp`
- `doc/fw_design.md`
- `doc/codex/steering/20260720-config-page-mcp-settings.md`

## 実装方針

1. Configページの既存タブ列へ、4つ目の `MCPs (Option)` タブを追加する。
2. MCPタブには5件分の入力枠を固定表示し、各枠で既存設定形式に対応する `name`、`disabled`、`url`、`port` を編集可能にする。`disabled` は `true` / `false` のドロップダウンで選択する。
3. `LLM_N_MCP_SERVERS_MAX` を10件から5件へ変更し、Web UI、設定読込、ランタイムの上限を統一する。
4. `StackchanExConfig` はYAML内の `mcpServers` を共通上限まで読み込み、6件目以降を無視して配列範囲外へ書き込まないようにする。
5. `GET /config` の `ex.llm.mcpServers` に、SPIFFS上の `SC_ExConfig.yaml` から読み込んだ設定を返す。SPIFFS設定がない場合はRAM上の `StackchanExConfig` から返す。
6. Web UIのReload時は取得した配列を5枠へ反映し、残りの枠を空にする。
7. Save時は、未使用の空欄を除外し、入力済みの枠だけを表示順に `ex.llm.mcpServers` 配列へ格納する。
8. 使用する枠は `name` と `url` を必須とし、`port` は1から65535の整数として検証する。入力途中など一部だけ値がある枠は保存せず黙って捨てるのではなく、エラー表示して保存を中止する。
9. `disabled` はドロップダウンの選択値を既存YAML形式の真偽値で保持する。空欄の枠は `disabled` の状態にかかわらず配列へ含めない。
10. `POST /config` は受け取った配列を共通上限で検証し、既存の `llm.type`、`llm.enableMemory` とともに `SC_ExConfig.yaml` の `llm.mcpServers` として生成する。
11. YAML文字列は既存のクォート処理を利用し、名前やURLにYAMLの予約文字が含まれても構造を壊さないようにする。
12. 既存のタブ切り替え、全タブ一括Save、選択中タブを維持するReload、Restartの動作を維持する。

## 設計上の注意点

- `LLM_N_MCP_SERVERS_MAX` を唯一のランタイム上限とし、固定配列とMCPクライアント配列も5件に統一する。
- 既存の `SC_ExConfig.yaml` に6件以上ある場合は先頭5件だけを使用する。Web APIから6件以上を保存する要求はエラーにする。
- MCPサーバー設定は任意であり、0件の `mcpServers` 配列を保存可能にする。
- `url` は既存 `MCPClient` が接続先ホストとして扱う値をそのまま保存し、今回URLスキームの変換や接続確認は追加しない。
- API側でも配列型、件数、必須文字列、ポート範囲を確認し、不正な入力から壊れたYAMLを生成しない。
- JSONドキュメント容量は5件分の文字列を扱えるよう見直し、組み込み機器のヒープ消費を必要以上に増やさない。
- 外部ライブラリは追加しない。

## 確認方法

- `MCPs (Option)` タブをクリックおよびキーボード操作で選択できること。
- MCPサーバーを0件から5件まで入力し、Save後の `SC_ExConfig.yaml` に `name`、`disabled`、`url`、`port` が正しい配列として保存されること。
- 各入力枠の `disabled` を `true` / `false` のドロップダウンで選択でき、Reload後も選択値が復元されること。
- Reloadおよびページ再読込で、保存済みの最大5件が同じ順序と値で復元されること。
- 空欄の入力枠が保存配列へ含まれないこと。
- 一部項目だけ入力した枠、範囲外または整数でないport、6件以上を含む直接APIリクエストがエラーになること。
- MCP設定を0件で保存しても、既存のWi-Fi、AI Service、Servo設定が維持されること。
- 既存YAMLに6件以上あっても配列範囲外アクセスが発生せず、先頭5件だけが読み込まれること。
- デスクトップ幅とスマートフォン幅で、4つのタブと5件分の入力欄に重なりや横方向のはみ出しがないこと。
- `m5stack-core2-realtime` と `m5stack-cores3-realtime` をビルドし、埋め込みWeb UIとAPI変更を含めてコンパイルできること。

## 戻し方

`MCPs (Option)` タブとWeb UIのMCP入出力処理を削除し、`GET /config`、`POST /config`、`SC_ExConfig.yaml` 生成を従来の `llm.type` と `llm.enableMemory` のみに戻す。`LLM_N_MCP_SERVERS_MAX` を10へ戻し、`StackchanExConfig` の上限処理を戻す。MCP設定形式自体は変更しないため、データ移行は不要。
