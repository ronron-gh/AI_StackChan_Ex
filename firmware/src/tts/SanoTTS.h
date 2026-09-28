#pragma once

#if defined(USE_SANOTTS)

#include <M5Unified.h>
#include "TTSBase.h"

class SanoTTS : public TTSBase {
public:
    SanoTTS();
    void stream(String text) override;
    int getLevel() override;
    bool isReady() const;

private:
    static void workerEntry(void *arg);
    void workerLoop();
    bool initialize();
    bool speak(const String& text);
#if !defined(SANOTTS_BUFFERED_PLAYBACK)
    bool speakSegment(const String& text);
    bool inferAndPlay(const int32_t *ids, int32_t count);
#endif

#if defined(SANOTTS_BUFFERED_PLAYBACK)
    struct LevelState {
        const uint16_t *envelope = nullptr;
        size_t count = 0;
        int64_t startUs = 0;
        int64_t endUs = 0;
    };
    void setLevelState(size_t slot, const uint16_t *envelope, size_t count,
                       int64_t startUs, int64_t endUs);
    void clearLevelState(size_t slot);
    LevelState levelStates_[2];
    portMUX_TYPE levelMux_ = portMUX_INITIALIZER_UNLOCKED;
#endif

    TaskHandle_t worker_ = nullptr;
    SemaphoreHandle_t requestMutex_ = nullptr;
    SemaphoreHandle_t requestReady_ = nullptr;
    SemaphoreHandle_t requestDone_ = nullptr;
    String requestText_;
    volatile bool ready_ = false;
    volatile int level_ = 0;
};

#endif
