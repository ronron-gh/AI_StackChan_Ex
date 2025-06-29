#ifndef _REALTIME_CHAT_GPT_H
#define _REALTIME_CHAT_GPT_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "ChatGPT.h"
//#include "MCPClient.h"
#include <WebSocketsClient.h>

extern String InitBuffer;
extern String json_ChatString;

class RealtimeChatGPT: public ChatGPT{
public:

    // for record
    //
    int16_t* rtRecBuf;
    int rtRecSamplerate;
    int rtRecLength;
    bool realtime_recording;

    // for play
    //
    uint8_t* audioBuf[2];    // Base64をデコードして得た音声データを格納するバッファ。再生直後に更新すると音が切れたのでダブルバッファとした
    int nextBufIdx;          // 次回データを格納するダブルバッファの面（0 or 1）


    RealtimeChatGPT(llm_param_t param);

    void webSocketLoop();
    int getAudioLevel();


    int base64_decode(const char* input, int size, char* output);
    void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
    void streamAudioDelta(String& delta);

};


#endif  //_REALTIME_CHAT_GPT_H