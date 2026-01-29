#if defined(REALTIME_API)

#ifndef _GEMINI_LIVE_H
#define _GEMINI_LIVE_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../RealtimeLLMBase.h"
#include <WebSocketsClient.h>


class GeminiLive: public RealtimeLLMBase{
public:
    GeminiLive(llm_param_t param);

    virtual void chat(String text, const char *base64_buf = NULL) {};   //dummy
    virtual String& buildInputAudioJson(String& jsonBuf, String& base64);
};


#endif  //_GEMINI_LIVE_H

#endif  //REALTIME_API