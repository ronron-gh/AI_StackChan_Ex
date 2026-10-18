# CoreS3 GPTLive Full Duplex 検証

## 目的
CoreS3 の内蔵マイクとスピーカーを ESP-IDF の I2S TX/RX 同時動作で使用し、GPTLive の応答再生中も入力音声の送信を継続できるか検証する。既存の半二重 GPTLive は比較・切戻し用として維持する。

## 調査結果・参照
- ユーザー提示コミット: https://github.com/ciniml/openai-realtime-embedded-sdk/commit/ae10019a1aa0af6334dabcc698e5323049b1c8ab
- 指定コミットのページ／patch／raw は調査時に取得できず、その差分そのものは未確認。
- 同じ作者の説明 https://zenn.dev/ciniml/articles/openai_embedded_sdk で、CoreS3 SE の共通I2Sポートに TX | RX を設定する方式と動作報告を確認。
- 同リポジトリの m5stack ブランチ src/media.cpp でも共通クロックによるTX/RX、ES7210／AW88298の直接初期化を確認。ただしブランチ側は新しいI2S APIを使っており、指定コミットと同一とは扱わない。
- 既存環境は PlatformIO espressif32@6.3.2 / Arduino-ESP32 2.0.9。初期実装は既存ツールチェーンで使える driver/i2s.h の旧APIを使用し、SDK更新は行わない。
- ローカル M5Unified 0.2.15 には CoreS3 音声ICの初期化・サンプリングレートに応じたAW88298設定が存在する。参考実装の8 kHz固定設定をそのまま24 kHzへ流用しない。
- main.cpp の効果音・アラーム、MP3/TTS、WakeWord等にもM5.Mic／Speakerアクセスがあるため、単にI2Sを差し替えるだけでは競合する。

## 対象範囲・主な変更ファイル
- src/llm/ChatGPT/GPTLiveFullDuplex.cpp / .h を新規作成する。
- src/driver/CoreS3FullDuplexAudio.cpp / .h を新規作成し、I2S・コーデック・バッファ・音声タスクを担当させる。
- src/llm/ChatGPT/GPTLive.cpp / .h は接続プロトコルを共用するために必要な最小限のフックと初期化選択を追加する。既存の半二重動作は維持する。
- src/Robot.cpp: 実験ビルドのときだけ新クラスを選択する。
- platformio.ini: m5stack-cores3-realtime-full-duplex 環境と GPTLIVE_FULL_DUPLEX フラグを追加する。
- src/main.cpp / src/share/Mutex.* / 必要な音声呼出し境界: 直接I2S使用中にM5音声が初期化・再設定されないよう、実験フラグ内で所有権と競合を扱う。main.cppへ機能固有の音声処理は置かない。
- doc/fw_design.md と検証手順の文書を更新する。

## 実装方針
1. 対象はCoreS3内蔵マイク・スピーカーの非TTS Realtimeビルドだけとし、Core2／AtomS3R／Module LLM／外部Audio Moduleでは実験機能を有効化しない。既存デフォルト環境は変更しない。
2. M5Unifiedの表示・電源・I2C機能は使い、実験動作中の音声入出力はM5.Mic／Speakerを経由しない。開始時に既存音声タスクを停止して所有権を取得し、終了時に直接I2Sを解放して従来の利用へ戻す。
3. CoreS3の共通クロック（MCLK GPIO0、BCLK GPIO34、WS GPIO33）と入力GPIO14・出力GPIO13を用い、単一I2Sポートを TX | RX で初期化する。16 bit PCMのスロット・左右・クロック比をES7210／AW88298の設定とそろえる。I2C書込み失敗は開始失敗として後始末する。
4. 入出力は24 kHz／モノラルPCM16を初期案とし、GPTLiveセッションと録音送信も24 kHzへそろえる。参考コードのOpus／8 kHzや現行基底クラスの16 kHz入力は持ち込まない。
5. RXは専用タスクで継続収集し、容量制限付きキューを介してWebSocketタスクへ渡す。WebSocketのloop／sendは単一タスクだけが行う。手動停止中は入力送信を止め、DMAの古い入力を次回へ持ち越さない。
6. 受信した音声はPSRAMリングへ追加し、TXタスクが短い単位でDMAへ送る。無音出力も扱い、無音判定でマイクを切らない。TX待ちでWebSocket受信を止めず、部分read／write、キュー満杯、リング満杯、確保失敗を明示的に処理する。DMA自体の領域は内部DMA対応メモリを使用する。
7. 自己発話の回り込みを切り分けるため、初回検証ではソフトウェアAECを追加せず、再生中も実マイク音声を送信する。音量を控えめにし、入出力同時進行と自己応答の有無を確認する。送信マスクが必要なら全二重の成立確認と別の課題として扱う。
8. 実験動作中の効果音・アラーム・TTS・WakeWordとの重複を抑止する。UI操作を音声mutex待ちで固めないよう、効果音は競合時に待たずスキップする等の境界を設ける。長時間の所有権とモード離脱がデッドロックしないことを確認する。
9. 切断・手動停止・モード停止・開始失敗で収集／再生タスクを協調停止し、I2S・音声IC・mutex・キューを順序立てて解放する。再開時の二重インストールと古いPCMの再生を防ぐ。
10. マイクRX／送信／スピーカーTXのサンプル数、欠落・満杯、レベルを低頻度でログに出す。リップシンクと音量設定は直接出力側に対応させる。

