
#if defined(USE_LLM_MODULE)

#include <ArduinoJson.h>
#include "ModuleLLMTTS.h"


ModuleLLMTTS::ModuleLLMTTS(){
  isOfflineService = true;  //オフラインで使用可能とする
}


void ModuleLLMTTS::stream(String text){
  int start = 0;
  int end = 0;
  String remain_text = text;
  String sub_text = text;
  
  while(1){    
    /* ピリオド・カンマで区切りながらTTSに渡す */

    end = remain_text.indexOf(',');
    if(end < 0){
      end = remain_text.indexOf('.');
    }

    if(end > 0){
      //sub_text = text.substring(start, end);
      sub_text = remain_text.substring(0, end);
      //start = end + 1;
      //remain_text = text.substring(start);
      remain_text = remain_text.substring(end + 1);
    }
    else{
      //sub_text = text.substring(start);
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