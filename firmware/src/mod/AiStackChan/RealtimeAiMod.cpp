#if defined(REALTIME_API)

#include <Arduino.h>
#include <deque>
#include <SD.h>
#include <SPIFFS.h>
#include "mod/ModManager.h"
#include "RealtimeAiMod.h"
#include <Avatar.h>
#include "Robot.h"
#include "llm/ChatGPT/FunctionCall.h"
#include <WiFiClientSecure.h>
#include "Scheduler.h"
#include "MySchedule.h"
#include "share/SDUtil.h"

using namespace m5avatar;


/// 外部参照 ///
extern Avatar avatar;
extern bool servo_home;
extern void sw_tone();
extern void alarm_tone();
///////////////



RealtimeAiMod::RealtimeAiMod(bool _isOffline)
  : isOffline{_isOffline}
{
  box_servo.setupBox(80, 120, 80, 80);
  box_stt.setupBox(0, 0, M5.Display.width(), 60);
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);

  pRtLLM = (RealtimeLLMBase*)robot->llm;
  pRtLLM->invokeWebSocketLoopTask();

  //servo_home = false;

#if 0
  if(!isOffline){
    //スケジューラ設定
    init_schedule();
  }
#endif
}


void RealtimeAiMod::init(void)
{
  //avatar.setSpeechText("Realtime AI");
  avatar.set_isSubWindowEnable(true);
  pRtLLM->resumeWebSocketLoopTask();
}

void RealtimeAiMod::pause(void)
{
  avatar.set_isSubWindowEnable(false);
  pRtLLM->suspendWebSocketLoopTask();
}


void RealtimeAiMod::update(int page_no)
{

}

void RealtimeAiMod::btnA_pressed(void)
{
#if defined(ARDUINO_M5STACK_ATOMS3R)
  Serial.println("Btn A pressed");
  sw_tone();
  toggleRealtimeRecord();
#endif
}

void RealtimeAiMod::btnB_longPressed(void)
{

}

void RealtimeAiMod::btnC_pressed(void)
{
  static bool isQrDrawing = false;
  if(!isQrDrawing){
    avatar.setSpeechText("");
    String url = String("http://") + WiFi.localIP().toString();
    avatar.updateSubWindowQrcode(url);
    avatar.set_isSubWindowEnable(true);
    isQrDrawing = true;
  }else{
    avatar.set_isSubWindowEnable(false);
    isQrDrawing = false;
  }
}

void RealtimeAiMod::display_touched(int16_t x, int16_t y)
{
  if (box_stt.contain(x, y))
  {
    sw_tone();
    toggleRealtimeRecord();
  }
#ifdef USE_SERVO
  if (box_servo.contain(x, y))
  {
    sw_tone();
    servo_home = !servo_home;
  }
#endif
  if (box_BtnA.contain(x, y))
  {
    //sw_tone();
  }
  if (box_BtnC.contain(x, y))
  {
    btnC_pressed();
  }

}

void RealtimeAiMod::doubleTapped(float ax, float ay, float az)
{
  Serial.printf("Mod double tapped. ax=%.3f ay=%.3f az=%.3f\n", ax, ay, az);
#if defined(ARDUINO_M5STACK_ATOMS3R)
  sw_tone();
  toggleRealtimeRecord();
#endif
}


void RealtimeAiMod::idle(void)
{
  logHeadTouchSensor();

#ifdef REALTIME_API_WITH_TTS

  if(robot->asyncPlaying || (pRtLLM->getOutputTextQueueSize() != 0)){
    // 発話中
    pRtLLM->setSpeaking(true);
    servo_home = false;
    avatar.setExpression(Expression::Happy);
  }
  else{
    // 発話停止中かつキューにテキストがない場合はLLM機能に発話終了を通知
    pRtLLM->setSpeaking(false);
    servo_home = true;
    avatar.setExpression(Expression::Neutral);
  }

#endif  //REALTIME_API_WITH_TTS

  // Alarm (Function Calling)
  alarmEventHandler();

#if 0 
  //スケジューラ処理
  if(!isOffline){
    run_schedule();
  }
#endif

}

void RealtimeAiMod::alarmEventHandler()
{
  if(xAlarmTimer != NULL){
    TickType_t xRemainingTime;

    /* Query the period of the timer that expires. */
    xRemainingTime = xTimerGetExpiryTime( xAlarmTimer ) - xTaskGetTickCount();
    avatarText = "Alarm countdown: " + String(xRemainingTime / 1000);
    avatar.set_isSubWindowEnable(true);
    avatar.updateSubWindowTxt(avatarText, 0, 0, 200, 50);
  }

  if (alarmTimerCallbacked) {
    alarmTimerCallbacked = false;
    avatar.set_isSubWindowEnable(false);
    alarm_tone();
  }

  if (alarmTimerCanceled) {
    alarmTimerCanceled = false;
    avatar.set_isSubWindowEnable(false);
  }

}

