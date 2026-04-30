#ifndef _QR_DISPLAY_MOD_H
#define _QR_DISPLAY_MOD_H

#include <arduino.h>
#include "mod/ModBase.h"


class QRdisplayMod: public ModBase{
private:
    box_t box_BtnA;
    box_t box_BtnB;
    box_t box_BtnC;
    box_t box_BtnUA;
    box_t box_BtnUC;

    bool isFaceDetected;

public:
    QRdisplayMod(void);
    void init(void);
    void pause(void);
    void update(void);
    void btnA_pressed(void);
    void btnB_pressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void idle(void);

    bool displayQR(bool isDisplay);

};

#endif  //_QR_DISPLAY_MOD_H