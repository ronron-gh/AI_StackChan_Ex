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


// for async TTS
//

String LLMBase::getOutputText()
{
    String text = "";
    if(outputTextQueue.size() != 0){
        text = outputTextQueue[0];
        outputTextQueue.pop_front();
    }
    return text;
}

int LLMBase::getOutputTextQueueSize()
{
    return outputTextQueue.size();
}

// 区切り文字の有無を確認
// 戻り値：区切り文字あり(true)、なし(false)
int LLMBase::search_delimiter(String& text)
{
  // 区切り文字を検出
  int idx = text.indexOf("。");
  if(idx < 0){
    idx = text.indexOf("？");
  }
  if(idx < 0){
    idx = text.indexOf("！");
  }

  return idx;
}
