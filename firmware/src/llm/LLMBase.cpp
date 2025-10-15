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


LLMBase::LLMBase(llm_param_t param, int _promptMaxSize)
  : param(param), promptMaxSize(_promptMaxSize), isOfflineService(false) 
{
  chat_doc = SpiRamJsonDocument(promptMaxSize);

}
