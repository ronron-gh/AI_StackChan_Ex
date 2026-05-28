#ifndef _IDLE_MOTION_H
#define _IDLE_MOTION_H

#include <Arduino.h>
#include <functional>

// 公式 M5Stackchan の IdleMotionModifier を参考にしたアイドル動作。
// 4〜8 秒の間隔でランダムな小さい動きをサーボに発火する。
//
// driver 層が高レベル（Robot）に依存しないよう、callback を注入する形式：
//
//   IdleMotion::ServoMoveFn move = [](int x, int y, uint32_t ms) {
//     robot->servo->moveTo(x, y, ms);
//   };
//   IdleMotion::CanMoveFn can_move = []() { return servo_home; };
//   idle_motion_init(move, can_move);
//
// loop() から idle_motion_tick() を毎周回呼ぶ。
namespace IdleMotion {
  using ServoMoveFn = std::function<void(int x, int y, uint32_t ms)>;
  using CanMoveFn   = std::function<bool()>;  // true なら動かしてよい状態
}

void idle_motion_init(IdleMotion::ServoMoveFn move_fn,
                      IdleMotion::CanMoveFn  can_move_fn);
void idle_motion_tick();
void idle_motion_pause();
void idle_motion_resume();

#endif  // _IDLE_MOTION_H