## 注意点・副作用
- 同時I2S入出力の成立と、エコーのない双方向会話は別の検証項目。作者の報告にもスピーカー音の回り込みがあり、AECなしの会話品質は保証しない。
- 参考実装の主な検証機はCoreS3 SE。ユーザーのCoreS3での同時動作、24 kHz、音量、クロック安定性は実機確認が必要。
- 検証中は従来の効果音・アラーム音等が鳴らない場合があることを手順に明記する。通常ビルドの使い方は維持する。
- 新規タスクのスタック・優先度、PSRAM容量、RXキュー容量は実装時に明示し、既存WebSocketタスク・I2C利用との競合を確認する。
- 参考コードを取り込む場合はライセンスと著作権表示を保持する。

## 確認方法
- 新規実験環境でコンパイル・リンク・サイズを確認する。既存のCoreS3 Realtime／Core2 Realtimeもビルドし、フック変更に応じてTTS等を確認する。
- 依存は既存キャッシュを優先する。追加取得・SDK更新が必要ならAGENTS.mdに従い事前確認する。
- 静的確認: 単一WebSocket所有者、部分I/O、バッファ寿命、タスク停止順序、再初期化、mutexの取得／解放とUI非ブロック、非対象ビルドの除外。
- 実機: まず直接I2Sの短いテスト音とマイクレベルを同時確認し、次にWebSocket接続・入力送信・応答再生中の入力継続を確認する。最後に手動停止・切断再接続・モード切替・音量変更・長時間動作を確認する。
- 実機書込み・音声出力の確認はユーザー環境で行い、ビルド成功と実機成功は区別して報告する。

## 戻し方
通常の m5stack-cores3-realtime をビルドすると既存GPTLiveへ戻る。コードを戻す場合は実験フラグ内の統合箇所と追加ファイルだけを戻す。

## レビュー状態
2026-10-15 ユーザーより「実装に進んでください」とレビュー了承済み。

## 実装・確認結果

- GPTLiveFullDuplex / CoreS3FullDuplexAudioを追加。GPTLiveの音声イベント3個をvirtualにし、manageAudio=falseで半二重用PSRAM確保を省略する。共通RealtimeLLMBaseは未変更。
- I2S0をTX | RX、24 kHz、16 bit、左モノラル、MCLK 256fsで初期化。ES7210は既存M5Unifiedの設定を基にし、AW88298レートコードは0x14C5。レジスタ列のMIT著作権・許諾表示をドライバへ保持した。
- RX/TXは各4096 byteスタック・優先度3。入力は480 sample窓×8のキュー、出力は512 KiBのPSRAMリング＋100 KiBのPSRAMデコード領域。DMAはSDKが内部DMA対応メモリへ確保する。部分I/Oを扱い、50 ms上限の待ち・500 ms無進行で障害検出する。
- WebSocket所有者は従来タスクのみ。出力中も24 kHz入力を送信し、無音判定・30秒録音タイムアウトによるマイク切替は適用しない。RXキュー満杯は新着窓を破棄してカウント、出力リング満杯はセッションを停止する。
- 効果音・アラームは実験構成で音声mutexを待たず、競合時にスキップ。WakeWordは実験環境で無効。非TTS構成の直接TTS呼出しも防ぐためRobot::speechを実験構成内で無効化し、src/WebAPI.cppの/speechは503を返す。通常構成のAPIは維持した。
- 停止時は同じWebSocketタスクがワーカー終了→I2S停止・解放→音声IC停止→キュー／PSRAM解放→従来M5音声の復元→音声mutex解放を行う。新しい入力世代・キューと新規I2Sで再開し、旧PCMを持ち越さない。
- GPTLIVE_FULL_DUPLEX_TEST_TONEの単独診断ではWebSocket処理・API送信を行わず、タッチで100 msテスト音とRX収集を開始する。無効時も診断コードは型・構文がコンパイルされ、通常経路では実行されない。実機実行は未確認。
- 検証・切戻し・ログの説明をdoc/gptlive_full_duplex.mdへ追加した。

| PlatformIO 環境 | 結果 |
| --- | --- |
| m5stack-cores3-realtime-full-duplex | 成功 |
| m5stack-cores3-realtime | 成功 |
| m5stack-core2-realtime | 成功 |
| m5stack-cores3-realtime-aquestalk | 成功 |

- 全環境で最終ソースのコンパイル・リンク・サイズチェック・firmware.bin生成を確認。全二重版のnm出力にGPTLiveFullDuplex::webSocketProcess / enqueueAudioDeltaとCoreS3FullDuplexAudio::begin / receiveLoop / transmitLoopを確認した。
- 依存は既存CoreS3環境のキャッシュを新環境へコピーし、追加取得・ネットワーク・更新チェックを無効化してビルドした。ログは.pio/gptlive-check/*-full-duplex.logに保存。
- git diff --checkは成功。既存のArduinoJson非推奨警告などは残る。
- 静的確認: 単一WebSocket所有者、PCM偶数境界とリング周回、DMAへコピー完了までの作業領域保持、リングmutexをI/O待ち中に保持しないこと、タスク生成途中の失敗、同一タスクでの音声mutex解放、入力世代切替、非対象ビルドの除外。
- 未確認: CoreS3実機の24 kHz同時入出力、クロック・左右スロット・コーデック設定の実効性、実音量・マイク信号、停止／復帰の安定性、タスクスタック余裕、長時間運転、自己発話の回り込み。AECは未実装。ビルド成功は実機成功を意味しない。
