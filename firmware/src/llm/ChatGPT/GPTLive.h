#pragma once
#ifdef GPTLIVE_FULL_DUPLEX
#include "../RealtimeLLMBase.h"
#include "driver/CoreS3FullDuplexAudio.h"
#include <atomic>

class GPTLive : public RealtimeLLMBase {
public:
    explicit GPTLive(llm_param_t param);
    void chat(String text, const char* base64_buf = nullptr) override {};
    String& buildInputAudioJson(String& jsonBuf, String& base64) override;
    void load_role() override;
    void webSocketProcess() override;
    void startRealtimeRecord() override;
    void stopRealtimeRecord() override;
    bool isRealtimeRecording() override { return recordingRequested.load(); }
    int getAudioLevel() override { return audio.getLevel(); }
    void suspendWebSocketLoopTask() override;
    void resumeWebSocketLoopTask() override;

private:
    enum class SessionState { Disconnected, Idle, Starting, Active, Closing };
    static constexpr uint32_t SESSION_START_TIMEOUT_MS = 15000;
    static constexpr uint32_t SESSION_CLOSE_TIMEOUT_MS = 15000;
    static constexpr size_t DECODE_BYTES = 100 * 1024;
    std::atomic<SessionState> sessionState{SessionState::Disconnected};
    std::atomic<bool> recordingRequested{false}, paused{false}, pauseAck{false};
    uint32_t sessionStateAt = 0;
    String role, userInfo, systemRole;
    String sessionId;
    double usageSeconds = 0;
    bool sessionFailed = false;
    CoreS3FullDuplexAudio audio;
    CoreS3FullDuplexAudio::InputFrame input;
    uint8_t* decoded = nullptr;
    uint32_t sentSamples = 0, sentLogAt = 0;

    void onWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void onTransportConnected();
    void onTransportDisconnected();
    bool handleSessionEvent(const String& type);
    bool sendSessionStart();
    bool processSessionLifecycle(bool stopping);
    void closeSession();
    void enqueueAudioDelta(const String& delta);
    void stopAudio();
    void stopSessionAudio();
};
#endif  // GPTLIVE_FULL_DUPLEX
