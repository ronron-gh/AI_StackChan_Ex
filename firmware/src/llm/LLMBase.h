#pragma once

#include <Stackchan_system_config.h>
#include <deque>
#include <Arduino.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "ChatGPT/MCPClient.h"

#define LLM_N_MCP_SERVERS_MAX   10

struct llm_conf_t {
    int type = 0;
    String model = "";
    int nMcpServers = 0;
    mcp_server_s mcpServer[LLM_N_MCP_SERVERS_MAX];
    bool enableMemory = false;
};

struct llm_param_t {
    String api_key;
    llm_conf_t llm_conf;
};

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

  void enableMemory(bool val) { _enableMemory = val; }
  bool enableMemory() { return _enableMemory; }

protected:
  llm_param_t param;
  int promptMaxSize;
  bool isOfflineService;
  bool speaking;
  bool _enableMemory;

  SpiRamJsonDocument chat_doc;
  SpiRamJsonDocument systemPrompt;

  std::deque<String> outputTextQueue;
};
