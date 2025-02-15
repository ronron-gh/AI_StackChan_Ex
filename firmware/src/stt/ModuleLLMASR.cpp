
#if defined(USE_LLM_MODULE)

#include <ArduinoJson.h>
#include "ModuleLLMASR.h"


ModuleLLMASR::ModuleLLMASR(){
  isOfflineService = true;  //オフラインで使用可能とする
}


String ModuleLLMASR::speech_to_text(){
  return wait_for_asr_result();
}


#endif //USE_LLM_MODULE