#include "GPTLiveFullDuplex.h"
#ifdef GPTLIVE_FULL_DUPLEX
#include <Avatar.h>
#include <base64.h>
#include <esp_heap_caps.h>
#include "libb64/cdecode.h"
#include "Robot.h"

extern m5avatar::Avatar avatar;
namespace {
#ifdef GPTLIVE_FULL_DUPLEX_TEST_TONE
constexpr bool diagnosticMode = true;
#else
constexpr bool diagnosticMode = false;
#endif
}

GPTLiveFullDuplex::GPTLiveFullDuplex(llm_param_t param) : GPTLive(param, false)
{
    decoded = (uint8_t*)heap_caps_malloc(DECODE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    rtRecSamplerate = CoreS3FullDuplexAudio::SAMPLE_RATE;
    rtRecLength = CoreS3FullDuplexAudio::FRAME_SAMPLES;
    if(!decoded) Serial.println("[FD] PSRAM decode allocation failed");
}

void GPTLiveFullDuplex::onAudioDisconnected()
{
    ready = false;
    requested.store(false);
    fault = false;
}

void GPTLiveFullDuplex::enqueueAudioDelta(const String& delta)
{
    if(!ready || fault || !recordingRequested.load() || sessionState.load() != SessionState::Active || !audio.isRunning()) return;
    const size_t encoded = delta.length();
    if(!encoded) return;
    if(!decoded || encoded % 4 || encoded / 4 * 3 > DECODE_BYTES){ fault = true; return; }
    size_t padding = (delta[encoded - 1] == '=') + (delta[encoded - 2] == '=');
    for(size_t i = 0; i < encoded - padding; ++i){
        const char c = delta[i];
        if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') || c == '+' || c == '/')){ fault = true; return; }
    }
    const size_t expected = encoded / 4 * 3 - padding;
    if(!expected || expected % sizeof(int16_t)){ fault = true; return; }
    base64_decodestate state;
    base64_init_decodestate(&state);
    const int bytes = base64_decode_block(delta.c_str(), encoded, (char*)decoded, &state);
    if(bytes != expected || !audio.enqueue(decoded, expected)) fault = true;
}

void GPTLiveFullDuplex::stopAudio()
{
    audio.end();
    RealtimeLLMBase::stopRealtimeRecord();
    speaking = false;
}

void GPTLiveFullDuplex::startRealtimeRecord()
{
    if(diagnosticMode) requested.store(true);
    else GPTLive::startRealtimeRecord();
}

void GPTLiveFullDuplex::stopRealtimeRecord()
{
    if(diagnosticMode) requested.store(false);
    else GPTLive::stopRealtimeRecord();
}

bool GPTLiveFullDuplex::isRealtimeRecording()
{
    return diagnosticMode ? requested.load() : GPTLive::isRealtimeRecording();
}

void GPTLiveFullDuplex::stopSessionAudio()
{
    stopAudio();
    ready = false;
    fault = false;
}

void GPTLiveFullDuplex::webSocketProcess()
{
    if(paused.load() && pauseAck.load()){
        delay(1);
        return;
    }
    if(diagnosticMode && paused.load()){
        requested.store(false);
        stopAudio();
        pauseAck.store(true);
        delay(1);
        return;
    }
    if(diagnosticMode){
        // Validate shared-clock RX/TX before introducing the network or API.
        if(audio.hasFailed()){
            requested.store(false);
            stopAudio();
        }
        if(requested.load() && !audio.isRunning()){
            if(!audio.begin()) requested.store(false);
        }else if(!requested.load() && audio.isRunning()){
            stopAudio();
        }
        audio.setVolume(robot->spk_volume);
        audio.logStats();
        avatar.setSpeechText(audio.isRunning() ? "I2S test: touch to stop" : "I2S test: please touch");
        delay(1);
        return;
    }
    const bool active = processSessionLifecycle(paused.load(), fault || audio.hasFailed());
    if(paused.load() && sessionState.load() == SessionState::Disconnected){
        pauseAck.store(true);
    }
    if(!active){ delay(1); return; }
    if(!audio.isRunning()){
        if(!decoded || !audio.begin()){
            Serial.println("[FD] Audio start failed; closing session");
            fault = true;
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
        if(!webSocket.sendTXT(buildInputAudioJson(json, encoded))){ fault = true; break; }
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

void GPTLiveFullDuplex::suspendWebSocketLoopTask()
{
    paused.store(true);
    while(!pauseAck.load()) delay(1);
    RealtimeLLMBase::suspendWebSocketLoopTask();
}

void GPTLiveFullDuplex::resumeWebSocketLoopTask()
{
    pauseAck.store(false);
    paused.store(false);
    RealtimeLLMBase::resumeWebSocketLoopTask();
}
#endif
