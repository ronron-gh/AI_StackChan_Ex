#pragma once

#include "TTSBase.h"

class OpenAITTS: public TTSBase{
private:

public:
    OpenAITTS(tts_param_t param) : TTSBase(param) {};
    void stream(String text);
};
