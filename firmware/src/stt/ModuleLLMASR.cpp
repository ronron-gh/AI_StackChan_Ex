
#if defined(USE_LLM_MODULE)

#include <ArduinoJson.h>
#include "ModuleLLMASR.h"


ModuleLLMASR::ModuleLLMASR(){

}


String ModuleLLMASR::speech_to_text(){
  return wait_for_asr_result();
}


#endif //USE_LLM_MODULE