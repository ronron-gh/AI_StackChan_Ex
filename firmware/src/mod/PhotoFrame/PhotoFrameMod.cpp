#include <Arduino.h>
#include <deque>
#include <SD.h>
#include "SDUtil.h"
#include "mod/ModManager.h"
#include "PhotoFrameMod.h"
#include <Avatar.h>
#include "Robot.h"
//#include "driver/WakeWord.h"
#include "driver/PlayMP3.h"
#include <WiFiClientSecure.h>
#include "llm/ChatGPT/FunctionCall.h"  //APP_DATA_PATHのため

using namespace m5avatar;


/// 外部参照 ///
extern Avatar avatar;
extern void sw_tone();

///////////////

std::deque<String> photoList;
static bool photoFrameTimerCallbacked = false;

void photoFrameTimerCallback(TimerHandle_t _xTimer){
  photoFrameTimerCallbacked = true;
}

PhotoFrameMod::PhotoFrameMod(bool _isOffline)
  : isOffline{_isOffline}
{
  box_servo.setupBox(80, 120, 80, 80);
  box_stt.setupBox(0, 0, M5.Display.width(), 60);
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);

}


void PhotoFrameMod::init(void)
{
  avatar.setSpeechText("Photo Frame");
  delay(1000);

  String dir = String(APP_DATA_PATH) + "photo";
  photoRoot = SD.open(dir.c_str());
  createPhotoList(photoRoot);
  updatePhoto();
  xTimer = NULL;
}

void PhotoFrameMod::pause(void)
{
  if(xTimer != NULL){
    xTimerStop(xTimer, 0);
    xTimerDelete(xTimer, 0);
    xTimer = NULL;
  }
  photoList.clear();
  avatar.set_isSubWindowEnable(false);
  avatar.setSpeechText("");

}


void PhotoFrameMod::btnA_pressed(void)
{
  updatePhoto();
}


void PhotoFrameMod::btnB_pressed(void)
{

}

void PhotoFrameMod::btnC_pressed(void)
{
  //sw_tone();
  Serial.println("Start timer");
  avatar.set_isSubWindowEnable(false);
  avatar.setSpeechText("スライドショー開始");
  delay(1000);
  avatar.set_isSubWindowEnable(true);

  if(xTimer == NULL){
    xTimer = xTimerCreate("Timer", 60 * 1000, pdFALSE, 0, photoFrameTimerCallback);
  }

  if(xTimer != NULL){
    xTimerStart(xTimer, 0);
  }
}

void PhotoFrameMod::display_touched(int16_t x, int16_t y)
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

void PhotoFrameMod::idle(void)
{

  if (photoFrameTimerCallbacked) {
    photoFrameTimerCallbacked = false;
    updatePhoto();
    xTimerStart(xTimer, 0);
  }
}


String PhotoFrameMod::getNextPhoto(){
  String fname = photoList[0];
  photoList.pop_front();
  photoList.push_back(fname);
  return fname;
}

void PhotoFrameMod::createPhotoList(File dir) {
  Serial.println("Creating photo list");
  while(true) {
    File entry = dir.openNextFile();
    if (! entry) {
      Serial.println("no more files");
      return;
    }

    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println(" (this is a directory)");
    } else {
      photoList.push_back(String(entry.name()));
      Serial.println(" (added to list)");
    }
  }
}

void PhotoFrameMod::updatePhoto(){
  String fname = String(APP_DATA_PATH) + "photo/" + getNextPhoto();
  Serial.printf("Next photo : %s\n", fname.c_str());
  avatar.set_isSubWindowEnable(false);
  avatar.setSpeechText("ロード中...");
  if(!avatar.updateSubWindowJpg(fname)){
    avatar.setSpeechText("ロード失敗");
  }
  else{
    avatar.setSpeechText("");
    avatar.set_isSubWindowEnable(true);
  }
}