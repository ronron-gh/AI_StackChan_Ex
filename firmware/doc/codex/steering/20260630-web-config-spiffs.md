# Web Config on SPIFFS

## 目的

SD カードの YAML 設定と同等の内容を M5Stack 内蔵 Web アプリから設定できるようにし、SD カードが無い状態でも Wi-Fi と API キーを設定できる導線を追加する。

初期実装は、Wi-Fi と API key を含む `SC_SecConfig.yaml` の設定導線を全ビルド共通にし、`SC_ExConfig.yaml` など Realtime API 固有または構成差分が大きい設定編集は `REALTIME_API` ビルドのみを対象にする。

## 対象範囲

- 起動時の設定読み込み順序
  - SD カードに設定 YAML がある場合は SD を優先する。
  - SD カードまたは SD 上の YAML が使えない場合は SPIFFS の YAML を使う。
  - SPIFFS にも設定が無い場合は既存のデフォルト値と未設定扱いで起動し、設定用 AP または Wi-Fi なし起動を選べるようにする。
- Wi-Fi 接続失敗時のユーザー選択
  - YAML の Wi-Fi 設定で接続できない場合、画面上のボタンで AP モード起動または Wi-Fi なし起動を選択する。
  - SmartConfig は廃止し、AP モードでの Web 設定を標準導線にする。
- 設定用 AP
  - AP モードで起動した場合は Web サーバーを起動し、画面に設定用 Web アプリ URL の QR コードを表示する。
  - AP の SSID、パスワード、IP は固定デフォルトを持たせ、必要なら後続で変更可能にする。
- Web アプリ設定保存
  - 全ビルドで `SC_SecConfig.yaml` を Web UI から編集またはフォーム入力し、既存 YAML フォーマットで SPIFFS に保存できるようにする。
  - `REALTIME_API` ビルドでは `SC_ExConfig.yaml` と必要に応じた `SC_BasicConfig.yaml` の編集も扱う。
  - SD カードの YAML を SPIFFS にコピーするボタンを Web UI に設ける。

## Phase1 実装範囲

最初の実装では、設定対象を `SC_SecConfig.yaml` のみに限定する。

- 実装する
  - SPIFFS の `/SC_SecConfig.yaml` 読み書き。
  - Web UI の `home.html` / `config.html` / `config.js` 追加。
  - `GET /config`、`POST /config`、`POST /config/restart`。
  - YAML の Wi-Fi 設定だけを使った接続。
  - 接続失敗時の AP/Offline 選択。
  - AP モードでの Web サーバー起動と QR 表示。
  - `SC_ExConfig.yaml`、`SC_SecConfig.yaml`、`SC_BasicConfig.yaml` の一式が揃わない場合は通常機能を起動せず、設定 Web へ誘導する。
- 実装しない
  - `SC_ExConfig.yaml` の Web 編集。
  - `SC_BasicConfig.yaml` の Web 編集。
  - Realtime API 固有タブやフォーム。

Phase1 後の追加実装で、`REALTIME_API` ビルドに限って `SC_ExConfig.yaml` と必要に応じた `SC_BasicConfig.yaml` を拡張する。

設定ファイル一式が不足している場合は、`REALTIME_API` ビルドでは `SC_SecConfig.yaml` だけを読み込んで Wi-Fi 接続を試す。接続できれば STA 接続上で設定 Web を起動し、接続できなければ Config AP を起動する。AtomS3R は画面が小さいため QR は表示せず、Wi-Fi 接続失敗時は Config AP へ直接進む。`REALTIME_API` 以外のビルドでは、不完全な設定を画面に表示して待機する。

## Phase2 実装範囲

Phase2 では `REALTIME_API` ビルド向けに、`SC_SecConfig.yaml`、`SC_BasicConfig.yaml`、`SC_ExConfig.yaml` をフォーム入力から生成して SPIFFS に保存する。

- 実装する
  - Config page を YAML 直書き textarea から項目別フォームへ変更する。
  - `GET /config` を JSON 応答に変更し、現在値、設定元、設定ファイル一式の状態を返す。
  - `POST /config` を JSON 入力に変更し、フォーム値から 3 つの YAML を生成して SPIFFS に保存する。
  - `SC_SecConfig.yaml`
    - `wifi.ssid`
    - `wifi.password`
    - `apikey.aiservice`
    - `apikey.tts`
    - `apikey.stt`
  - `SC_BasicConfig.yaml`
    - Servo 関連のみ生成する。
    - Servo 以外の BasicConfig 項目は AI_StackChan_Ex では使わないため YAML から省略する。
  - `SC_ExConfig.yaml`
    - `llm.type`
    - `llm.enableMemory`
    - Realtime API ビルドで不要な ExConfig 項目は YAML から省略する。
  - Realtime AI Service の選択肢
    - OpenAI Realtime -> `llm.type: 0`
    - Google Gemini Live -> `llm.type: 3`
    - モデル名は Realtime API ビルドでは固定のため設定対象外。
  - Servo Type 選択時は、Pin/Offset/Center/Limit/Enable Takao Base のプリセット適用前に確認ダイアログを出す。
  - Save 成功後は「設定した値を有効にするために、Restart する前に SD カードを抜く」注意を表示する。
