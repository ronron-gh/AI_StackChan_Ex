#include "HeadTouchSensor.h"

#if defined(ARDUINO_M5STACK_CORES3)
#include <M5Unified.h>
#include "drivers/Si12T/Si12T.h"
#endif

namespace HeadTouchSensor {

namespace {

#if defined(ARDUINO_M5STACK_CORES3)
enum class TouchState { Idle, Touched, Swiping };

si12t_handle_t si12tHandle = nullptr;
bool initialized = false;
bool initTried = false;
unsigned long lastSampleMs = 0;
TouchState touchState = TouchState::Idle;
int16_t initialPosition = 0;

constexpr uint32_t I2C_FREQ = 100000;
constexpr unsigned long SAMPLE_INTERVAL_MS = 50;
constexpr int16_t SWIPE_THRESHOLD = 40;

Gesture updateGesture(const uint8_t channel[3])
{
  const uint16_t total = channel[0] + channel[1] + channel[2];
  int16_t currentPosition = 0;
  if (total != 0) {
    const int32_t weighted = channel[0] * (-100) + channel[1] * 0 + channel[2] * 100;
    currentPosition = static_cast<int16_t>(weighted / total);
  }

  const uint8_t maxIntensity = max(channel[0], max(channel[1], channel[2]));
  const bool touched = maxIntensity >= 1;
  Gesture gesture = Gesture::None;

  switch (touchState) {
    case TouchState::Idle:
      if (touched) {
        touchState = TouchState::Touched;
        initialPosition = currentPosition;
        gesture = Gesture::Press;
      }
      break;

    case TouchState::Touched:
      if (!touched) {
        touchState = TouchState::Idle;
        gesture = Gesture::Release;
      } else {
        const int16_t delta = currentPosition - initialPosition;
        if (delta > SWIPE_THRESHOLD) {
          touchState = TouchState::Swiping;
          gesture = Gesture::SwipeForward;
        } else if (delta < -SWIPE_THRESHOLD) {
          touchState = TouchState::Swiping;
          gesture = Gesture::SwipeBackward;
        }
      }
      break;

    case TouchState::Swiping:
      if (!touched) {
        touchState = TouchState::Idle;
        gesture = Gesture::Release;
      }
      break;
  }

  return gesture;
}
#endif

}  // namespace

void begin(void)
{
#if defined(ARDUINO_M5STACK_CORES3)
  if (initTried) {
    return;
  }
  initTried = true;

  si12t_config_t config = {};
  config.i2c = &M5.In_I2C;
  config.dev_addr = SI12T_GND_ADDRESS;
  config.freq = I2C_FREQ;

  esp_err_t ret = si12t_init(&config, &si12tHandle);
  if (ret != ESP_OK) {
    Serial.printf("[HeadTouch] Si12T init failed: %s\n", esp_err_to_name(ret));
    return;
  }

  ret = si12t_setup(si12tHandle, SI12T_TYPE_LOW, SI12T_SENSITIVITY_LEVEL_3);
  if (ret != ESP_OK) {
    Serial.printf("[HeadTouch] Si12T setup failed: %s\n", esp_err_to_name(ret));
    si12t_delete(si12tHandle);
    si12tHandle = nullptr;
    return;
  }

  initialized = true;
  Serial.println("[HeadTouch] Si12T initialized");
#endif
}

Gesture update(void)
{
#if defined(ARDUINO_M5STACK_CORES3)
  begin();
  if (!initialized) {
    return Gesture::None;
  }

  const unsigned long now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) {
    return Gesture::None;
  }
  lastSampleMs = now;

  uint8_t touchResult = 0;
  uint8_t channel[3] = {0, 0, 0};
  esp_err_t ret = si12t_read_touch_result(si12tHandle, &touchResult);
  if (ret != ESP_OK) {
    Serial.printf("[HeadTouch] read failed: %s\n", esp_err_to_name(ret));
    return Gesture::None;
  }

  si12t_parse_touch_result_to(touchResult, channel);
  return updateGesture(channel);
#else
  return Gesture::None;
#endif
}

bool isAvailable(void)
{
#if defined(ARDUINO_M5STACK_CORES3)
  begin();
  return initialized;
#else
  return false;
#endif
}

bool isPetGesture(Gesture gesture)
{
  return gesture == Gesture::SwipeForward || gesture == Gesture::SwipeBackward;
}

const char *gestureName(Gesture gesture)
{
  switch (gesture) {
    case Gesture::Press:
      return "Press";
    case Gesture::Release:
      return "Release";
    case Gesture::SwipeForward:
      return "SwipeForward";
    case Gesture::SwipeBackward:
      return "SwipeBackward";
    case Gesture::None:
    default:
      return "None";
  }
}

}  // namespace HeadTouchSensor
