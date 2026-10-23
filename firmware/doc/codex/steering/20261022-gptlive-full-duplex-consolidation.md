# GPT-Liveの全二重実装への統一

## 目的

CoreS3限定で使用するGPT-Liveを、実機で安定動作している全二重通信に統一する。半二重経路を削除し、継承による処理の分散と音声状態の重複をなくす。

## 対象範囲・主な変更ファイル

- `src/llm/ChatGPT/GPTLive.cpp` / `.h`: 全二重音声とセッション管理を単一のGPTLiveクラスに統合する。
- `src/llm/ChatGPT/GPTLiveFullDuplex.cpp` / `.h`: 統合後に削除する。
- `src/Robot.cpp`: フラグ有効時に生成するクラスをGPTLiveに変更する。
- `doc/fw_design.md` / `doc/gptlive_full_duplex.md`: 半二重の現行仕様を除き、統合後の構造と切り替え方法を記載する。

## 実装方針

- GPTLiveをRealtimeLLMBaseの直接の派生クラスとして維持し、GPTLiveFullDuplexの音声処理・PSRAMデコード領域・診断モードを移す。
- GPTLive内の半二重再生リング、RMS無音判定、先行保持、マイク／スピーカー切り替え、関連する定数・状態・関数を削除する。
- 全二重ドライバーCoreS3FullDuplexAudio内のPSRAM再生リングとRX/TXキューは同時入出力に必要なため維持する。I2Sドライバーの動作・容量・音量処理は変更しない。
- manageAudio引数、派生クラス用のvirtualフック、重複する操作・休止・音声状態を整理する。GPT-Liveで使わないRealtime API／TTS用のイベント分岐や無効コードも、依存関係を確認して除去する。
- GPTLiveの宣言・実装・includeをGPTLIVE_FULL_DUPLEX有効時に限定する。CoreS3内蔵音声・非TTSなどの既存制約は維持する。
- GPTLIVE_FULL_DUPLEX有効時はGPTLive、無効時はRealtimeChatGPTというRobotの選択条件を維持する。Web UIのサービス名、効果音、Web API、WakeWord等の既存フラグ連動は変更しない。
- タッチ開始、session.started後の音声受付、store=false、正常終了と最終利用量、各15秒の開始・終了タイムアウト、再接続後の再タッチ待ちは維持する。
- GPTLIVE_FULL_DUPLEX_TEST_TONEによるネットワークなしの診断も維持する。

## 注意点・副作用

- 半二重GPTLiveへの切り戻し経路は削除する。フラグ無効時は従来のOpenAI Realtime APIとなる。
- セッション管理と音声資源の所有者はWebSocketタスクとし、UIからはatomicな要求を渡す構造を維持する。
- 未使用のGPTLiveがCore2やTTS構成でコンパイルされる必要をなくす一方、RealtimeChatGPT・Geminiの動作は維持する。
- 履歴としての既存ステアリングファイルは削除しない。ユーザーの無関係な差分には触れない。

## 確認方法

- GPTLiveFullDuplexのコード参照、半二重用状態・RMS判定、不要な継承フックが残っていないことを検索・差分で確認する。
- m5stack-cores3-realtime、m5stack-core2-realtime、m5stack-cores3-realtime-aquestalkをキャッシュ依存でビルドする。CoreS3のフラグ無効構成も一時的なビルド設定で確認し、配布設定は変更しない。
- 診断フラグの有効／無効経路が統合後もコンパイルできることを確認する。
- 実機では開始・同時入出力・停止・再開・モード切り替えを確認する。未実施の項目は未確認として報告する。
- ネットワークによる依存取得が必要なら実行前に確認する。

## 戻し方

今回の統合差分を取り消し、削除した派生クラスと半二重経路を復元する。設定ファイルの移行は不要。

## 実装・確認結果

- 全二重音声処理をGPTLiveへ統合し、GPTLiveFullDuplex.cpp/.hを削除した。半二重のバッファ・無音判定と旧API/TTS分岐を削除し、未使用だったMCP/FunctionCallの初期化も除いた。バックエンド委譲の設定は維持した。
- コールバックをthisを捕捉するラムダで登録し、静的インスタンス参照と派生クラス用の公開フックを除いた。操作要求・診断要求とエラー状態も共通化した。
- CoreS3FullDuplexAudioとplatformio.iniは変更していない。GPTLiveFullDuplexおよび半二重用の状態・関数へのコード参照は残っていない。
- セッション管理の主要関数を統合前と比較し、音声フックの除去・失敗状態の共通化以外に変更がないことを静的に確認した。
- ネットワーク無効・キャッシュ依存のPlatformIOビルドで、m5stack-cores3-realtime、m5stack-core2-realtime、m5stack-cores3-realtime-aquestalkが成功した。
- 一時設定のCoreS3フラグ無効ビルドも成功した。生成ELFにRealtimeChatGPTのコンストラクターがあり、GPTLiveのコンストラクターがないことを確認した。
- 一時設定でGPTLIVE_FULL_DUPLEX_TEST_TONEを有効にし、別ビルドディレクトリでGPTLive.cpp、CoreS3FullDuplexAudio.cpp、Robot.cppのオブジェクト生成が成功した。診断ファームウェア全体のリンク・実機試験は未実施。
- git diff --checkは問題なし。統合後の開始・同時入出力・停止・再開・モード切り替えは実機未確認。
- 2026-10-24: ユーザーより統合後の動作確認完了の報告を受けた。個別の異常系・診断モードの実機試験結果は未報告。ユーザーによる設計書の追加整理と、録音操作時のsw_tone有効化も同じコミットに含める。
