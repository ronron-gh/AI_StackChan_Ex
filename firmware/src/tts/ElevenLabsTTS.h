#pragma once

#include "TTSBase.h"

class ElevenLabsTTS: public TTSBase{
private:

public:
    ElevenLabsTTS(tts_param_t param) : TTSBase(param) {};
    void stream(String text);
};


