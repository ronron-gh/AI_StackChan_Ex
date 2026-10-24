# GPT-LiveのFunction Calling対応

## 目的

CoreS3のGPT-Liveで、RealtimeChatGPTと共通の組み込み関数および既存MCPクライアント経由の関数を利用できるようにする。安定動作している全二重音声とタッチによるセッション開始・終了を維持する。

## 対象範囲・主な変更ファイル

- `src/llm/ChatGPT/GPTLive.cpp` / `.h`: ツール登録、委譲イベントの受信、関数実行と結果返却、終了時の後処理。
- `src/llm/ChatGPT/FunctionCall.cpp` / `.h`: 既存実行処理を再利用する。共通側の変更は引数検証や並行実行上の問題を解消するために必要な範囲に限定する。
- `src/llm/ChatGPT/MCPClient.cpp` / `.h`: 無期限の応答待ちによる休止不能を避けるため、任意のタイムアウト引数を追加する。GPT-Liveは30秒を指定し、既存呼び出しはデフォルト値0で従来の待機動作を維持する。接続・HTTP呼び出し中の待ち時間は別途加わる。
- `doc/fw_design.md`: 既存のGPT-Live・Function Calling関連節に処理の流れとタスク責務を反映する。
- `doc/gptlive_full_duplex.md`: 利用できる機能や制約が変わる箇所を更新する。

## 公式仕様と既存実装との差分

- 関数定義はRealtimeの`session.tools`ではなく、`session.delegation.responses.tools`にResponses形式で登録する。
- GPT-Liveがバックエンドへ委譲し、設定済みResponsesモデルが関数を選択する。既存のバックエンドモデルとweb_searchは維持する。
- 外側の`response.event`を展開し、内側の`response.output_item.done`の`item`から`function_call`を取得する。`name`、`arguments`、`call_id`に加え、外側の`delegation_id`とバックエンドresponse IDを追跡する。
- `response.completed`等の`response.output`は空配列となるため、RealtimeChatGPTの`response.done`走査は流用しない。argumentsの完了イベントだけでも実行しない。
- 結果は`response.item.create`の`function_call_output`で元の`call_id`に対応付けて返す。必要な結果をすべて送信後、`response.create`でバックエンドを継続する。

## 実装方針

1. GPTLiveに既存方式のMCPクライアント初期化とFunctionCallインスタンスを追加する。接続済み・有効なMCPツールと、ビルド条件に応じた組み込み関数を登録する。ツール名重複、未接続クライアント、JSON容量不足を確認し、不整合な定義を送らない。
2. 音声モデルのinstructionsには利用可能なバックエンド機能と委譲条件を記載し、関数選択・実行手順は`delegation.responses.instructions`へ設定する。ロール・ユーザー情報・メモリの既存用途を確認し、必要な情報をバックエンドにも渡す。公式推奨の委譲指示を確認して適用する。
3. FunctionCall::exec_calledFuncを実行する専用ワーカーと、要求・結果を渡す上限付きキューを用意する。WebSocketタスクはイベント処理・音声送受信・結果送信を担当し、ワーカーからWebSocketへアクセスしない。長いHTTP/MCP処理やdelayでWebSocketループを停止させない。
4. 受信JSONドキュメントの参照をワーカーへ渡さず、必要な文字列を所有する要求としてコピーする。大きな引数・結果はPSRAMを活用し、件数・サイズに上限を設ける。未知の関数、不正引数、キュー満杯、実行失敗には対応するエラー結果を返す。
5. 初期実装では`parallel_tool_calls=false`とし、ローカル関数実行は直列化する。継続要求の重複送信を防ぎ、複数段の関数呼び出しにも対応する。結果JSONはArduinoJsonで生成し、引用符・改行等を正しくエスケープする。
6. セッション世代・委譲ID・response ID・call_idで実行と返却を管理する。同一call_idを二重実行せず、切断・停止後の結果を新セッションへ送らない。副作用を伴う関数を自動再実行しない。
7. 停止・切断時は未着手の要求を破棄し、実行中の処理の結果は世代確認後に破棄できる構造にする。実行済みの副作用は取り消せないことを前提とする。ワーカーや参照先オブジェクトを実行中に解放せず、モード切り替え時の休止・終了と整合させる。

## 設計上の注意点・副作用

