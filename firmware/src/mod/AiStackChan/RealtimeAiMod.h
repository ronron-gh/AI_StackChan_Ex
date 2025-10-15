#if defined(REALTIME_API)

#ifndef _REALTIME_AI_MOD_H
#define _REALTIME_AI_MOD_H

#include <Arduino.h>
#include "mod/ModBase.h"
#include "llm/ChatGPT/RealtimeChatGPT.h"

class RealtimeAiMod: public ModBase{
private:
    box_t box_servo;
    box_t box_stt;
    box_t box_BtnA;
    box_t box_BtnC;

    String avatarText;
    bool isOffline;

    RealtimeChatGPT* pRtLLM;

    // for TTS
    String ttsText;
public:
    RealtimeAiMod(bool _isOffline);

    void init(void);
    void pause(void);
    void update(int page_no);
    void btnA_pressed(void);
    void btnB_longPressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void idle(void);
};


#endif  //_REALTIME_AI_MOD_H

#endif  //REALTIME_API