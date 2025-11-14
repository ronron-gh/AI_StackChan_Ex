#if defined(REALTIME_API)

#ifndef _REALTIME_CHAT_GPT_H
#define _REALTIME_CHAT_GPT_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "ChatGPT.h"
#include <WebSocketsClient.h>

//#define REALTIME_API_RECORD_TEST

#define RT_REC_LENGTH       (2000)      //0.125s 
#define RT_REC_SAMPLE_RATE  (16000)

#ifdef REALTIME_API_RECORD_TEST
#define REALTIME_RECORD_TIMEOUT     (4 * 1000)      //ms  ※録音テスト再生用バッファのサイズに合わせる
#else
#define REALTIME_RECORD_TIMEOUT     (30 * 1000)      //ms
#endif

extern String InitBuffer;
extern String json_ChatString;

class RealtimeChatGPT: public ChatGPT{
//private:
public:   //本当はprivateにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした

    // for record
    //
    //int16_t* rtRecBuf;
    int rtRecSamplerate;
    int rtRecLength;
    bool realtime_recording;
    bool response_done;
    portTickType startTime;

#ifdef REALTIME_API_RECORD_TEST
    int16_t* recTestBuf;
    int recTestLenMax;
    int recTestLenCnt;
#endif

    // for play
    //
    uint8_t* audioBuf[2];    // Base64をデコードして得た音声データを格納するバッファ。再生直後に更新すると音が切れたのでダブルバッファとした
    int nextBufIdx;          // 次回データを格納するダブルバッファの面（0 or 1）

public:
    RealtimeChatGPT(llm_param_t param);

    void webSocketProcess();
    int getAudioLevel();
    void startRealtimeRecord();
    void stopRealtimeRecord();
    void resetRealtimeRecordStartTime();
    portTickType checkRealtimeRecordTimeout();

    int base64_decode(const char* input, int size, char* output);
    void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
    void streamAudioDelta(String& delta);

    // for TTS
    //
    String outputText;

};


#endif  //_REALTIME_CHAT_GPT_H

#endif  //REALTIME_API