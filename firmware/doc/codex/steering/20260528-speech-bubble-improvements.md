# Speech bubble improvements (M5公式版準拠)

## Purpose

M5Stack 公式キット (M5STACK-K151) で動かす際、吹き出しの挙動を M5公式版 (`../Stackchan`) に近づける：

1. 吹き出しを顔の回転から独立させ、画面上で位置固定する
2. 通常 ChatGPT モードでも AI応答全文を吹き出しに表示する

## Scope

- `lib/m5stack-avatar/src/Face.cpp` — Balloon の描画位置を回転外レイヤーへ移動
- `src/llm/ChatGPT/ChatGPT.cpp` — 応答テキストを `avatar.setSpeechText()` に渡す

## Main changes

### Face.cpp

- 回転前 sprite への `b.draw(sprite, br, ctx)` 呼び出しを削除
- 回転後の tmpSprite に `battery.draw` / `subWindow->draw` と並んで `b.draw(tmpSprite, br, ctx)` を追加
- 回転スキップ時のブランチ (`sprite->pushSprite(&M5.Display, ...)`) でも Balloon を画面に直接描画

### ChatGPT.cpp

- `robot->speech(response)` の直前に `avatar.setSpeechText(response.c_str())` を追加
- TTS 再生完了後 `avatar.setSpeechText("")` でクリア
- Realtime モード (`RealtimeLLMBase.cpp`) は既に同パターン実装済みのため変更不要

## Design notes

- Battery / SubWindow は既に「Phase B: 回転外」描画されているため、Balloon を同じ層に置くのが最小変更・コード一貫性◎
- Balloon クラス内部の座標系 (cx=240, cy=220) は変更しない。位置調整は将来 Balloon.h の定数で対応可能
- 既存の `setSpeechText("考え中…")` / `setSpeechText("エラーです")` などの状態表示は維持
- 録音開始時のクリアは `AiStackChanMod.cpp` に既存実装あり

## Verification

- `pio run -e m5stack-cores3` でビルド完走
- 実機（M5StackChan キット + CoreS3）で：
  - アバターが回転している間も吹き出しが画面の同じ位置に留まる
  - 額タッチ → 発話 → AI応答が吹き出しに表示される
  - 次の録音開始時に吹き出しがクリアされる
- Realtime API モード（`m5stack-cores3-realtime`）でも既存挙動が維持されることを確認

## Rollback

- 各コミット単位で `git revert` 可能
- Face.cpp は L124 と L145-152 周辺、ChatGPT.cpp は応答処理 (response 取得後) の2行程度の局所修正

## Out of scope

- 長文応答のスクロール表示・自動折返し（M5公式の LV_LABEL_LONG_MODE_SCROLL_CIRCULAR 相当）
- 吹き出しの絶対座標 (cx=240, cy=220) の微調整
