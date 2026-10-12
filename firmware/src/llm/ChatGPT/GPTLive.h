#if defined(REALTIME_API)

#ifndef _GPT_LIVE_H
#define _GPT_LIVE_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../RealtimeLLMBase.h"
#include "ChatGPT.h"
#include <WebSocketsClient.h>


class GPTLive: public RealtimeLLMBase{
public:   //本当はprivateにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした
    MCPClient* mcpClient[LLM_N_MCP_SERVERS_MAX];
    FunctionCall* fnCall;
    
    String role;
    String userInfo;
    String systemRole;

public:
    GPTLive(llm_param_t param);

    virtual void chat(String text, const char *base64_buf = NULL) {};   //dummy
    virtual String& buildInputAudioJson(String& jsonBuf, String& base64);
    virtual void load_role();
};


#endif  //_GPT_LIVE_H

#endif  //REALTIME_API