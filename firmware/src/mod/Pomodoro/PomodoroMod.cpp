#include <Arduino.h>
#include <deque>
#include <SD.h>
#include <SPIFFS.h>
#include "mod/ModManager.h"
#include "PomodoroMod.h"
#include <Avatar.h>
#include "Robot.h"
//#include "driver/WakeWord.h"
#include "driver/PlayMP3.h"
#include <WiFiClientSecure.h>

#include "driver/AudioWhisper.h"       //speechToText
#include "stt/Whisper.h"               //speechToText
#include "driver/Audio.h"              //speechToText
#include "stt/CloudSpeechClient.h"     //speechToText
#include "rootCA/rootCACertificate.h"  //speechToText
#include "rootCA/rootCAgoogle.h"       //speechToText
#include "driver/Audio.h"              //speechToText

#include "llm/ChatGPT/FunctionCall.h"  //APP_DATA_PATHのため

using namespace m5avatar;


/// 外部参照 ///
extern Avatar avatar;
extern bool servo_home;
extern bool wakeword_is_enable;
extern void sw_tone();
///////////////

static bool pomodoroTimerCallbacked = false;


void pomodoroTimerCallback(TimerHandle_t _xTimer){
  pomodoroTimerCallbacked = true;

}


PomodoroMod::PomodoroMod(bool _isOffline)
  : isOffline{_isOffline},
    isSilentMode{true}
{
  box_servo.setupBox(80, 120, 80, 80);
  box_stt.setupBox(0, 0, M5.Display.width(), 60);
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);

}


void PomodoroMod::init(void)
{
  avatar.setSpeechText("Pomodoro Timer");
  delay(1000);
  xTimer = NULL;
  status = READY;
  avatar.setSpeechFont(&fonts::efontJA_12);
}

void PomodoroMod::pause(void)
{
  if((status == TIMER25) || (status == TIMER5)){
    xTimerStop(xTimer, 0);
    xTimerDelete(xTimer, 0);
    xTimer = NULL;
    status = READY;
  }

  avatar.setSpeechFont(&fonts::efontJA_16);
  avatar.setSpeechText("");
  //avatar_stop();
}


void PomodoroMod::btnA_pressed(void)
{
  sw_tone();
  if(status == READY){
    xTimer = xTimerCreate("Timer", POMODORO_TIMER_25MIN, pdFALSE, 0, pomodoroTimerCallback);
    if(xTimer != NULL){
      xTimerStart(xTimer, 0);
      status = TIMER25;
      avatar.setSpeechFont(&fonts::efontJA_16);
    }
    else{
      avatar.setExpression(Expression::Sad);
      avatar.setSpeechText("タイマー設定失敗");
      delay(2000);
      avatar.setSpeechText("");
      avatar.setExpression(Expression::Neutral);
    }
  }
  else if((status == TIMER25) || (status == TIMER5)){
    xTimerStop(xTimer, 0);
    xTimerDelete(xTimer, 0);
    init();
  }
}


void PomodoroMod::btnB_pressed(void)
{
  //MP3再生テスト
  avatar.setSpeechText("音声テスト");
  if(!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    Serial.println("Failed to mount SD Card.");
    return ;
  }
  notification("集中時間終了です", "pomodoro1.mp3");
  notification("休憩時間終了です", "pomodoro2.mp3");
  avatar.setSpeechText("");
}

void PomodoroMod::btnC_pressed(void)
{
  isSilentMode = !isSilentMode;
  if(isSilentMode){
    avatar.setSpeechText("無音モード");
  }
  else{
    avatar.setSpeechText("無音モード解除");
  }
  delay(3000);
}

void PomodoroMod::display_touched(int16_t x, int16_t y)
{
  if (box_stt.contain(x, y))
  {
    sw_tone();
  }

  if (box_servo.contain(x, y))
  {
    //servo_home = !servo_home;
    sw_tone();
  }

  if (box_BtnA.contain(x, y))
  {
    btnA_pressed();
  }

  if (box_BtnC.contain(x, y))
  {
    btnC_pressed();
  }

}

void PomodoroMod::idle(void)
{
  if(xTimer != NULL){
    if(xTimerIsTimerActive( xTimer ) != pdFALSE){
      TickType_t xRemainingTime;
      String phase;
      if(status == TIMER25){
        phase = String("集中 ");
        avatar.setExpression(Expression::Neutral);
      }
      else if(status == TIMER5){
        phase = String("休憩 ");
        avatar.setExpression(Expression::Sleepy);
      }

      xRemainingTime = xTimerGetExpiryTime( xTimer ) - xTaskGetTickCount();
      avatarText = phase + "残り" + String(xRemainingTime / 1000 / 60 + 1) + "分";
      avatar.setSpeechText(avatarText.c_str());
    }

  }
  else{
    avatar.setSpeechText("Btn A:スタート/リセット");
  }


  if (pomodoroTimerCallbacked) {
    pomodoroTimerCallbacked = false;
    if(status == TIMER25){
      notification("集中時間終了です", "pomodoro1.mp3");
      xTimer = xTimerCreate("Timer", POMODORO_TIMER_5MIN, pdFALSE, 0, pomodoroTimerCallback);
      if(xTimer != NULL){
        xTimerStart(xTimer, 0);
        status = TIMER25;
      }
      else{
        avatar.setExpression(Expression::Sad);
        avatar.setSpeechText("タイマー設定失敗");
        delay(2000);
        avatar.setSpeechText("");
        avatar.setExpression(Expression::Neutral);
      }
      status = TIMER5;
    }
    else if(status == TIMER5){
      notification("休憩時間終了です", "pomodoro2.mp3");
      xTimer = xTimerCreate("Timer", POMODORO_TIMER_25MIN, pdFALSE, 0, pomodoroTimerCallback);
      if(xTimer != NULL){
        xTimerStart(xTimer, 0);
        status = TIMER25;
      }
      else{
        avatar.setExpression(Expression::Sad);
        avatar.setSpeechText("タイマー設定失敗");
        delay(2000);
        avatar.setSpeechText("");
        avatar.setExpression(Expression::Neutral);
      }
      status = TIMER25;
    }
  }

}


void PomodoroMod::notification(String text, String mp3name)
{
  avatar.setSpeechText(text.c_str());
  
  if(isSilentMode){
    //無音モードならテキストのみ
    avatar.setExpression(Expression::Happy);
    servo_home = false;
    delay(5000);
    avatar.setExpression(Expression::Neutral);
    servo_home = true;
  }
  else{
    //mp3があれば再生、なければtts
    String fname = String(APP_DATA_PATH) + mp3name;
    bool result = playMP3SD(fname.c_str());
    if(!result){
      //mp3がなければtts
      if(!isOffline){
        robot->speech("集中時間終了です。5分間休憩してください。");
      }
      else{
        //オフラインモードならttsも無し（テキストのみ）
        avatar.setExpression(Expression::Happy);
        servo_home = false;
        delay(5000);
        avatar.setExpression(Expression::Neutral);
        servo_home = true;
      }
    }

  }

}