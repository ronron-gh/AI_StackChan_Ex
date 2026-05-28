#include "LedController.h"
#include <Arduino.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

LedController::FillFn  s_fill  = nullptr;
LedController::ClearFn s_clear = nullptr;

// LED 自動消灯タイマー（TTS ハング時の保険）
volatile unsigned long g_led_off_at_ms = 0;
constexpr uint32_t LED_AUTO_OFF_MS = 20UL * 1000UL;  // 固定色: 20秒

// 呼吸アニメ用
volatile bool g_led_breathing = false;
volatile uint32_t g_led_breath_base_rgb = 0;
unsigned long g_led_breath_start_ms = 0;
constexpr uint32_t LED_BREATH_PERIOD_MS = 1500;  // 周期 1.5秒
constexpr uint32_t LED_BREATH_AUTO_OFF_MS = 60UL * 1000UL;  // 呼吸時: 60秒

void watchdog_task(void*) {
  Serial.println("LED watchdog task started");
  uint32_t heartbeat = 0;
  for (;;) {
    // breathing 中だけ細かく更新（240ms ≒ 1.5sec / 6step）
    // 非 breathing 時は I2C を叩かないよう 1 秒待機
    vTaskDelay(pdMS_TO_TICKS(g_led_breathing ? 240 : 1000));
    heartbeat++;

    unsigned long off_at = g_led_off_at_ms;
    if (off_at != 0 && millis() > off_at) {
      Serial.printf("LED auto-off triggered at %lu\n", millis());
      g_led_breathing = false;
      if (s_clear) s_clear();
      g_led_off_at_ms = 0;
      continue;
    }

    if (g_led_breathing && s_fill) {
      uint32_t base = g_led_breath_base_rgb;
      uint8_t br = (base >> 16) & 0xFF;
      uint8_t bg = (base >> 8) & 0xFF;
      uint8_t bb = base & 0xFF;
      float t = (float)((millis() - g_led_breath_start_ms) % LED_BREATH_PERIOD_MS)
                / (float)LED_BREATH_PERIOD_MS;
      float bright = 0.5f + 0.5f * sinf(t * 2.0f * 3.14159265f);
      uint8_t r = (uint8_t)(br * bright);
      uint8_t g = (uint8_t)(bg * bright);
      uint8_t b = (uint8_t)(bb * bright);
      s_fill(r, g, b);
    }

    if (heartbeat % 200 == 0) {
      Serial.printf("LED watchdog alive (breathing=%d, off_at=%lu, now=%lu)\n",
                    g_led_breathing ? 1 : 0, g_led_off_at_ms, millis());
    }
  }
}

}  // namespace

void LedController::init(FillFn fill, ClearFn clear) {
  s_fill = fill;
  s_clear = clear;
}

void LedController::start_watchdog_task() {
  xTaskCreate(watchdog_task, "led_watchdog", 4 * 1024, nullptr, 1, nullptr);
}

void led_set(uint32_t rgb) {
  if (!s_fill) return;
  g_led_breathing = false;
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;
  s_fill(r, g, b);
  g_led_off_at_ms = millis() + LED_AUTO_OFF_MS;
  Serial.printf("led_set(0x%06X), auto_off_at=%lu\n", rgb, g_led_off_at_ms);
}

void led_breath(uint32_t rgb) {
  if (!s_fill) return;
  g_led_breath_base_rgb = rgb;
  g_led_breath_start_ms = millis();
  g_led_breathing = true;
  g_led_off_at_ms = millis() + LED_BREATH_AUTO_OFF_MS;
  Serial.printf("led_breath(0x%06X)\n", rgb);
}

void led_off() {
  if (!s_clear) return;
  g_led_breathing = false;
  s_clear();
  g_led_off_at_ms = 0;
}
