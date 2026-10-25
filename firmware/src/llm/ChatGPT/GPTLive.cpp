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

  if(!diagnosticMode && !initTools())
    Serial.println("[GPTLive] Function tool initialization failed");


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
            resetToolCalls();
            sessionFailed = true;
            break;
        }
        const String type = msgDoc["type"].as<String>();
        if(handleSessionEvent(type)) break;
        if(type == "session.output_audio.delta"){
            enqueueAudioDelta(msgDoc["delta"].as<String>());
        }else if(type == "session.input_transcript.delta" || type == "session.output_transcript.delta"){
            if(type == "session.output_transcript.delta")
                Serial.printf("[GPTLive] Transcript: %s\n", msgDoc["delta"].as<const char*>() ?: "");
            if(sessionState.load() == SessionState::Active && recordingRequested.load()){
                String text = msgDoc["delta"] | "";
                text.trim();
                if(!text.isEmpty()) lastConversationAt = millis();
            }
        }else if(type == "response.event"){
            handleBackendEvent();
        }
        break;
    }
    case WStype_ERROR:
        resetToolCalls();
        sessionFailed = true;
        break;
    default:
        break;
    }
}

bool GPTLive::sendSessionStart()
{
    if(!toolTask) return false;
    SpiRamJsonDocument doc(64 * 1024);
    if(deserializeJson(doc, session_start)) return false;
    doc["session"]["store"] = false;
    doc["session"]["instructions"] = role + " " + systemRole + " " + userInfo +
        "\nDelegation policy: Generate <|SpawnThinking|> when a request needs current information, "
        "web search, device functions (date/time, volume, brightness, expression, timers), "
        "memory updates, or configured external tools. Never claim an action succeeded before its result.";
    JsonObject backend = doc["session"]["delegation"]["responses"];
    backend["instructions"] = role + " " + systemRole + " " + userInfo +
        "\nUse the provided tools for device operations and current information. "
        "Return verified results or a concise error. Do not retry actions automatically.";
    backend["parallel_tool_calls"] = false;
    for(JsonObject definition : toolDefinitions.as<JsonArray>()) backend["tools"].add(definition);
    if(doc.overflowed() || measureJson(doc) > 60 * 1024) return false;
    String json;
    serializeJson(doc, json);
    String jsonPretty;
    serializeJsonPretty(doc, jsonPretty);
    Serial.printf("[GPTLive] session start json: %s\n", jsonPretty.c_str());
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
    resetToolCalls();
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
        lastConversationAt = millis();
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
        resetToolCalls();
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
        Serial.printf("[GPTLive] Command error: %s: %s\n", msgDoc["error"]["code"] | "unknown",
                      msgDoc["error"]["message"] | "");
        resetToolCalls();
        sessionFailed = true;
        return true;
    }
    return false;
}

