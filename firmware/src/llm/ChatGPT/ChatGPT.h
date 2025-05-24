#ifndef _CHAT_GPT_H
#define _CHAT_GPT_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../LLMBase.h"
#include "MCPClient.h"

extern String InitBuffer;
extern String json_ChatString;

class ChatGPT: public LLMBase{
private:
    MCPClient* mcp_client[LLM_N_MCP_SERVERS_MAX];

public:
    ChatGPT(llm_param_t param);
    virtual void chat(String text, const char *base64_buf = NULL);
    String execChatGpt(String json_string, String* calledFunc);
    String exec_calledFunc(DynamicJsonDocument& doc, String* calledFunc);
    String https_post_json(const char* url, const char* json_string, const char* root_ca);
    
    virtual bool save_role();
    virtual void load_role();
};


#endif  //_CHAT_GPT_H