- 実装しない
  - Copy from SD ボタン。仕様が複雑になるため Phase2 で削除する。
  - Realtime API 以外のビルド向けフォーム拡張。
  - Realtime API で使わない `tts`、`stt`、`wakeword`、`audio`、`moduleLLM`、`mcpServers`、`customEndpoint` などのフォーム項目。

### Phase2 API

- `GET /config`
  - JSON を返す。
  - 例:
    - `source`: `sd` / `spiffs` / `partial` / `none`
    - `complete`: 3 YAML が揃っているか
    - `sec`: Wi-Fi と API key
    - `basic`: Servo 設定
    - `ex`: Realtime AI Service と memory 設定
- `POST /config`
  - JSON を受け取る。
  - 入力値を検証し、`SC_SecConfig.yaml`、`SC_BasicConfig.yaml`、`SC_ExConfig.yaml` を SPIFFS へ保存する。
  - 保存後は RAM 上の `system_config` に反映できる範囲は反映するが、実運用上は Restart を促す。
- `POST /config/copy_sd`
  - Phase2 で削除する。
- `POST /config/restart`
  - 既存どおり再起動する。

### Phase2 YAML 生成方針

生成する YAML は、動作に必要な最小キーのみを含める。

- `SC_SecConfig.yaml`
  - `wifi`
  - `apikey`
- `SC_BasicConfig.yaml`
  - `servo`
  - `takao_base`
  - `servo_type`
- `SC_ExConfig.yaml`
  - `llm.type`
  - `llm.enableMemory`

既存 YAML を読み込める場合でも、Phase2 の Save ではフォーム対象外項目を SPIFFS 側の生成 YAML へ持ち越さない。

## 主な変更候補ファイル

- `src/main.cpp`
  - 設定読み込み元の選択、Wi-Fi 接続失敗時の AP/Offline 選択、AP 起動、QR 表示。
- `src/StackchanExConfig.*`
  - 必要に応じて、読み込み元状態や設定保存補助の追加。ただし既存 `StackchanSystemConfig::loadConfig()` の使い方を優先する。
- `src/WebAPI.*`
  - 設定取得、保存、検証、SD から SPIFFS へのコピー API を追加する。
- `incbin/personalize.html`
- `incbin/personalize.js`
  - 既存 Personalize 画面に設定ページへの導線を追加するか、設定用 HTML/JS を別ファイルとして incbin する。
- `incbin/home.html`
  - Web アプリの入口。Personalize と Config への分岐を置く。
- `incbin/config.html`
- `incbin/config.js`
  - SPIFFS 設定編集用 UI。
- `doc/fw_design.md`
  - 起動時設定優先順位、SPIFFS 永続化ファイル、Web API を追記する。

## 設計方針

### 設定ファイル配置

SPIFFS 側のファイル名は既存 AtomS3R の配置に合わせる。

- `/SC_BasicConfig.yaml`
- `/SC_SecConfig.yaml`
- `/SC_ExConfig.yaml`

SD 側は現行の `loadConfig(SD, "/app/AiStackChanEx/SC_ExConfig.yaml")` に合わせ、既存ライブラリのデフォルトパスである `/yaml/SC_SecConfig.yaml` と `/yaml/SC_BasicConfig.yaml` を使う。SD から SPIFFS へのコピーでは、この 3 ファイルを対象にする。

### 読み込み優先順位

1. SPIFFS は全ボードで先に `begin()` する。
2. SD が利用可能で、少なくとも SD の主要 YAML が開ける場合は SD から `system_config.loadConfig()` する。
3. SD が利用できない、または SD 設定が不足する場合は SPIFFS から `system_config.loadConfig()` する。
4. どちらも無い場合は SPIFFS から存在しないファイルとして読み込み、既存の default/callback に寄せる。ただし SD 無しを理由に再起動しない。

SD 優先を保つため、SD が存在する場合に SPIFFS 設定を暗黙に上書きしない。SD から SPIFFS への反映は Web UI の明示操作のみとする。

### Wi-Fi 起動状態

Wi-Fi 接続は読み込んだ YAML の `wifi.ssid` / `wifi.password` のみで試す。

