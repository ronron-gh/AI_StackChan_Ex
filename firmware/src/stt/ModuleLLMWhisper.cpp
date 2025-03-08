
#if defined(USE_LLM_MODULE)

#include <ArduinoJson.h>
#include "ModuleLLMWhisper.h"


ModuleLLMWhisper::ModuleLLMWhisper(){
  isOfflineService = true;  //オフラインで使用可能とする
}


String ModuleLLMWhisper::speech_to_text(){
  return wait_for_whisper_result();
}


#endif //USE_LLM_MODULE