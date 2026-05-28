#include "ServoCustom.h"

void ServoCustom::moveToOrigin(){
  moveXY(_init_param.servo[AXIS_X].start_degree, _init_param.servo[AXIS_Y].start_degree, 1000);
}

void ServoCustom::moveTo(int degX, int degY){
  moveXY(_init_param.servo[AXIS_X].start_degree + degX, _init_param.servo[AXIS_Y].start_degree + degY, 1000);
}

void ServoCustom::moveTo(int degX, int degY, uint32_t millis_for_move){
  moveXY(_init_param.servo[AXIS_X].start_degree + degX, _init_param.servo[AXIS_Y].start_degree + degY, millis_for_move);
}

void ServoCustom::fillLeds(uint8_t r, uint8_t g, uint8_t b, uint8_t count) {
  if (!_ioexpander) return;
  _ioexpander->setLedCount(count);
  for (uint8_t i = 0; i < count; i++) {
    _ioexpander->setLedColor(i, r, g, b);
  }
  _ioexpander->refreshLeds();
}

void ServoCustom::clearLeds() {
  fillLeds(0, 0, 0);
}
