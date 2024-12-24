#ifndef _STATUS_MONITOR_MOD_H
#define _STATUS_MONITOR_MOD_H

#include <arduino.h>
#include "mod/ModBase.h"


class StatusMonitorMod: public ModBase{
private:
    box_t box_BtnA;
    box_t box_BtnC;
    box_t box_BtnUA;
    box_t box_BtnUC;
    const int MAX_PAGE_NO = 3;
    int current_page_no;
public:
    StatusMonitorMod(void);
    void init(void);
    void pause(void);
    void update(int page_no);
    void btnA_pressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void idle(void);
};

#endif  //_STATUS_MONITOR_MOD_H