#ifndef _STT_BASE_H
#define _STT_BASE_H

#include <Arduino.h>
#include "StackchanExConfig.h"

struct stt_param_t
{
  String api_key;
  stt_s stt_conf;
};


class STTBase{
protected:
    stt_param_t param;

public:
    bool isOfflineService;
    
    STTBase() {};
    STTBase(stt_param_t param) : param{param}, isOfflineService{false} {};
    virtual String speech_to_text() = 0;

};



#endif //_STT_BASE_H