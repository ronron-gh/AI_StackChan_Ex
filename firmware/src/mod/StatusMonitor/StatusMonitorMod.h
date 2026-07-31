#ifndef _STATUS_MONITOR_MOD_H
#define _STATUS_MONITOR_MOD_H

#include <Arduino.h>
#include "mod/ModBase.h"
#include <Avatar.h>


class StatusMonitorMod: public ModBase{
private:
    box_t box_BtnA;
    box_t box_BtnC;
    box_t box_TabSystem;
    box_t box_TabAiService;
    static const int TAB_COUNT = 2;
    int current_page_no;
    String buildSystemStatus();
    String buildAiServiceStatus();
    const char* getAiServiceName(int llm_type);
    void drawStatusMonitor(M5Canvas *spi, m5avatar::BoundingRect rect,
                            m5avatar::DrawContext *ctx);
    static void drawSubWindow(M5Canvas *spi, m5avatar::BoundingRect rect,
                              m5avatar::DrawContext *ctx, void *userData);
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
