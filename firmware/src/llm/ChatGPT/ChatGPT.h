#ifndef _CHAT_GPT_H
#define _CHAT_GPT_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../LLMBase.h"
#include "MCPClient.h"
#include "FunctionCall.h"

#define CHATGPT_PROMPT_MAX_SIZE   (1024*50)

extern String InitBuffer;
extern const String json_ChatString;

class ChatGPT: public LLMBase{
//protected:
public:  //本当はprotectedにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした
    MCPClient* mcpClient[LLM_N_MCP_SERVERS_MAX];
    FunctionCall* fnCall;

protected:

public:
    ChatGPT(llm_param_t param, int _promptMaxSize = CHATGPT_PROMPT_MAX_SIZE);
    virtual void chat(String text, const char *base64_buf = NULL);
    String execChatGpt(String json_string, String& calledFunc);
    String https_post_json(const char* url, const char* json_string, const char* root_ca);
    
    virtual bool init_chat_doc(const char *data);
    virtual void load_role();
};


#endif  //_CHAT_GPT_H