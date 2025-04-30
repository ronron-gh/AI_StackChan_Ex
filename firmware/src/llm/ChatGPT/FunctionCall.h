#ifndef _FUNCTION_CALL_H
#define _FUNCTION_CALL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "StackchanExConfig.h" 

#define APP_DATA_PATH   "/app/AiStackChanEx/"
#define FNAME_NOTEPAD   "notepad.txt"
#define FNAME_BUS_TIMETABLE             "bus_timetable.txt"
#define FNAME_BUS_TIMETABLE_HOLIDAY     "bus_timetable_holiday.txt"
#define FNAME_BUS_TIMETABLE_SAT         "bus_timetable_sat.txt"
#define FNAME_ALARM_MP3 "alarm.mp3"

/*
 *  各種MCPの有効化
 */
#define MCP_GOOGLE_CALENDAR
#define MCP_BRAVE_SEARCH

extern String json_ChatString;
extern TimerHandle_t xAlarmTimer;
extern String note;

extern bool register_wakeword_required;
extern bool wakeword_enable_required;
extern bool alarmTimerCallbacked;

void init_func_call_settings(StackchanExConfig& system_config);
String exec_calledFunc(DynamicJsonDocument& doc, String* calledFunc);

//
// Functions for Function Calling
//
String timer(int32_t time, const char* action);
String timer_change(int32_t time);

#endif //_FUNCTION_CALL_H