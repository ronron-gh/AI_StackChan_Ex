#include "IdleMotion.h"
#include <M5Unified.h>
#include <esp_random.h>

namespace {

constexpr uint32_t INTERVAL_MIN_MS = 4000;
constexpr uint32_t INTERVAL_MAX_MS = 8000;

IdleMotion::ServoMoveFn s_move_fn = nullptr;
IdleMotion::CanMoveFn   s_can_move_fn = nullptr;
uint32_t s_next_tick_ms = 0;
bool s_paused = false;

int random_int(int min_val, int max_val) {
  if (max_val <= min_val) return min_val;
  return min_val + (int)(esp_random() % (uint32_t)(max_val - min_val + 1));
}

void perform_idle_motion() {
  if (!s_move_fn) return;
  if (s_can_move_fn && !s_can_move_fn()) return;

  int action = random_int(0, 99);
  int x, y;
  uint32_t ms;

  if (action < 50) {
    // 随意見回し（広め）
    x  = random_int(-15, 15);
    y  = random_int(-12, 5);
    ms = (uint32_t)random_int(500, 1000);
    Serial.printf("[idle] look-around (%d, %d) %dms\n", x, y, (int)ms);
  } else if (action < 80) {
    // 微小な観察動作
    x  = random_int(-8, 8);
    y  = random_int(-6, 6);
    ms = (uint32_t)random_int(400, 800);
    Serial.printf("[idle] micro-move (%d, %d) %dms\n", x, y, (int)ms);
  } else if (action < 90) {
    // 素早く視線
    x  = random_int(-20, 20);
    y  = random_int(-5, 15);
    ms = (uint32_t)random_int(250, 500);
    Serial.printf("[idle] quick-glance (%d, %d) %dms\n", x, y, (int)ms);
  } else {
    // yaw 中央復帰
    x  = 0;
    y  = random_int(-5, 5);
    ms = (uint32_t)random_int(400, 800);
    Serial.printf("[idle] yaw-center (%d, %d) %dms\n", x, y, (int)ms);
  }

  s_move_fn(x, y, ms);
}

}  // namespace

void idle_motion_init(IdleMotion::ServoMoveFn move_fn,
                      IdleMotion::CanMoveFn  can_move_fn) {
  s_move_fn = move_fn;
  s_can_move_fn = can_move_fn;
  s_next_tick_ms = millis() + 1000;  // 起動から 1 秒後に最初の動き
}

void idle_motion_pause() {
  s_paused = true;
}

void idle_motion_resume() {
  if (s_paused) {
    s_paused = false;
    s_next_tick_ms = millis() + 500;
  }
}

void idle_motion_tick() {
  if (s_paused || !s_move_fn) return;
  uint32_t now = millis();
  if (now < s_next_tick_ms) return;

  // 他の動きで使用中なら 500ms 後にリトライ
  if (s_can_move_fn && !s_can_move_fn()) {
    s_next_tick_ms = now + 500;
    return;
  }

  perform_idle_motion();
  s_next_tick_ms = now + (uint32_t)random_int((int)INTERVAL_MIN_MS, (int)INTERVAL_MAX_MS);
}
