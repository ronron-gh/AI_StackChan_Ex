#ifndef _LLM_BASE_H
#define _LLM_BASE_H

#include <Arduino.h>
#include "SpiRamJsonDocument.h"
#include "ChatHistory.h"

extern SpiRamJsonDocument chat_doc;
extern ChatHistory chatHistory;

struct llm_param_t
{
  String api_key;
  llm_s llm_conf;
};


class LLMBase{
protected:
  llm_param_t param;
  String InitBuffer;

public:
  bool isOfflineService;

  LLMBase(llm_param_t param);
  virtual void chat(String text, const char *base64_buf = NULL) = 0;
  bool init_chat_doc(const char *data);
  virtual bool save_role() {};
  virtual void load_role() {};
};



#endif //_LLM_BASE_H