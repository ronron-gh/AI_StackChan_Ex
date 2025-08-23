
#if defined(USE_LLM_MODULE)

#include <ArduinoJson.h>
#include "ModuleLLMTTS.h"


ModuleLLMTTS::ModuleLLMTTS(){
  isOfflineService = true;  //オフラインで使用可能とする
}


void ModuleLLMTTS::stream(String text){
  int end = 0;
  int delimiter_size = 1;
  String remain_text = text;
  String sub_text = text;
  
  while(1){    
    /* ピリオド・カンマで区切りながらTTSに渡す */

    end = remain_text.indexOf(',');
    if(end < 0){
      end = remain_text.indexOf('.');
    }
    if(end < 0){
      end = remain_text.indexOf("。");
      delimiter_size = strlen("。");
    }
    if(end < 0){
      end = remain_text.indexOf("、");
      delimiter_size = strlen("、");
    }

    if(end > 0){
      sub_text = remain_text.substring(0, end);
      remain_text = remain_text.substring(end + delimiter_size);
    }
    else{
      sub_text = remain_text;
      remain_text = "";
    }

    Serial.printf("Sub text: %s\n", sub_text.c_str());
    Serial.printf("Remain text: %s\n", remain_text.c_str());

    /* Push text to TTS module and wait inference result */
    module_llm.tts.inference(tts_work_id, sub_text.c_str(), 10000);

    if(remain_text == ""){
      break;
    }

  }
}


#endif //USE_LLM_MODULE