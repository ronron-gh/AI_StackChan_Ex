# Phrase variations and intro fade-out

## Purpose

1. ハードコードされた定型セリフをずんだもん風にバリエーション化し、
   会話に親しみと自然さを出す。
2. 起動時の「AI Stack-chan」吹き出しが出っ放しになる挙動を、
   一定時間後に自動消去するよう修正。

## Scope

- `firmware/src/share/Phrases.h` 新規作成
- `firmware/src/share/Phrases.cpp` 新規作成
- `firmware/src/mod/AiStackChan/AiStackChanMod.cpp` 文言を関数呼び出しに置換 + 起動時吹き出しの自動消去
- `firmware/src/llm/ChatGPT/ChatGPT.cpp` 状態文言の置換

## Main changes

### Phrases.h / .cpp

各シチュエーションごとに 3〜5 個の候補からランダム選択する関数群を提供：

| 関数 | 状況 | 候補例 |
|------|------|--------|
| `phrases::listening()` | 初回受付 | "なになに？" "聞いてるよー" "御用なのだー？" |
| `phrases::followup()` | 連続会話 | "他にあるー？" "まだあるー？" "なになに？" |
| `phrases::thinking()` | 応答待ち | "うーん…" "ちょっと待ってねー" "考え中なのだー" |
| `phrases::not_heard()` | 認識失敗 | "もう一回言ってー" "聞こえなかったー…" |
| `phrases::head_pet()` | 頭撫で | "えへへ" "うふふー" "気持ちいいー" |
| `phrases::error()` | エラー | "うー、エラーなのだー" "ごめんなさいー" |
| `phrases::unknown()` | わからない | "わかんないー" "むずかしいー" |
| `phrases::greeting()` | 起動時挨拶 | "やー！スタックチャンなのだー" "こんにちはー！" |

### 起動時吹き出し自動消去

`AiStackChanMod::init()` で挨拶を表示後、`update()` の中で経過時間を見て一定時間後にクリア。

```cpp
// in init()
avatar.setSpeechText(phrases::greeting());
m_intro_shown_ms = millis();
m_intro_active = true;

// in update() or main loop
if (m_intro_active && millis() - m_intro_shown_ms > INTRO_DURATION_MS) {
  avatar.setSpeechText("");
  m_intro_active = false;
}
```

`INTRO_DURATION_MS` は 5000 (5秒) を初期値。

## Design notes

- `random(0, N)` で選択（Arduino random）
- ランダムシードは別途気にしない（同じ文言が連続しても許容）
- ChatGPT 応答後のクリア処理はそのまま維持（応答全文表示を上書きしない）
- 既存のメンバ変数の命名規則（`m_*`）に合わせる
- 頭撫での "えへへ" も main.cpp 側から phrases::head_pet() を呼ぶ

## Verification

- ビルド成功
- 額タッチを何度か繰り返す → 毎回違うセリフが出る
- 起動 5 秒後に「AI Stack-chan」相当の挨拶が自動消去される
- 既存の Realtime API モード、Function Calling の応答は変わらない

## Rollback

- Phrases.h/.cpp を削除し、置換箇所を元のリテラルに戻す
- `git revert` でも復元可能

## Out of scope

- TTS 発話にも候補のバリエーションを適用（既存挙動を変えるリスク）
- yaml で候補を設定可能にする
- 時間帯・気分に応じた選択ロジック
