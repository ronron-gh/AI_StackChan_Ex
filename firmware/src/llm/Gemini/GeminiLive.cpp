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
#include "../ChatGPT/FunctionCall.h"    // GeminiとChatGPTのFunction Calling仕様は共通
//#include "MCPClient.h"
#include "Robot.h"

#include <base64.h>
#include "libb64/cdecode.h"
#include <WebSocketsClient.h>

using namespace m5avatar;
extern Avatar avatar;

static const char session_update[] =
        "{"
          "\"setup\": {"
            "\"model\": \"models/gemini-2.5-flash-native-audio-preview-12-2025\","
            "\"generationConfig\": {"
#ifndef REALTIME_API_WITH_TTS
              "\"responseModalities\": [\"AUDIO\"]"
#else
              "\"responseModalities\": [\"TEXT\"]"
              // 2026.2.5現在、出力形式をテキストに設定すると Code: 1007 (Cannot extract voices from a non-audio request.)
              // というエラーが返ってくる。Googleに対してissueは投げられている模様 (https://github.com/livekit/agents/issues/4423)
#endif
            "},"
            "\"tools\": ["
              "{\"functionDeclarations\": []},"
              "{\"googleSearch\": {}}"
            "],"
            "\"systemInstruction\": {"
              "\"parts\": [{\"text\": \"You are an AI robot named Stack-chan. Please speak in Japanese.\"}],"
              "\"role\": \"user\""
            "}"
          "}"
        "}";

static const char input_audio_append[] =
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
static const char function_response[] =
        "{"
            "\"tool_response\": {"
              "\"function_responses\": [{"
                "\"name\": \"REPLACE_TO_TOOL_NAME\","
                "\"response\": {\"result\":\"REPLACE_TO_OUTPUT\"},"
                "\"id\": \"REPLACE_TO_CALL_ID\""
              "}]"
            "}"
        "}";

// for debug (音声の代わりにテキストのプロンプトを入力する)
//
static const char input_text[] =
     "{"
        "\"client_content\": {"
          "\"turn_complete\": true,"
          "\"turns\": [{"
              "\"role\": \"user\","
              "\"parts\": ["
                  "{\"inline_data\": {\"mime_type\": \"text/plain\", \"data\": \"REPLACE_TO_TEXT_BASE64\"}}"
              "]"
          "}]"
        "}"
      "}";

