#include <Arduino.h>
#include <M5Unified.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "ChatHistory.h"
#include "Robot.h"
#include "LLMBase.h"


// 保存する質問と回答の最大数
const int MAX_HISTORY = 20;

// 過去の質問と回答を保存するデータ構造
ChatHistory chatHistory(MAX_HISTORY);   // TODO: 本当はLLMBaseのメンバ変数にしたい

//DynamicJsonDocument chat_doc(1024*10);
SpiRamJsonDocument chat_doc(0);     // PSRAMから確保するように変更。サイズの確保はsetup()で実施（初期化後でないとPSRAMが使えないため）。
                                    // TODO: 本当はLLMBaseのメンバ変数にしたい


LLMBase::LLMBase(llm_param_t param):param{param}
{
  chat_doc = SpiRamJsonDocument(1024*50);

}

bool LLMBase::init_chat_doc(const char *data)
{
  DeserializationError error = deserializeJson(chat_doc, data);
  if (error) {
    Serial.println("DeserializationError");

    String json_str; //= JSON.stringify(chat_doc);
    serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
    Serial.println(json_str);

    return false;
  }
  String json_str; //= JSON.stringify(chat_doc);
  serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
//  Serial.println(json_str);
  return true;
}

