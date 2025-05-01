#include <Arduino.h>
#include "mod/ModManager.h"
#include "VolumeSettingMod.h"
#include "Robot.h"
#include "Scheduler.h"
#include "MySchedule.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include <WiFiClientSecure.h>
#include <Avatar.h>

using namespace m5avatar;

/// 外部参照 ///
extern Avatar avatar;
extern void sw_tone();

///////////////

VolumeSettingMod::VolumeSettingMod(void)
{
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnB.setupBox(140, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);
  box_BtnUA.setupBox(0, 0, 80, 60);
  box_BtnUC.setupBox(240, 0, 80, 60);

}

void VolumeSettingMod::init(void)
{
  avatar.setSpeechText("Volume Setting");
  delay(1000);
  update();
  avatar.set_isSubWindowEnable(true);
}


void VolumeSettingMod::pause(void)
{
  avatar.set_isSubWindowEnable(false);
}

void VolumeSettingMod::update()
{
  String str = "";
  char tmp[256];

  avatar.setSpeechText("A:- B:Test C:+");

  str += "Volume: " + String(robot->spk_volume);

  //M5.Display.print(str);
  avatar.updateSubWindowTxt(str, 0, 0, 150, 50);
}


void VolumeSettingMod::btnA_pressed(void)
{
  if(robot->spk_volume >= 10){
    robot->spk_volume -= 10;
  }
  else{
    robot->spk_volume = 0;
  }
  M5.Speaker.setVolume(robot->spk_volume);
  sw_tone();
}

void VolumeSettingMod::btnB_pressed(void)
{
  robot->speech("volume test");
}

void VolumeSettingMod::btnC_pressed(void)
{
  if(robot->spk_volume <= 245){
    robot->spk_volume += 10;
  }
  else{
    robot->spk_volume = 255;
  }
  M5.Speaker.setVolume(robot->spk_volume);
  sw_tone();
}

void VolumeSettingMod::display_touched(int16_t x, int16_t y)
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


void VolumeSettingMod::idle(void)
{
  update();
}
