#ifndef _Whisper_H
#define _Whisper_H
#include "STTBase.h"
#include <WiFiClientSecure.h>
#include "driver/AudioWhisper.h"

class Whisper : public STTBase {
  WiFiClientSecure client;

public:
  Whisper(stt_param_t param);
  ~Whisper();
  String Transcribe(AudioWhisper* audio);
  virtual String speech_to_text();
};

#endif // _Whisper_H

