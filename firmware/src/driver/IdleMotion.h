#ifndef _IDLE_MOTION_H
#define _IDLE_MOTION_H

#include <Arduino.h>

// 公式 M5Stackchan の IdleMotionModifier を参考にしたアイドル動作。
// 4〜8 秒の間隔でサーボに小さな動きをつける。
// 会話中や頭撫で中は idle_motion_pause() で抑制する。

void idle_motion_init();
void idle_motion_tick();
void idle_motion_pause();
void idle_motion_resume();

#endif  // _IDLE_MOTION_H
