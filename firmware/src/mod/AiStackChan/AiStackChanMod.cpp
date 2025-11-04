#include <Arduino.h>
#include <deque>
#include <SD.h>
#include <SPIFFS.h>
#include "mod/ModManager.h"
#include "AiStackChanMod.h"
#include <Avatar.h>
#include "Robot.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "driver/PlayMP3.h"
#include "driver/WakeWord.h"
#include "driver/ModuleLLM.h"
#include <WiFiClientSecure.h>
#include "MailClient.h"
#include "Scheduler.h"
#include "MySchedule.h"
#include "SDUtil.h"
#if defined( ENABLE_CAMERA )
#include "driver/Camera.h"
#endif
#include "driver/AudioWhisper.h"       //speechToText
#include "stt/Whisper.h"               //speechToText
#include "driver/Audio.h"              //speechToText
#include "stt/CloudSpeechClient.h"     //speechToText
#include "rootCA/rootCACertificate.h"  //speechToText
#include "rootCA/rootCAgoogle.h"       //speechToText
#include "driver/Audio.h"              //speechToText

using namespace m5avatar;

#if defined(ENABLE_WAKEWORD)
bool wakeword_is_enable = false;
#endif

/// 外部参照 ///
extern Avatar avatar;
extern bool servo_home;
//extern bool wakeword_is_enable;
extern void sw_tone();
extern void alarm_tone();
///////////////


static void report_batt_level(){
  char buff[100];
  int level = M5.Power.getBatteryLevel();
#if defined(ENABLE_WAKEWORD)
  mode = 0;
#endif
  if(M5.Power.isCharging())
    sprintf(buff,"充電中、バッテリーのレベルは%d％です。",level);
  else
    sprintf(buff,"バッテリーのレベルは%d％です。",level);
  avatar.setExpression(Expression::Happy);
#if defined(ENABLE_WAKEWORD)
  mode = 0; 
#endif
  robot->speech(String(buff));
  delay(1000);
  avatar.setExpression(Expression::Neutral);
}


static void STT_ChatGPT(const char *base64_buf = NULL) {
  bool prev_servo_home = servo_home;
#ifdef USE_SERVO
  servo_home = true;
#endif

  avatar.setExpression(Expression::Happy);
  avatar.setSpeechText("御用でしょうか？");

  String ret = robot->listen();
  avatar.setSpeechText("");

#ifdef USE_SERVO
  //servo_home = prev_servo_home;
  servo_home = false;
#endif
  Serial.println("音声認識終了");
  Serial.println("音声認識結果");
  if(ret != "") {
    Serial.println(ret);
    robot->chat(ret, base64_buf);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);
    servo_home = true;
  } else {
    Serial.println("音声認識失敗");
    avatar.setExpression(Expression::Sad);
    avatar.setSpeechText("聞き取れませんでした");
    delay(2000);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);
    servo_home = true;
  } 
}



AiStackChanMod::AiStackChanMod(bool _isOffline)
  : isOffline{_isOffline}
{
  box_servo.setupBox(80, 120, 80, 80);
#if defined(ENABLE_CAMERA)
  box_stt.setupBox(107, 0, M5.Display.width()-107, 80);
  box_subWindow.setupBox(0, 0, 107, 80);
#else
  box_stt.setupBox(0, 0, M5.Display.width(), 60);
#endif
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);

  //SDカードのMP3ファイル（アラーム用）をSPIFFSにコピーする（SDカードだと音が途切れ途切れになるため）。
  //すでにSPIFFSにファイルがあればコピーはしない。強制的にコピー（上書き）したい場合は第2引数をtrueにする。
  //String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
  //copySDFileToSPIFFS(fname.c_str(), false);

  if(!isOffline){
    //メール受信設定
    imap_init();
    //スケジューラ設定
    init_schedule();
  }

  if(robot->m_config.getExConfig().llm.type == LLM_TYPE_CHATGPT){
    // Function Call関連の設定
    init_func_call_settings(robot->m_config);
  }

  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    // Nothing to initialize here
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    wakeword_init();
#endif
  }

}


void AiStackChanMod::init(void)
{
  avatar.setSpeechText("AI Stack-chan");
#if defined(ENABLE_CAMERA)
  if(isSubWindowON){
    avatar.set_isSubWindowEnable(true);
  }
#endif
}

void AiStackChanMod::pause(void)
{
#if defined(ENABLE_CAMERA)
  if(isSubWindowON){
    avatar.set_isSubWindowEnable(false);
  }
#endif
}


void AiStackChanMod::update(int page_no)
{

}

void AiStackChanMod::btnA_pressed(void)
{
#if defined(ENABLE_WAKEWORD)
  if(mode >= 0){
    sw_tone();
    if(mode == 0){
      avatar.setSpeechText("ウェイクワード有効");
      mode = 1;
      wakeword_is_enable = true;
    } else {
      avatar.setSpeechText("ウェイクワード無効");
      mode = 0;
      wakeword_is_enable = false;
    }
    delay(1000);
    avatar.setSpeechText("");
  }
#endif
}


void AiStackChanMod::btnB_longPressed(void)
{
#if defined(ENABLE_WAKEWORD)
  M5.Mic.end();
  M5.Speaker.tone(1000, 100);
  delay(500);
  M5.Speaker.tone(600, 100);
  delay(1000);
  M5.Speaker.end();
  M5.Mic.begin();
  wakeword_is_enable = false; //wakeword 無効
  mode = -1;
#ifdef USE_SERVO
    servo_home = true;
    delay(500);
#endif
  avatar.setSpeechText("ウェイクワード登録開始");
#endif
}