void GPTLive::closeSession()
{
    resetToolCalls();
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
    if(state == SessionState::Active && recordingRequested.load() &&
       (uint32_t)(millis() - lastConversationAt) >= SESSION_IDLE_TIMEOUT_MS){
        Serial.println("[GPTLive] No conversation for 60 seconds; closing session");
        recordingRequested.store(false);
    }
    if(state == SessionState::Idle && recordingRequested.load() && pendingToolJobs.load() == 0){
        resetToolCalls();
        load_role();
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
    processToolResults();
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
    if(paused.load() && sessionState.load() == SessionState::Disconnected && pendingToolJobs.load() == 0){
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
bool GPTLive::initTools()
{
    if(param.llm_conf.nMcpServers) initMcpClientList(mcpClient, param.llm_conf.mcpServer, param.llm_conf.nMcpServers, 30000);
    toolDefinitions.to<JsonArray>();
    SpiRamJsonDocument builtins(16 * 1024);
    if(deserializeJson(builtins, json_Functions)) return false;
    auto add = [this](JsonObject source) {
        const char* name = source["name"] | "";
        if(!*name || strlen(name) >= sizeof(ToolJob::name)) return false;
        // These functions acquire audio input or suspend the current realtime session.
        if(!strcmp(name, "ask") || !strcmp(name, "register_wakeword") ||
           !strcmp(name, "wakeword_enable") || !strcmp(name, "delete_wakeword")) return true;
        for(JsonObject existing : toolDefinitions.as<JsonArray>())
            if(!strcmp(existing["name"] | "", name)) return false;
        JsonObject target = toolDefinitions.as<JsonArray>().createNestedObject();
        target["type"] = "function";
        target["name"] = String(name);
        target["description"] = source["description"].as<String>();
        target["parameters"] = source["parameters"];
        // Legacy schemas do not require every optional property (Responses strict mode does).
        target["strict"] = false;
        return !toolDefinitions.overflowed() && measureJson(toolDefinitions) <= 24 * 1024 &&
            target["parameters"].is<JsonObject>();
    };
    for(JsonObject definition : builtins.as<JsonArray>()) if(!add(definition)) return false;
    for(int s = 0; s < param.llm_conf.nMcpServers; ++s){
        if(!mcpClient[s] || !mcpClient[s]->isConnected()) continue;
        for(int t = 0; t < mcpClient[s]->nTools; ++t)
            if(!add(mcpClient[s]->toolsListDoc["result"]["tools"][t].as<JsonObject>())) return false;
    }
    fnCall = new FunctionCall(param, this, mcpClient);
    toolRequests = xQueueCreate(2, sizeof(ToolJob*));
    toolResults = xQueueCreate(4, sizeof(ToolJob*));
    if(toolRequests && toolResults &&
       xTaskCreate(toolWorker, "gptLiveTools", 8192, this, 1, &toolTask) == pdPASS) return true;
    if(toolRequests) vQueueDelete(toolRequests);
    if(toolResults) vQueueDelete(toolResults);
    toolRequests = toolResults = nullptr;
    delete fnCall;
    fnCall = nullptr;
    return false;
}

void GPTLive::toolWorker(void* context)
{
    auto* self = static_cast<GPTLive*>(context);
    for(;;){
        ToolJob* job = nullptr;
        if(xQueueReceive(self->toolRequests, &job, portMAX_DELAY) != pdTRUE) continue;
        if(job->generation == self->toolGeneration.load()){
            String result = self->executeTool(*job);
            if(result.length() >= sizeof(job->output)) result = "{\"error\":\"Tool result exceeds 8191 bytes\"}";
            strlcpy(job->output, result.c_str(), sizeof(job->output));
        }
        // The websocket owner drains this queue even while closing the session.
        if(xQueueSend(self->toolResults, &job, portMAX_DELAY) != pdTRUE) free(job);
        self->pendingToolJobs.fetch_sub(1);
    }
}

String GPTLive::executeTool(ToolJob& job)
{
    JsonObject definition;
    for(JsonObject candidate : toolDefinitions.as<JsonArray>()){
        if(!strcmp(candidate["name"] | "", job.name)){ definition = candidate; break; }
    }
    if(definition.isNull()) return "{\"error\":\"Unknown function\"}";
    SpiRamJsonDocument args(16 * 1024);
    if(deserializeJson(args, job.arguments) || !args.is<JsonObject>())
        return "{\"error\":\"Invalid function arguments\"}";
    JsonObject schema = definition["parameters"];
    for(JsonVariant required : schema["required"].as<JsonArray>()){
        if(!args.containsKey(required.as<const char*>())) return "{\"error\":\"Missing required argument\"}";
    }
    for(JsonPair property : schema["properties"].as<JsonObject>()){
        if(!args.containsKey(property.key().c_str())) continue;
        JsonVariant value = args[property.key().c_str()];
        String type = property.value()["type"] | "";
        if((type == "string" && !value.is<const char*>()) ||
           (type == "integer" && !value.is<int>()) ||
           (type == "number" && !value.is<double>()) ||
           (type == "boolean" && !value.is<bool>()) ||
           (type == "object" && !value.is<JsonObject>()) ||
           (type == "array" && !value.is<JsonArray>())) return "{\"error\":\"Incorrect argument type\"}";
        JsonArray choices = property.value()["enum"];
        if(!choices.isNull()){
            bool matches = false;
            for(JsonVariant choice : choices) if(choice == value) matches = true;
            if(!matches) return "{\"error\":\"Invalid argument value\"}";
        }
    }
    if(!strcmp(job.name, "timer") || !strcmp(job.name, "timer_change")){
        int seconds = args["time"] | -1;
        if(seconds < 0 || seconds > INT32_MAX / 1000 || (!strcmp(job.name, "timer") && seconds == 0))
            return "{\"error\":\"Invalid timer duration\"}";
        if(!strcmp(job.name, "timer_change") && !xAlarmTimer) return "{\"error\":\"No active timer\"}";
    }
    // ArduinoJson 6 enforces the legacy handler's fixed capacity; v7 grows dynamically.
#if ARDUINOJSON_VERSION_MAJOR < 7
    if(args.memoryUsage() > 256) return "{\"error\":\"Arguments exceed legacy handler capacity\"}";
#endif
    String result = fnCall->exec_calledFunc(job.name, job.arguments);
    return result.length() ? result : String("{\"error\":\"Function returned no result\"}");
}

void GPTLive::resetToolCalls()
{
    toolGeneration.fetch_add(1);
    ToolJob* job = nullptr;
    if(toolRequests) while(xQueueReceive(toolRequests, &job, 0) == pdTRUE){
        free(job);
        pendingToolJobs.fetch_sub(1);
    }
    toolCalls.clear();
    backendResponses.clear();
}

bool GPTLive::sendToolResult(const char* callId, const char* output)
{
    SpiRamJsonDocument doc(12 * 1024);
    doc["type"] = "response.item.create";
    doc["item"]["type"] = "function_call_output";
    doc["item"]["call_id"] = callId;
    doc["item"]["output"] = output;
    String json;
    if(doc.overflowed()) return false;
    serializeJson(doc, json);
    return webSocket.sendTXT(json);
}

void GPTLive::handleBackendEvent()
{
    if(sessionState.load() != SessionState::Active || !recordingRequested.load() || sessionFailed) return;
    JsonObject event = msgDoc["event"];
    String type = event["type"] | "";
    if(type == "error"){
        Serial.printf("[GPTLive] Backend error: %s\n", event["message"] | "unknown");
        resetToolCalls();
        sessionFailed = true;
        return;
    }
    String delegationId = msgDoc["delegation_id"] | "";
    String responseId = event["response_id"] | "";
    if(responseId.isEmpty()) responseId = event["response"]["id"] | "";
    if(type != "response.created" && type != "response.output_item.done" &&
       type != "response.completed" && type != "response.failed" && type != "response.incomplete" &&
       type != "response.cancelled") return;
    if(type == "response.output_item.done"){
        const char* existingId = event["item"]["call_id"] | "";
        for(const auto& call : toolCalls){
            if(call.callId != existingId) continue;
            if(call.delegationId != delegationId || (!responseId.isEmpty() && call.responseId != responseId))
                sessionFailed = true;
            return;
        }
    }
    // Responses item events need not carry response_id. response.created supplies
    // the association for subsequent nested events of the same delegation.
    if(responseId.isEmpty() && type != "response.created"){
        for(auto it = backendResponses.rbegin(); it != backendResponses.rend(); ++it){
            if(it->delegationId == delegationId){ responseId = it->responseId; break; }
        }
    }
    if(responseId.isEmpty() || delegationId.isEmpty() || responseId.length() >= 128 || delegationId.length() >= 128){
        sessionFailed = true; return;
    }
    BackendState* backend = nullptr;
    for(auto& state : backendResponses)
        if(state.responseId == responseId && state.delegationId == delegationId) backend = &state;
    if(!backend){
        if(backendResponses.size() >= MAX_TOOL_CALLS){ sessionFailed = true; return; }
        BackendState state;
        state.responseId = responseId;
        state.delegationId = delegationId;
        backendResponses.push_back(state);
        backend = &backendResponses.back();
    }
    if(type == "response.completed"){ backend->completed = true; return; }
    if(type == "response.failed" || type == "response.incomplete" || type == "response.cancelled"){
        backend->failed = true;
        // Do not retry possibly executed actions after a backend failure.
        resetToolCalls();
        sessionFailed = true;
        return;
    }
    if(type != "response.output_item.done" || strcmp(event["item"]["type"] | "", "function_call")) return;
    JsonObject item = event["item"];
    const char* callId = item["call_id"] | "";
    const char* name = item["name"] | "";
    const char* arguments = item["arguments"] | "";
    if(!*callId || strlen(callId) >= sizeof(ToolJob::callId)){ sessionFailed = true; return; }
    if(toolCalls.size() >= MAX_TOOL_CALLS){ sessionFailed = true; return; }
    ToolCallState call;
    call.callId = callId;
    call.responseId = responseId;
    call.delegationId = delegationId;
    toolCalls.push_back(call);
    auto reject = [this, callId](const char* reason) {
        const uint32_t generation = toolGeneration.load();
        const bool sent = sendToolResult(callId, reason);
        if(generation != toolGeneration.load()) return;
        for(auto& state : toolCalls) if(state.callId == callId) state.returned = sent;
        if(!sent) sessionFailed = true;
    };
    if(strlen(name) >= sizeof(ToolJob::name) || strlen(arguments) >= sizeof(ToolJob::arguments)){
        reject("{\"error\":\"Function request exceeds size limit\"}"); return;
    }
    ToolJob* job = static_cast<ToolJob*>(heap_caps_calloc(1, sizeof(ToolJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if(!job){ reject("{\"error\":\"Function allocation failed\"}"); return; }
    job->generation = toolGeneration.load();
    strlcpy(job->callId, callId, sizeof(job->callId));
    strlcpy(job->name, name, sizeof(job->name));
    strlcpy(job->arguments, arguments, sizeof(job->arguments));
    pendingToolJobs.fetch_add(1);
    if(xQueueSend(toolRequests, &job, 0) != pdTRUE){
        pendingToolJobs.fetch_sub(1);
        free(job);
        reject("{\"error\":\"Function queue is full\"}");
    }
}

void GPTLive::processToolResults()
{
    ToolJob* job = nullptr;
    while(toolResults && xQueueReceive(toolResults, &job, 0) == pdTRUE){
        if(job->generation == toolGeneration.load() && sessionState.load() == SessionState::Active &&
           recordingRequested.load() && !sessionFailed){
            bool pending = false;
            for(const auto& call : toolCalls)
                if(call.callId == job->callId && !call.returned) pending = true;
            if(pending){
                const bool sent = sendToolResult(job->callId, job->output);
                if(job->generation == toolGeneration.load()){
                    for(auto& call : toolCalls) if(call.callId == job->callId) call.returned = sent;
                    if(!sent) sessionFailed = true;
                }
            }
        }
        free(job);
    }
    if(sessionFailed || sessionState.load() != SessionState::Active || !recordingRequested.load()) return;
    // Live continuation is session-wide: all pending results must precede a single create.
    bool continueBackend = false;
    for(const auto& call : toolCalls){
        if(!call.returned) return;
        for(const auto& backend : backendResponses){
            if(call.responseId != backend.responseId || call.delegationId != backend.delegationId || backend.continued) continue;
            if(!backend.completed || backend.failed) return;
            continueBackend = true;
        }
    }
    if(continueBackend){
        for(auto& backend : backendResponses) if(backend.completed) backend.continued = true;
        if(!webSocket.sendTXT("{\"type\":\"response.create\"}")) sessionFailed = true;
    }
}
#endif  // GPTLIVE_FULL_DUPLEX
