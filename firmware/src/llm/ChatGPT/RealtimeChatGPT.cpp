#include <Arduino.h>
#include <M5Unified.h>
//#include <SPIFFS.h>
#include <Avatar.h>
//#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCACertificate.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "RealtimeChatGPT.h"
//#include "../ChatHistory.h"
//#include "FunctionCall.h"
//#include "MCPClient.h"
#include "Robot.h"

#include <base64.h>
#include "libb64/cdecode.h"
#include <WebSocketsClient.h>

using namespace m5avatar;
extern Avatar avatar;



WebSocketsClient webSocket;

SpiRamJsonDocument msgDoc(0);

char session_update[] =
      "{"
        "\"type\": \"session.update\","
        "\"session\": {"
          //"\"turn_detection\": null,"
          "\"input_audio_transcription\": {"
            "\"model\": \"whisper-1\","
            "\"language\": \"ja\""
          "},"
          //"\"voice\":\"alloy\","
          "\"voice\":\"sage\","
          //"\"output_audio_format\":\"g711_ulaw\","    //pcm16よりだいぶ軽いので使いたかったが、再生できるライブラリがなかった
          "\"instructions\": \"You are an AI robot named Stack-chan. Please speak in Japanese.\""
        "}"
      "}";

char input_audio_append[] =
        "{"
          "\"type\": \"input_audio_buffer.append\","
          "\"audio\": \"REPLACE_TO_AUDIO_BASE64\""
        "}";



// WebSocketのコールバック関数としてクラスメソッドを渡せないので、コールバック関数を
// 通常の関数にして静的変数を経由してクラスのthisポインタを渡す。
RealtimeChatGPT* p_this;
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    String msgType, delta;
    DeserializationError error;

	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			Serial.printf("[WSc] Connected to url: %s\n", payload);

			// send message to server when Connected
			webSocket.sendTXT(session_update);
			break;
		case WStype_TEXT:
			//Serial.printf("[WSc] get text: %s\n", payload);
			Serial.printf("[WSc] text size: %d\n", strlen((char*)payload));

            error = deserializeJson(msgDoc, payload);
            if (error) {
                Serial.printf("WebSocket Event: JSON deserialization error %d\n", error.code());
            }

            msgType = msgDoc["type"].as<String>();
            Serial.printf("[WSc] text type: %s\n", msgType.c_str());

            if(msgType.equals("session.updated")){
                avatar.setSpeechText("Please touch");
            }
            else if(msgType.equals("input_audio_buffer.speech_started")){
                p_this->resetRealtimeRecordStartTime();
            }
            else if(msgType.equals("input_audio_buffer.committed")){
                Serial.printf("[WSc] input audio committed\n");
                p_this->stopRealtimeRecord();
                M5.Mic.end();
                M5.Speaker.begin();
            }
            else if(msgType.equals("response.audio_transcript.delta")){
                delta = msgDoc["delta"].as<String>();
                Serial.printf("[WSc] delta: %s\n", delta.c_str());
            }
            else if(msgType.equals("response.audio.delta")){
                delta = msgDoc["delta"].as<String>();
                p_this->streamAudioDelta(delta);
            }
            else if(msgType.equals("response.done")){
                Serial.printf("[WSc] response.done\n");
                p_this->startRealtimeRecord();
                while (M5.Speaker.isPlaying()) { /*vTaskDelay(1);*/ }
                M5.Speaker.end();
                M5.Mic.begin();
                for(int i=0; i<2; i++){
                    memset(p_this->audioBuf[i], 0, 100 * 1024);
                }
            }
            else if(msgType.equals("rate_limits.updated")){
                //Serial.printf("[WSc] payload: %s\n", payload);
            }

			break;
		case WStype_BIN:
			Serial.printf("[WSc] get binary length: %u\n", length);
			p_this->hexdump(payload, length);
			break;
		case WStype_ERROR:
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
 			Serial.printf("[WSc] payload: %s\n", payload);		
            break;
        default:
			Serial.printf("[WSc] Unknown event\n");
            //Serial.printf("[WSc] payload: %s\n", payload);
            break;
	}

}