void RealtimeAiMod::initHeadTouchSensor(void)
{
  if (headTouchInitTried) {
    return;
  }
  headTouchInitTried = true;

  si12t_config_t si12tConfig = {};
  si12tConfig.i2c = &M5.In_I2C;
  si12tConfig.dev_addr = SI12T_GND_ADDRESS;
  si12tConfig.freq = 100000;

  esp_err_t ret = si12t_init(&si12tConfig, &headTouchHandle);
  if (ret != ESP_OK) {
    Serial.printf("[HeadTouch] Si12T init failed: %s\n", esp_err_to_name(ret));
    return;
  }

  ret = si12t_setup(headTouchHandle, SI12T_TYPE_LOW, SI12T_SENSITIVITY_LEVEL_3);
  if (ret != ESP_OK) {
    Serial.printf("[HeadTouch] Si12T setup failed: %s\n", esp_err_to_name(ret));
    si12t_delete(headTouchHandle);
    headTouchHandle = nullptr;
    return;
  }

  headTouchInitialized = true;
  Serial.println("[HeadTouch] Si12T initialized");
}

void RealtimeAiMod::logHeadTouchSensor(void)
{
#if defined(ARDUINO_M5STACK_CORES3)
  initHeadTouchSensor();
  if (!headTouchInitialized) {
    return;
  }

  const unsigned long now = millis();
  if (now - lastHeadTouchSampleMs < 50) {
    return;
  }
  lastHeadTouchSampleMs = now;

  uint8_t touchResult = 0;
  uint8_t channel[3] = {0, 0, 0};
  esp_err_t ret = si12t_read_touch_result(headTouchHandle, &touchResult);
  if (ret != ESP_OK) {
    Serial.printf("[HeadTouch] read failed: %s\n", esp_err_to_name(ret));
    return;
  }

  si12t_parse_touch_result_to(touchResult, channel);
  int16_t position = 0;
  HeadTouchGesture gesture = updateHeadTouchGesture(channel, &position);
  if (gesture != HeadTouchGesture::None) {
    Serial.printf("[HeadTouch] gesture=%s raw=0x%02X ch0=%u ch1=%u ch2=%u pos=%d\n",
                  headTouchGestureName(gesture), touchResult, channel[0], channel[1], channel[2], position);
  }
#endif
}

RealtimeAiMod::HeadTouchGesture RealtimeAiMod::updateHeadTouchGesture(const uint8_t channel[3], int16_t *position)
{
  const uint16_t total = channel[0] + channel[1] + channel[2];
  int16_t currentPosition = 0;
  if (total != 0) {
    const int32_t weighted = channel[0] * (-100) + channel[1] * 0 + channel[2] * 100;
    currentPosition = static_cast<int16_t>(weighted / total);
  }
  if (position != nullptr) {
    *position = currentPosition;
  }

  const uint8_t maxIntensity = max(channel[0], max(channel[1], channel[2]));
  const bool touched = maxIntensity >= 1;
  HeadTouchGesture gesture = HeadTouchGesture::None;
  constexpr int16_t swipeThreshold = 40;

  switch (headTouchState) {
    case HeadTouchState::Idle:
      if (touched) {
        headTouchState = HeadTouchState::Touched;
        headTouchInitialPosition = currentPosition;
        gesture = HeadTouchGesture::Press;
      }
      break;

    case HeadTouchState::Touched:
      if (!touched) {
        headTouchState = HeadTouchState::Idle;
        gesture = HeadTouchGesture::Release;
      } else {
        const int16_t delta = currentPosition - headTouchInitialPosition;
        if (delta > swipeThreshold) {
          headTouchState = HeadTouchState::Swiping;
          gesture = HeadTouchGesture::SwipeForward;
        } else if (delta < -swipeThreshold) {
          headTouchState = HeadTouchState::Swiping;
          gesture = HeadTouchGesture::SwipeBackward;
        }
      }
      break;

    case HeadTouchState::Swiping:
      if (!touched) {
        headTouchState = HeadTouchState::Idle;
        gesture = HeadTouchGesture::Release;
      }
      break;
  }

  return gesture;
}

const char *RealtimeAiMod::headTouchGestureName(HeadTouchGesture gesture) const
{
  switch (gesture) {
    case HeadTouchGesture::Press:
      return "Press";
    case HeadTouchGesture::Release:
      return "Release";
    case HeadTouchGesture::SwipeForward:
      return "SwipeForward";
    case HeadTouchGesture::SwipeBackward:
      return "SwipeBackward";
    case HeadTouchGesture::None:
    default:
      return "None";
  }
}

bool RealtimeAiMod::isBusy(void)
{
  if(pRtLLM->isRealtimeRecording() || pRtLLM->isSpeaking()){
    return true;
  }else{
    return false;
  }
}

void RealtimeAiMod::toggleRealtimeRecord(void)
{
  if(pRtLLM->isRealtimeRecording()){
    pRtLLM->stopRealtimeRecord();
  }else{
    pRtLLM->startRealtimeRecord();
  }
}

#endif //REALTIME_API
