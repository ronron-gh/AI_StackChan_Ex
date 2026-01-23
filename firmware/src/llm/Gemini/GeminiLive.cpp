#if defined(REALTIME_API)

#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
//#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCAgoogleGemini.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "GeminiLive.h"
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
int16_t rtRecBuf[RT_REC_LENGTH];    // リアルタイム録音用メモリ
                                    // Core2だとヒープが不足するので静的な配列とした

const char session_update[] =
#if 0
      "{"
        "\"type\": \"session.update\","
        "\"session\": {"
          "\"type\": \"realtime\","
          "\"model\": \"gpt-realtime\","
#ifdef REALTIME_API_WITH_TTS
          "\"output_modalities\": [\"text\"],"
#else
          "\"output_modalities\": [\"audio\"],"
#endif
          "\"audio\": {"
            "\"input\": {"
              "\"format\": {"
                "\"type\": \"audio/pcm\","
                "\"rate\": 24000"
              "},"
              "\"turn_detection\": {"
                "\"type\": \"semantic_vad\""
              "}"
            "},"
            "\"output\": {"
              "\"format\": {"
                "\"type\": \"audio/pcm\","
                "\"rate\": 24000"
              "},"
              //"\"voice\": \"sage\""
              "\"voice\": \"marin\""
            "}"
          "},"
          "\"instructions\": \"You are an AI robot named Stack-chan. Please speak in Japanese.\","
          "\"tools\":[]"
        "}"
      "}";
#endif
        "{"
          "\"setup\": {"
            "\"model\": \"models/gemini-2.5-flash-native-audio-preview-12-2025\","
            "\"generationConfig\": {"
              "\"responseModalities\": [\"AUDIO\"]"
            "},"
            "\"tools\": ["
              "{\"googleSearch\": {}}"
            "],"
            "\"systemInstruction\": {"
              "\"parts\": [{\"text\": \"You are an AI robot named Stack-chan. Please speak in Japanese.\"}],"
              "\"role\": \"user\""
            "}"
          "}"
        "}";

const char input_audio_append[] =
        "{"
          "\"realtimeInput\": {"
            "\"audio\":{"
              "\"data\": \"REPLACE_TO_AUDIO_BASE64\","
              "\"mime_type\": \"audio/pcm\""
            "}"
          "}"
        "}";

// for function calling
//
const char conversation_item_create[] =
        "{"
            "\"type\": \"conversation.item.create\","
            "\"item\": {"
                "\"type\": \"function_call_output\","
                "\"call_id\": \"REPLACE_TO_CALL_ID\","
                "\"output\": \"{\\\"result\\\":\\\"REPLACE_TO_OUTPUT\\\"}\""
            "}"
        "}";

//const char response_create[] =
//        "{"
//            "\"type\": \"response.create\""
//        "}";

