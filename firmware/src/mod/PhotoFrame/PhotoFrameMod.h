#ifndef _PHOTO_FRAME_MOD_H
#define _PHOTO_FRAME_MOD_H

#include <arduino.h>
#include "mod/ModBase.h"


class PhotoFrameMod: public ModBase{
private:
    box_t box_servo;
    box_t box_stt;
    box_t box_BtnA;
    box_t box_BtnC;

    File photoRoot;

    TimerHandle_t xTimer;

    String avatarText;
    bool isOffline;

public:
    PhotoFrameMod(bool _isOffline = false);

    void init(void);
    void pause(void);
    void btnA_pressed(void);
    void btnB_pressed(void);
    void btnC_pressed(void);
    void display_touched(int16_t x, int16_t y);
    void idle(void);

    void createPhotoList(File dir);
    String getNextPhoto();
    void updatePhoto();
};


#endif  //_PHOTO_FRAME_MOD_H