#if defined(REALTIME_API)

#include <Arduino.h>
#include <deque>
#include <SD.h>
#include <SPIFFS.h>
#include "mod/ModManager.h"
#include "RealtimeAiMod.h"
#include <Avatar.h>
#include "Robot.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
//#include "driver/PlayMP3.h"
#include <WiFiClientSecure.h>
#include "Scheduler.h"
#include "MySchedule.h"
#include "SDUtil.h"



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

  pRtLLM = (RealtimeChatGPT*)robot->llm;

  //servo_home = false;

#if 0
  if(!isOffline){
    //スケジューラ設定
    init_schedule();
  }

  if(robot->m_config.getExConfig().llm.type == LLM_TYPE_CHATGPT){
    // Function Call関連の設定
    init_func_call_settings(robot->m_config);
  }
#endif
}


void RealtimeAiMod::init(void)
{
  //avatar.setSpeechText("Realtime AI");
  avatar.set_isSubWindowEnable(true);
}

void RealtimeAiMod::pause(void)
{
  avatar.set_isSubWindowEnable(false);
}


void RealtimeAiMod::update(int page_no)
{

}

void RealtimeAiMod::btnA_pressed(void)
{

}


void RealtimeAiMod::btnB_longPressed(void)
{

}

void RealtimeAiMod::btnC_pressed(void)
{

}

void RealtimeAiMod::display_touched(int16_t x, int16_t y)
{
  if (box_stt.contain(x, y))
  {
    sw_tone();
    pRtLLM->startRealtimeRecord();
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
    //sw_tone();
  }

}

void RealtimeAiMod::idle(void)
{
  pRtLLM->webSocketProcess();

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

  // Alarm
  //
  if(xAlarmTimer != NULL){
    TickType_t xRemainingTime;

    /* Query the period of the timer that expires. */
    xRemainingTime = xTimerGetExpiryTime( xAlarmTimer ) - xTaskGetTickCount();
    avatarText = "Alarm countdown: " + String(xRemainingTime / 1000);
    avatar.set_isSubWindowEnable(true);
    avatar.updateSubWindowTxt(avatarText, 0, 0, 200, 50);
  }
  else{
    avatar.set_isSubWindowEnable(false);
  }

  if (alarmTimerCallbacked) {
    alarmTimerCallbacked = false;
    avatar.set_isSubWindowEnable(false);
    alarm_tone();
  }

#if 0 
  //スケジューラ処理
  if(!isOffline){
    run_schedule();
  }
#endif

}


#endif //REALTIME_API