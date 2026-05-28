#ifndef _SERVO_CUSTOM_H
#define _SERVO_CUSTOM_H

#include <Stackchan_servo.h>

class ServoCustom : public StackchanSERVO {
public:
    ServoCustom(){};
    void moveToOrigin();
    void moveTo(int degX, int degY);
    void moveTo(int degX, int degY, uint32_t millis_for_move);

    // M5StackChan サーボベース搭載の WS2812C RGB LED (PY32 IOExpander 経由)
    void fillLeds(uint8_t r, uint8_t g, uint8_t b, uint8_t count = 12);
    void clearLeds();

};

#endif  //_SERVO_CUSTOM_H