1. `wifi.ssid` が空の場合は接続を試さず、画面に AP/Offline 選択 UI を表示する。
2. `wifi.ssid` がある場合は `WiFi.begin(ssid, password)` で接続を試す。
3. 接続に失敗した場合、画面に選択 UI を表示する。
   - BtnA または画面左: AP モードで設定する。
   - BtnC または画面右: Wi-Fi なしで起動する。

AP モード選択時は `WiFi.mode(WIFI_AP)` と `WiFi.softAP()` を使い、Web サーバーを起動する。AP モードは「設定可能だが通常のオンライン AI サービスは未接続」として扱うため、会話処理は offline 相当にする。

引数なしの `WiFi.begin()` による前回接続情報の試行は行わない。SmartConfig 廃止後は前回接続情報と YAML 設定の二重管理を避け、起動時の接続元を YAML に一本化する。

### QR 表示

QR は既存 `avatar.updateSubWindowQrcode()` の利用を第一候補にする。ただし Avatar 初期化前の起動中画面で必要な場合は、M5GFX の QR 描画 API または簡易表示関数を別途確認して使う。

表示 URL は AP IP に合わせて `http://192.168.4.1/` を基本とする。

### Web API

`SC_SecConfig.yaml` を扱う最小 API は全ビルドで有効化する。`SC_ExConfig.yaml` や Realtime API 固有項目を扱う API は `REALTIME_API` ビルドのみで有効化する。

- `GET /config`
  - SPIFFS 上の YAML 内容と、現在の設定読み込み元を返す。
  - 全ビルドでは `SC_SecConfig.yaml` を返す。
  - `REALTIME_API` ビルドでは `SC_ExConfig.yaml` と必要に応じた `SC_BasicConfig.yaml` も返す。
- `POST /config`
  - YAML 文字列または JSON フォーム入力を受け取り、YAML として SPIFFS に保存する。
  - 全ビルドでは `SC_SecConfig.yaml` のみ保存対象にする。
  - `REALTIME_API` ビルドでは `SC_ExConfig.yaml` と必要に応じた `SC_BasicConfig.yaml` も保存対象にする。
  - 最初の実装では YAML 文字列保存を優先し、フォーム UI は主要項目から段階的に増やす。
- `POST /config/restart`
  - 保存後に再起動するための明示 API。自動再起動は UI 側で確認してから呼ぶ。

保存前に `deserializeYml()` で最低限の構文検証を行い、壊れた YAML を保存しない。

### `StackchanExConfig` の追加責務

`StackchanSystemConfig` は外部ライブラリ配下のため直接変更しない。派生クラスである `StackchanExConfig` に、Web UI から扱う設定ファイルの薄い保存・検証 API を追加する。

`SC_SecConfig.yaml` は親クラスの protected メンバー `_secret_config` と protected 関数 `setSecretConfig()` / `loadSecretConfig()` が担当しているため、`StackchanExConfig` 側ではこれらを public な用途別関数で包む。

追加候補の public 関数:

- `bool validateSecretConfigYaml(const String& yaml, String* error = nullptr)`
  - Web UI から受け取った YAML を `deserializeYml()` で構文検証する。
  - `wifi.ssid`、`wifi.password`、`apikey.aiservice`、`apikey.tts`、`apikey.stt` のキーが扱える形か確認する。
  - API key は空文字を許可する。Wi-Fi SSID も、意図的に未設定で保存する余地を残すため空文字自体はエラーにしない。
- `bool saveSecretConfigYaml(fs::FS& fs, const char* path, const String& yaml, String* error = nullptr, bool apply_now = true)`
  - `validateSecretConfigYaml()` で検証する。
  - `path + ".tmp"` に一度書き込み、成功後に本ファイルへ反映する。
  - `apply_now` が true の場合は、保存した YAML を `setSecretConfig()` に渡して RAM 上の `_secret_config` へ反映する。
  - ただし Wi-Fi 再接続はこの関数では行わず、Web UI から再起動を促す。
- `bool loadSecretConfigYaml(fs::FS& fs, const char* path, uint32_t yaml_size = 2048)`
  - 起動時や保存後の明示再読み込み用の wrapper。
  - 内部では親クラスの `loadSecretConfig()` を呼ぶ。
- `String exportSecretConfigYaml(bool mask_secret = false)`
  - RAM 上の `_secret_config` から `SC_SecConfig.yaml` 形式の YAML を生成する。
  - `mask_secret` が true の場合は password/API key を伏せた表示用文字列にする。
  - 実保存や編集用には原則として SPIFFS 上のファイル内容を返し、ファイルが無い場合の初期テンプレートとして使う。

追加候補の private 関数:

