# Si12T Arduino Port

## 目的

M5Stack 公式版ハードの頭部 3ch タッチセンサ Si12T を、AI_StackChan_Ex firmware の Arduino/PlatformIO 環境で読み出せるようにする。初期確認として RealtimeAiMod の idle() から 3ch のタッチ値を取得し、シリアルモニタへ定期ログ出力する。

## 対象範囲

- `lib/M5StackChan/src/drivers/Si12T/Si12T.*`
- `lib/M5StackChan/src/hal_head_touch.cpp`
- `src/mod/AiStackChan/RealtimeAiMod.*`

## 主な変更方針

- `Si12T` ドライバは ESP-IDF の `i2c_master_*` 依存を外し、Arduino 環境で利用できる I2C 実装へ移植する。
- RealtimeAiMod 側では初期化を一度だけ行い、`idle()` で一定間隔ごとに `si12t_read_touch_result()` と `si12t_parse_touch_result_to()` を使って 3ch 値をログ出力する。
- `hal_head_touch.cpp` は公式 HAL、`hal_bridge`、`mooncake_log` など本リポジトリにない依存があるため、今回のテスト経路では直接使わない。ビルド対象に入って問題になる場合は、Arduino 環境でコンパイルされないよう最小限のガードを追加する。
- テスト用ログは長期運用に残しすぎないよう、間隔を空けるかビルドフラグで無効化しやすい形にする。

## 設計上の注意点

- 既存の M5Unified 初期化後に I2C を使うため、I2C バスの再初期化で既存デバイスへ副作用を出さない。
- 公式コピー元の MIT ライセンス表記は維持する。
- Core2/CoreS3/AtomS3R のビルド条件に注意し、少なくとも Realtime API 環境でビルドできる状態を保つ。
- 公式版ハード固有のセンサなので、未接続環境では初期化失敗をログ出力し、通常動作を継続する。

## 確認方法

- `pio run -e m5stack-cores3-realtime`
- 可能なら対象ハードで起動し、シリアルモニタに 3ch の値が出ることを確認する。
- センサ未接続時も RealtimeAiMod が落ちず、通常の idle 処理が継続することを確認する。
