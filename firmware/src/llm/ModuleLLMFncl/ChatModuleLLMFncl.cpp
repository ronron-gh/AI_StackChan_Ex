#if defined(USE_LLM_MODULE)

#include <Arduino.h>
#include <M5Unified.h>
//#include <SPIFFS.h>
#include <Avatar.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "driver/ModuleLLM.h"
#include "ChatModuleLLMFncl.h"
#include "../ChatHistory.h"
#include "../ChatGPT/FunctionCall.h"
#include "Robot.h"

using namespace m5avatar;
extern Avatar avatar;

ChatModuleLLMFncl::ChatModuleLLMFncl(llm_param_t param) : LLMBase(param, 0) {
  isOfflineService = true;  //オフラインで使用可能とする
  load_role();
}

bool ChatModuleLLMFncl::save_role(){
  //現状、ModuleLLM用のロールはSPIFFSへの保存は非対応
  return true;
}

void ChatModuleLLMFncl::load_role(){
  //現状、ModuleLLM用のロールはSPIFFSへの保存は非対応
  //Function Calling用モデルのSmolLM-360M-Instruct-fnclのロールは
  //ModuleLLM側で起動するtokenizer.pyで定義されている
}


void ChatModuleLLMFncl::chat(String text, const char *base64_buf) {
  static String response = "";

  Serial.printf("inference input:%s\n", text.c_str());

  /* Push question to LLM module and wait inference result */
  module_llm.llm.inferenceAndWaitResult(llm_work_id, text.c_str(), [](String& result) {
      /* Show result on screen */
      Serial.printf("inference:%s\n", result.c_str());
      response += result;

  });

  Serial.println(response);
  int searchIdx = response.indexOf("<toolcall>");
  //Serial.printf("Fncl JSON search result:%d\n", searchIdx);
  if(searchIdx >= 0){
    DynamicJsonDocument doc(2000);
    DeserializationError error = deserializeJson(doc, response.c_str() + strlen("<toolcall>"));
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
    }
    else{
      //String json_string;
      //serializeJson(doc, json_string);
      //Serial.println(json_string);
      response = executeFunction(doc);
    }
    
  }
  Serial.printf("%s\n", response.c_str());
  robot->speech(response);
  response = "";
}


String ChatModuleLLMFncl::executeFunction(DynamicJsonDocument doc){

  String response = "";
  const char* name = doc["name"];
  Serial.println(name);

  if(strcmp(name, "set_alarm") == 0){
    const int min = doc["arguments"]["min"];
    Serial.printf("min:%d\n",min);
    timer(min * 60, "alarm");
  }
  else if(strcmp(name, "stop_alarm") == 0){
    timer_change(0);
  }

  return response;
}

#endif  //USE_LLM_MODULE