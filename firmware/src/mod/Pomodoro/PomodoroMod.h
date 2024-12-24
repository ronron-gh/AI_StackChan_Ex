#ifndef _POMODORO_MOD_H
#define _POMODORO_MOD_H

#include <arduino.h>
#include "mod/ModBase.h"

#define POMODORO_TIMER_25MIN    (25*60*1000)
#define POMODORO_TIMER_5MIN    (5*60*1000)

typedef enum {
    READY,
    TIMER25,
    TIMER5,
} PomodoroStatus;

class PomodoroMod: public ModBase{
private:
    box_t box_servo;
    box_t box_stt;
    box_t box_BtnA;
    box_t box_BtnC;

    PomodoroStatus status;
    String avatarText;
    TimerHandle_t xTimer;
    bool isOffline;
    bool isSilentMode;
public:
    PomodoroMod(bool _isOffline = false);

    void init(void);
    void pause(void);
    void btnA_pressed(void);
    void btnB_pressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void idle(void);

private :
    void notification(String text, String mp3name);
};


#endif  //_POMODORO_MOD_H