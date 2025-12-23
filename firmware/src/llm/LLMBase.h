#ifndef _LLM_BASE_H
#define _LLM_BASE_H

#include <Arduino.h>
#include "SpiRamJsonDocument.h"
#include "ChatHistory.h"
#include "StackchanExConfig.h"

extern SpiRamJsonDocument chat_doc;
extern ChatHistory chatHistory;

struct llm_param_t
{
  String api_key;
  llm_s llm_conf;
};


class LLMBase{
//protected:
public:   //本当はprotectedにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした
  llm_param_t param;
  String InitBuffer;
  int promptMaxSize;

protected:
  bool _enableMemory;
  virtual bool save_chat_doc_to_spiffs() { return false; };        //TODO: LLMBaseで実装してもよいかも

public:
  bool isOfflineService;

  LLMBase(llm_param_t param, int _promptMaxSize);
  virtual void chat(String text, const char *base64_buf = NULL) = 0;
  virtual bool init_chat_doc(const char *data) { return false; };  //TODO: LLMBaseで実装してもよいかも
  virtual bool save_role(String role) { return false; };
  virtual bool save_userInfo(String userInfo) { return false; };
  virtual void load_role() {};

  String get_InitBuffer() { return InitBuffer; };
  SpiRamJsonDocument& get_chat_doc() { return chat_doc; };

  bool enableMemory() { return _enableMemory; };
  void enableMemory(bool isEnable) { _enableMemory = isEnable; };

  // for async TTS
  //
  std::deque<String> outputTextQueue;
  bool speaking;
  String getOutputText();
  int getOutputTextQueueSize();
  void setSpeaking(bool _speaking){ speaking = _speaking; };
  int search_delimiter(String& text);
};


#endif //_LLM_BASE_H