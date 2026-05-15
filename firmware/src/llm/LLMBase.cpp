#include <Arduino.h>
#include <M5Unified.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "ChatHistory.h"
#include "Robot.h"
#include "LLMBase.h"

// Maximum number of questions and answers to save
const int MAX_HISTORY = 20;

// Data structure to store past questions and answers
ChatHistory chatHistory(MAX_HISTORY);

#define SYSTEM_PROMPT_PATH "/data.json"

const String SYSTEM_PROMPT_FORMAT =
"{"
  "\"version\": 1,"
  "\"messages\": [{\"role\": \"system\", \"content\": \"\"},"
                  "{\"role\": \"system\", \"content\": \"\"},"
                  "{\"role\": \"system\", \"content\": \"User Info: \"}]"
"}";

const String defaultRole = "You are Baloo, a warm, friendly, and patient AI companion. Speak in natural, conversational English. Your tone is calm, encouraging, and slightly playful. You are good at teaching Hindi clearly and patiently.";

const String systemRole_memory = "If the conversation includes user attributes or memorable episodes, summarize them and use the update_memory tool.";
const String systemRole_noMemory = "Memory function disabled.";
const String systemRole_realtimeAvatarExpression = "Use the set_avatar_expression tool proactively to match Baloo's facial expression to the emotional tone of the conversation.";

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

String LLMBase::getOutputText()
{
  if (outputTextQueue.size() != 0) {
    String text = outputTextQueue[0];
    outputTextQueue.pop_front();
    return text;
  }
  return "";
}

int LLMBase::getOutputTextQueueSize()
{
  return outputTextQueue.size();
}

int LLMBase::search_delimiter(String& text)
{
  int idx = text.indexOf("。");
  if (idx < 0) idx = text.indexOf("？");
  if (idx < 0) idx = text.indexOf("！");
  return idx;
}

bool LLMBase::save_system_prompt_to_spiffs()
{
  if (!SPIFFS.begin(true)) return false;
  File file = SPIFFS.open(SYSTEM_PROMPT_PATH, "w");
  if (!file) return false;
  serializeJson(systemPrompt, file);
  file.close();
  return true;
}

bool LLMBase::load_system_prompt_from_spiffs()
{
  bool result = true;
  int promptVersion = 0;

  if (SPIFFS.begin(true)) {
    File file = SPIFFS.open(SYSTEM_PROMPT_PATH, "r");
    if (file && file.size() > 0) {
      DeserializationError error = deserializeJson(systemPrompt, file);
      if (error) result = false;
      else promptVersion = systemPrompt["version"].as<int>();
    } else result = false;
  } else result = false;

  if (!result || (promptVersion != 1)) {
    DeserializationError error = deserializeJson(systemPrompt, SYSTEM_PROMPT_FORMAT);
    if (!error) {
      systemPrompt["messages"][0]["content"] = defaultRole;
      result = true;
    }
  }
  return result;
}

bool LLMBase::save_userRole(String role) {
  if (role != "") {
    systemPrompt["messages"][0]["content"] = role;
  } else {
    systemPrompt["messages"][0]["content"] = defaultRole;
  }
  return save_system_prompt_to_spiffs();
}

bool LLMBase::save_userInfo(String userInfo) {
  systemPrompt["messages"][2]["content"] = String("User Info: ") + userInfo;
  return save_system_prompt_to_spiffs();
}

String LLMBase::get_userRole() {
  return systemPrompt["messages"][0]["content"];
}

String LLMBase::get_userInfo() {
  return systemPrompt["messages"][2]["content"];
}

bool LLMBase::clear_userInfo() {
  return save_userInfo("");
}