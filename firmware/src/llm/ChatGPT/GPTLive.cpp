#ifdef GPTLIVE_FULL_DUPLEX
#include "GPTLive.h"
#include <Avatar.h>
#include <base64.h>
#include <esp_heap_caps.h>
#include "libb64/cdecode.h"
#include "rootCA/rootCACertificate.h"
#include "Robot.h"

extern m5avatar::Avatar avatar;
namespace {
#ifdef GPTLIVE_FULL_DUPLEX_TEST_TONE
constexpr bool diagnosticMode = true;
#else
constexpr bool diagnosticMode = false;
#endif
}

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

static const char* input_audio_append = R"(
  {
    "type": "session.input_audio.append",
    "audio": "REPLACE_TO_AUDIO_BASE64"
  }
)";

GPTLive::GPTLive(llm_param_t param)
  : RealtimeLLMBase(param, false),
    role(""),
    userInfo("User Info: "),
    systemRole("")
{
  msgDoc = SpiRamJsonDocument(1024*150);
  decoded = (uint8_t*)heap_caps_malloc(DECODE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  rtRecSamplerate = CoreS3FullDuplexAudio::SAMPLE_RATE;
  rtRecLength = CoreS3FullDuplexAudio::FRAME_SAMPLES;
  if(!decoded) Serial.println("[GPTLive] PSRAM decode allocation failed");

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
  webSocket.onEvent([this](WStype_t type, uint8_t* payload, size_t length){
      onWebSocketEvent(type, payload, length);
  });
  String auth = "Bearer " + param.api_key;
  webSocket.setAuthorization(auth.c_str());

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);

}


void GPTLive::onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length)
{
    switch(type){
    case WStype_CONNECTED:
        Serial.println("[GPTLive] WebSocket connected");
        onTransportConnected();
        break;
    case WStype_DISCONNECTED:
        Serial.println("[GPTLive] WebSocket disconnected");
        onTransportDisconnected();
        break;
    case WStype_TEXT: {
        const DeserializationError error = deserializeJson(msgDoc, payload, length);
        if(error){
            Serial.printf("[GPTLive] JSON deserialization error: %d\n", error.code());
            sessionFailed = true;
            break;
        }
        const String type = msgDoc["type"].as<String>();
        if(handleSessionEvent(type)) break;
        if(type == "session.output_audio.delta"){
            enqueueAudioDelta(msgDoc["delta"].as<String>());
        }else if(type == "session.output_transcript.delta"){
            Serial.printf("[GPTLive] Transcript: %s\n", msgDoc["delta"].as<const char*>() ?: "");
        }
        break;
    }
    case WStype_ERROR:
        sessionFailed = true;
        break;
    default:
        break;
    }
}

