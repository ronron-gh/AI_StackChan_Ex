# FW Design Information
Notes on FW design, etc.

- [Task](#task)
- [Mod](#mod)
  - [ESP-NOW Remote Control Mod](#esp-now-remote-control-mod)
  - [GPTLive Audio Playback](#gptlive-audio-playback)
  - [GPTLive Full Duplex](#gptlive-full-duplex)
- [Function Calling](#function-calling)
  - [Avatar Expression](#avatar-expression)
- [Wi-Fi config portal](#wi-fi-config-portal)
- [Web App](#web-app)
  - [Home page](#home-page)
  - [Config page](#config-page)
  - [Personalize page](#personalize-page)
- [Status Monitor](#status-monitor)
- [Head Touch Sensor](#head-touch-sensor)


## Task

| Task name | function | Stack size [bytes] | Priority |
| --- | --- | --- | --- |
| loopTask | Arduino loop task | 8192 | 1 |
| drawLoop | Avatar control | 4 * 1024 | 2 |
| facialLoop | Avatar control | 1024 | 2 |
| lipSync | Lip Sync for avatar| 2048 | 2 |
| servo | Servo control synchronized with the avatar | 2048 | 1 |
| battery_check | Battery level check | 2048 | 1 |
| asyncTtsStreamTask | TTS streaming play | 5 * 1024 | 2 |
| webSocketLoopTask | WebSocket processing for LLM Realtime API | 6 * 1024 | 3 |
| fullDuplexRx | CoreS3 experimental direct I2S RX | 4096 | 3 |
| fullDuplexTx | CoreS3 experimental direct I2S TX | 4096 | 3 |
| SanoTTS | SanoTTS parsing, inference, and playback | 16 * 1024 | 2 |

SanoTTS は同期 `stream()` の要求を専用ワーカーで直列処理する。CoreS3 の SanoTTS 環境では `SANOTTS_BUFFERED_PLAYBACK` により、句読点などで分割した各区間の PCM を PSRAM に全量確保・合成してから再生する。M5Unified が現在区間を再生している間に同じワーカーが次区間を合成し、2個のPCMバッファを交互に再利用する。1区間は最大30秒で約1.26 MiB、先行生成時は最大約2.52 MiBのPCMに加え、176 KiBの解析・推論 arenaを使う。次区間用バッファを確保できない場合は現在区間の再生後に直列生成で再試行する。リップシンク用レベルはPCMから作った音量エンベロープを再生時刻で参照する。フラグを外すと先読みと循環バッファによる逐次再生になる。いずれも再生終了後にマイクとスピーカーの元の状態を復元する。切り替え方法は[doc/sanotts.md](sanotts.md) を参照。

GPTLive の音声出力は追加タスクを作らず、既存の webSocketLoopTask で受信・蓄積・再生要求を処理する。音声 mutex は同じタスクが取得・解放し、再生中は保持する。モード停止要求は atomic フラグで通知し、同タスクが音声の後始末を完了したことを通知してから suspend する。

## Mod
### ESP-NOW Remote Control Mod

初期実装では `src/mod/EspNowRemote` に Receiver 固定の Mod を追加する。

- Wi-Fi channel は `1` 固定。
- ESP-NOW 受信コールバックでは payload を固定バッファへコピーするだけにし、Serial 出力は Mod の `idle()` で行う。
- 標準 firmware の Sender から受けた場合は Arduino `esp_now` の受信 data でアプリ payload が offset `20` から始まるため、offset `20` を優先して標準 8 byte 形式として decode 表示する。8 byte だけ届く場合は offset `0` から decode する。
- ESP-NOW Remote Mod 実行中は `main.cpp` の Avatar gaze 連動 servo task を抑止し、受信した yaw/pitch を `ServoCustom::moveTo()` に渡す。
- yaw `-1280..1280` は servo x `45..-45` 度へ、pitch `0..900` は servo y `0..-30` 度へ変換する。
- payload の speed は `ServoCustom::moveTo()` の移動時間 ms として渡す。
- laser にはまだ反映しない。
- ESP-NOW Mod 実行中は Wi-Fi channel 変更により Web/FTP/Realtime API と干渉する可能性がある。
- ESP-NOW Mod 離脱時は ESP-NOW を停止し、offline mode でなければ Wi-Fi STA の再接続を最大 5 秒待つ。

### GPTLive Audio Playback

対象は GPTLive を使う REALTIME_API ビルドのうち、REALTIME_API_WITH_TTS が未定義の構成。CoreS3 + M5Unified の I2S 制約に合わせて、マイクとスピーカーを半二重で切り替える。既存の RealtimeChatGPT、GeminiLive、TTS 経由の再生処理は維持する。

- session.output_audio.delta の Base64 を PCM16 / モノラル / 24 kHz にデコードし、PSRAM に確保した 512 KiB のリングに追加する。容量は約 10.9 秒分であり、応答全体の長さの上限ではなく未再生分の上限。別途 100 KiB の PSRAM デコード領域と 5760 byte の判定・先行保持領域を使う。GPTLive では基底クラスの 100 KiB × 2 のダブルバッファを確保しない。
- デコードした PCM は 20 ms（480 sample）の窓で RMS を判定する。開始は RMS 300 以上、発話継続は RMS 150 以上を初期閾値とし、比較用の平方和は 64 bit で計算する。delta をまたぐ窓は次の受信へ持ち越し、受信が 150 ms 途絶えた場合は途中の窓も判定する。
- 発話待機中は無音を再生リングに追加せず、直前 100 ms（4800 byte）のみを PSRAM に循環保持する。開始閾値を超えた窓で先行保持分とその窓を順にリングへ追加する。発話中は短い無音も再生し、継続閾値未満が 1200 ms（28800 sample）続いたら待機へ戻って以後の無音を破棄する。猶予は PCM のサンプル数で数える。
- 受信コールバックはデータの検証、音量判定、保持・追加のみを行う。ループ側は mutex を待たずに取得を試み、取得後に録音停止 → Mic.end → Speaker.begin とする。再生中は入力音声を送信しない。
- リングの連続領域から最大 8192 byte（約 171 ms）を Speaker.playRaw に渡す。再生完了までその領域を使用量に含め、受信による上書きを防ぐ。リングが空でも、スピーカーの再生中または発話継続中はマイクを戻さない。発話終了を確認し、リングとスピーカーの再生が完了してから、再生対象 PCM の追加がない状態を 150 ms 待ち、Speaker.end の後に元のマイク状態を復元して mutex を解放する。
- 録音操作の意図と実際の録音状態を分離する。GPTLive の isRealtimeRecording は操作上の録音有効状態を返し、再生による一時停止中も有効のままとする。再生中のタッチは録音を無効にし、終了後の自動再開を抑止する。再生中の音声による割り込みは受け付けない。
- PCM 受信自体が 1200 ms 途絶えた場合も壁時計で発話終了とする。途中の窓を確定する 150 ms と、発話終了の受信途絶 1200 ms は別の条件とし、窓の確定やリング枯渇だけでは発話を終了しない。終了理由はログの PCM silence / receive timeout で識別できる。
- 破棄した無音は再生終了の猶予をリセットしないため、無音 delta の連続受信中もマイクを維持・復帰できる。通常の再生終了では途中の判定窓と先行保持分を残し、切断・異常・モード停止ではそれらもクリアする。
- リング・判定窓・先行保持の読み書きとセッション状態は webSocketLoopTask だけが更新する。操作要求・停止要求と再生レベルの共有には atomic を使う。リップシンクには再生チャンクの PCM ピーク値を渡す。
- PSRAM 確保失敗、不正な Base64／PCM、容量超過、スピーカー初期化・再生失敗はログを出して音声状態をリセットし、WebSocket を切断する。再接続後も録音は自動開始せず、session.started 後のタッチで再開する。容量超過時には今回の受信で追加済みの未再生 PCM も破棄し、不正チャンクの後続だけを再生しない。通常の切断も録音を停止し、蓄積 PCM を破棄する。
- モード停止時は未再生 PCM を破棄し、Speaker.end により PCM 参照の終了を待ってからマイクを復元する。WebSocket も切断して停止前の応答が復帰時に流れ込むのを防ぐ。復帰時は再接続し、session.started 後のタッチで録音を開始する。マイク初期化失敗時も録音要求を無効にする。

RMS の最小／最大値を約 2 秒ごとに、発話開始／終了を遷移時にログへ出す。閾値 300／150、無音終了・受信途絶 1200 ms、再生終了猶予 150 ms は初期設定値で、弱い発話の見逃し・ノイズ誤検出・実機での音切れ・切替頻度は未確認。発話内の無音または受信途絶が 1200 ms 以上になると再切替が発生し得る。CoreS3 実機で短い／長い応答、遅延、切断、録音停止、モード切替を確認する。リング再生の導入記録は [ステアリング](codex/steering/20261012-gptlive-audio-ring-buffer.md)、無音判定の導入記録は [ステアリング](codex/steering/20261013-gptlive-silence-gate.md)、終了猶予・復帰条件の確認結果は [ステアリング](codex/steering/20261014-gptlive-speech-end-grace.md) に記録する。

### GPTLive Full Duplex

m5stack-cores3-realtime-full-duplex の GPTLIVE_FULL_DUPLEX フラグでのみ、OpenAI選択時に GPTLiveFullDuplex を生成する。既存の GPTLive は通常環境で維持する。接続・セッション設定はGPTLiveを継承して共用し、音声イベントをvirtualフックで差し替える。半二重用のPSRAMはこのクラスでは確保しない。

- CoreS3FullDuplexAudioがI2S0のTX／RXを一括所有する。MCLK GPIO0、BCLK GPIO34、WS GPIO33、DIN GPIO14、DOUT GPIO13、24 kHz／16 bit／左モノラルスロット、MCLK 256fsを使う。ES7210／AW88298の初期化はM5Unifiedのレジスタ設定を基に直接行い、AW88298のレートコードは24 kHzへ合わせる。画面・電源・I2Cは引き続きM5Unifiedを使う。
- WebSocketタスクが音声mutexを待たずに取得し、既存M5音声を終了して直接I2Sを開始する。同じタスクが終了・復元・mutex解放まで担当する。実験ビルドの効果音／アラーム音はmutexを取得できなければスキップし、UIタスクを所有権待ちで止めない。WakeWordはビルドフラグで無効、非TTS構成なので非同期TTSは起動しない。別TTSのRobot::speechも実験フラグ内で無効化し、/speechは503を返す。
- fullDuplexRxは20 ms（480 sample）ずつ継続収集し、世代番号付き8窓のFreeRTOSキューへ渡す。WebSocketタスクだけがloop／sendを行い、1回最大2窓を送信する。入力キュー満杯時は新着窓を破棄し、RXdropへ計上する。I2Sの部分readは同じ窓へ蓄積する。停止中・古い世代の窓は送信しない。
- 受信PCMは100 KiBのPSRAM作業領域でデコードし、512 KiBのPSRAMリングへ全チャンクの空きを確認して追加する。fullDuplexTxは20 msずつ専用タスクの作業領域へコピーしてからリングを解放し、部分writeを完了まで進める。データがなければゼロを送ってクロックとRXを維持する。DMA領域はI2Sドライバの内部DMAメモリを使う（6 descriptor、480 sample）。
- リングだけを専用mutexで短時間保護し、WebSocket待ち・I2S待ち中には保持しない。動作／失敗／録音許可／世代／音量／レベル／カウンタはatomic。RX/TXそれぞれ4096 byteのタスクスタックを使う。音量はPCMへ0〜255の二乗比で適用し、DMAへ送る窓の出力レベルをリップシンクへ返す（実再生とはDMAの待ち時間分ずれる）。
- PCM出力の無音判定でマイクを止めず、出力中も実マイクを送信する。30秒の自動録音タイムアウトは適用せず、手動停止まで継続する。AECと送信マスクは未実装で、回り込みによる自己応答は未解決の評価項目。
- 手動停止／モード停止／切断／不正PCM／リング満杯／I2S障害では協調停止する。50 ms上限のI/O待ちからRX/TXが戻るのを待ち、I2S・音声ICを停止し、キュー／PSRAMを解放してから従来M5音声を復元し、mutexを解放する。I/Oが500 ms進行しなければ失敗にする。再開時はI2Sを新規に作り、旧PCMを持ち越さない。タスク生成途中の失敗も同じ後始末で扱う。
- RX／TX／Sent、キュー破棄数、TX無音窓数、入力／出力ピークを約2秒ごとに記録する。TXカウンタはゼロ送信も含むため、応答出力の確認にはoutLevelも使う。

任意の GPTLIVE_FULL_DUPLEX_TEST_TONE フラグでは、タッチで100 msのテスト音と連続RXを動かす単独診断モードとなり、WebSocket処理・API送信は行わない。通常ビルドでも診断コードの型・構文をコンパイルし、無効時は実行しない。

実機の同時動作、24 kHzの安定性、停止／復帰、音量、自己発話の回り込みは未確認。検証・切戻しの手順は [GPTLive Full Duplex](gptlive_full_duplex.md)、ビルド・静的確認の結果は [ステアリング](codex/steering/20261015-gptlive-full-duplex.md) に記録する。

## Function Calling

### Avatar Expression

Realtime API ビルドでは、Function Calling により AI が会話中の感情に合わせて Avatar の表情を変更できる。

- 対象ビルド
  - `REALTIME_API` が定義される PlatformIO 環境。
- 関数名
  - `set_avatar_expression`
- 引数
  - `expression`: `neutral`, `happy`, `angry`, `sad`, `doubt`, `sleepy`
- 実装
  - `src/llm/ChatGPT/FunctionCall.cpp`
  - `m5avatar::Expression` に変換して `Avatar::setExpression()` を呼び出す。
  - `LLMBase.cpp` の `systemRole_realtimeAvatarExpression` で Realtime API の system instructions に利用方針を明示する。
- スコープ
  - `json_Functions` は通常 ChatGPT や Gemini Live からも参照されるため、この関数の schema と実行処理は `#if defined(REALTIME_API)` で限定する。
  - `systemRole_realtimeAvatarExpression` は Realtime 系 LLM の `load_role()` で `systemRole_memory` または `systemRole_noMemory` に追加する。


## Wi-Fi config portal

SmartConfig は使わず、起動時の Wi-Fi 接続元は YAML の `wifi.ssid` / `wifi.password` のみにする。

- SD に設定 YAML がある場合は SD を優先する。
- SD が無い、または主要 YAML が無い場合は SPIFFS の YAML を使う。
- `SC_ExConfig.yaml`、`SC_SecConfig.yaml`、`SC_BasicConfig.yaml` の一式が揃っていない場合は通常機能を起動しない。
  - `REALTIME_API` ビルドでは、`SC_SecConfig.yaml` がある場合は Wi-Fi 接続だけ試し、成功したら STA 接続上で設定 Web を起動する。
  - `REALTIME_API` ビルドでは、Wi-Fi 接続できない場合、または `SC_SecConfig.yaml` も無い場合は Config AP を起動する。
  - `REALTIME_API` 以外のビルドでは、不完全な設定を画面に表示し、そのまま待機する。
- 設定一式が揃っている通常起動時は、`wifi.ssid` が空、または接続に失敗した場合に画面上で Config AP 起動または Offline 起動を選ぶ。
- Config AP は SSID `StackChanEx-Config-NNNNNN`、password `stackchan` で起動し、`http://192.168.4.1/` の QR コードを画面に表示する。SSID 末尾の 6 桁数字は AP 起動時に生成する。
- AP モード中は会話機能は offline 相当として扱うが、Web サーバーは動かし続ける。
- AtomS3R は画面が小さいため QR コードは表示せず、Wi-Fi 接続失敗時は Config AP を直接起動する。


## Web App
WebAPI.cpp のインラインアセンブラ(マクロ：IMPORT_FILE)で incbinフォルダ内のhtmlファイルやjsファイルをプログラム領域に埋め込む。

### Home page
Web アプリの入口 `/` として `home.html` を返す。`home.html` には次の 2 つの導線を置く。

- Personalize
  - `/personalize.html` に遷移する。
  - Role と Memory の管理を行う。
- Config
  - `/config.html` に遷移する。
  - Wi-Fi、API key、Realtime API 用 YAML 設定、SD から SPIFFS へのコピーを行う。

設定用 AP モードで QR に載せる URL も `/` とし、スマートフォンで開いた直後に用途を選べるようにする。

### Config page

- Wi-Fi、AI Service、Servo、MCPs (Option) の設定領域をタブで切り替えて表示する。
- タブを切り替えても未保存の入力値は保持し、Save は全タブの設定内容をまとめて保存する。
- Save、Reload、Restart と処理結果のメッセージはタブ領域の外に配置し、どのタブからでも操作可能とする。
- Reload は全タブの設定値を更新し、現在選択中のタブは維持する。

- ファイル構成
  - `incbin/config.html`
  - `incbin/config.js`
- 言語
  - ページやダイアログ内の言語は英語版のみ。
- 概要
  - 画面の入力内容を SC_SecConfig.yaml、SC_BasicConfig.yaml、SC_ExConfig.yaml のフォーマットにしてAPIで設定する。
  - 画面表示時、更新時、Reloadボタン押下時はAPIで設定値を取得して画面の入力値に反映する。
- 画面構成
  - Wi-Fi
    - SSID
    - Password (値は'*'で隠す/表示するを切り換え可能とする)
  - Realtime AI Service
    - ドロップダウン内の以下サービスから選択
      - OpenAI Realtime
      - Google Gemini Live
    - API Key　(値は'*'で隠す/表示するを切り換え可能とする)
    - Enable Memory (true or false を設定)
  - MCPs (Option)
    - MCPサーバーを最大5件設定する。未使用の入力枠は保存しない。
    - 各サーバーに Name、Disabled、URL / Host、Port を設定する。
    - Disabled は `true` / `false` のドロップダウンで選択する。
    - Name、URL / Host、Port の一部だけが入力された場合は保存エラーとする。
  - Servo
    - 以下項目を設定。詳細は [Servo Setting Details](#servo-setting-details) に記載
      - Type
      - Pin
      - Offset
      - Center
      - Lower limit
      - Upper limit
      - Enable Takao Base
  - Save ボタン
    - 画面の入力値をSC_SecConfig.yaml、SC_BasicConfig.yaml、SC_ExConfig.yaml のフォーマットにしてAPIで設定する。
  - Reload ボタン
    - APIで現在の設定を取得し、画面の入力値を更新する。
  - Restart ボタン
    - 設定内容をシステムに反映するためのシステムリセットを実行。

#### Servo Setting Details 

- Type
  - ドロップダウン内の以下タイプから選択
    - PWM : SG90PWMServo
    - SCS : Feetech SCS0009
    - DYN_XL330 : Dynamixel XL330
    - RT_DYN_XL330 : RTVersion 
    - M5_SCS : M5StackChan Servo
- Pin (x, y) 
  - 選んだTypeに応じて選択肢をドロップダウンで提示 (シリアルサーボは(x:RX, y:TX)に読み替え)。任意の値に変更も可。
    - PWM, SCS, DYN_XL330
      - Core2 PortA x:33, y:32 
      - Core2 PortB x:36, y:26
      - Core2 PortC x:13, y:14
      - CoreS3 PortA x:2, y:1
      - CoreS3 PortB x:9, y:8
      - CoreS3 PortC x:17, y:18
    - RT_DYN_XL330
      - x:6, y:7
    - M5_SCS
      - x:7, y:6
- Offset (x, y)
  - Typeによらないが、Typeを選んだときに x:0, y:0 に初期化。任意の値に変更も可。
- Center (x, y)
  - サーボの初期位置を、選択したTypeに応じて以下のように初期化。任意の値に変更も可。
    - PWM
      - x:90, y:90
    - SCS
      - x:150, y:150
    - DYN_XL330
      - x:180, y:270
    - RT_DYN_XL330
      - x:180, y:5
    - M5_SCS
      - x:150, y:85
- Lower limit (x, y)
  - サーボの可動範囲の下限を、選択したTypeに応じて以下のように初期化。任意の値に変更も可。
    - PWM
      - x:0, y:60
    - SCS
      - x:0, y:120
    - DYN_XL330
      - x:0, y:220
    - RT_DYN_XL330
      - x:90, y:-5
    - M5_SCS
      - x:0, y:0
- Upper limit (x, y)
  - サーボの可動範囲の上限を、選択したTypeに応じて以下のように初期化。任意の値に変更も可。
    - PWM
      - x:180, y:90
    - SCS
      - x:300, y:150
    - DYN_XL330
      - x:360, y:270
    - RT_DYN_XL330
      - x:270, y:15
    - M5_SCS
      - x:300, y:90
- Enable Takao Base
  - ドロップダウンで true か false を選択。Type選択時は false に初期化。

#### API
- `GET /config`: JSONで現在の設定値を返す。SPIFFS の設定ファイルが無い場合はデフォルト値またはRAM上の設定値を返す。
  - `source`: `spiffs` または `none`
  - `complete`: SPIFFS に `SC_SecConfig.yaml`、`SC_BasicConfig.yaml`、`SC_ExConfig.yaml` が揃っているか
  - `sec`: Wi-Fi と API key
  - `basic`: Servo 設定
  - `ex`: Realtime AI Service、Enable Memory、最大5件のMCPサーバー設定
- `POST /config`: POST body の JSON を検証し、SPIFFS に `SC_SecConfig.yaml`、`SC_BasicConfig.yaml`、`SC_ExConfig.yaml` として保存する。
- `POST /config/restart`: 設定反映のため再起動する。

#### 保存処理
- `SC_SecConfig.yaml` は `StackchanExConfig::saveSecretConfigYaml()` が `deserializeYml()` で構文と `wifi` / `apikey` セクションを検証する。
- `SC_BasicConfig.yaml` は Servo 関連、`takao_base`、`servo_type` のみを生成する。
- `SC_ExConfig.yaml` は `llm.type`、`llm.enableMemory`、`llm.mcpServers` を生成する。MCPサーバーは最大5件とし、各要素に `name`、`disabled`、`url`、`port` を保存する。
- `LLM_N_MCP_SERVERS_MAX` は5とし、設定読込、Web API、MCPクライアント配列で共通の上限として使用する。YAMLに6件以上ある場合は先頭5件だけを読み込む。
- 保存後は RAM 上の `_secret_config` も更新するが、Wi-Fi 再接続は行わず Web UI から再起動を促す。
- Save成功時は、設定した値を有効にするため Restart 前に SD カードを抜くよう画面に表示する。


### Personalize page
- ファイル構成
  - incbin/personalize.html
  - incbin/personalize.js
- 言語
  - ページやダイアログ内の言語は英語版のみ。
- 画面構成
  - Role (Custom Instructions)
  - Memory

#### Role (Custom Instructions)
- 構成
  - フォーム
    - ロール（カスタム指示）の入出力。
  - 設定ボタン
    - API /role_set をPOSTし、フォームの内容を設定する。
- 画面更新時の動作
  - API /role_get をPOSTし、現在設定されているカスタム指示を取得してフォームに表示する。 

#### Memory
- 構成
  - フォーム
    - 取得した記憶内容を表示。
  - Clearボタン
    - API /memory_clear をPOSTし、記憶内容を消去する。
    - 消去を実行する前に、OK/Cancelのダイアログを表示して本当に消去してよいかを確認する。
- 画面更新時の動作
  - API /memory_get をPOSTし、記憶内容を取得してフォームに表示する。
 
## Status Monitor

`StatusMonitorMod` shows runtime state in tab form on the avatar sub-window.

| Tab | Contents |
| --- | --- |
| System | Existing system status: firmware version, Wi-Fi IP/MAC, heap, battery. |
| AI Service | AI service name, memory enabled/disabled, MCP server list. |

The AI Service tab reads `llm.type`, `llm.enableMemory`, and `llm.mcpServers` from `StackchanExConfig`. The old Function Call info view is not shown because it belongs to the previous Function Calling design.

The graphical tab header is drawn through `Avatar::updateSubWindowCustom()`. This keeps Avatar running and lets `StatusMonitorMod` render directly into the SubWindow during the Avatar draw cycle.
The tabs are touch targets. `StatusMonitorMod` owns tab `box_t` hit areas aligned with the drawn tab rectangles, while physical BtnA/BtnC still move to the previous/next tab.

## Head Touch Sensor

`src/driver/HeadTouchSensor.*` provides the shared polling driver for the official CoreS3 head touch sensor.

- The driver owns Si12T initialization, 3ch sampling, and gesture detection.
- `AiStackChanMod` and `RealtimeAiMod` call `HeadTouchSensor::update()` from `idle()`.
- `SwipeForward` and `SwipeBackward` are treated as pet gestures.
- Each Mod keeps its own 3 second Happy expression timeout so the driver does not depend on Avatar.
- On non-CoreS3 builds, or when Si12T is not found, the driver is a no-op and normal Mod behavior continues.
