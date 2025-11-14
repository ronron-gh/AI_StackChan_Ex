#ifndef _ROBOT_H
#define _ROBOT_H

#include <Arduino.h>
//#include <Stackchan_servo.h>
#include "ServoCustom.h"
#include "StackchanExConfig.h"
#include "llm/LLMBase.h" 
#include "tts/TTSBase.h"
#include "stt/STTBase.h"
#include "driver/ModuleLLM.h"



class Robot{
private:

public:
    StackchanExConfig& m_config;
    LLMBase *llm;
    TTSBase *tts;
    STTBase *stt;
#if defined(USE_LLM_MODULE)
    module_llm_param_t module_llm_param;
#endif
    ServoCustom *servo;
    uint8_t spk_volume;

    Robot(StackchanExConfig& config);
    bool isAllOfflineService();
    void initLLM(StackchanExConfig& config);
    void initSTT(StackchanExConfig& config);
    void initTTS(StackchanExConfig& config);

    void speech(String text);
    String listen();
    void chat(String text, const char *base64_buf = NULL);

    // TTS非同期版
    //
    bool asyncPlaying;
    void invokeAsyncTtsStreamTask(void);
};

extern Robot* robot;

#endif //_ROBOT_H