bool GPTLive::sendSessionStart()
{
    SpiRamJsonDocument doc(1024 * 10);
    if(deserializeJson(doc, session_start)) return false;
    doc["session"]["store"] = false;
    doc["session"]["instructions"] = role + " " + systemRole + " " + userInfo;
    String json;
    serializeJson(doc, json);
    return webSocket.sendTXT(json);
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



void GPTLive::onTransportConnected()
{
    recordingRequested.store(false);
    sessionFailed = false;
    sessionState.store(SessionState::Idle);
    avatar.setSpeechText("Please touch");
}

void GPTLive::onTransportDisconnected()
{
    const SessionState state = sessionState.load();
    if(state == SessionState::Starting || state == SessionState::Active || state == SessionState::Closing){
        Serial.printf("[GPTLive] Final usage unconfirmed: id=%s latest=%.3f seconds\n",
                      sessionId.c_str(), usageSeconds);
    }
    sessionState.store(SessionState::Disconnected);
    recordingRequested.store(false);
    stopSessionAudio();
    sessionFailed = false;
}

bool GPTLive::handleSessionEvent(const String& type)
{
    const SessionState state = sessionState.load();
    if(type == "session.started"){
        if(state != SessionState::Starting) return true;
        sessionId = msgDoc["session"]["id"].as<String>();
        sessionState.store(SessionState::Active);
        Serial.printf("[GPTLive] Session started: %s\n", sessionId.c_str());
        return true;
    }
    if(type == "session.usage.updated"){
        if(state == SessionState::Starting || state == SessionState::Active || state == SessionState::Closing){
            if(msgDoc["usage"]["seconds"].is<double>()) usageSeconds = msgDoc["usage"]["seconds"].as<double>();
        }
        return true;
    }
    if(type == "session.closed"){
        if(state != SessionState::Starting && state != SessionState::Active && state != SessionState::Closing) return true;
        const bool hasUsage = msgDoc["usage"]["seconds"].is<double>();
        if(hasUsage) usageSeconds = msgDoc["usage"]["seconds"].as<double>();
        sessionId = msgDoc["session"]["id"].as<String>();
        Serial.printf("[GPTLive] Session closed: id=%s reason=%s seconds=%.3f final=%s\n",
                      sessionId.c_str(), msgDoc["reason"].as<const char*>() ?: "unknown",
                      usageSeconds, hasUsage ? "confirmed" : "missing");
        recordingRequested.store(false);
        stopSessionAudio();
        // Mark completion before disconnect(), whose callback runs synchronously.
        sessionState.store(SessionState::Disconnected);
        webSocket.disconnect();
        return true;
    }
    if(type == "error"){
        Serial.println("[GPTLive] Session command error");
        sessionFailed = true;
        return true;
    }
    return false;
}

void GPTLive::closeSession()
{
    recordingRequested.store(false);
    sessionState.store(SessionState::Closing);
    sessionStateAt = millis();
    stopSessionAudio();
    sessionFailed = false;
    if(!webSocket.sendTXT("{\"type\":\"session.close\"}")){
        Serial.println("[GPTLive] Close send failed; final usage unconfirmed");
        webSocket.disconnect();
        onTransportDisconnected();
    }
}

bool GPTLive::processSessionLifecycle(bool stopping)
{
    // Keep draining events even while closing or preparing to suspend the task.
    webSocket.loop();
    const bool failed = sessionFailed || audio.hasFailed();
    SessionState state = sessionState.load();
    if(stopping || failed || sessionFailed) recordingRequested.store(false);
    if(state == SessionState::Idle && recordingRequested.load()){
        sessionId = "";
        usageSeconds = 0;
        sessionStateAt = millis();
        sessionState.store(SessionState::Starting);
        if(!sendSessionStart()) sessionFailed = true;
        state = sessionState.load();
    }
    if((state == SessionState::Active && !recordingRequested.load()) ||
       (state == SessionState::Starting && (stopping || failed || sessionFailed ||
        (uint32_t)(millis() - sessionStateAt) >= SESSION_START_TIMEOUT_MS))){
        if(state == SessionState::Starting) Serial.println("[GPTLive] Startup aborted; closing session");
        closeSession();
    }else if(state == SessionState::Closing &&
             (uint32_t)(millis() - sessionStateAt) >= SESSION_CLOSE_TIMEOUT_MS){
        Serial.println("[GPTLive] Close timed out; final usage unconfirmed");
        webSocket.disconnect();
        onTransportDisconnected();
    }else if((stopping || failed || sessionFailed) && state == SessionState::Idle){
        webSocket.disconnect();
        onTransportDisconnected();
    }else if(state == SessionState::Disconnected){
        recordingRequested.store(false);
    }
    switch(sessionState.load()){
    case SessionState::Idle: avatar.setSpeechText("Please touch"); break;
    case SessionState::Starting: avatar.setSpeechText("Starting..."); break;
    case SessionState::Closing: avatar.setSpeechText("Stopping..."); break;
    case SessionState::Disconnected: avatar.setSpeechText("Connecting..."); break;
    case SessionState::Active: return recordingRequested.load();
    }
    return false;
}

void GPTLive::enqueueAudioDelta(const String& delta)
{
    if(sessionFailed || !recordingRequested.load() || sessionState.load() != SessionState::Active || !audio.isRunning()) return;
    const size_t encoded = delta.length();
    if(!encoded) return;
    if(!decoded || encoded % 4 || encoded / 4 * 3 > DECODE_BYTES){ sessionFailed = true; return; }
    size_t padding = (delta[encoded - 1] == '=') + (delta[encoded - 2] == '=');
    for(size_t i = 0; i < encoded - padding; ++i){
        const char c = delta[i];
        if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '+' || c == '/')){ sessionFailed = true; return; }
    }
    const size_t expected = encoded / 4 * 3 - padding;
    if(!expected || expected % sizeof(int16_t)){ sessionFailed = true; return; }
    base64_decodestate state;
    base64_init_decodestate(&state);
    const int bytes = base64_decode_block(delta.c_str(), encoded, (char*)decoded, &state);
    if(bytes != expected || !audio.enqueue(decoded, expected)) sessionFailed = true;
}