- `bool writeFileAtomic(fs::FS& fs, const char* path, const String& data, String* error)`
  - 一時ファイルへの書き込み、flush/close、rename をまとめる。
- `bool parseSecretConfigYaml(const String& yaml, DynamicJsonDocument& doc, String* error)`
  - 検証と `setSecretConfig()` の両方で使う YAML parse 共通処理。

Web UI から `SC_SecConfig.yaml` を保存する内部処理は次の流れにする。

1. `config.js` が `/config` へ対象 `SC_SecConfig.yaml` と YAML 本文を POST する。
2. `WebAPI.cpp` が本文を受け取り、`system_config.saveSecretConfigYaml(SPIFFS, "/SC_SecConfig.yaml", body, &error)` を呼ぶ。
3. `StackchanExConfig` が YAML を parse し、壊れた YAML や想定外のトップレベル構造を拒否する。
4. 検証成功時だけ SPIFFS へ一時ファイル経由で保存する。
5. 保存後、RAM 上の `_secret_config` を更新する。
6. `WebAPI.cpp` は成功を返し、UI は「再起動すると新しい Wi-Fi 設定で接続する」旨を表示して `/config/restart` の操作を促す。

この分担により、Web API 側は HTTP とファイル種別の分岐に集中し、YAML の意味や `_secret_config` の更新は `StackchanExConfig` に閉じ込める。

### Web UI

初期実装では設定の完全性と実装リスクを優先し、以下の二段構えにする。

- YAML 編集ビュー
  - 全ビルドでは `SC_SecConfig.yaml` を編集。
  - `REALTIME_API` ビルドでは `SC_SecConfig.yaml`、`SC_ExConfig.yaml`、必要なら `SC_BasicConfig.yaml` をタブで編集。
  - 保存、再読み込み、再起動、SD からコピーの操作を提供する。
- 主要項目フォーム
  - Wi-Fi SSID/password。
  - OpenAI/Realtime API 用 API key。
  - Realtime API で必要な最小限のモデルや音量など。

既存 `personalize.html/js` は Role/Memory 用として残し、設定 UI は `config.html/js` を追加する方針を第一候補にする。

### 画面切り替え

Web アプリの入口として `home.html` を追加し、`/` は `home.html` を返す。`home.html` には次の 2 つの導線を置く。

- Personalize
  - 既存の `/personalize.html` に遷移する。
  - Role と Memory の管理を行う。
- Config
  - 新規の `/config.html` に遷移する。
  - Wi-Fi、API key、Realtime API 用 YAML 設定、SD から SPIFFS へのコピーを行う。

設定用 AP モードで QR に載せる URL も `/` とし、スマートフォンで開いた直後に用途を選べるようにする。既存利用者向けに `/personalize.html` はそのまま残す。

`home.html` は説明を増やしすぎず、2 つの大きな操作ボタンと現在の接続状態または設定読み込み元の短い表示に留める。AP モードでは Config を主導線として上に配置し、通常 STA 接続時は Personalize と Config を並列に扱う。

## 設計上の注意点

- API キーや Wi-Fi パスワードをログへ出さない。既存ログに表示される箇所は今回の実装時に抑制を検討する。
- SPIFFS 保存中の電源断で設定が壊れる可能性があるため、可能なら一時ファイルへ書いてからリネームする。
- AP モードで Web サーバーを動かす場合、`isOffline` の意味が「Web サーバー停止」と衝突する。`wifiMode` のような状態を分ける設計にする。
- `StackchanSystemConfig` は外部ライブラリ配下のため、直接変更せず `StackchanExConfig` と起動側で吸収する。
- AtomS3R は画面とボタン制約が異なるため、初期実装の対象デバイス確認を Core2/CoreS3 優先にする。
- `SC_SecConfig.yaml` の AP 設定導線は全ビルドに入るため、`WebAPI.cpp` の基本ルートと SPIFFS 書き込み処理は `REALTIME_API` に依存しない構成にする。
- `REALTIME_API` 以外のビルドでは、Config UI 上の Realtime API 固有タブや項目は表示しない。

## 確認方法

- `m5stack-core2-realtime` または `m5stack-cores3-realtime` でビルド確認する。
- SD あり、SD なし、SPIFFS 設定あり、SPIFFS 設定なしの起動分岐をログで確認する。
- Wi-Fi 接続失敗時に AP/Offline を選択できることを確認する。
- AP 接続後、QR の URL から Web UI に到達できることを確認する。
- Web UI で SPIFFS に YAML 保存し、再起動後に設定が読み込まれることを確認する。
- SD から SPIFFS へのコピー後、SD を抜いた状態で同じ設定が使われることを確認する。
