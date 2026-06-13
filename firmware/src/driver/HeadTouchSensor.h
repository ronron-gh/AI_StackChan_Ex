#ifndef _HEAD_TOUCH_SENSOR_H
#define _HEAD_TOUCH_SENSOR_H

#include <Arduino.h>

namespace HeadTouchSensor {

enum class Gesture {
  None,
  Press,
  Release,
  SwipeForward,
  SwipeBackward,
};

void begin(void);
Gesture update(void);
bool isAvailable(void);
bool isPetGesture(Gesture gesture);
const char *gestureName(Gesture gesture);

}  // namespace HeadTouchSensor

#endif  // _HEAD_TOUCH_SENSOR_H
