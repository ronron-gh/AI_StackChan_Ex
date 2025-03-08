#if defined(USE_LLM_MODULE)
#pragma once

#include <M5Unified.h>
#include "STTBase.h"
#include "driver/ModuleLLM.h"

class ModuleLLMWhisper: public STTBase{
private:

public:
    ModuleLLMWhisper();
    virtual String speech_to_text();

};


#endif //USE_LLM_MODULE