- FunctionCallはアバター、音量、設定、メモリ、MCP通信などを操作するため、別タスク化に伴う共有状態の競合と既存タスクへの影響を確認する。WebSocket送受信用ドキュメントとワーカーの作業領域は分離する。
- CoreS3FullDuplexAudioが音声資源を所有する。RealtimeChatGPTのenterMutexAudioやM5.Mic／M5.Speaker切り替えを移植しない。音声資源の再取得やセッション自身の休止を伴う関数は、デッドロックを避けるため実装前に個別確認し、利用できないものは登録対象から除く。
- 音声会話はバックエンド処理中も継続する。割り込みや停止によって、すでに実行されたタイマー設定等の操作が自動で取り消されるとは扱わない。
- store=false、session.started後の音声開始、正常終了と最終利用量、開始・終了タイムアウトは維持する。
- GPTLIVE_FULL_DUPLEXによるサービス切り替え、ネットワークなしの診断モード、RealtimeChatGPT・Geminiの既存動作は維持する。新しいWeb UI設定やAPIキーは追加しない。

## 確認方法

- 静的確認: 関数定義形式、イベント階層、call_id対応、継続の一回送信、登録・実行対象の一致、PSRAM・キュー・タスクの寿命を確認する。
- 制御処理の確認: 単発・連続・複数段呼び出し、重複イベント、不正引数、未知の関数、キュー上限、実行失敗、切断後の遅延結果を対象とする。
- ビルド: `m5stack-cores3-realtime`、`m5stack-core2-realtime`、CoreS3のGPTLIVE_FULL_DUPLEX無効構成を確認する。共通FunctionCallを変更した場合はTTS構成等の関係環境にも広げる。
- 実機: 日時取得、音量・明るさ変更、表情、タイマー、メモリ更新、有効なMCPツール、web_searchとの共存を確認する。長い関数処理中の同時音声入出力、停止・再接続・モード切り替えも確認する。
- 依存取得が必要な場合は事前確認する。未実施のビルド・実機項目は明示する。

## 戻し方

今回追加する登録・イベント・実行キューとワーカーの差分を取り消し、web_searchのみのGPT-Liveへ戻す。永続設定の移行は行わない。

## 参照資料

- [Delegation and tools in GPT-Live](https://developers.openai.com/api/docs/guides/live-delegation)
- [Migrate to GPT-Live](https://developers.openai.com/api/docs/guides/live-migration)
- [Prompting GPT-Live](https://developers.openai.com/api/docs/guides/live-prompting)

## 状態

ユーザーレビュー・実装了承済み。実装・ビルド確認済み。2026-10-25にユーザーより動作確認完了の報告を受けた。

## 実装・確認結果

- GPTLiveに組み込み関数11件と接続済みMCPツールの登録、専用ワーカー、PSRAM要求・結果領域、イベント受信・結果返却を追加した。既存FunctionCallの実行処理は変更していない。
- response.createdのIDを委譲IDに対応付け、response_idを持たないoutput_item.doneにも対応した。全関数結果とバックエンド完了を待って継続要求を一度送る。
- 引数の必須キー・型・enum、タイマーの範囲、件数・サイズ上限を検証する。重複call_idの再実行と、旧セッションの結果送信を抑止した。
- MCPClientに省略可能な応答待ちタイムアウトを追加した。GPT-Liveのみ30秒を指定し、他サービスの従来動作は維持した。モード休止は実行中処理の終了を待ち、ワーカーを実行途中で強制停止しない。
- ネットワーク無効・キャッシュ依存のPlatformIOビルドでm5stack-cores3-realtime、m5stack-core2-realtime、m5stack-cores3-realtime-aquestalkが成功した。最後のイベント処理修正後もCoreS3の通常ファームウェアを再ビルドし成功した。
- 別ビルドディレクトリの一時設定で、GPTLIVE_FULL_DUPLEX無効時のGPTLive.cpp／MCPClient.cpp／Robot.cpp、およびGPTLIVE_FULL_DUPLEX_TEST_TONE有効時のGPTLive.cppのオブジェクト生成が成功した。これら一時構成のファームウェア全体のリンクは未実施。
- CoreS3の組み込み関数定義を抽出し、11件の名前の一意性、JSON形式、必須キーとプロパティの整合性を静的確認した。git diff --checkは問題なし。既存のArduinoJson非推奨API等の警告は残る。
- 実API接続・関数実行と音声継続、MCPタイムアウト、停止・再接続・モード切り替え、異常イベントの実行試験は未実施。キュー・イベント制御の自動実行テストも未実施。
- 2026-10-25: ユーザーより実装後の動作確認完了の報告を受けた。上記の未実施項目はエージェントによる確認状況であり、ユーザーの個別試験範囲は未報告。ユーザーが追加したsession.startのJSONログも同じコミットに含める。