void GPTLive::stopAudio()
{
    audio.end();
    RealtimeLLMBase::stopRealtimeRecord();
    speaking = false;
}

void GPTLive::startRealtimeRecord()
{
    if(diagnosticMode || sessionState.load() == SessionState::Idle) recordingRequested.store(true);
}

void GPTLive::stopRealtimeRecord()
{
    if(diagnosticMode || sessionState.load() == SessionState::Active) recordingRequested.store(false);
}

void GPTLive::stopSessionAudio()
{
    stopAudio();
    sessionFailed = false;
}

void GPTLive::webSocketProcess()
{
    if(paused.load() && pauseAck.load()){
        delay(1);
        return;
    }
    if(diagnosticMode && paused.load()){
        recordingRequested.store(false);
        stopAudio();
        pauseAck.store(true);
        delay(1);
        return;
    }
    if(diagnosticMode){
        // Validate shared-clock RX/TX before introducing the network or API.
        if(audio.hasFailed()){
            recordingRequested.store(false);
            stopAudio();
        }
        if(recordingRequested.load() && !audio.isRunning()){
            if(!audio.begin()) recordingRequested.store(false);
        }else if(!recordingRequested.load() && audio.isRunning()){
            stopAudio();
        }
        audio.setVolume(robot->spk_volume);
        audio.logStats();
        avatar.setSpeechText(audio.isRunning() ? "I2S test: touch to stop" : "I2S test: please touch");
        delay(1);
        return;
    }
    const bool active = processSessionLifecycle(paused.load());
    if(paused.load() && sessionState.load() == SessionState::Disconnected){
        pauseAck.store(true);
    }
    if(!active){ delay(1); return; }
    if(!audio.isRunning()){
        if(!decoded || !audio.begin()){
            Serial.println("[FD] Audio start failed; closing session");
            sessionFailed = true;
            return;
        }
        sentSamples = 0;
        sentLogAt = millis();
        audio.setCapture(true);
        RealtimeLLMBase::startRealtimeRecord();
    }
    audio.setVolume(robot->spk_volume);
    // RX/TX workers never access WebSocketsClient. Bound the send batch so loop() keeps running.
    for(int count = 0; count < 2 && recordingRequested.load() && audio.read(input); ++count){
        String encoded = base64::encode((uint8_t*)input.pcm, sizeof(input.pcm));
        String json;
        if(!webSocket.sendTXT(buildInputAudioJson(json, encoded))){ sessionFailed = true; break; }
        sentSamples += CoreS3FullDuplexAudio::FRAME_SAMPLES;
    }
    speaking = audio.getLevel() >= 100;
    audio.logStats();
    if((uint32_t)(millis() - sentLogAt) >= 2000){
        Serial.printf("[FD] Sent=%u samples (continues during output)\n", (unsigned)sentSamples);
        sentLogAt = millis();
    }
    avatar.setSpeechText(speaking ? "" : "Listening...");
    delay(1);
}

void GPTLive::suspendWebSocketLoopTask()
{
    paused.store(true);
    while(!pauseAck.load()) delay(1);
    RealtimeLLMBase::suspendWebSocketLoopTask();
}

void GPTLive::resumeWebSocketLoopTask()
{
    pauseAck.store(false);
    paused.store(false);
    RealtimeLLMBase::resumeWebSocketLoopTask();
}
#endif  // GPTLIVE_FULL_DUPLEX
