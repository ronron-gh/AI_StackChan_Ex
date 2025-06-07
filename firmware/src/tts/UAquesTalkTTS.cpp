
#if defined(USE_AQUESTALK)

#include <ArduinoJson.h>
#include "UAquesTalkTTS.h"


UAquesTalkTTS::UAquesTalkTTS(){
  isOfflineService = true;  //オフラインで使用可能とする
  
  // AquesTalkの初期化
  Serial.println("Initialize AquesTalk");
  if (int ret = TTS.createK()) { // 漢字テキスト使用
    if(ret==501){
      Serial.println("ERR:SD card not found");
    }else {
      Serial.println("ERR:TTS.createK()");
    }
    return;
  }
}


void UAquesTalkTTS::stream(String text){
  int ret;
  int end = 0;
  String remain_text = text;
  String sub_text = text;
  
  M5.Mic.end();
  M5.Speaker.begin();
  while(1){    
    /* ピリオド・カンマで区切りながらTTSに渡す */

    end = remain_text.indexOf("、");
    if(end < 0){
      end = remain_text.indexOf("。");
    }

    if(end > 0){
      sub_text = remain_text.substring(0, end);
      remain_text = remain_text.substring(end + strlen("、"));
    }
    else{
      sub_text = remain_text;
      remain_text = "";
    }

    Serial.printf("Sub text: %s\n", sub_text.c_str());
    Serial.printf("Remain text: %s\n", remain_text.c_str());

    /* Push text to TTS module and wait inference result */
    ret = TTS.playK(sub_text.c_str(), 100);
    if(ret){
      Serial.printf("ERR:TTS.playK()=&d\n", ret);
      break;
    }

    TTS.wait();

    if(remain_text == ""){
      break;
    }
  }
  M5.Speaker.end();
  M5.Mic.begin();
}

int UAquesTalkTTS::getLevel(){
  return TTS.getLevel();
}

#endif //USE_AQUESTALK