RealtimeChatGPT::RealtimeChatGPT(llm_param_t param) : 
    ChatGPT(param),
    rtRecSamplerate(16000),
    rtRecLength(2000),          //0.125s 
    realtime_recording(false),
    startTime(0),
    nextBufIdx(0)
{

  // リアルタイム録音用のバッファを初期化
  rtRecBuf = (int16_t*)heap_caps_malloc(rtRecLength * sizeof(*rtRecBuf), MALLOC_CAP_8BIT);

  // ストリーミング音声再生用のダブルバッファを初期化
  for(int i=0; i<2; i++){
    audioBuf[i] = (uint8_t*)malloc(100 * 1024);
    memset(audioBuf[i], 0, 100 * 1024);
  }

  msgDoc = SpiRamJsonDocument(1024*150);


  // WebSocket connect
  //
  avatar.setSpeechText("Connecting...");
  webSocket.beginSslWithCA("api.openai.com", 443, "/v1/realtime?model=gpt-4o-realtime-preview-2025-06-03", root_ca_openai);
  // event handler
  p_this = this;    //コールバック関数に静的変数経由でthisポインタを渡す
  webSocket.onEvent(webSocketEvent);
  String auth = "Bearer " + param.api_key;
  webSocket.setAuthorization(auth.c_str());
  webSocket.setExtraHeaders("OpenAI-Beta: realtime=v1");
  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

}

void RealtimeChatGPT::webSocketProcess()
{
    webSocket.loop();

    if(realtime_recording){
        //M5.Mic.begin();
        if(!M5.Mic.record(rtRecBuf, rtRecLength, rtRecSamplerate)){
            Serial.println("Mic.record() returns false");
            delay(1000);
        }
        //M5.Mic.end();
        String audio_base64;
        audio_base64 = base64::encode((u8*)rtRecBuf, rtRecLength * sizeof(int16_t));

        String json(input_audio_append);
        json.replace("REPLACE_TO_AUDIO_BASE64", audio_base64);
        webSocket.sendTXT(json);

        checkRealtimeRecordTimeout();
    }
}

int RealtimeChatGPT::getAudioLevel()
{
    return abs(*audioBuf[nextBufIdx ^ 1]) * 50;
}

void RealtimeChatGPT::startRealtimeRecord()
{
    if(!realtime_recording){
        Serial.println("Start realtime recording");
        realtime_recording = true;
        startTime = xTaskGetTickCount();
        avatar.setSpeechText("Listening...");
    }
}

void RealtimeChatGPT::stopRealtimeRecord()
{
    if(realtime_recording){
        Serial.println("Stop realtime recording");
        realtime_recording = false;
        startTime = 0;
        avatar.setSpeechText("");
    }
}

void RealtimeChatGPT::resetRealtimeRecordStartTime()
{
    startTime = xTaskGetTickCount();
}

void RealtimeChatGPT::checkRealtimeRecordTimeout()
{
    portTickType elapsedTime;
    elapsedTime = (xTaskGetTickCount() - startTime) * portTICK_RATE_MS;
    if(elapsedTime > REALTIME_RECORD_TIMEOUT){
        Serial.println("Realtime recording timeout");
        stopRealtimeRecord();
        avatar.setSpeechText("Please touch");
    }
}

int RealtimeChatGPT::base64_decode(const char* input, int size, char* output)
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


void RealtimeChatGPT::hexdump(const void *mem, uint32_t len, uint8_t cols) {
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


void RealtimeChatGPT::streamAudioDelta(String& delta)
{
  int base64Size = delta.length();
  Serial.printf("audio base64 size: %d byte\n", base64Size);
  //char* buf = nullptr;                // デコード後のPCM16を格納するバッファ
  //buf = (char*)malloc(base64Size);    // デコード結果はBASE64データのサイズよりも小さくなる
  //if(buf == nullptr){
  //  Serial.println("base64_decode: malloc failed.");
  //}
  uint8_t* buf = audioBuf[nextBufIdx];
  int len = base64_decode(delta.c_str(), base64Size, (char*)buf);
  Serial.printf("audio pcm16 size: %d byte\n", len);
  
  while (M5.Speaker.isPlaying()) { /*vTaskDelay(1);*/ }
  M5.Speaker.playRaw((int16_t*)buf, len/2, 24000, false);
  nextBufIdx ^= 1;  //ダブルバッファを切り替え

}

