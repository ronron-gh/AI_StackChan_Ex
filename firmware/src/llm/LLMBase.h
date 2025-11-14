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
public:   //本当はprivateにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした

  llm_param_t param;
  String InitBuffer;
  int promptMaxSize;

public:
  bool isOfflineService;

  LLMBase(llm_param_t param, int _promptMaxSize);
  virtual void chat(String text, const char *base64_buf = NULL) = 0;
  virtual bool init_chat_doc(const char *data) {};
  virtual bool save_role() {};
  virtual void load_role() {};

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