#ifndef _HEAD_PET_DETECT_H
#define _HEAD_PET_DETECT_H

#include <Arduino.h>

// CoreS3 内蔵 IMU の加速度センサで「頭が小刻みに揺れる」を検出する。
// loop() から headPetDetected() を毎周回呼び出すと、検出が立っていれば
// true を返してフラグをクリアする（消費型）。

// タスクを起動（M5.begin() より後で呼ぶこと）
void invokeHeadPetDetectTask();

// 検出フラグの取得（true を一度返すと自動でクリアされる）
bool headPetDetected();

// サーボ動作などで誤検出を避けたい間、検出を抑制する
void headPetMaskFor(uint32_t ms);

#endif  // _HEAD_PET_DETECT_H
