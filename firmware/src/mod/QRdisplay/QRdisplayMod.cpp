#include <Arduino.h>
#include "mod/ModManager.h"
#include "QRdisplayMod.h"
#include "Robot.h"
#include "Scheduler.h"
#include "MySchedule.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include <WiFiClientSecure.h>
#include <Avatar.h>
#include "driver/Camera.h"

using namespace m5avatar;

/// 外部参照 ///
extern Avatar avatar;
extern void sw_tone();
extern bool servo_home;
extern bool servo_hold;
///////////////

QRdisplayMod::QRdisplayMod(void)
  : isFaceDetected(false)
{
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnB.setupBox(140, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);
  box_BtnUA.setupBox(0, 0, 80, 60);
  box_BtnUC.setupBox(240, 0, 80, 60);

}

void QRdisplayMod::init(void)
{
  avatar.setSpeechText("QR Code Display");
  setCpuFrequencyMhz(80);                         // 動作周波数を落とす
  delay(1000);
  avatar.setSpeechText("");
  //update();
  servo_home = true;
  btnB_pressed();
}


void QRdisplayMod::pause(void)
{
  setCpuFrequencyMhz(240);                         // 動作周波数を元に戻す
  isFaceDetected = false;
  displayQR(false);
  avatar.set_isSubWindowEnable(false);
}

void QRdisplayMod::update()
{

}


void QRdisplayMod::btnA_pressed(void)
{

}

void QRdisplayMod::btnB_pressed(void)
{
  static bool qrShow = false;
  if(!qrShow){
    displayQR(true);
    qrShow = true;
  }else{
    displayQR(false);
    qrShow = false;
  }
}

void QRdisplayMod::btnC_pressed(void)
{

}

void QRdisplayMod::display_touched(int16_t x, int16_t y)
{

  if (box_BtnA.contain(x, y))
  {
    btnA_pressed();
  }

  if (box_BtnB.contain(x, y))
  {
    btnB_pressed();
  }

  if (box_BtnC.contain(x, y))
  {
    btnC_pressed();
  }

  if (box_BtnUA.contain(x, y))
  {

  }

  if (box_BtnUC.contain(x, y))
  {

  }
}


void QRdisplayMod::idle(void)
{
  update();

}


bool QRdisplayMod::displayQR(bool isDisplay)
{
  if(isDisplay){
    String url = String("https://example.com/");
    avatar.updateSubWindowQrcode(url, 78);
    avatar.setSpeechText("");
    avatar.set_isSubWindowEnable(true);
    avatar.setScale(0.7);
    avatar.setFaceOffsetY(-80);
  }
  else{
    avatar.set_isSubWindowEnable(false);
    avatar.setScale(1.0);
    avatar.setFaceOffsetY(0);
  }

  return true;
}

