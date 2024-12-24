
#if defined(USE_AQUESTALK)

#include <ArduinoJson.h>
#include "UAquesTalkTTS.h"


UAquesTalkTTS::UAquesTalkTTS(){
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
  
  ret = TTS.playK(text.c_str(), 100);

  if(ret){
    Serial.printf("ERR:TTS.playK()=&d\n", ret);
  }
}

int UAquesTalkTTS::getLevel(){
  return TTS.getLevel();
}

#endif //USE_AQUESTALK