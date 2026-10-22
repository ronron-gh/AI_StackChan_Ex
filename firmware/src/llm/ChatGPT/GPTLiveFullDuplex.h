#pragma once
#ifdef GPTLIVE_FULL_DUPLEX
#include "GPTLive.h"
#include "driver/CoreS3FullDuplexAudio.h"

class GPTLiveFullDuplex : public GPTLive {
public:
    explicit GPTLiveFullDuplex(llm_param_t param);
    void webSocketProcess() override;
    void startRealtimeRecord() override;
    void stopRealtimeRecord() override;
    bool isRealtimeRecording() override;
    int getAudioLevel() override { return audio.getLevel(); }
    void suspendWebSocketLoopTask() override;
    void resumeWebSocketLoopTask() override;
    void enqueueAudioDelta(const String& delta) override;
    void onAudioDisconnected() override;
    void onAudioSessionStarted() override { ready = true; }
protected:
    void stopSessionAudio() override;
private:
    static constexpr size_t DECODE_BYTES = 100 * 1024;
    CoreS3FullDuplexAudio audio;
    CoreS3FullDuplexAudio::InputFrame input;
    uint8_t* decoded = nullptr;
    bool ready = false, fault = false;
    std::atomic<bool> requested{false}, paused{false}, pauseAck{false};
    uint32_t sentSamples = 0, sentLogAt = 0;
    void stopAudio();
};
#endif
