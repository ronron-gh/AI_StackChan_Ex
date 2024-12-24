#ifndef _SERVO_CUSTOM_H
#define _SERVO_CUSTOM_H

#include <Stackchan_servo.h>

class ServoCustom : public StackchanSERVO {
public:
    ServoCustom(){};
    void moveToOrigin();
    void moveToGaze(int gazeX, int gazeY);

};

#endif  //_SERVO_CUSTOM_H