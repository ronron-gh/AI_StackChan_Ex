# ESP-NOW Remote Servo 反映 設計方針

> 2026-05-19 update: yaw は符号反転し、`-1280..1280` を servo x `45..-45` 度へ線形変換する。payload の speed は `ServoCustom::moveTo()` の移動時間 ms として渡す。

## 目的

ESP-NOW Remote Control Mod で受信した標準 payload の yaw/pitch を、AI_StackChan_Ex のサーボ制御へ反映する。

今回は Receiver 側のサーボ反映に絞り、Sender、設定 UI、laser 制御は引き続き対象外とする。

## 対象範囲

- `src/mod/EspNowRemote/EspNowRemoteMod.cpp`
- `src/mod/EspNowRemote/EspNowRemoteMod.h`
- `src/main.cpp`
- 必要に応じて `doc/fw_design.md`
- 既存の `ServoCustom::moveTo(int degX, int degY)` を利用し、`ServoCustom` には原則変更を加えない。

## 既存構造

- `main.cpp` の `servo(void *args)` task は Avatar の gaze を読み取り、`robot->servo->moveTo()` でサーボを動かしている。
- この task が動いたままだと、ESP-NOW 受信値で動かした直後に gaze 連動制御で上書きされる。
- `ServoCustom::moveTo(int degX, int degY)` は原点からの相対角度を受け取り、内部で `moveXY(start_degree + degX, start_degree + degY, 1000)` を呼ぶ。

## 実装方針

- ESP-NOW Remote Mod が有効な間だけ、`main.cpp` の servo task による Avatar gaze 連動制御を停止する。
- 停止方法はグローバルな `servo_home` の意味を増やしすぎず、ESP-NOW remote 用の共有フラグを追加する案を優先する。
- `EspNowRemoteMod::init()` で remote servo override を有効化し、`pause()` で無効化する。
- `servo()` task は remote override が有効な間、`moveToOrigin()` や gaze 連動 `moveTo()` を呼ばず、ESP-NOW Mod 側の制御を邪魔しない。
- ESP-NOW 受信コールバックではサーボを直接動かさず、これまで通り受信データをバッファへコピーするだけにする。
- decode とサーボ制御は `EspNowRemoteMod::idle()` 側で行う。

## 角度変換

受信 payload のアプリデータは標準 Sender との実機確認により offset `20` を優先し、8 byte payload のみの場合は offset `0` を fallback とする。

標準 payload:

| Offset | 型 | 内容 |
| --- | --- | --- |
| 0 | `uint8_t` | target id |
| 1-2 | `int16_t` little endian | yaw |
| 3-4 | `int16_t` little endian | pitch |
| 5-6 | `int16_t` little endian | speed |
| 7 | `uint8_t` | laser |

サーボへ渡す値:

- yaw `-1280` から `1280` を servo x `-45` 度から `45` 度へ線形変換する。
- pitch `0` から `900` を servo y `0` 度から `-30` 度へ線形変換する。
- pitch は符号に注意し、`pitch = 900` のとき `y = -30` とする。
- 範囲外の値は入力範囲へ clamp してから変換する。
- 変換後の `x` と `y` は `robot->servo->moveTo(x, y)` へ渡す。

## 影響と副作用

- ESP-NOW Remote Mod 実行中は Avatar gaze 連動のサーボ制御が停止する。
- 他 Mod に切り替えたときは `pause()` で remote override を解除し、既存の servo task が再び動ける状態に戻す。
- ESP-NOW 受信ごとに `moveTo()` を呼ぶため、受信頻度が高い場合はサーボコマンドが多くなる。初回は標準 Sender の 50 ms 間隔を前提にし、必要なら後続で間引きを追加する。
- Wi-Fi/Web/Realtime API との channel 干渉は前段の Receiver 実装と同じく残る。

## 戻し方

- `EspNowRemoteMod` での `robot->servo->moveTo()` 呼び出しを削除する。
- remote override フラグと `servo()` task 側の分岐を削除する。
- `doc/fw_design.md` のサーボ反映メモを戻す。

## 確認方法

- `pio run -e m5stack-core2-realtime`
- 可能なら `pio run -e m5stack-cores3-realtime`
- 実機で ESP-NOW Sender から yaw/pitch を送信し、Serial decode とサーボの向きが対応することを確認する。
- ESP-NOW Remote Mod から別 Mod へ切り替えた後、Avatar gaze 連動サーボ制御が復帰することを確認する。

## 未決事項

- target id によるフィルタリングを今回入れるか、サーボ反映後の次段階にするか。
- `speed` を `ServoCustom::moveTo()` の移動時間へ反映するか。今回の指定では `moveTo()` の既定動作に従う。
- 受信頻度が高い場合のサーボ制御間引き、同一値スキップ、移動中判定の扱い。
