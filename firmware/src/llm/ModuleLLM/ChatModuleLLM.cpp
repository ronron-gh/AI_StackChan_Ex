#if defined(USE_LLM_MODULE)

#include <Arduino.h>
#include <M5Unified.h>
//#include <SPIFFS.h>
#include <Avatar.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "driver/ModuleLLM.h"
#include "ChatModuleLLM.h"
#include "../ChatHistory.h"
//#include "FunctionCall.h"
#include "Robot.h"

using namespace m5avatar;
extern Avatar avatar;

static ChatModuleLLM* p_this;   //コールバック関数に静的変数経由でthisポインタを渡す

ChatModuleLLM::ChatModuleLLM(llm_param_t param) : LLMBase(param, 0) {
  p_this = this;       //コールバック関数に静的変数経由でthisポインタを渡す
  isOfflineService = true;  //オフラインで使用可能とする
  load_role();
}

bool ChatModuleLLM::save_role(){
  //※現状、ModuleLLM用のロールはSPIFFSへの保存は非対応
  
#if 0
  InitBuffer="";
  serializeJson(chat_doc, InitBuffer);
  Serial.println("InitBuffer = " + InitBuffer);

  // SPIFFSをマウントする
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  // JSONファイルを作成または開く
  File file = SPIFFS.open("/data.json", "w");
  if(!file){
    Serial.println("Failed to open file for writing");
    return false;
  }

  // JSONデータをシリアル化して書き込む
  serializeJson(chat_doc, file);
  file.close();
#endif
  return true;
}

void ChatModuleLLM::load_role(){
  //※現状、ModuleLLM用のロールはSPIFFSへの保存は非対応

#if 0   //inferenceAndWaitResult()にJSONを渡してよいのだろうか？
  //String promptBase = "{\"messages\":[{\"role\": \"system\", \"content\": \"あなたはスタックチャンという名前のアシスタントロボットです.\"}]}";
  String promptBase = "{\"messages\":[{\"role\": \"system\", \"content\": \"\"}]}";

  //Serial.println(promptBase);
  init_chat_doc(promptBase.c_str());
  serializeJson(chat_doc, InitBuffer);      //InitBufferを初期化
  String json_str; 
  serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
  Serial.println(json_str);
#endif
}


void ChatModuleLLM::chat(String text, const char *base64_buf) {
  static String response = "";

#if 0   //inferenceAndWaitResult()にJSONを渡してよいのだろうか？
  init_chat_doc(InitBuffer.c_str());

#if 0 //履歴はqwen2.5-0.5bでは厳しそう
  // 質問をチャット履歴に追加
  chatHistory.push_back(String("user"), String(""), text);

  for (int i = 0; i < chatHistory.get_size(); i++)
  {
    JsonArray messages = chat_doc["messages"];
    JsonObject systemMessage1 = messages.createNestedObject();
    systemMessage1["role"] = chatHistory.get_role(i);
    systemMessage1["content"] = chatHistory.get_content(i);
  }
#else
  JsonArray messages = chat_doc["messages"];
  JsonObject systemMessage1 = messages.createNestedObject();
  systemMessage1["role"] = "user";
  systemMessage1["content"] = text;

  String json_string;
  serializeJson(chat_doc, json_string);

  Serial.println("====================");
  Serial.println(json_string);
  Serial.println("====================");
#endif

  /* Push question to LLM module and wait inference result */
  module_llm.llm.inferenceAndWaitResult(llm_work_id, json_string.c_str(), [](String& result) {
      /* Show result on screen */
      Serial.printf("inference:%s\n", result.c_str());
      response += result;

  });
#endif

  /* Push question to LLM module and wait inference result */
  module_llm.llm.inferenceAndWaitResult(llm_work_id, text.c_str(), [](String& result) {
    /* Show result on screen */
    Serial.printf("inference:%s\n", result.c_str());

    // TTSがModuleLLMではなくAquesTalkの場合はテキストを再生キューに入力する
    if(!robot->module_llm_param.enableTTS){
      response += result;
      // 区切り文字を検出したらテキストをキューに追加
      int idx = p_this->search_delimiter(response);
      if(idx > 0){
        String inputText = response.substring(0, idx);
        Serial.printf("Push text: %s\n", inputText.c_str());
        p_this->outputTextQueue.push_back(inputText);
        response = response.substring(idx + strlen("。"), response.length());
      }
    }
  });

  //chatHistory.push_back(String("assistant"), String(""), response);   // 返答をチャット履歴に追加

}

#endif  //USE_LLM_MODULE