void AiStackChanMod::btnC_pressed(void)
{
  sw_tone();
  report_batt_level();
}

void AiStackChanMod::display_touched(int16_t x, int16_t y)
{
  if (box_stt.contain(x, y))
  {
    sw_tone();
#if defined(ENABLE_CAMERA)
    avatar.set_isSubWindowEnable(false);
    if(isSubWindowON){
      String base64;
      bool ret = camera_capture_base64(base64);
      STT_ChatGPT(base64.c_str());
    }
    else{
      STT_ChatGPT();
    }
    avatar.set_isSubWindowEnable(isSubWindowON);
#else
    STT_ChatGPT();
#endif
  }
#ifdef USE_SERVO
  if (box_servo.contain(x, y))
  {
    //servo_home = !servo_home;
    sw_tone();
  }
#endif
  if (box_BtnA.contain(x, y))
  {
#if defined(ENABLE_CAMERA)
    isSilentMode = !isSilentMode;
    if(isSilentMode){
      avatar.setSpeechText("サイレントモード");
    }
    else{
      avatar.setSpeechText("サイレントモード解除");
    }
    delay(2000);
    avatar.setSpeechText("");
#else
    sw_tone();
#endif
  }
  if (box_BtnC.contain(x, y))
  {
    //sw_tone();
  }
#if defined(ENABLE_CAMERA)
  if (box_subWindow.contain(x, y))
  {
    isSubWindowON = !isSubWindowON;
    avatar.set_isSubWindowEnable(isSubWindowON);
  }
#endif //ENABLE_CAMERA

}

void AiStackChanMod::idle(void)
{

  /// Face detect ///
#if defined(ENABLE_CAMERA)
  //顔が検出されれば音声認識を開始。
  bool isFaceDetected;
  isFaceDetected = camera_capture_and_face_detect();
  if(!isSilentMode){

#if defined(ENABLE_FACE_DETECT)
    if(isFaceDetected){
      avatar.set_isSubWindowEnable(false);
      sw_tone();
      STT_ChatGPT();                              //音声認識

      // フレームバッファを読み捨てる（ｽﾀｯｸﾁｬﾝが応答した後に、過去のフレームで顔検出してしまうのを防ぐため）
      M5.In_I2C.release();
      camera_fb_t *fb = esp_camera_fb_get();
      esp_camera_fb_return(fb);
      avatar.set_isSubWindowEnable(isSubWindowON);
    }
#endif
  }
  else{
#if defined(ENABLE_FACE_DETECT)
    if(isFaceDetected){
      avatar.setExpression(Expression::Happy);
      //delay(2000);
      //avatar.setExpression(Expression::Neutral);
    }
    else{
      avatar.setExpression(Expression::Neutral);
    }
#endif
  }
#endif  //ENABLE_CAMERA

  //Wakeword
  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    if( check_kws_wakeup() ){
      sw_tone();
      STT_ChatGPT();
    }
#else
    Serial.println("ModuleLLM is not enabled. Please define USE_LLM_MODULE.");
    delay(1000);
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    if (mode == 0) { /* return; */ }
    else if (mode < 0) {
      int idx = wakeword_regist();
      if(idx >= 0){
        String text = String("ウェイクワード#") + String(idx) + String("登録終了");
        avatar.setSpeechText(text.c_str());
        delay(1000);
        avatar.setSpeechText("");
        //mode = 0;
        //wakeword_is_enable = false;
        mode = 1;
        wakeword_is_enable = true;

      }
    }
    else if (mode > 0 && wakeword_is_enable) {
      int idx = wakeword_compare();
      if( idx >= 0){
        Serial.println("wakeword_compare OK!");
        String text = String("ウェイクワード#") + String(idx);
        avatar.setSpeechText(text.c_str());
        sw_tone();
        STT_ChatGPT();
      }
    }

#if defined(ARDUINO_M5STACK_CORES3)
#if defined(USE_EXTENSION_FUNCTIONS)
    // Function Callからの要求でウェイクワード有効化
    if (wakeword_enable_required)
    {
      wakeword_enable_required = false;
      btnA_pressed();
    }

    // Function Callからの要求でウェイクワード登録
    if(register_wakeword_required)
    {
      register_wakeword_required = false;
      btnB_longPressed();
    }
#endif  //defined(USE_EXTENSION_FUNCTIONS)
#endif  //defined(ARDUINO_M5STACK_CORES3)
#endif  //ENABLE_WAKEWORD
  }

  /// Alarm ///
  if(xAlarmTimer != NULL){
    TickType_t xRemainingTime;

    /* Query the period of the timer that expires. */
    xRemainingTime = xTimerGetExpiryTime( xAlarmTimer ) - xTaskGetTickCount();
    avatarText = "残り" + String(xRemainingTime / 1000) + "秒";
    avatar.setSpeechText(avatarText.c_str());
  }

  if (alarmTimerCallbacked) {
    alarmTimerCallbacked = false;
#if defined(ENABLE_CAMERA)
    avatar.set_isSubWindowEnable(false);
#endif    
    if(!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    //if(!SPIFFS.begin(true)){
      Serial.println("Failed to mount SD card. Use alarm tone.");
      alarm_tone();
    }
    else{
      String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
      bool result = playMP3SD(fname.c_str());
      if(!result){
        alarm_tone();
      }
    }
#if defined(ENABLE_CAMERA)
    avatar.set_isSubWindowEnable(isSubWindowON);
#endif  
  }

  //スケジューラ処理
  if(!isOffline){
    run_schedule();
  }

}


