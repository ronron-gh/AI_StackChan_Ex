#include <Arduino.h>
#include <M5Unified.h>
#include <SPIFFS.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "ChatHistory.h"
#include "Robot.h"
#include "LLMBase.h"

// Maximum number of questions and answers to save
const int MAX_HISTORY = 20;

// Data structure to store past questions and answers
ChatHistory chatHistory(MAX_HISTORY); // TODO: Ideally make this a member variable of LLMBase

#define SYSTEM_PROMPT_PATH "/data.json"

const String SYSTEM_PROMPT_FORMAT =
"{"
  "\"version\": 1,"
  "\"messages\": [{\"role\": \"system\", \"content\": \"\"}," // User-defined role
                  "{\"role\": \"system\", \"content\": \"\"}," // System role
                  "{\"role\": \"system\", \"content\": \"User Info: \"}]" // Long-term memory summary
"}";

// Default user role (used if nothing is saved)
const String defaultRole = "You are Baloo, a warm, friendly, and patient AI companion. Speak in natural, conversational English. Your tone is calm, encouraging, and slightly playful. You are good at teaching Hindi clearly and patiently.";

// System role for memory / function calling
const String systemRole_memory = "If the conversation includes user attributes (such as hobbies or interests) or memorable episodes, summarize them and use the update_memory tool to update the User Info in the system prompt. The summary should also inherit the contents of the old User Info as much as possible.";

const String systemRole_noMemory = "Memory function disabled. Do not use update_memory tool.";

const String systemRole_realtimeAvatarExpression = "Use the set_avatar_expression tool proactively to match Baloo's facial expression to the emotional tone of the conversation. Call it when your emotion changes or when the user's emotion suggests a suitable reaction. Use neutral, happy, angry, sad, doubt, or sleepy.";

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

// Check for delimiter characters
// Return value: delimiter found (true), not found (false)
int LLMBase::search_delimiter(String& text)
{
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
// SD Card Prompt Loading (Stage 2)
//--------------------------
bool LLMBase::load_system_prompt_from_sd()
{
  // Common SD card mount paths used in M5Stack projects
  const char* sdPaths[] = {
    "/sd/baloo_prompt.txt",
    "/baloo_prompt.txt",
    "baloo_prompt.txt"
  };

  for (size_t i = 0; i < sizeof(sdPaths) / sizeof(sdPaths[0]); i++) {
    if (SD.exists(sdPaths[i])) {
      File file = SD.open(sdPaths[i], FILE_READ);
      if (file) {
        String content = file.readString();
        file.close();

        if (content.length() > 0) {
          // Trim whitespace and newlines
          content.trim();

          systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"] = content;

          Serial.printf("Loaded system prompt from SD card: %s\n", sdPaths[i]);
          Serial.println("SD prompt content preview: " + content.substring(0, 80) + "...");

          // Save it to SPIFFS so it persists and can be updated later
          save_system_prompt_to_spiffs();
          return true;
        }
      }
    }
  }

  return false; // No SD prompt file found
}

//--------------------------
// for system prompt
//--------------------------
bool LLMBase::save_system_prompt_to_spiffs()
{
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  File file = SPIFFS.open(SYSTEM_PROMPT_PATH, "w");
  if(!file){
    Serial.println("Failed to open file for writing");
    return false;
  }

  serializeJson(systemPrompt, file);
  file.close();
  return true;
}

bool LLMBase::load_system_prompt_from_spiffs()
{
  bool result = true;
  int promptVersion = 0;

  // === Stage 2: Try loading from SD card first ===
  if (load_system_prompt_from_sd()) {
    // SD card prompt was loaded and saved to SPIFFS
    String json_str;
    serializeJsonPretty(systemPrompt, json_str);
    Serial.println("System prompt (loaded from SD):");
    Serial.println(json_str);
    return true;
  }

  // === Normal SPIFFS flow (fallback) ===
  if(SPIFFS.begin(true)){
    File file = SPIFFS.open(SYSTEM_PROMPT_PATH, "r");

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
  serializeJsonPretty(systemPrompt, json_str);
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

  if(!save_system_prompt_to_spiffs()){
    return false;
  }

  String json_str;
  serializeJsonPretty(systemPrompt, json_str);
  Serial.println("New system prompt:");
  Serial.println(json_str);

  return true;
}

bool LLMBase::save_userInfo(String userInfo){
  Serial.println("Save role to SPIFFS.");
  systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"] = String("User Info: ") + userInfo;

  if(!save_system_prompt_to_spiffs()){
    return false;
  }

  String json_str;
  serializeJsonPretty(systemPrompt, json_str);
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
