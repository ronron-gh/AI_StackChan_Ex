#ifndef _PHRASES_H
#define _PHRASES_H

// ずんだもん風の応答バリエーション。各シチュエーションでランダムに返す。
namespace phrases {

const char* listening();   // 初回受付（額タッチ／ウェイクワード直後）
const char* followup();    // 連続会話モード（応答完了後）
const char* thinking();    // ChatGPT 応答待ち
const char* not_heard();   // 音声認識失敗
const char* head_pet();    // 頭撫で
const char* error();       // エラー発生
const char* unknown();     // わからない応答
const char* greeting();    // 起動時挨拶

}  // namespace phrases

#endif  // _PHRASES_H
