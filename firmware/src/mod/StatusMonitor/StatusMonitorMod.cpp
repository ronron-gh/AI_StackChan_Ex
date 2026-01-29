#include <Arduino.h>
#include "mod/ModManager.h"
#include "StatusMonitorMod.h"
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

StatusMonitorMod::StatusMonitorMod(void)
{
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);
  box_BtnUA.setupBox(0, 0, 80, 60);
  box_BtnUC.setupBox(240, 0, 80, 60);
  current_page_no = 0;
}

void StatusMonitorMod::init(void)
{
  update(current_page_no);
  avatar.set_isSubWindowEnable(true);
}


void StatusMonitorMod::pause(void)
{
  avatar.set_isSubWindowEnable(false);
}

void StatusMonitorMod::update(int page_no)
{
  String str = "";
  char tmp[256];

  str += "<- prev                 next ->\n";

  if(page_no == 0){
    byte mac[6];
    esp_efuse_mac_get_default(mac);

    str += "======== System status ========\n";
    sprintf(tmp, "Wifi:\n  IP addr:%s\n  MAC addr:%02x:%02x:%02x:%02x:%02x:%02x\n",
                    WiFi.localIP().toString().c_str(),
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
    str += tmp;
    sprintf(tmp, "Heap largest free block:\n  DMA:%d\n  SPIRAM:%d\n",
                    heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
                    heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) );
    str += tmp;
    sprintf(tmp, "Prompt buffer usage:\n  %d / %d [byte]\n",
                    robot->llm->chat_doc.memoryUsage(),
                    robot->llm->chat_doc.capacity() );
    str += tmp;
    sprintf(tmp, "Battery level:  %d %%\n", M5.Power.getBatteryLevel());
    str += tmp;
  }  
  if(page_no == 1){
    str += "===== Function call info  =====\n";
    str += "Reminder:\n";
    for(int i=0; i<MAX_SCHED_NUM; i++){
      ScheduleBase *sched = get_schedule(i);
      if(sched != nullptr){
          if (sched->get_sched_type() == SCHED_REMINDER) {  
              ScheduleReminder *sched_rem = (ScheduleReminder*)sched;
              str += " " + sched_rem->get_time_string() + " " + sched_rem->get_remind_string() + "\n";
          }
      }
    }
  }
  else if(page_no == 2){
    str += "===== Function call info  =====\n";
    str += "Memo:\n";
    str += note;
  }

  //M5.Display.print(str);
  avatar.updateSubWindowTxt(str);
}


void StatusMonitorMod::btnA_pressed(void)
{
  current_page_no--;
  if(current_page_no < 0) current_page_no = MAX_PAGE_NO - 1;
  update(current_page_no);
}

void StatusMonitorMod::btnC_pressed(void)
{
  current_page_no++;
  if(current_page_no >= MAX_PAGE_NO) current_page_no = 0;
  update(current_page_no);
}

void StatusMonitorMod::display_touched(int16_t x, int16_t y)
{

  if (box_BtnA.contain(x, y))
  {

  }

  if (box_BtnC.contain(x, y))
  {
    //sw_tone();
  }

  if (box_BtnUA.contain(x, y))
  {
    btnA_pressed();
  }

  if (box_BtnUC.contain(x, y))
  {
    btnC_pressed();
  }
}


void StatusMonitorMod::idle(void)
{
  update(current_page_no);
}
