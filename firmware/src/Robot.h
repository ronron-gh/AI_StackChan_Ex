#ifndef _ROBOT_H
#define _ROBOT_H

#include <Arduino.h>
//#include <Stackchan_servo.h>
#include "ServoCustom.h"
#include "StackchanExConfig.h"
#include "llm/LLMBase.h" 
#include "tts/TTSBase.h"
#include "stt/STTBase.h"



class Robot{
private:

public:
    StackchanExConfig& m_config;
    LLMBase *llm;
    TTSBase *tts;
    STTBase *stt;
    ServoCustom *servo;

    Robot(StackchanExConfig& config);
    void speech(String text);
    String listen();
    void chat(String text, const char *base64_buf = NULL);
    void chat_audio(const char *audio_base64);
};

extern Robot* robot;

#endif //_ROBOT_H