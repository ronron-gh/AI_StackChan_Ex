#ifndef _CHAT_GPT_H
#define _CHAT_GPT_H

#include <Arduino.h>
#include <M5Unified.h>
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../LLMBase.h"

extern String InitBuffer;


class ChatGPT: public LLMBase{
private:

public:
    ChatGPT(llm_param_t param);
    virtual void chat(String text, const char *base64_buf = NULL);
    void chat_audio(const char *audio_base64);
    String execChatGpt(String json_string, String* calledFunc);
    String https_post_json(const char* url, const char* json_string, const char* root_ca);
    
    virtual bool save_role();
    virtual void load_role();
};


#endif  //_CHAT_GPT_H