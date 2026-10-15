#if defined(REALTIME_API)

#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
#include "share/Mutex.h"
//#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCACertificate.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "GPTLive.h"
#include "FunctionCall.h"
#include "MCPClient.h"
#include "Robot.h"

#include <base64.h>
#include "libb64/cdecode.h"
#include <WebSocketsClient.h>
#include <esp_heap_caps.h>
#include <math.h>

using namespace m5avatar;
extern Avatar avatar;



static const char* session_start = R"(
  {
    "type": "session.start",
    "event_id": "event_start",
    "session": {
      "model": "gpt-live-1",
      "instructions": "Be concise. Delegate requests needing current information to the backend, which can search the web.",
      "audio": {
        "format": { "type": "audio/pcm", "rate": 24000 },
        "output": { "voice": "marin" }
      },
      "delegation": {
        "type": "responses",
        "responses": {
          "model": "gpt-5.6-luna",
          "tools": [{ "type": "web_search" }],
          "tool_choice": "auto"
        }
      }
    }
  }
)";

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

static const char* input_audio_append = R"(
  {
    "type": "session.input_audio.append",
    "audio": "REPLACE_TO_AUDIO_BASE64"
  }
)";

#if 0
static const char input_audio_append[] =
        "{"
          "\"type\": \"input_audio_buffer.append\","
          "\"audio\": \"REPLACE_TO_AUDIO_BASE64\""
        "}";
#endif

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
static GPTLive* p_this;
static void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    String msgType, delta;
    DeserializationError error;

	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
#ifndef REALTIME_API_WITH_TTS
            p_this->onAudioDisconnected();
