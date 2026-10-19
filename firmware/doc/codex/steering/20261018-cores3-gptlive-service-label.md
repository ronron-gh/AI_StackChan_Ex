# CoreS3 GPT-LiveのWeb UI表示名

## 目的

CoreS3の標準Realtime環境がGPT-Live全二重通信を使用する構成に合わせ、Web UIのAIサービス選択を「OpenAI GPT-Live」と表示する。

## 対象範囲・主な変更ファイル

- `src/WebAPI.cpp`: GET `/config` にビルドに応じたOpenAIサービス表示名を追加する。
- `incbin/config.js`: 取得した表示名をサービス番号0の選択肢に反映する。
- `doc/fw_design.md`: 設定画面の表示名の決定方法を既存の関連節に追記する。

## 実装方針

- 実際のLLM生成処理と同じ `GPTLIVE_FULL_DUPLEX` を判定条件にする。定義時は「OpenAI GPT-Live」、それ以外は「OpenAI Realtime」とする。
- GET `/config` のルートに表示専用メタデータを追加し、保存対象の `sec` / `basic` / `ex` と分ける。
- JavaScriptで選択肢の `textContent` のみを変更する。メタデータがない場合は従来の「OpenAI Realtime」に戻す。
- サービス番号0、YAMLの `llm.type`、APIキー、Geminiの選択肢は維持する。
- ユーザー変更中の `platformio.ini` と `src/Robot.cpp` は変更しない。

## 設計上の注意点・副作用

- ボード名だけで判定せず、CoreS3でも従来のRealtime APIを使用するビルドには従来の名称を表示する。
- 設定の読み込み・再読み込みの両方で表示名を反映する。設定保存時には表示専用情報を送信・永続化しない。
- HTML全体をRAMに複製する処理や、新しいHTTPエンドポイントは追加しない。

## 確認方法

- JavaScriptの構文と、メタデータあり・なしでの表示名、保存するサービス番号が変わらないことを確認する。
- キャッシュ済み依存関係を使用し、ネットワーク取得なしで `m5stack-cores3-realtime` と `m5stack-core2-realtime` をビルドする。
- 実機での最終確認項目は、設定画面の初回表示・再読み込み・保存後のサービス選択維持とする。

## 戻し方

今回追加する表示専用メタデータとJavaScriptの表示切り替え、および設計メモのみを取り消す。保存設定の移行は不要。

## 確認結果

- JavaScriptをDOM・HTTPの簡易モックで実行し、初回のGPT-Live表示、再読み込み時の従来名への切り替え、メタデータ欠落時のフォールバックを確認した。
- OpenAIとGeminiの保存値がそれぞれ0・3のままで、POSTに表示専用メタデータを含めないことを確認した。
- ネットワークを無効化したキャッシュ依存ビルドで `m5stack-core2-realtime` と `m5stack-cores3-realtime` がともに成功した。
- 今回変更したファイルの `git diff --check` は問題なし。設定画面の実機確認は未実施。
