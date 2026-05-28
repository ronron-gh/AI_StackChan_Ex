#ifndef _APP_LED_CONTROLLER_H
#define _APP_LED_CONTROLLER_H

#include <Arduino.h>
#include <functional>

// M5StackChan サーボベース搭載 WS2812C RGB LED の制御。
// PY32 IOExpander 経由のため、ハードウェアアクセスは callback で受け取る。
//
// 起動時:
//   LedController::init(
//     [](uint8_t r, uint8_t g, uint8_t b){ robot->servo->fillLeds(r,g,b); },
//     [](){ robot->servo->clearLeds(); }
//   );
//   LedController::start_watchdog_task();
//
// 機能:
//   - led_set(rgb): 固定色点灯。20 秒で自動消灯。
//   - led_breath(rgb): 呼吸アニメで点灯。60 秒で自動消灯。
//   - led_off(): 即時消灯。
//   - 別 FreeRTOS タスクで動く watchdog が呼吸アニメと auto-off を担当。

namespace LedController {
  using FillFn  = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
  using ClearFn = std::function<void()>;

  void init(FillFn fill, ClearFn clear);
  void start_watchdog_task();
}

void led_set(uint32_t rgb);
void led_breath(uint32_t rgb);
void led_off();

#endif  // _APP_LED_CONTROLLER_H
