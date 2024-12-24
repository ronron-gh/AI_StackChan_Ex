#ifndef _STT_BASE_H
#define _STT_BASE_H

#include <Arduino.h>

struct stt_param_t
{
  String api_key;
};


class STTBase{
protected:
    stt_param_t param;
public:
    STTBase() {};
    STTBase(stt_param_t param) : param{param} {};
    virtual String speech_to_text() = 0;

};



#endif //_STT_BASE_H