# ESP-NOW Mod 離脱時 Wi-Fi 再接続 設計方針

## 目的

ESP-NOW Remote Mod から他の Mod へ切り替えたあと、Wi-Fi/Web/FTP/Realtime API を使う Mod が再び動けるように、ESP-NOW 停止後に Wi-Fi STA 接続を復帰する。

## 背景

ESP-NOW Remote Mod は Receiver 用に `WiFi.mode(WIFI_STA)`、`WiFi.disconnect(false, false)`、`esp_wifi_set_channel(1, ...)`、`esp_now_init()` を行う。これにより、起動時に確立していた Wi-Fi 接続は切断され、channel も ESP-NOW 用に固定される。

既存の `main.cpp` では起動時に Wi-Fi 接続、Web server 初期化、FTP server 初期化、NTP 同期を行い、`loop()` では `isOffline == false` のとき `web_server_handle_client()` と `ftpSrv.handleFTP()` を継続実行している。Web/FTP server は起動済みなので、ESP-NOW Mod 離脱時は原則として Wi-Fi STA の再接続だけを行う。

## 対象範囲

- `src/mod/EspNowRemote/EspNowRemoteMod.h`
- `src/mod/EspNowRemote/EspNowRemoteMod.cpp`
- 必要に応じて `doc/fw_design.md`

`main.cpp` の起動時 Wi-Fi 接続処理や SmartConfig の流れは今回変更しない。

## 実装方針

- `EspNowRemoteMod::pause()` で ESP-NOW を停止したあと、Wi-Fi 再接続を試みる。
- `isOffline == true` の場合は再接続を行わない。
- 再接続処理は Mod 内の helper に閉じ込める。
- 既存の `Wifi_connection_check()` は M5.Display へ進捗表示する起動時向け処理なので、Mod 離脱時には使わない。
- Mod 離脱時の再接続待ちは Serial 出力中心にし、最大 5 秒程度で打ち切る。
- 認証情報は新たに読まない。まずは `WiFi.begin()` で Arduino Wi-Fi の保持している前回設定を使う。
- `WiFi.begin()` で失敗した場合でも `isOffline` は変更しない。次の Mod で Web/Realtime API が使えない可能性は Serial に出す。

## 処理順序

1. `espnow_remote_servo_override = false`
2. `stopEspNowReceiver()`
3. `avatar` 表示を戻す
4. offline でなければ Wi-Fi reconnect

Wi-Fi reconnect の中身:

1. `WiFi.disconnect(false, false)`
2. `WiFi.mode(WIFI_STA)`
3. `WiFi.begin()`
4. 最大 5 秒、`WiFi.status() == WL_CONNECTED` を待つ
5. 成功時は IP address を Serial 出力
6. 失敗時は timeout を Serial 出力

## 影響と副作用

- Mod 切り替え時に最大 5 秒程度待つ可能性がある。
- ESP-NOW Mod 離脱後、AP の channel に再接続できれば Web/FTP/Realtime API は既存の server/task を使って復帰できる見込み。
- 既存接続中の Web/FTP client は ESP-NOW Mod 開始時点で切断される可能性がある。
- 起動時に offline mode へ入った場合は再接続しないため、既存の offline 判定を尊重する。

## 戻し方

- `EspNowRemoteMod::pause()` から Wi-Fi reconnect helper 呼び出しを削除する。
- reconnect helper を削除する。
- `doc/fw_design.md` の該当メモを戻す。

## 確認方法

- `pio run -e m5stack-core2-realtime`
- 可能なら `pio run -e m5stack-cores3-realtime`
- 実機で Wi-Fi 接続済み状態から ESP-NOW Remote Mod に切り替え、別 Mod へ戻ったとき Serial に reconnect success と IP address が出ることを確認する。
- 別 Mod へ戻ったあと、Web UI または Realtime API など Wi-Fi 依存機能が動くことを確認する。

## 未決事項

- Wi-Fi 再接続失敗時に画面へ通知するか。
- `WiFi.begin()` だけで復帰しない環境向けに、`system_config.getWiFiSetting()` の SSID/password を使う fallback を入れるか。
- Web/FTP server の再初期化が必要なケースがあるか。
