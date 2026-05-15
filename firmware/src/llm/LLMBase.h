#pragma once

#include <Stackchan_system_config.h>   // MUST BE FIRST - defines llm_param_t
#include <deque>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"

class LLMBase {
public:
  LLMBase(llm_param_t param, int promptMaxSize);
  virtual ~LLMBase() {}

  virtual void chat(String text, const char* base64_buf = NULL) = 0;
  virtual String listen() = 0;

  String getOutputText();
  int getOutputTextQueueSize();

  bool save_system_prompt_to_spiffs();
  bool load_system_prompt_from_spiffs();
  bool save_userRole(String role);
  bool save_userInfo(String userInfo);
  String get_userRole();
  String get_userInfo();
  bool clear_userInfo();

  int search_delimiter(String& text);

protected:
  llm_param_t param;
  int promptMaxSize;
  bool isOfflineService;
  bool speaking;
  bool _enableMemory;

  SpiRamJsonDocument chat_doc;
  SpiRamJsonDocument systemPrompt;

  std::deque<String> outputTextQueue;   // for async TTS
};