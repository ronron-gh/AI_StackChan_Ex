#pragma once
#ifdef GPTLIVE_FULL_DUPLEX
#if !defined(ARDUINO_M5STACK_CORES3) || !defined(REALTIME_API) || defined(REALTIME_API_WITH_TTS) || defined(USE_AUDIO_MODULE) || defined(ENABLE_WAKEWORD)
#error GPTLIVE_FULL_DUPLEX requires CoreS3 internal audio, REALTIME_API, and no TTS/WakeWord/Audio Module
#endif
#include <Arduino.h>
#include <atomic>
#include <freertos/queue.h>
#include <freertos/semphr.h>

class CoreS3FullDuplexAudio {
public:
    static constexpr size_t FRAME_SAMPLES = 480;
    static constexpr uint32_t SAMPLE_RATE = 24000;
    struct InputFrame {
        uint32_t generation;
        int16_t pcm[FRAME_SAMPLES];
    };
    // begin/end are called by the same WebSocket task that owns mutexAudio.
    bool begin();
    void end();
    bool isRunning() const { return running.load(); }
    bool hasFailed() const { return failed.load(); }
    void setCapture(bool enabled);
    bool read(InputFrame& frame);
    bool enqueue(const uint8_t* pcm, size_t bytes);
    void setVolume(uint8_t volume) { volume_.store(volume); }
    int getLevel() const { return outputLevel.load(); }
    void logStats();
private:
    static constexpr size_t RING_BYTES = 512 * 1024;
    uint8_t* ring = nullptr;
    size_t readPos = 0, writePos = 0, used = 0;
    SemaphoreHandle_t ringMutex = nullptr;
    QueueHandle_t inputQueue = nullptr;
    TaskHandle_t rxTask = nullptr, txTask = nullptr;
    bool ownsAudio = false, installed = false;
    bool restoreMic = false, restoreSpeaker = false;
    std::atomic<bool> running{false}, failed{false}, capture{false};
    std::atomic<bool> rxDone{true}, txDone{true};
    std::atomic<uint32_t> generation{0};
    std::atomic<uint32_t> rxSamples{0}, txSamples{0}, rxDropped{0}, txUnderruns{0};
    std::atomic<int> outputLevel{0}, inputLevel{0};
    std::atomic<uint8_t> volume_{64};
    uint32_t statsAt = 0;
    static void rxEntry(void* arg);
    static void txEntry(void* arg);
    void receiveLoop();
    void transmitLoop();
    bool initCodecs();
    void disableCodecs();
};
#endif
