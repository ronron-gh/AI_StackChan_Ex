#if defined(REALTIME_API)

#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
//#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCACertificate.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "RealtimeChatGPT.h"
#include "FunctionCall.h"
//#include "MCPClient.h"
#include "Robot.h"

#include <base64.h>
#include "libb64/cdecode.h"
#include <WebSocketsClient.h>

using namespace m5avatar;
extern Avatar avatar;

static const char session_update[] =
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


static const char input_audio_append[] =
        "{"
          "\"type\": \"input_audio_buffer.append\","
          "\"audio\": \"REPLACE_TO_AUDIO_BASE64\""
        "}";

// for function calling
//
static const char conversation_item_create[] =
        "{"
            "\"type\": \"conversation.item.create\","
            "\"item\": {"
                "\"type\": \"function_call_output\","
                "\"call_id\": \"REPLACE_TO_CALL_ID\","
                "\"output\": \"{\\\"result\\\":\\\"REPLACE_TO_OUTPUT\\\"}\""
            "}"
        "}";

static const char response_create[] =
        "{"
            "\"type\": \"response.create\""
        "}";

// WebSocketのコールバック関数としてクラスメソッドを渡せないので、コールバック関数を
// 通常の関数にして静的変数を経由してクラスのthisポインタを渡す。
static RealtimeChatGPT* p_this;
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
             * session.updateでAPIの振る舞いをカスタマイズする
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
                sessionUpdateDoc["session"]["instructions"] = p_this->role + " "
                                                              + p_this->systemRole + " "
                                                              + p_this->userInfo;

                // MCP tools listをfunctionとして挿入
                //
                for(int s=0; s<p_this->param.llm_conf.nMcpServers; s++){
                    if(!p_this->mcpClient[s]->isConnected()){
                        continue;
                    }

                    for(int t=0; t < p_this->mcpClient[s]->nTools; t++){
                        sessionUpdateDoc["session"]["tools"].add(p_this->mcpClient[s]->toolsListDoc["result"]["tools"][t]);
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

                String sessionUpdateStr;
                serializeJson(sessionUpdateDoc, sessionUpdateStr);
                String jsonPretty;
                serializeJsonPretty(sessionUpdateDoc, jsonPretty);
                Serial.printf("[WSc] session update json: %s\n", jsonPretty.c_str());
                p_this->webSocket.sendTXT(sessionUpdateStr.c_str());
            }
			break;
		case WStype_TEXT:
			//Serial.printf("[WSc] get text: %s\n", payload);
			Serial.printf("[WSc] text size: %d\n", strlen((char*)payload));

            error = deserializeJson(p_this->msgDoc, payload);
            if (error) {
                Serial.printf("WebSocket Event: JSON deserialization error %d\n", error.code());
            }

            msgType = p_this->msgDoc["type"].as<String>();
            Serial.printf("[WSc] text type: %s\n", msgType.c_str());

            if(msgType.equals("session.updated")){
                Serial.printf("[WSc] payload: %s\n", payload);
                avatar.setSpeechText("Please touch");
            }
            else if(msgType.equals("input_audio_buffer.speech_started")){
                p_this->resetRealtimeRecordStartTime();
            }
            else if(msgType.equals("input_audio_buffer.committed")){
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
#ifndef REALTIME_API_WITH_TTS            
            else if(msgType.equals("response.output_audio_transcript.delta")){
                delta = p_this->msgDoc["delta"].as<String>();
                Serial.printf("[WSc] delta: %s\n", delta.c_str());
            }
            else if(msgType.equals("response.output_audio.delta")){
                delta = p_this->msgDoc["delta"].as<String>();
                p_this->streamAudioDelta(delta);
            }
#else
            else if(msgType.equals("response.output_text.delta")){
                p_this->outputText += p_this->msgDoc["delta"].as<String>();

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
            else if(msgType.equals("response.done")){
                Serial.printf("[WSc] response.done payload: %s\n", payload);
                int outputNum = p_this->msgDoc["response"]["output"].size();
                Serial.printf("output num: %d\n", outputNum);
                bool isFuncCall = false;
                for(int i = 0; i < outputNum; i++){
                    String outputType = p_this->msgDoc["response"]["output"][i]["type"].as<String>();
                    if(outputType.equals("function_call")){
                        //Serial.printf("[WSc] function call payload: %s\n", payload);
                        isFuncCall = true;
                        const char* name = p_this->msgDoc["response"]["output"][i]["name"];
                        const char* args = p_this->msgDoc["response"]["output"][i]["arguments"];
                        const char* call_id = p_this->msgDoc["response"]["output"][i]["call_id"];
                        Serial.printf("name: %s, args: %s\n", name, args);

                        //avatar.setSpeechFont(&fonts::efontJA_12);
                        //avatar.setSpeechText(name);
                        String response = p_this->fnCall->exec_calledFunc(name, args);
                        response.replace("\"", "\\\"");     //JSON内の文字列を囲む"にエスケープ(\)を付ける

                        String json(conversation_item_create);
                        json.replace("REPLACE_TO_CALL_ID", call_id);
                        json.replace("REPLACE_TO_OUTPUT", response.c_str());
                        Serial.printf("[WSc] function output: %s\n", json.c_str());
                        p_this->webSocket.sendTXT(json);
                        p_this->webSocket.sendTXT(response_create);
                    }
                }

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
            else if(msgType.equals("rate_limits.updated")){
                //Serial.printf("[WSc] payload: %s\n", payload);
            }
            else if(msgType.equals("error")){
                Serial.printf("[WSc] payload: %s\n", payload);
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


RealtimeChatGPT::RealtimeChatGPT(llm_param_t param)
  : RealtimeLLMBase(param),
    role(""),
    userInfo("User Info: "),
    systemRole("")
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
  webSocket.beginSslWithCA("api.openai.com", 443, "/v1/realtime?model=gpt-realtime", root_ca_openai);

  // event handler
  p_this = this;    //コールバック関数に静的変数経由でthisポインタを渡す
  webSocket.onEvent(webSocketEvent);
  String auth = "Bearer " + param.api_key;
  webSocket.setAuthorization(auth.c_str());

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

}


void RealtimeChatGPT::load_role(){
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

String& RealtimeChatGPT::buildInputAudioJson(String& jsonBuf, String& base64)
{
    jsonBuf.concat(input_audio_append);
    jsonBuf.replace("REPLACE_TO_AUDIO_BASE64", base64);
    //Serial.println(jsonBuf);
    return jsonBuf;
}


#endif  //REALTIME_API