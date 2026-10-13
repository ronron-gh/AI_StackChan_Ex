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
    uint8_t* audioRing = nullptr;
    uint8_t* audioDecode = nullptr;
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
    void resetAudioPlayback();
    void processAudioPlayback();
#endif
};


#endif  //_GPT_LIVE_H

#endif  //REALTIME_API