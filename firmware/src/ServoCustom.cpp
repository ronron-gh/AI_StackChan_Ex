#include "ServoCustom.h"

void ServoCustom::moveToOrigin(){
  moveXY(_init_param.servo[AXIS_X].start_degree, _init_param.servo[AXIS_Y].start_degree, 1000);
}

void ServoCustom::moveToGaze(int gazeX, int gazeY){
  moveXY(_init_param.servo[AXIS_X].start_degree + gazeX, _init_param.servo[AXIS_Y].start_degree + gazeY, 1000);
}