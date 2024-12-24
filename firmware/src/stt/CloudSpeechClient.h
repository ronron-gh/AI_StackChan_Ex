#ifndef _CLOUDSPEECHCLIENT_H
#define _CLOUDSPEECHCLIENT_H
#include "STTBase.h"
#include <WiFiClientSecure.h>
#include "driver/Audio.h"

enum Authentication {
  USE_ACCESSTOKEN,
  USE_APIKEY
};

class CloudSpeechClient : public STTBase{
  WiFiClientSecure client;
  void PrintHttpBody2(Audio* audio);
  
public:
  CloudSpeechClient(stt_param_t param);
  ~CloudSpeechClient();
  String Transcribe(Audio* audio);
  virtual String speech_to_text();
};

#endif // _CLOUDSPEECHCLIENT_H

