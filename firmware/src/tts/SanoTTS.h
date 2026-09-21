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
    bool speakSegment(const String& text);
    bool inferAndPlay(const int32_t *ids, int32_t count);

    TaskHandle_t worker_ = nullptr;
    SemaphoreHandle_t requestMutex_ = nullptr;
    SemaphoreHandle_t requestReady_ = nullptr;
    SemaphoreHandle_t requestDone_ = nullptr;
    String requestText_;
    volatile bool ready_ = false;
    volatile int level_ = 0;
};

#endif
