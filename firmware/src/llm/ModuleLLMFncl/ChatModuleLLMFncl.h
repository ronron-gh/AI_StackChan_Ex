#if defined(USE_LLM_MODULE)

#ifndef _CHAT_MODULE_LLM_FNCL_H
#define _CHAT_MODULE_LLM_FNCL_H

#include <Arduino.h>
#include <M5Unified.h>
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../LLMBase.h"
#include "../ChatGPT/FunctionCall.h"

extern String InitBuffer;


class ChatModuleLLMFncl: public LLMBase{
private:
    FunctionCall* fnCall;

public:
    ChatModuleLLMFncl(llm_param_t param);
    virtual void chat(String text, const char *base64_buf = NULL);
    String execChatGpt(String json_string, String* calledFunc);
    
    virtual bool save_role();
    virtual void load_role();

    //
    // Function Calling
    //
    String executeFunction(DynamicJsonDocument doc);
};


#endif  //_CHAT_MODULE_LLM_FNCL_H

#endif  //USE_LLM_MODULE