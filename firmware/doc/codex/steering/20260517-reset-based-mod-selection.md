# リセット前提の Mod 選択設計方針

> 保留中: Wi-Fi/Web/FTP/Realtime API などの共通基盤を使い回して簡単に Mod を追加できる既存思想も重要なため、本見直しは現時点では実装しない。

## 目的

Wi-Fi を使う Mod と ESP-NOW を使う Mod を同一ランタイム内で切り替える場合の後始末をなくすため、Mod 管理を「起動時に 1 つ選択し、別 Mod へ移るときはシステムリセットして選択画面に戻る」方式へ見直す。

これにより、各 Mod の初期化処理は前の Mod の Wi-Fi、ESP-NOW、Web server、FTP、Realtime API、タスク状態を考慮しない設計にする。

## 背景

現在の `ModManager` は `std::deque<ModBase*> modList` を持ち、フリックやボタン操作で `change_mod()` を呼び、同一プロセス内で `pause()` と `init()` を順番に実行して Mod を巡回する。

この方式では軽い UI Mod には向いているが、Wi-Fi channel、ESP-NOW、Web/FTP server、Realtime API、音声タスク、servo 制御のようなグローバル状態を持つ Mod では、前の Mod の状態を次の Mod が意識する必要が出る。

## 対象範囲

- `src/mod/ModManager.h`
- `src/mod/ModManager.cpp`
- `src/mod/ModBase.h`
- `src/main.cpp`
- 必要に応じて各 Mod の「選択画面へ戻る」操作
- `doc/fw_design.md`

ESP-NOW Remote Control Mod 自体の詳細設計は、`20260517-espnow-remote-mod.md` を保留し、本設計の後に再開する。

## 基本方針

- 起動直後は Mod 選択画面を表示する。
- 選択された Mod だけを生成または初期化する。
- 実行中 Mod から他の Mod へ切り替える操作は、直接 `change_mod()` せず、システムリセットして Mod 選択画面へ戻る。
- 各 Mod は「自分が最初に起動される」前提で初期化できるようにする。
- Mod 終了時の複雑な Wi-Fi/ESP-NOW 復旧処理は原則不要にする。

## UI 方針

- 起動時の Mod 選択画面は、M5Unified の Display/Touch/Buttons と既存 Avatar 表示の範囲で実装する。
- Core2/CoreS3 はタッチ選択と A/C 移動、B 決定を基本にする。
- AtomS3R は画面/ボタン制約があるため、A/C 相当がない場合は短押しで移動、長押しで決定など、既存操作に合わせて分岐する。
- 実行中 Mod で「戻る」操作が必要な場合は、共通ヘルパーを通じて `ESP.restart()` を行う。

## ModManager の見直し案

- `add_mod(ModBase*)` で実体を全部作って巡回する方式をやめる、または互換用に残しても通常経路からは使わない。
- Mod 一覧は名称、生成関数、利用条件を持つ軽量な定義として管理する。
- 選択後に該当 Mod のみを `new` して `init()` する。
- `get_current_mod()` は選択済み Mod のポインタを返す。
- `change_mod()` は廃止候補とし、残す場合も `request_mod_select_restart()` 相当へ置き換える。

## 起動シーケンス方針

初期化順序は次のどちらかを検討する。

案 A: 現状に近い順序

- 設定読み込み、Wi-Fi 接続、Robot 作成、Avatar 初期化を行う。
- その後に Mod 選択画面を出し、選択 Mod を初期化する。
- 既存 AI/Web 系 Mod への影響が小さい。
- ESP-NOW 専用 Mod でも一度 Wi-Fi 接続を試みるため、起動が遅くなる。

案 B: Mod 選択を先に行う順序

- 最小限の M5/Display/設定読み込みだけ行い、先に Mod を選択する。
- 選択 Mod の属性に応じて Wi-Fi/Web/FTP/Realtime API を初期化する。
- ESP-NOW 専用 Mod 起動時の余計な Wi-Fi 初期化を避けられる。
- `setup()` の分割が大きくなるため、段階的な移行が必要。

初回実装は案 A で既存動作を崩さず導入し、ESP-NOW Mod 追加時に案 B へ進めるか判断する。

## 設計上の注意点

- `ESP.restart()` 前にユーザーへ短い表示を出し、意図した再起動であることを分かるようにする。
- SD-Updater、設定読み込み失敗、SmartConfig、offline mode の既存挙動を壊さない。
- `loop()` のフリック処理は直接 `change_mod()` せず、選択画面へ戻る操作に変更する。
- タッチや長押しが連続発火して再起動を繰り返さないよう、restart request は一度だけ処理する。
- API キー、Wi-Fi パスワード、個人情報をドキュメントやコードに追加しない。

## 段階的な実装手順

1. Mod 定義と選択画面を追加し、起動時に選択した Mod のみ `init()` する。
2. `loop()` の左右フリックによる `change_mod()` を、選択画面へ戻るための再起動要求へ変更する。
3. 既存 Mod が単独起動で動くことを確認する。
4. `change_mod()` の扱いを整理し、不要なら削除、互換が必要なら非推奨として残す。
5. `doc/fw_design.md` に Mod 管理の新方針を追記する。

## 確認方法

- `pio run -e m5stack-core2-realtime`
- `pio run -e m5stack-cores3-realtime`
- 必要に応じて `pio run -e m5stack-atoms3r-realtime`
- 実機で起動時に Mod 選択画面が出ることを確認する。
- 各既存 Mod を選択して単独起動できることを確認する。
- 実行中の戻る操作またはフリックで再起動し、Mod 選択画面へ戻ることを確認する。

## 未決事項

- 選択した Mod を次回起動時に記憶するか、毎回選択画面を出すか。
- 初期化順序を案 A から案 B へ移すタイミング。
- Mod ごとの Wi-Fi 必要/不要、Web/FTP 必要/不要をどの粒度で定義するか。
- 既存の左右フリック操作を「選択画面へ戻る」にするか、誤操作防止のため長押しなどに変更するか。
