# Head Touch Driver Happy Expression

## 目的

公式版 CoreS3 ハードの頭部タッチセンサ処理を RealtimeAiMod から切り離し、AiStackChanMod と RealtimeAiMod の両方で利用できる driver 層の機能にする。頭を撫でられたと判定した場合、Avatar の表情を 3 秒間 Happy にする。

## 対象範囲

- `src/driver/HeadTouchSensor.*`
- `src/mod/AiStackChan/AiStackChanMod.*`
- `src/mod/AiStackChan/RealtimeAiMod.*`
- 必要に応じて `doc/fw_design.md`

## 主な変更方針

- `src/driver/HeadTouchSensor.cpp` と対応するヘッダを追加し、Si12T の初期化、3ch 読み出し、Press / Release / SwipeForward / SwipeBackward 判定を移す。
- RealtimeAiMod に入っているテスト用の Si12T ハンドル、状態機械、ログ処理を削除し、driver API 呼び出しに置き換える。
- driver は CoreS3 以外やセンサ未接続でも通常動作を妨げないよう、初期化失敗時は無効状態で継続する。
- 各 Mod の `idle()` で driver をポーリングし、SwipeForward または SwipeBackward を「撫でられた」と扱って Happy 表情を 3 秒間保持する。
- 既存の発話中表情やカメラ/顔検出時の表情制御との競合を最小化するため、Happy の期限管理は各 Mod 側に持たせ、期限切れ時に必要な範囲で Neutral へ戻す。

## 設計上の注意点

- `main.cpp` には機能固有ロジックを増やさず、driver と Mod 側に閉じる。
- `hal_head_touch.cpp` は公式 HAL 依存の参考実装として維持し、Arduino ビルド対象外の扱いを変えない。
- idle() の負荷を抑えるため、HeadTouchSensor 側でサンプリング周期を管理する。
- Realtime API with TTS の発話中 Happy 表情と、撫で Happy が互いに不自然に上書きしないようにする。
- まずは公式ハード向けの CoreS3 有効化に限定し、Core2/AtomS3R では no-op とする。

## 確認方法

- `pio run -e m5stack-cores3-realtime`
- 必要に応じて `pio run -e m5stack-cores3`
- 実機で頭部を撫で、AiStackChanMod と RealtimeAiMod の両方で Avatar が 3 秒間 Happy になることを確認する。
- センサ未接続時や CoreS3 以外の環境で通常の idle 処理が継続することを確認する。
