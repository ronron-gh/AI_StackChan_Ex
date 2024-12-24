#if defined(USE_AQUESTALK)
#pragma once

#include <M5Unified.h>
#include "TTSBase.h"
#include "AquesTalkTTS.h" // AquesTalk-ESP32のラッパークラス

class UAquesTalkTTS: public TTSBase{
private:

public:
    UAquesTalkTTS();
    void stream(String text);
    int getLevel();
};


#endif //USE_AQUESTALK