// WebSocketのコールバック関数としてクラスメソッドを渡せないので、コールバック関数を
// 通常の関数にして静的変数を経由してクラスのthisポインタを渡す。
static GeminiLive* p_this;
static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    String msgType, delta;
    DeserializationError error;

	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			Serial.printf("[WSc] Connected to url: %s\n", payload);

            /*
             * JSON "setup"でAPIの振る舞いをカスタマイズする
             */
            {
                SpiRamJsonDocument sessionUpdateDoc(1024*10);
                DeserializationError error = deserializeJson(sessionUpdateDoc, session_update);
                if (error) {
                    Serial.println("webSocketEvent: JSON deserialization error (session_update)");
                }

                // instructionsにロール、前回会話の要約を設定
                //
                Serial.printf("role: %s\n", p_this->role.c_str());
                Serial.printf("sysRole: %s\n", p_this->systemRole.c_str());
                Serial.printf("userInfo: %s\n", p_this->userInfo.c_str());
                sessionUpdateDoc["setup"]["systemInstruction"]["parts"][0]["text"] = p_this->role;
                sessionUpdateDoc["setup"]["systemInstruction"]["parts"][1]["text"] = p_this->systemRole;
                sessionUpdateDoc["setup"]["systemInstruction"]["parts"][2]["text"] = p_this->userInfo;

                // MCP tools listをfunctionとして挿入
                //
                for(int s=0; s<p_this->param.llm_conf.nMcpServers; s++){
                    if(!p_this->mcpClient[s]->isConnected()){
                        continue;
                    }

                    for(int t=0; t < p_this->mcpClient[s]->nTools; t++){
                        sessionUpdateDoc["setup"]["tools"][0]["functionDeclarations"].add(p_this->mcpClient[s]->toolsListDoc["result"]["tools"][t]);
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
                for(int i=0; i<nFuncs; i++){
                    sessionUpdateDoc["setup"]["tools"][0]["functionDeclarations"].add(functionsDoc[i]);
                }


                String sessionUpdateStr;
                serializeJson(sessionUpdateDoc, sessionUpdateStr);
                String jsonPretty;
                serializeJsonPretty(sessionUpdateDoc, jsonPretty);
                Serial.printf("[WSc] session update json: %s\n", jsonPretty.c_str());
                p_this->webSocket.sendTXT(sessionUpdateStr.c_str());
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

            error = deserializeJson(p_this->msgDoc, payload);
            if (error) {
                Serial.printf("WebSocket Event: JSON deserialization error %d\n", error.code());
            }

            if(!p_this->msgDoc["setupComplete"].isNull()){
                Serial.printf("[WSc] setupComplete\n");
                //Serial.printf("[WSc] payload: %s\n", payload);
                avatar.setSpeechText("Please touch");

#if 0   // for debug (音声の代わりにテキストのプロンプトを入力する)
                String text_base64;
                text_base64 = base64::encode((u8*)"What time is it now ?", strlen("What time is it now ?"));
                String json(input_text);
                json.replace("REPLACE_TO_TEXT_BASE64", text_base64.c_str());
                Serial.printf("[WSc] input text for test: %s\n", json.c_str());
                p_this->webSocket.sendTXT(json);
#endif
            }
            else if(!p_this->msgDoc["serverContent"]["modelTurn"]["parts"][0]["text"].isNull()){
                Serial.printf("[WSc] modelTurn text\n");
            }
#ifndef REALTIME_API_WITH_TTS
            else if(!p_this->msgDoc["serverContent"]["modelTurn"]["parts"][0]["inlineData"]["data"].isNull()){
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

                delta = p_this->msgDoc["serverContent"]["modelTurn"]["parts"][0]["inlineData"]["data"].as<String>();
                p_this->streamAudioDelta(delta);
            }
#else
            // TODO: Gemini Liveのメッセージ形式に変更（これはOpenAIの形式)
            #if 0
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
#endif
            else if(!p_this->msgDoc["toolCall"]["functionCalls"][0].isNull()){
                Serial.printf("[WSc] toolCall: %s\n", payload);

                String name = p_this->msgDoc["toolCall"]["functionCalls"][0]["name"].as<String>();
                String args = p_this->msgDoc["toolCall"]["functionCalls"][0]["args"].as<String>();
                String call_id = p_this->msgDoc["toolCall"]["functionCalls"][0]["id"].as<String>();
                Serial.printf("name: %s, args: %s, id: %s\n", name.c_str(), args.c_str(), call_id.c_str());

                String response = p_this->fnCall->exec_calledFunc(name.c_str(), args.c_str());
                response.replace("\"", "\\\"");     //JSON内の文字列を囲む"にエスケープ(\)を付ける

                String json(function_response);
                json.replace("REPLACE_TO_TOOL_NAME", name.c_str());
                json.replace("REPLACE_TO_CALL_ID", call_id.c_str());
                json.replace("REPLACE_TO_OUTPUT", response.c_str());
                Serial.printf("[WSc] function output: %s\n", json.c_str());
                p_this->webSocket.sendTXT(json);
            }
            else if(!p_this->msgDoc["serverContent"]["turnComplete"].isNull()){
                Serial.printf("[WSc] turnComplete: %s\n", payload);

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


GeminiLive::GeminiLive(llm_param_t param) : RealtimeLLMBase(param)
{
  p_this = this;    //コールバック関数に静的変数経由でthisポインタを渡す
  msgDoc = SpiRamJsonDocument(1024*150);

  M5.Lcd.println("MCP Servers:");
  for(int i=0; i<param.llm_conf.nMcpServers; i++){
    mcpClient[i] = new MCPClient(param.llm_conf.mcpServer[i].url, 
                                  param.llm_conf.mcpServer[i].port);
    
    if(mcpClient[i]->isConnected()){
      M5.Lcd.println(param.llm_conf.mcpServer[i].name);
    }
  }

  fnCall = new FunctionCall(param, this, mcpClient);
  //fnCall->init_func_call_settings(robot->m_config);

  enableMemory(param.llm_conf.enableMemory);
  if(enableMemory()){
    Serial.println("Memory is enabled");
    M5.Lcd.println("Memory is enabled");
  }

  load_role();


  // WebSocket connect
  //
  avatar.setSpeechText("Connecting...");
  webSocket.beginSslWithCA("generativelanguage.googleapis.com", 443, "/ws/google.ai.generativelanguage.v1beta.GenerativeService.BidiGenerateContent", root_ca_google_gemini);
  
  webSocket.onEvent(webSocketEvent);
  String auth = "x-goog-api-key: " + param.api_key;
  
  //webSocket.setAuthorization(auth.c_str());
  webSocket.setExtraHeaders(auth.c_str());

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

}


void GeminiLive::load_role(){
  Serial.println("Load role from SPIFFS.");
  if(enableMemory()){
    systemRole = systemRole_memory;
  }else{
    systemRole = systemRole_noMemory;
  }

  if(load_system_prompt_from_spiffs()){
    role = String((const char*)systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"]);
    //Serial.printf("role length: %d\n", role.length());
    if (role == "") {
      Serial.println("SPIFFS user role is empty. set default role.");
      role = defaultRole;
    }

    userInfo = String((const char*)systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"]);
    //Serial.println(userInfo);
    int idx = userInfo.indexOf("User Info");
    if(idx < 0 || !enableMemory()){
      userInfo = "User Info: ";
    }
  }else{
    // load_system_prompt_from_spiffs()内でSPIFFSからの取得失敗かつ
    // デフォルトのシステムプロンプト設定に失敗した場合（通常起こり得ない）。
    role = defaultRole;
    userInfo = "User Info: ";
  }
}

String& GeminiLive::buildInputAudioJson(String& jsonBuf, String& base64)
{
    jsonBuf.concat(input_audio_append);
    jsonBuf.replace("REPLACE_TO_AUDIO_BASE64", base64);
    //Serial.println(jsonBuf);
    return jsonBuf;
}

#endif  //REALTIME_API