# ESP-NOW Remote Control Mod 設計方針

> 検討再開: Mod 管理の大きな見直しは保留し、既存 Mod 管理の思想を維持したまま ESP-NOW Remote Control Mod の追加方針を検討する。

## 目的

標準 firmware の `AppEspnowControl` と互換の ESP-NOW Remote Control 機能を、AI_StackChan_Ex の `src/mod` 配下の mod として追加する。

## 今回の小実装スコープ

まずは ESP-NOW の受信経路だけを確認する。

- Receiver のみ実装する。
- Wi-Fi channel は `1` 固定とする。
- 受信した payload は servo や UI へ反映せず、シリアルモニタへ hex dump として出力する。
- target id、yaw/pitch/speed、laser bit の解釈は行ってもよいが、動作には使わない。
- 標準 firmware の Sender から受けた場合、Arduino `esp_now` の受信 data ではアプリ payload が offset `20` から始まる実機結果があるため、decode は offset `20` を優先し、8 byte だけ届く場合は offset `0` を fallback とする。
- Sender、channel/id 選択 UI、servo 制御、laser 制御は今回の対象外とする。

## 対象範囲

- 標準実装 `app_espnow_ctrl.cpp` のパケット仕様、Receiver/Sender の役割、channel/id の考え方を踏襲する。
- AI_StackChan_Ex は Arduino/PlatformIO 実装のため、標準実装の Mooncake/LVGL/HAL は直接移植せず、既存の `ModBase` と M5Unified/Arduino ESP-NOW API に合わせて実装する。
- まずは Core2/CoreS3 系の `USE_SERVO` 環境を主対象とする。AtomS3R は UI/ボタン操作の差分を確認し、必要なら制限を明示する。

## 主な変更候補ファイル

- `src/mod/EspNowRemote/EspNowRemoteMod.h`
- `src/mod/EspNowRemote/EspNowRemoteMod.cpp`
- `src/main.cpp`
- `doc/fw_design.md`（必要に応じて）

## 標準仕様との対応

ESP-NOW payload は標準実装に合わせて 8 byte 固定とする。

| Offset | 型 | 内容 |
| --- | --- | --- |
| 0 | `uint8_t` | target id。`0` は broadcast |
| 1-2 | `int16_t` little endian | yaw angle |
| 3-4 | `int16_t` little endian | pitch angle |
| 5-6 | `int16_t` little endian | speed |
| 7 | `uint8_t` | laser enabled。`0` off、非 0 on |

標準値は Receiver ID `1`、Wi-Fi channel `1`、Sender の送信間隔 `50 ms`、Sender speed `800` を踏襲する。

## 実装方針

- `EspNowRemoteMod` は `ModBase` を継承し、今回の小実装では `init()` で Receiver/channel 1 固定として ESP-NOW を開始する。
- 起動時の選択 UI は今回作らない。
- ESP-NOW 初期化は mod 内の薄いヘルパーに閉じ込める。Arduino 環境で利用できる `esp_now.h` と `esp_wifi.h` を使い、Wi-Fi は STA mode、channel は `1` 固定とする。
- 送信先 MAC は標準実装と同じ broadcast を基本とし、target id による論理フィルタリングで互換性を保つ。
- Receiver は受信コールバック内で payload を固定バッファへコピーし、Serial 出力は `idle()` 側で行う。
- servo と laser は今回触らない。

## 設計上の注意点

- ESP-NOW channel 固定は既存 Wi-Fi/Web/FTP/Realtime API と干渉する可能性がある。オンライン機能との併用可否を UI とドキュメントで明示する。
- `esp_now_deinit()` や Wi-Fi channel 復帰の扱いは `pause()` で整理する。必要なら mod 離脱時に再起動を促す標準実装相当の運用も検討する。
- コールバックから avatar、servo、M5.Display を直接触らない。
- payload の endian、サイズ、id filter は標準互換を優先し、独自拡張は行わない。
- API キーや Wi-Fi 認証情報を追加しない。

## 確認方法

- `pio run -e m5stack-core2-realtime`
- 可能なら `pio run -e m5stack-cores3-realtime`
- 実機で ESP-NOW Sender から channel 1 へ送信し、シリアルモニタに受信 payload が表示されることを確認する。

## 未決事項

- role/channel/id の設定を毎回選択にするか、YAML/NVS/SPIFFS に保存するか。
- yaw/pitch の標準角度範囲を AI_StackChan_Ex の servo 設定へどう正規化するか。
- laser bit を将来どの GPIO/外部モジュールに割り当てるか。
- ESP-NOW mod 使用中に Wi-Fi/Web/Realtime API を停止するか、同一 channel 接続時のみ併用可とするか。
