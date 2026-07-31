# FW Design Information
Notes on FW design, etc.

- [Task](#task)
- [Mod](#mod)
  - [ESP-NOW Remote Control Mod](#esp-now-remote-control-mod)
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