// WebSocketのコールバック関数としてクラスメソッドを渡せないので、コールバック関数を
// 通常の関数にして静的変数を経由してクラスのthisポインタを渡す。
GeminiLive* p_this;
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    String msgType, delta;
    DeserializationError error;

	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			Serial.printf("[WSc] Connected to url: %s\n", payload);

            /*
             * session.updateでAPIの振る舞いをカスタマイズする
             */
            {
                SpiRamJsonDocument sessionUpdateDoc(1024*10);
                DeserializationError error = deserializeJson(sessionUpdateDoc, session_update);
                if (error) {
                    Serial.println("webSocketEvent: JSON deserialization error (session_update)");
                }
#if 0
                // instructionsにロール、前回会話の要約を設定
                //
                SpiRamJsonDocument& sysPrmpt = p_this->get_chat_doc();
                String role = String((const char*)sysPrmpt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"]);
                Serial.printf("role: %s\n", role.c_str());
                String sysRole = String((const char*)sysPrmpt["messages"][SYSTEM_PROMPT_INDEX_SYSTEM_ROLE]["content"]);
                Serial.printf("sysRole: %s\n", sysRole.c_str());
                String userInfo = String((const char*)sysPrmpt["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"]);
                Serial.printf("userInfo: %s\n", userInfo.c_str());
                sessionUpdateDoc["session"]["instructions"] = role + sysRole + userInfo;

                // MCP tools listをfunctionとして挿入
                //
                for(int s=0; s<p_this->param.llm_conf.nMcpServers; s++){
                    if(!p_this->mcp_client[s]->isConnected()){
                        continue;
                    }

                    for(int t=0; t < p_this->mcp_client[s]->nTools; t++){
                        sessionUpdateDoc["session"]["tools"].add(p_this->mcp_client[s]->toolsListDoc["result"]["tools"][t]);
                        sessionUpdateDoc["session"]["tools"][t]["type"] = "function";
                    }
                }

                // FunctionCall.cppで定義したfunctionをsession.updateに挿入
                //
                SpiRamJsonDocument functionsDoc(1024*10);
                error = deserializeJson(functionsDoc, json_Functions.c_str());
                if (error) {
                    Serial.println("FunctionCall: JSON deserialization error");
                }

                int nFuncs = functionsDoc.size();
                int nMcpFuncs = sessionUpdateDoc["session"]["tools"].size();
                for(int i=0; i<nFuncs; i++){
                    sessionUpdateDoc["session"]["tools"].add(functionsDoc[i]);
                    sessionUpdateDoc["session"]["tools"][nMcpFuncs + i]["type"] = "function";
                }
#endif
                String sessionUpdateStr;
                serializeJson(sessionUpdateDoc, sessionUpdateStr);
                String jsonPretty;
                serializeJsonPretty(sessionUpdateDoc, jsonPretty);
                Serial.printf("[WSc] session update json: %s\n", jsonPretty.c_str());
                webSocket.sendTXT(sessionUpdateStr.c_str());
            }
			break;
		case WStype_TEXT:
			Serial.printf("[WSc] get text: %s\n", payload);
			//Serial.printf("[WSc] text size: %d\n", strlen((char*)payload));
			break;
		case WStype_BIN:
			Serial.printf("[WSc] get binary length: %u\n", length);
			//p_this->hexdump(payload, length);
            //Serial.printf("[WSc] get binary: %s\n", payload);

            error = deserializeJson(msgDoc, payload);
            if (error) {
                Serial.printf("WebSocket Event: JSON deserialization error %d\n", error.code());
            }

#ifndef REALTIME_API_WITH_TTS
            if(!msgDoc["setupComplete"].isNull()){
                Serial.printf("[WSc] setupComplete\n");
                //Serial.printf("[WSc] payload: %s\n", payload);
                avatar.setSpeechText("Please touch");
            }
            //else if(msgType.equals("input_audio_buffer.speech_started")){
            //    p_this->resetRealtimeRecordStartTime();
            //}
            else if(!msgDoc["serverContent"]["modelTurn"]["parts"][0]["text"].isNull()){
                Serial.printf("[WSc] modelTurn text\n");
            }
            else if(!msgDoc["serverContent"]["modelTurn"]["parts"][0]["inlineData"]["data"].isNull()){
                if(p_this->speaking == false){
                    Serial.printf("[WSc] input audio committed\n");
                    p_this->stopRealtimeRecord();
#ifndef REALTIME_API_WITH_TTS
                    M5.Mic.end();
                    M5.Speaker.begin();
                    p_this->speaking = true;
#else
                    p_this->speaking = true;
#endif
                }

                delta = msgDoc["serverContent"]["modelTurn"]["parts"][0]["inlineData"]["data"].as<String>();
                p_this->streamAudioDelta(delta);
            }
#else
            else if(msgType.equals("response.output_text.delta")){
                p_this->outputText += msgDoc["delta"].as<String>();

                // 区切り文字を検出したらテキストをキューに追加
                int idx = p_this->search_delimiter(p_this->outputText);
                if(idx > 0){
                    String inputText = p_this->outputText.substring(0, idx);
                    Serial.printf("[WSc] Push text: %s\n", inputText.c_str());
                    p_this->outputTextQueue.push_back(inputText);
                    p_this->outputText = p_this->outputText.substring(idx + strlen("。"), p_this->outputText.length());
                }
            }
#endif
            else if(!msgDoc["serverContent"]["turnComplete"].isNull()){
                Serial.printf("[WSc] turnComplete: %s\n", payload);
                bool isFuncCall = false;
#if 0
                int outputNum = msgDoc["response"]["output"].size();
                Serial.printf("output num: %d\n", outputNum);
                for(int i = 0; i < outputNum; i++){
                    String outputType = msgDoc["response"]["output"][i]["type"].as<String>();
                    if(outputType.equals("function_call")){
                        //Serial.printf("[WSc] function call payload: %s\n", payload);
                        isFuncCall = true;
                        const char* name = msgDoc["response"]["output"][i]["name"];
                        const char* args = msgDoc["response"]["output"][i]["arguments"];
                        const char* call_id = msgDoc["response"]["output"][i]["call_id"];
                        Serial.printf("name: %s, args: %s\n", name, args);

                        //avatar.setSpeechFont(&fonts::efontJA_12);
                        //avatar.setSpeechText(name);
                        String response = p_this->exec_calledFunc(name, args);
                        response.replace("\"", "\\\"");     //JSON内の文字列を囲む"にエスケープ(\)を付ける

                        String json(conversation_item_create);
                        json.replace("REPLACE_TO_CALL_ID", call_id);
                        json.replace("REPLACE_TO_OUTPUT", response.c_str());
                        Serial.printf("[WSc] function output: %s\n", json.c_str());
                        webSocket.sendTXT(json);
                        webSocket.sendTXT(response_create);
                    }
                }
#endif

                if(!isFuncCall){
#ifndef REALTIME_API_WITH_TTS
                    p_this->startRealtimeRecord();
                    while (M5.Speaker.isPlaying()) { /*vTaskDelay(1);*/ }
                    M5.Speaker.end();
                    M5.Mic.begin();

                    for(int i=0; i<2; i++){
                        memset(p_this->audioBuf[i], 0, 100 * 1024);
                    }
                    p_this->speaking = false;
#else
                    p_this->response_done = true;
#endif
                }
            }

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


GeminiLive::GeminiLive(llm_param_t param) : 
    LLMBase(param, GEMINI_PROMPT_MAX_SIZE),
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

  msgDoc = SpiRamJsonDocument(1024*150);

  // WebSocket connect
  //
  avatar.setSpeechText("Connecting...");
  webSocket.beginSslWithCA("generativelanguage.googleapis.com", 443, "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent", root_ca_google_gemini);
  
  // event handler
  p_this = this;    //コールバック関数に静的変数経由でthisポインタを渡す
  webSocket.onEvent(webSocketEvent);
  //String auth = "Bearer " + param.api_key;
  String auth = "x-goog-api-key: " + param.api_key;
  
  //webSocket.setAuthorization(auth.c_str());
  webSocket.setExtraHeaders(auth.c_str());

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

}

void GeminiLive::webSocketProcess()
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
        String json(input_audio_append);
        json.replace("REPLACE_TO_AUDIO_BASE64", audio_base64);
        webSocket.sendTXT(json);
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

int GeminiLive::getAudioLevel()
{
    return abs(*audioBuf[nextBufIdx ^ 1]) * 50;
}

void GeminiLive::startRealtimeRecord()
{
    if(!realtime_recording){
        Serial.println("Start realtime recording");
        realtime_recording = true;
        startTime = xTaskGetTickCount();
    }
}

void GeminiLive::stopRealtimeRecord()
{
    if(realtime_recording){
        Serial.println("Stop realtime recording");
        realtime_recording = false;
        startTime = 0;
    }
}

void GeminiLive::resetRealtimeRecordStartTime()
{
    startTime = xTaskGetTickCount();
}

portTickType GeminiLive::checkRealtimeRecordTimeout()
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
        // 今までの会話を要約してSPIFFSに保存
        //
    }

    return elapsedTime;
}

int GeminiLive::base64_decode(const char* input, int size, char* output)
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


void GeminiLive::hexdump(const void *mem, uint32_t len, uint8_t cols) {
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


void GeminiLive::streamAudioDelta(String& delta)
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