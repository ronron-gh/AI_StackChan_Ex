# Continuous conversation mode

## Purpose

ChatGPT モードで応答完了後、追加でウェイクワード不要で次の指示を受け付ける。
3往復までの会話が、毎回呼び出しなしで継続できるようにする。

## Scope

- `firmware/src/mod/AiStackChan/AiStackChanMod.cpp`
  - `STT_ChatGPT()` で応答完了後にループで `robot->listen()` を呼ぶ

## Main changes

応答完了後（既存処理）に以下のループを追加：

```cpp
const int MAX_CONTINUOUS_TURNS = 2;
for (int turn = 0; turn < MAX_CONTINUOUS_TURNS; turn++) {
  avatar.setSpeechText("次のご用件は？");
  led_set(0x00FF00);
  String more = robot->listen();
  if (more == "" || more.length() < 2) break;
  led_set(0x0080FF);
  robot->chat(more, base64_buf);
}
```

## Design notes

- `robot->listen()` は固定 7 秒録音 → 無音時は空文字列を返す前提
- 最大 2 ターン（初回 + 連続 2 ターン = 計 3 往復）で打ち切り、無限ループを防ぐ
- 無音 / 認識失敗で即終了 → 自然な会話終了
- LED は既存の制御を流用（受付＝緑、応答＝水色）
- Realtime API モードは変更しない（既に対話的な動作のため）

## Verification

- 「こんにちは」→ 応答 → 「次のご用件は？」表示 → 無音 → 終了
- 「天気は？」→ 応答 → 「次のご用件は？」→ 「時間は？」→ 応答 → 「次のご用件は？」→ 無音 → 終了
- 3 ターン目で必ず終了する（無限ループしない）

## Rollback

- 追加したループ部分を削除
- `git revert` でも復元可能

## Out of scope

- ターン数の YAML 設定化
- VAD（Voice Activity Detection）による録音時間の自動調整
- 連続会話中のウェイクワード再検知
