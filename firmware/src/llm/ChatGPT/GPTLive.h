#if defined(REALTIME_API)

#ifndef _GPT_LIVE_H
#define _GPT_LIVE_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "../ChatHistory.h"
#include "../RealtimeLLMBase.h"
#include "ChatGPT.h"
#include <WebSocketsClient.h>
#ifndef REALTIME_API_WITH_TTS
#include <atomic>
#endif


class GPTLive: public RealtimeLLMBase{
public:   //本当はprivateにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした
    MCPClient* mcpClient[LLM_N_MCP_SERVERS_MAX];
    FunctionCall* fnCall;
    
    String role;
    String userInfo;
    String systemRole;

public:
    GPTLive(llm_param_t param);

    virtual void chat(String text, const char *base64_buf = NULL) {};   //dummy
    virtual String& buildInputAudioJson(String& jsonBuf, String& base64);
    virtual void load_role();

#ifndef REALTIME_API_WITH_TTS
    void webSocketProcess() override;
    void startRealtimeRecord() override;
    void stopRealtimeRecord() override;
    bool isRealtimeRecording() override { return recordingRequested.load(); }
    int getAudioLevel() override { return playbackLevel.load(); }
    void suspendWebSocketLoopTask() override;
    void resumeWebSocketLoopTask() override;
    void enqueueAudioDelta(const String& delta);
    void onAudioDisconnected();
    void onAudioSessionStarted();

private:
    static constexpr size_t AUDIO_RING_BYTES = 512 * 1024;
    static constexpr size_t AUDIO_DECODE_BYTES = 100 * 1024;
    static constexpr size_t AUDIO_PLAY_BYTES = 8192;
    static constexpr uint32_t AUDIO_IDLE_MS = 150;
    static constexpr size_t AUDIO_FRAME_SAMPLES = 480;
    static constexpr size_t AUDIO_FRAME_BYTES = AUDIO_FRAME_SAMPLES * sizeof(int16_t);
    static constexpr size_t AUDIO_PREROLL_BYTES = 5 * AUDIO_FRAME_BYTES;
    static constexpr size_t AUDIO_SILENCE_SAMPLES = 9600;
    static constexpr uint32_t AUDIO_START_RMS = 300;
    static constexpr uint32_t AUDIO_CONTINUE_RMS = 150;
    static constexpr uint32_t AUDIO_RMS_LOG_MS = 2000;
    uint8_t* audioRing = nullptr;
    uint8_t* audioDecode = nullptr;
    uint8_t* audioGateBuffer = nullptr;
    size_t audioFrameUsed = 0;
    size_t audioPrerollWrite = 0;
    size_t audioPrerollUsed = 0;
    size_t audioSilentSamples = 0;
    bool audioSpeechActive = false;
    uint32_t audioLastPcmAt = 0;
    uint32_t audioRmsLogAt = 0;
    uint32_t audioRmsMin = UINT32_MAX;
    uint32_t audioRmsMax = 0;
    size_t audioRead = 0;
    size_t audioWrite = 0;
    size_t audioUsed = 0;
    size_t audioInFlight = 0;
    uint32_t audioIdleSince = 0;
    bool audioIdle = false;
    bool audioOwned = false;
    bool restoreMic = false;
    bool sessionReady = false;
    bool audioFailed = false;
    std::atomic<bool> recordingRequested{false};
    std::atomic<bool> pauseRequested{false};
    std::atomic<bool> pauseAcknowledged{false};
    std::atomic<int> playbackLevel{0};
    bool appendAudioPcm(const uint8_t* pcm, size_t bytes);
    void processAudioFrame(size_t bytes);
    void resetAudioGate();
    void resetAudioPlayback(bool resetGate = true);
    void processAudioPlayback();
#endif
};


#endif  //_GPT_LIVE_H

#endif  //REALTIME_API