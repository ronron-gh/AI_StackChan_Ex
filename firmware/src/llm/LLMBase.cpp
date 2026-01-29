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

#define SYSTEM_PROMPT_PATH  "/data.json"
const String SYSTEM_PROMPT_FORMAT = 
"{"
  "\"version\": 1,"
  "\"messages\": [{\"role\": \"system\", \"content\": \"\"},"     // ユーザーが設定するロール
                  "{\"role\": \"system\", \"content\": \"\"},"    // システム用のロール
                  "{\"role\": \"system\", \"content\": \"User Info: \"}]"  // 長期記憶の要約
"}";

// ユーザーが設定するロールのデフォルト設定用
const String defaultRole = "You are an AI robot named Stack-chan. Please speak in Japanese.";
// システム用のロール（Function Callingの利用方針など）
const String systemRole_memory = "If the conversation includes user attributes (such as hobbies or interests) or memorable episodes, summarize them and use the update_memory tool to update the User Info in the system prompt. The summary should also inherit the contents of the old User Info as much as possible.";
const String systemRole_noMemory = "Memory function disabled. Do not use update_memory tool.";



LLMBase::LLMBase(llm_param_t param, int _promptMaxSize)
  : param(param),
    promptMaxSize(_promptMaxSize),
    isOfflineService(false),
    speaking(false),
    _enableMemory(false),
    chat_doc(0),
    systemPrompt(0)
{
  chat_doc = SpiRamJsonDocument(promptMaxSize);
  systemPrompt = SpiRamJsonDocument(2048);
}

//--------------------------
// for async TTS
//--------------------------

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

//--------------------------
// for system prompt
//--------------------------

bool LLMBase::save_system_prompt_to_spiffs()
{
  // SPIFFSをマウントする
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  // JSONファイルを作成または開く
  File file = SPIFFS.open(SYSTEM_PROMPT_PATH, "w");
  if(!file){
    Serial.println("Failed to open file for writing");
    return false;
  }

  // JSONデータをシリアル化して書き込む
  serializeJson(systemPrompt, file);
  file.close();
  return true;
}

bool LLMBase::load_system_prompt_from_spiffs()
{
  bool result = true;
  int promptVersion = 0;

  if(SPIFFS.begin(true)){
    File file = SPIFFS.open(SYSTEM_PROMPT_PATH, "r");
    //Serial.printf("SPIFFS file size: %d\n", file.size());
    if(file.size() > 0){
      DeserializationError error = deserializeJson(systemPrompt, file);
      if(error){
        Serial.println("Failed to deserialize JSON. Init doc by default.");
        result = false;
      }
      else{
        promptVersion = systemPrompt["version"].as<int>();
        Serial.printf("System Prompt Version: %d\n", promptVersion);
        result = true;
      }
    } else {
      Serial.println("Failed to open file. Initialize by default.");
      result = false;
    }
  } else {
    Serial.println("An Error has occurred while mounting SPIFFS");
    result = false;
  }

  if(!result || (promptVersion != 1)){
    Serial.println("Initialize SPIFFS System Prompt by default.");
    DeserializationError error = deserializeJson(systemPrompt, SYSTEM_PROMPT_FORMAT);
    if (error) {
      Serial.println("DeserializationError");
    }else{
      systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"] = defaultRole;
      result = true;
    }
  }

  String json_str;
  serializeJsonPretty(systemPrompt, json_str);  // 文字列をシリアルポートに出力する
  Serial.println("System prompt:");
  Serial.println(json_str);
  
  return result;
}


bool LLMBase::save_userRole(String role){
  Serial.println("Save User Role to SPIFFS.");

  if (role != "") {
    systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"] = role;
  } else {
    Serial.println("Input role is empty. Initialize by default.");
    systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"] = defaultRole;
  }

  // 更新したプロンプトをSPIFFSに保存
  if(!save_system_prompt_to_spiffs()){
    return false;
  }

  String json_str;
  serializeJsonPretty(systemPrompt, json_str);  // 文字列をシリアルポートに出力する
  Serial.println("New system prompt:");
  Serial.println(json_str);

  return true;
}

bool LLMBase::save_userInfo(String userInfo){
  Serial.println("Save role to SPIFFS.");

  systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"] = String("User Info: ") + userInfo;

  // 更新したプロンプトをSPIFFSに保存
  if(!save_system_prompt_to_spiffs()){
    return false;
  }

  String json_str;
  serializeJsonPretty(systemPrompt, json_str);  // 文字列をシリアルポートに出力する
  Serial.println("New system prompt:");
  Serial.println(json_str);

  return true;
}


String LLMBase::get_userRole() {
  return systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"];
}

String LLMBase::get_userInfo() {
  return systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"];
}

bool LLMBase::clear_userInfo() {
  return save_userInfo("");
}