#if defined(USE_LLM_MODULE)
#pragma once

#include <M5Unified.h>
#include "TTSBase.h"
#include "driver/ModuleLLM.h"

class ModuleLLMTTS: public TTSBase{
private:

public:
    ModuleLLMTTS();
    void stream(String text);
    //int getLevel();
};


#endif //USE_LLM_MODULE
