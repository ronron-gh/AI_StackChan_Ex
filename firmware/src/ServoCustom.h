#ifndef _SERVO_CUSTOM_H
#define _SERVO_CUSTOM_H

#include <Stackchan_servo.h>

class ServoCustom : public StackchanSERVO {
public:
    ServoCustom(){};
    void moveToOrigin();
    void moveTo(int degX, int degY);
    void moveTo(int degX, int degY, uint32_t millis_for_move);

};

#endif  //_SERVO_CUSTOM_H
