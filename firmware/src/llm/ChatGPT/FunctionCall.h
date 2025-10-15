#ifndef _FUNCTION_CALL_H
#define _FUNCTION_CALL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "StackchanExConfig.h" 
#include "MCPClient.h"

//#define USE_EXTENSION_FUNCTIONS

#define APP_DATA_PATH   "/app/AiStackChanEx/"
#define FNAME_NOTEPAD   "notepad.txt"
#define FNAME_BUS_TIMETABLE             "bus_timetable.txt"
#define FNAME_BUS_TIMETABLE_HOLIDAY     "bus_timetable_holiday.txt"
#define FNAME_BUS_TIMETABLE_SAT         "bus_timetable_sat.txt"
#define FNAME_ALARM_MP3 "alarm.mp3"

extern const String json_Functions;
extern TimerHandle_t xAlarmTimer;
extern String note;

extern bool register_wakeword_required;
extern bool wakeword_enable_required;
extern bool alarmTimerCallbacked;

void init_func_call_settings(StackchanExConfig& system_config);


//
// Functions for Function Calling
//
String timer(int32_t time, const char* action);
String timer_change(int32_t time);

String get_date();
String get_time();
String get_week();

#if defined(USE_EXTENSION_FUNCTIONS)
String reminder(int hour, int min, const char* text);
String ask(const char* text);

String save_note(const char* text);
String read_note();
String delete_note();

String get_bus_time(int nNext);

String send_mail(String msg);
String read_mail(void);

#if defined(ARDUINO_M5STACK_CORES3)
String register_wakeword(void);
String wakeword_enable(void);
String delete_wakeword(int idx);
#endif
String get_news();
String get_weathers();
#endif

#endif //_FUNCTION_CALL_