#endif
			break;
		case WStype_CONNECTED:
			Serial.printf("[WSc] Connected to url: %s\n", payload);

            /*
             * session.updateでAPIの振る舞いをカスタマイズする
             */
            {
                SpiRamJsonDocument sessionUpdateDoc(1024*10);
                DeserializationError error = deserializeJson(sessionUpdateDoc, session_start);
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
#if 0
                // MCP tools listをfunctionとして挿入
                //
                for(int s=0; s < p_this->param.llm_conf.nMcpServers; s++){
                    if(true == p_this->param.llm_conf.mcpServer[s].disabled){
                        continue;
                    }
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
#endif
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

            error = deserializeJson(p_this->msgDoc, payload, length);
            if (error) {
                Serial.printf("WebSocket Event: JSON deserialization error %d\n", error.code());
#ifndef REALTIME_API_WITH_TTS
                p_this->onAudioDisconnected();
#endif
                break;
            }

            msgType = p_this->msgDoc["type"].as<String>();
            Serial.printf("[WSc] text type: %s\n", msgType.c_str());

            //if(msgType.equals("session.updated")){
            if(msgType.equals("session.started")){
                Serial.printf("[WSc] payload: %s\n", payload);
                avatar.setSpeechText("Please touch");
#ifndef REALTIME_API_WITH_TTS
                p_this->onAudioSessionStarted();
#endif
            }
#if 0   //GPT Liveでは来ない
            else if(msgType.equals("input_audio_buffer.speech_started")){
                p_this->resetRealtimeRecordStartTime();
            }
            else if(msgType.equals("input_audio_buffer.committed")){
                Serial.printf("[WSc] input audio committed\n");
                p_this->stopRealtimeRecord();
#ifndef REALTIME_API_WITH_TTS
                enterMutexAudio();
                M5.Mic.end();
                M5.Speaker.begin();
                p_this->speaking = true;
#else
                p_this->speaking = true;
#endif
            }
#endif  //GPT Liveでは来ない
#ifndef REALTIME_API_WITH_TTS
            //else if(msgType.equals("response.output_audio_transcript.delta")){
            else if(msgType.equals("session.output_transcript.delta")){
                delta = p_this->msgDoc["delta"].as<String>();
                Serial.printf("[WSc] delta: %s\n", delta.c_str());
            }
            //else if(msgType.equals("response.output_audio.delta")){
            else if(msgType.equals("session.output_audio.delta")){
                delta = p_this->msgDoc["delta"].as<String>();
                p_this->enqueueAudioDelta(delta);
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
#if 0   //GPT Liveでは来ない
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
                    while (M5.Speaker.isPlaying()) { vTaskDelay(1); }
                    M5.Speaker.end();
                    M5.Mic.begin();
                    exitMutexAudio();
                    p_this->startRealtimeRecord();

                    for(int i=0; i<2; i++){
                        memset(p_this->audioBuf[i], 0, 100 * 1024);
                    }
                    p_this->speaking = false;
#else
                    p_this->response_done = true;
#endif
                }
            }
#endif   //GPT Liveでは来ない
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


GPTLive::GPTLive(llm_param_t param)
  : RealtimeLLMBase(param, false),
    role(""),
    userInfo("User Info: "),
    systemRole("")
{
  p_this = this;    //コールバック関数に静的変数経由でthisポインタを渡す
  msgDoc = SpiRamJsonDocument(1024*150);
#ifndef REALTIME_API_WITH_TTS
  audioRing = (uint8_t*)heap_caps_malloc(AUDIO_RING_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  audioDecode = (uint8_t*)heap_caps_malloc(AUDIO_DECODE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  audioGateBuffer = (uint8_t*)heap_caps_malloc(AUDIO_FRAME_BYTES + AUDIO_PREROLL_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if(!audioRing || !audioDecode || !audioGateBuffer){
    heap_caps_free(audioRing);
    heap_caps_free(audioDecode);
    heap_caps_free(audioGateBuffer);
    audioRing = nullptr;
    audioDecode = nullptr;
    audioGateBuffer = nullptr;
    Serial.println("[GPTLive] PSRAM audio allocation failed; audio playback disabled");
  }
#endif
  
  initMcpClientList(mcpClient, param.llm_conf.mcpServer, param.llm_conf.nMcpServers);
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
  webSocket.beginSslWithCA("api.openai.com", 443, "/v1/live/sessions", root_ca_openai, "");
  webSocket.setExtraHeaders("");
  // event handler
  p_this = this;    //コールバック関数に静的変数経由でthisポインタを渡す
  webSocket.onEvent(webSocketEvent);
  String auth = "Bearer " + param.api_key;
  webSocket.setAuthorization(auth.c_str());

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

}


void GPTLive::load_role(){
  Serial.println("Load role from SPIFFS.");
  if(enableMemory()){
    systemRole = systemRole_memory;
  }else{
    systemRole = systemRole_noMemory;
  }
  systemRole += " " + systemRole_realtimeAvatarExpression;

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

String& GPTLive::buildInputAudioJson(String& jsonBuf, String& base64)
{
    jsonBuf.concat(input_audio_append);
    jsonBuf.replace("REPLACE_TO_AUDIO_BASE64", base64);
    //Serial.println(jsonBuf);
    return jsonBuf;
}



#ifndef REALTIME_API_WITH_TTS
void GPTLive::startRealtimeRecord()
{
    recordingRequested.store(true);
}

void GPTLive::stopRealtimeRecord()
{
    recordingRequested.store(false);
}

void GPTLive::onAudioSessionStarted()
{
    sessionReady = true;
}

void GPTLive::onAudioDisconnected()
{
    sessionReady = false;
    recordingRequested.store(false);
    audioFailed = true;
}

void GPTLive::enqueueAudioDelta(const String& delta)
{
    if(audioFailed || !sessionReady) return;
    const size_t encoded = delta.length();
    if(!encoded) return;
    if(!audioRing || !audioDecode || !audioGateBuffer || encoded % 4 || encoded / 4 * 3 > AUDIO_DECODE_BYTES){
        Serial.println("[GPTLive] Invalid audio size or unavailable PSRAM");
        audioFailed = true;
        return;
    }
    size_t padding = 0;
    if(delta[encoded - 1] == '=') ++padding;
    if(delta[encoded - 2] == '=') ++padding;
    for(size_t i = 0; i < encoded - padding; ++i){
        const char c = delta[i];
        if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '+' || c == '/')){
            Serial.println("[GPTLive] Invalid audio Base64");
            audioFailed = true;
            return;
        }
    }
    const size_t expected = encoded / 4 * 3 - padding;
    if(!expected || expected % sizeof(int16_t)){
        Serial.printf("[GPTLive] Invalid PCM length: bytes=%u used=%u\n",
                      (unsigned)expected, (unsigned)audioUsed);
        audioFailed = true;
        return;
    }
    base64_decodestate state;
    base64_init_decodestate(&state);
    const int decoded = base64_decode_block(delta.c_str(), encoded, (char*)audioDecode, &state);
    if(decoded != expected){
        Serial.println("[GPTLive] PCM decode length mismatch");
        audioFailed = true;
        return;
    }
    audioLastPcmAt = millis();
    size_t offset = 0;
    while(offset < expected && !audioFailed){
        size_t bytes = AUDIO_FRAME_BYTES - audioFrameUsed;
        if(bytes > expected - offset) bytes = expected - offset;
        memcpy(audioGateBuffer + audioFrameUsed, audioDecode + offset, bytes);
        audioFrameUsed += bytes;
        offset += bytes;
        if(audioFrameUsed == AUDIO_FRAME_BYTES){
            processAudioFrame(audioFrameUsed);
            audioFrameUsed = 0;
        }
    }
}

bool GPTLive::appendAudioPcm(const uint8_t* pcm, size_t bytes)
{
    if(bytes > AUDIO_RING_BYTES - audioUsed){
        Serial.printf("[GPTLive] Audio ring overflow: bytes=%u used=%u\n",
                      (unsigned)bytes, (unsigned)audioUsed);
        audioFailed = true;
        return false;
    }
    const size_t tail = AUDIO_RING_BYTES - audioWrite;
    const size_t first = bytes < tail ? bytes : tail;
    memcpy(audioRing + audioWrite, pcm, first);
    memcpy(audioRing, pcm + first, bytes - first);
    audioWrite = (audioWrite + bytes) % AUDIO_RING_BYTES;
    audioUsed += bytes;
    audioIdle = false;
    return true;
}

void GPTLive::processAudioFrame(size_t bytes)
{
    const size_t samples = bytes / sizeof(int16_t);
    if(!samples) return;
    const auto* pcm = (const int16_t*)audioGateBuffer;
    uint64_t energy = 0;
    for(size_t i = 0; i < samples; ++i){
        const int32_t sample = pcm[i];
        energy += (uint64_t)((int64_t)sample * sample);
    }
    const uint32_t rms = (uint32_t)sqrt((double)energy / samples);
    if(rms < audioRmsMin) audioRmsMin = rms;
    if(rms > audioRmsMax) audioRmsMax = rms;
    if((uint32_t)(millis() - audioRmsLogAt) >= AUDIO_RMS_LOG_MS){
        Serial.printf("[GPTLive] Output RMS min=%u max=%u active=%d start=%u continue=%u\n",
                      (unsigned)audioRmsMin, (unsigned)audioRmsMax, audioSpeechActive,
                      (unsigned)AUDIO_START_RMS, (unsigned)AUDIO_CONTINUE_RMS);
        audioRmsLogAt = millis();
        audioRmsMin = UINT32_MAX;
        audioRmsMax = 0;
    }
    auto* preroll = audioGateBuffer + AUDIO_FRAME_BYTES;
    if(!audioSpeechActive){
        if(energy >= (uint64_t)AUDIO_START_RMS * AUDIO_START_RMS * samples){
            // 先行保持分と発話開始窓を一括で収容できることを確認する。
            if(audioPrerollUsed + bytes > AUDIO_RING_BYTES - audioUsed){
                Serial.println("[GPTLive] Audio ring overflow at speech start");
                audioFailed = true;
                return;
            }
            const size_t start = (audioPrerollWrite + AUDIO_PREROLL_BYTES - audioPrerollUsed) % AUDIO_PREROLL_BYTES;
            size_t first = AUDIO_PREROLL_BYTES - start;
            if(first > audioPrerollUsed) first = audioPrerollUsed;
            appendAudioPcm(preroll + start, first);
            appendAudioPcm(preroll, audioPrerollUsed - first);
            appendAudioPcm(audioGateBuffer, bytes);
            audioPrerollUsed = audioPrerollWrite = 0;
            audioSilentSamples = 0;
            audioSpeechActive = true;
            Serial.printf("[GPTLive] Output speech started: RMS=%u\n", (unsigned)rms);
        }else{
            size_t first = AUDIO_PREROLL_BYTES - audioPrerollWrite;
            if(first > bytes) first = bytes;
            memcpy(preroll + audioPrerollWrite, audioGateBuffer, first);
            memcpy(preroll, audioGateBuffer + first, bytes - first);
            audioPrerollWrite = (audioPrerollWrite + bytes) % AUDIO_PREROLL_BYTES;
            audioPrerollUsed += bytes;
            if(audioPrerollUsed > AUDIO_PREROLL_BYTES) audioPrerollUsed = AUDIO_PREROLL_BYTES;
        }
        return;
    }
    if(!appendAudioPcm(audioGateBuffer, bytes)) return;
    if(energy < (uint64_t)AUDIO_CONTINUE_RMS * AUDIO_CONTINUE_RMS * samples){
        audioSilentSamples += samples;
        if(audioSilentSamples >= AUDIO_SILENCE_SAMPLES){
            audioSpeechActive = false;
            audioSilentSamples = 0;
            Serial.printf("[GPTLive] Output speech ended: %u ms PCM silence\n",
                          (unsigned)AUDIO_SPEECH_END_MS);
        }
    }else{
        audioSilentSamples = 0;
    }
}

void GPTLive::resetAudioGate()
{
    audioFrameUsed = audioPrerollWrite = audioPrerollUsed = audioSilentSamples = 0;
    audioSpeechActive = false;
    audioLastPcmAt = audioRmsLogAt = millis();
    audioRmsMin = UINT32_MAX;
    audioRmsMax = 0;
}

void GPTLive::resetAudioPlayback(bool resetGate)
{
    // M5Unified は PCM を参照するので、タスク停止を待つ end() の後で領域を再利用する。
    if(audioOwned){
        M5.Speaker.end();
        if(restoreMic && !M5.Mic.begin()){
            Serial.println("[GPTLive] Mic.begin failed");
            recordingRequested.store(false);
        }
        exitMutexAudio();
        audioOwned = false;
    }
    audioRead = audioWrite = audioUsed = audioInFlight = 0;
    audioIdle = false;
    playbackLevel.store(0);
    speaking = false;
    audioSpeechActive = false;
    audioSilentSamples = 0;
    if(resetGate) resetAudioGate();
}

void GPTLive::processAudioPlayback()
{
    // 受信が途絶えた場合も最後の 20 ms 未満の窓を取りこぼさない。
    if(audioFrameUsed && (uint32_t)(millis() - audioLastPcmAt) >= AUDIO_IDLE_MS){
        processAudioFrame(audioFrameUsed);
        audioFrameUsed = 0;
        if(audioFailed) return;
    }
    if(audioSpeechActive && (uint32_t)(millis() - audioLastPcmAt) >= AUDIO_RECEIVE_TIMEOUT_MS){
        audioSpeechActive = false;
        audioSilentSamples = 0;
        Serial.printf("[GPTLive] Output speech ended: %u ms receive timeout\n",
                      (unsigned)AUDIO_RECEIVE_TIMEOUT_MS);
    }
    if(audioUsed && !audioOwned){
        // ほかの音声処理が使用中でも WebSocket の受信を止めない。
        if(xSemaphoreTake(mutexAudio, 0) != pdTRUE) return;
        audioOwned = true;
        restoreMic = M5.Mic.isRunning();
        RealtimeLLMBase::stopRealtimeRecord();
        M5.Mic.end();
        if(!M5.Speaker.begin() || !M5.Speaker.isRunning()){
            Serial.println("[GPTLive] Speaker.begin failed");
            audioFailed = true;
            return;
        }
        speaking = true;
        Serial.println("[GPTLive] Mic OFF / Speaker ON");
    }
    if(!audioOwned) return;
    if(!M5.Speaker.isRunning()){
        audioFailed = true;
        return;
    }
    if(M5.Speaker.isPlaying()) return;
    if(audioInFlight){
        // 再生完了まで使用量に含め、受信側による上書きを防ぐ。
        audioRead = (audioRead + audioInFlight) % AUDIO_RING_BYTES;
        audioUsed -= audioInFlight;
        audioInFlight = 0;
        playbackLevel.store(0);
    }
    if(audioUsed){
        size_t bytes = AUDIO_RING_BYTES - audioRead;
        if(bytes > audioUsed) bytes = audioUsed;
        if(bytes > AUDIO_PLAY_BYTES) bytes = AUDIO_PLAY_BYTES;
        auto* pcm = (int16_t*)(audioRing + audioRead);
        int peak = 0;
        for(size_t i = 0; i < bytes / sizeof(int16_t); ++i){
            const int level = abs((int)pcm[i]);
            if(level > peak) peak = level;
        }
        if(!M5.Speaker.playRaw(pcm, bytes / sizeof(int16_t), 24000, false, 1, 0, false)){
            Serial.println("[GPTLive] Speaker.playRaw failed");
            audioFailed = true;
            return;
        }
        audioInFlight = bytes;
        playbackLevel.store(peak);
        audioIdle = false;
    }else if(audioSpeechActive){
        // リングの枯渇だけでは句読点の間と発話終了を区別できない。
        audioIdle = false;
    }else if(!audioIdle){
        audioIdle = true;
        audioIdleSince = millis();
    }else if((uint32_t)(millis() - audioIdleSince) >= AUDIO_IDLE_MS){
        resetAudioPlayback(false);
        Serial.println("[GPTLive] Speaker OFF / Mic restored");
    }
}

void GPTLive::webSocketProcess()
{
    if(pauseRequested.load()){
        if(!pauseAcknowledged.load()){
            recordingRequested.store(false);
            RealtimeLLMBase::stopRealtimeRecord();
            resetAudioPlayback();
            // 復帰時に停止前の応答が socket から流れ込むのを防ぐ。
            sessionReady = false;
            if(webSocket.isConnected()) webSocket.disconnect();
            audioFailed = false;
            pauseAcknowledged.store(true);
        }
        delay(1);
        return;
    }
    webSocket.loop();
    if(audioFailed){
        // 不正なチャンクの後続を再生しないよう、新しいセッションに切り替える。
        recordingRequested.store(false);
        RealtimeLLMBase::stopRealtimeRecord();
        resetAudioPlayback();
        if(webSocket.isConnected()) webSocket.disconnect();
        audioFailed = false;
    }
    processAudioPlayback();
    if(audioOwned || audioUsed || audioFailed){
        RealtimeLLMBase::stopRealtimeRecord();
        avatar.setSpeechText("");
        delay(1);
        return;
    }
    if(recordingRequested.load() && sessionReady){
        if(!realtime_recording){
            enterMutexAudio();
            if(M5.Speaker.isRunning()) M5.Speaker.end();
            const bool micReady = M5.Mic.begin();
            exitMutexAudio();
            if(!micReady){
                Serial.println("[GPTLive] Mic.begin failed; recording stopped");
                recordingRequested.store(false);
            }else{
                RealtimeLLMBase::startRealtimeRecord();
            }
        }
    }else{
        RealtimeLLMBase::stopRealtimeRecord();
    }
    processRealtimeInput();
}

void GPTLive::suspendWebSocketLoopTask()
{
    // mutex を取得した WebSocket タスク自身に解放させてから suspend する。
    pauseRequested.store(true);
    while(!pauseAcknowledged.load()) delay(1);
    RealtimeLLMBase::suspendWebSocketLoopTask();
}

void GPTLive::resumeWebSocketLoopTask()
{
    pauseAcknowledged.store(false);
    pauseRequested.store(false);
    RealtimeLLMBase::resumeWebSocketLoopTask();
}
#endif  // !REALTIME_API_WITH_TTS

#endif  //REALTIME_API
