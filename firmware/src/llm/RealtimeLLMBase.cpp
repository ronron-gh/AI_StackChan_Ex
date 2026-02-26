#if defined(REALTIME_API)

#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
//#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCAgoogleGemini.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "RealtimeLLMBase.h"
//#include "FunctionCall.h"
//#include "MCPClient.h"
#include "Robot.h"

#include <base64.h>
#include "libb64/cdecode.h"
#include <WebSocketsClient.h>

using namespace m5avatar;
extern Avatar avatar;

int16_t rtRecBuf[RT_REC_LENGTH];    // リアルタイム録音用メモリ
                                    // Core2だとヒープが不足するので静的な配列とした

RealtimeLLMBase::RealtimeLLMBase(llm_param_t param) : 
    LLMBase(param, 0),
    msgDoc(0),
    rtRecSamplerate(RT_REC_SAMPLE_RATE),
    rtRecLength(RT_REC_LENGTH),
    realtime_recording(false),
    response_done(false),
    startTime(0),
    nextBufIdx(0),
    outputText(String(""))
{
#ifdef REALTIME_API_RECORD_TEST
  // リアルタイム録音のチャンクデータを蓄積してテスト再生するためのバッファ（約4s）
  recTestLenMax = rtRecLength * 40;
  recTestLenCnt = 0;
  recTestBuf = (int16_t*)heap_caps_malloc(recTestLenMax * sizeof(*rtRecBuf), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

#ifndef REALTIME_API_WITH_TTS
  // ストリーミング音声再生用のダブルバッファを初期化
  for(int i=0; i<2; i++){
    audioBuf[i] = (uint8_t*)malloc(100 * 1024);
    memset(audioBuf[i], 0, 100 * 1024);
  }
#endif

}

void RealtimeLLMBase::webSocketProcess()
{
    webSocket.loop();

#ifdef REALTIME_API_WITH_TTS
    if(response_done && !speaking){
        startRealtimeRecord();
        response_done = false;
    }
#endif

    if(realtime_recording){
        //M5.Mic.begin();
        if(!M5.Mic.record(rtRecBuf, rtRecLength, rtRecSamplerate)){
            Serial.println("Mic.record() returns false");
            delay(1000);
        }
        //M5.Mic.end();
        String audio_base64;
        audio_base64 = base64::encode((u8*)rtRecBuf, rtRecLength * sizeof(int16_t));

#ifdef REALTIME_API_RECORD_TEST
        if((recTestLenCnt + rtRecLength) < recTestLenMax){
            memcpy((u8*)&recTestBuf[recTestLenCnt], (u8*)rtRecBuf, rtRecLength * sizeof(int16_t));
            recTestLenCnt += rtRecLength;
        }
#else
        String audioJsonBuf("");
        webSocket.sendTXT(buildInputAudioJson(audioJsonBuf, audio_base64));
#endif

        portTickType elapsedTime = checkRealtimeRecordTimeout();

#if 0   //Debug リスニング経過時間の表示
        static char speechTxt[64];
        sprintf(speechTxt, "Listening:%ds", int(elapsedTime / 1000));
        avatar.setSpeechText(speechTxt);
#else
        avatar.setSpeechText("Listening...");
#endif
    }
    else{
        if(speaking){
            //発話中もしくはテキスト生成中
            avatar.setSpeechText("");
            resetRealtimeRecordStartTime(); //長いテキストを発話中にタイムアウトしてしまうのを防ぐ
        }
        else{
            avatar.setSpeechText("Please touch");
        }
    }
}

int RealtimeLLMBase::getAudioLevel()
{
    return abs(*audioBuf[nextBufIdx ^ 1]) * 50;
}

void RealtimeLLMBase::startRealtimeRecord()
{
    if(!realtime_recording){
        Serial.println("Start realtime recording");
        realtime_recording = true;
        startTime = xTaskGetTickCount();
    }
}

void RealtimeLLMBase::stopRealtimeRecord()
{
    if(realtime_recording){
        Serial.println("Stop realtime recording");
        realtime_recording = false;
        startTime = 0;
    }
}

void RealtimeLLMBase::resetRealtimeRecordStartTime()
{
    startTime = xTaskGetTickCount();
}

portTickType RealtimeLLMBase::checkRealtimeRecordTimeout()
{
    portTickType elapsedTime;
    elapsedTime = (xTaskGetTickCount() - startTime) * portTICK_RATE_MS;
    if(elapsedTime > REALTIME_RECORD_TIMEOUT){
        Serial.println("Realtime recording timeout");
        stopRealtimeRecord();
#ifdef REALTIME_API_RECORD_TEST
        M5.Mic.end();
        if (M5.Speaker.begin())
        {
            M5.Speaker.playRaw(recTestBuf, recTestLenCnt, rtRecSamplerate);
            while (M5.Speaker.isPlaying()) { delay(10); }
            M5.Speaker.end();
            M5.Mic.begin();
        }
        recTestLenCnt = 0;
#endif
    }

    return elapsedTime;
}

int RealtimeLLMBase::base64_decode(const char* input, int size, char* output)
{
	/* keep track of our decoded position */
	char* c = output;
	/* store the number of bytes decoded by a single call */
	int cnt = 0;
	/* we need a decoder state */
	base64_decodestate s;
	
	/*---------- START DECODING ----------*/
	/* initialise the decoder state */
	base64_init_decodestate(&s);
	/* decode the input data */
	cnt = base64_decode_block(input, strlen(input), c, &s);
	c += cnt;
	/* note: there is no base64_decode_blockend! */
	/*---------- STOP DECODING  ----------*/
	
	/* we want to print the decoded data, so null-terminate it: */
	*c = 0;
	
	return cnt;
}


void RealtimeLLMBase::hexdump(const void *mem, uint32_t len, uint8_t cols) {
	const uint8_t* src = (const uint8_t*) mem;
	Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		Serial.printf("%02X ", *src);
		src++;
	}
	Serial.printf("\n");
}


void RealtimeLLMBase::streamAudioDelta(String& delta)
{
  int base64Size = delta.length();
  Serial.printf("audio base64 size: %d byte\n", base64Size);
  uint8_t* buf = audioBuf[nextBufIdx];
  int len = base64_decode(delta.c_str(), base64Size, (char*)buf);
  Serial.printf("audio pcm16 size: %d byte\n", len);
  
  while (M5.Speaker.isPlaying()) { /*vTaskDelay(1);*/ }
  M5.Speaker.playRaw((int16_t*)buf, len/2, 24000, false);
  nextBufIdx ^= 1;  //ダブルバッファを切り替え
}


#endif  //REALTIME_API