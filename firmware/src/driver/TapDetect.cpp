#if defined(ENABLE_TAP_DETECT)
#include <Arduino.h>
#include <M5Unified.h>
#include "TapDetect.h"


#define ACCEL_DIM   (3)
TaskHandle_t taskHandle_doubleTapDetect;
bool doubleTapDetected = false;
bool isDoubleTapDetectionRunning = true;
float firstAcc[ACCEL_DIM] = {0.0, 0.0, 0.0};
float detectedAcc[ACCEL_DIM] = {0.0, 0.0, 0.0};
float firstNorm = 0.0;

// ---------------------------
// 感度等の調整パラメータ
// ---------------------------
// タップを検出する加速度の閾値
const float TAP_DELTA_MIN = 0.4f;
const float TAP_DELTA_MAX = 0.9f;
// タップの回数(ダブルタップを検出したい場合は2とする)
const float TAP_COUNT = 2;
// ハイパスフィルタが安定するまでのカウント数
const float FIRST_NOISE_COUNT = 10;
// 2回目のタップが同じ方向からのタップかどうかを判定するためのコサイン類似度の閾値
//（1.0から-1.0の間で設定。1.0に近づくほど類似度が高い(判定は厳しくなる)。-1.0に近づくと真逆の方向）
const float COS_SIMILAR = 0.9;


// ---------------------------
// ダブルタップ検出タスク
// ---------------------------
void doubleTapDetectTask(void *arg) {
  Serial.println("Double tap detection task created");

  // 注意: M5.IMU.getAccel の API は環境により差があるため、
  // ビルドエラーが出た場合は M5.Imu.getAccel 等に置き換えてください。

  unsigned long first_tap_time = 0;
  int tap_count = 0;
  float accel[ACCEL_DIM] = {0.0, 0.0, 0.0};
  float gravity[ACCEL_DIM] = {0.0, 0.0, 0.0};
  float norm = 0.0;
  bool ok = false;
  int firstNoiseCount = 0;
  
  while(1){
    // try common IMU API (adjust if your M5 library uses a different name)
    #if defined(M5_IMU)
    ok = M5.IMU.getAccel(&ax, &ay, &az);
    #else
    // Fallback attempt (some versions use Imu)
    ok = M5.Imu.getAccel(&accel[0], &accel[1], &accel[2]);
    #endif

    if (ok) {
      unsigned long now = millis();
      if(now - first_tap_time > 1000){
        tap_count = 0;
        first_tap_time = 0;
      }

      // ハイパスフィルタで重力を除去
      //
      const float ALPHA = 0.8;
      // ローパスフィルタで重力値を抽出
      gravity[0] = ALPHA * gravity[0] + (1 - ALPHA) * accel[0];
      gravity[1] = ALPHA * gravity[1] + (1 - ALPHA) * accel[1];
      gravity[2] = ALPHA * gravity[2] + (1 - ALPHA) * accel[2];
      // 重力を除去
      accel[0] = accel[0] - gravity[0];
      accel[1] = accel[1] - gravity[1];
      accel[2] = accel[2] - gravity[2];
      // ベクトルの大きさを計算
      norm = sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);

      // ハイパスフィルタが安定するまで計算結果を破棄
      if(firstNoiseCount < FIRST_NOISE_COUNT){
        firstNoiseCount ++;
        delay(10);  // [ms]
        continue;
      }

      // 定期的に加速度値を出力（デバッグ用）
#if 0
      static unsigned long last_print = 0;
      if(now - last_print > 100) {
        Serial.printf("IMU ax=%.3f ay=%.3f az=%.3f norm=%.3f\n", accel[0], accel[1], accel[2], norm);
        last_print = now;
      }
#endif

      if ((TAP_DELTA_MIN < norm) && (norm < TAP_DELTA_MAX)) {
        // タップあり
        float cos = 0.0;

        if(tap_count == 0){
          // 1回目のタップ
          for(int i = 0; i < ACCEL_DIM; i++){
            firstAcc[i] = accel[i];
          }
          firstNorm = norm;
          first_tap_time = now;
          tap_count = 1;
        }else{
          // 2回目以降は同じ方向のタップのみカウントするためにコサイン類似度を計算
          float innPro = firstAcc[0] * accel[0] + firstAcc[1] * accel[1] + firstAcc[2] * accel[2];
          cos = innPro / (firstNorm * norm);
        }

        Serial.printf("Tap detected. ax=%.3f ay=%.3f az=%.3f norm=%.3f cos=%.3f\n", accel[0], accel[1], accel[2], norm, cos);
        if((300 < (now - first_tap_time)) && ((now - first_tap_time) < 1000)) {
          // 最初のタップから300ms以上700ms以内なら連続タップと判定
          //（300ms以上としたのは、振り回したときに同方向の加速度を連続検知してダブルタップと認識してしまうのを防ぐため）
          if(cos > COS_SIMILAR){  // コサイン類似度で同じ方向かを判定
            tap_count ++;
          }
        }
        Serial.printf("tap_count=%d\n", tap_count);
      }

      if (tap_count >= TAP_COUNT) {
        // ダブルタップ検出
        Serial.println("Double tap detected");
        tap_count = 0;
        doubleTapDetected = true;
        for(int i = 0; i < ACCEL_DIM; i++){
          detectedAcc[i] = firstAcc[i];
        }
        isDoubleTapDetectionRunning = false;
      }
    } else {
      Serial.println("IMU getAccel() returned false or not available");
    }

    // loopタスクから再開要求の通知が来るまで待機
    if(!isDoubleTapDetectionRunning){
      Serial.println("Waiting notify...");
      ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
      isDoubleTapDetectionRunning = true;
      firstNoiseCount = 0;
    }

    delay(10);  // [ms]
  }
}

// ---------------------------
// ダブルタップ検出タスクの起動
// ---------------------------
void invokeDoubleTapDetectTask(void)
{
  xTaskCreate(doubleTapDetectTask, /* Function to implement the task */
            "doubleTapDetectTask", /* Name of the task */
            3*1024,                /* Stack size in words */
            NULL,                  /* Task input parameter */
            2,                     /* Priority of the task */
            &taskHandle_doubleTapDetect);   /* Task handle. */
}

// ---------------------------
// ダブルタップ検出タスクの停止
// ---------------------------
void stopDoubleTapDetectTask(void)
{
  if(isDoubleTapDetectionRunning){
    isDoubleTapDetectionRunning = false;
  }
}

// ---------------------------
// ダブルタップ検出タスクの再開
// ---------------------------
void resumeDoubleTapDetectTask(void)
{
  if(!isDoubleTapDetectionRunning){
    xTaskNotifyGive(taskHandle_doubleTapDetect);
  }
}

#endif  //ENABLE_TAP_DETECT