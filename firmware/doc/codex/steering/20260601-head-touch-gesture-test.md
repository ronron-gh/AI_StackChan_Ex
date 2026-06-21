# Head Touch Gesture Test

## 目的

公式版 CoreS3 ハードの頭部 3ch タッチセンサで、頭を撫でられた動きを判定できるかを最小実装で確認する。初期確認として RealtimeAiMod::idle() から判定結果をシリアルモニタへログ出力する。

## 対象範囲

- `src/mod/AiStackChan/RealtimeAiMod.*`
- `lib/M5StackChan/src/hal_head_touch.cpp`
- 必要に応じて `doc/fw_design.md`

## 主な変更方針

- 公式ファーム由来の `TouchData` と `GestureRecognizer` の考え方を RealtimeAiMod のテスト実装に取り込む。
- 既存の `logHeadTouchSensor()` で行っている Si12T 読み出しを流用し、3ch 値から Press / Release / SwipeForward / SwipeBackward を判定する。
- 判定結果が変化したときだけ、シリアルモニタへ raw 値、3ch 値、位置、ジェスチャ名を出力する。
- まずは RealtimeAiMod 内に閉じたテスト実装とし、イベント化や他 Mod への共有は今回の範囲外にする。

## 設計上の注意点

- `hal_head_touch.cpp` は Arduino ビルド対象外にしているため、今回はコードを直接リンクせず、ロジックのみ小さく移植する。
- idle() の処理を重くしないよう、読み出し周期を短すぎない値にする。
- センサ未接続時は初期化失敗ログのみで通常動作を継続する。
- 公式コピー元の MIT ライセンス表記は維持し、コピー元ファイルを不要に改変しない。

## 確認方法

- `pio run -e m5stack-cores3-realtime`
- 実機で頭部センサに触れ、Press / Release / SwipeForward / SwipeBackward のログが出ることを確認する。
- 未接続または初期化失敗時も RealtimeAiMod の通常処理が継続することを確認する。
