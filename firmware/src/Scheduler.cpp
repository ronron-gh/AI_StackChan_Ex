#include <Arduino.h>
#include "Scheduler.h"
#include "Robot.h"
#include "driver/PlayMP3.h"

ScheduleBase* scheduleList[MAX_SCHED_NUM] = {nullptr};


void add_schedule(ScheduleBase* schedule)
{
    for(int i=0; i<MAX_SCHED_NUM; i++){
        if(scheduleList[i]==nullptr){
            scheduleList[i] = schedule;
            Serial.printf("add_schedule: %d\n",i);
            break;
        }
    }
}

ScheduleBase* get_schedule(int idx)
{
    if(idx > MAX_SCHED_NUM){
        return nullptr;
    }
    return scheduleList[idx];
}

void run_schedule(void)
{
    struct tm now_time;
    
    if (!getLocalTime(&now_time)) {
        Serial.printf("failed to get time\n");
        return;
    }

    for(int i=0; i<MAX_SCHED_NUM; i++){
        if(scheduleList[i]!=nullptr){
            //Serial.printf("run schedule: %d\n",i);
            ScheduleBase* schedule = scheduleList[i];
            schedule->run(now_time);

            if(schedule->destroy){
                Serial.printf("destroy schedule: %d\n",i);
                delete schedule;
                scheduleList[i] = nullptr;
            }

        }

        
    }
}



ScheduleBase::ScheduleBase()
{
    destroy = false;
}

ScheduleEveryDay::ScheduleEveryDay(int hour, int min, void (*func)(void))
{
    struct tm now_time = {};

    sched_type = SCHED_EVERY_DAY;
    sched_hour = hour;
    sched_min = min;
    callback = func;

    //If the time has already passed, set the status to executed.
    if (getLocalTime(&now_time)) {
        int sched = sched_hour * 60 + sched_min;
        int now = now_time.tm_hour * 60 + now_time.tm_min;

        if(now >= sched){
            is_executed = true;
        }else{
            is_executed = false;
        }

        prev_time = now_time;
    }
    else{
        Serial.printf("failed to get time\n");
        is_executed = false;
        prev_time = now_time;
    }

}

void ScheduleEveryDay::run(struct tm now_time)
{
    int sched = sched_hour * 60 + sched_min;
    int now = now_time.tm_hour * 60 + now_time.tm_min;

    if(now >= sched){
        if(!is_executed){
            Serial.printf("Schedule::run callback()\n");
            callback();
            is_executed = true;
        }
    }

    // Reset schedule when date is updated.
    if(prev_time.tm_hour == 23 && now_time.tm_hour == 0){
        Serial.printf("Reset schedule\n");
        is_executed = false;
    }
    prev_time = now_time;
}



ScheduleEveryHour::ScheduleEveryHour(void (*func)(void), int _start_hour, int _end_hour)
{
    struct tm now_time = {};

    sched_type = SCHED_EVERY_HOUR;
    start_hour = _start_hour;
    end_hour = _end_hour;
    callback = func;

    getLocalTime(&now_time);
    prev_time = now_time;

    //Test code
    //prev_time.tm_hour = 22;
}

void ScheduleEveryHour::run(struct tm now_time)
{
    if(start_hour <= now_time.tm_hour && now_time.tm_hour <= end_hour){
        if(now_time.tm_hour != prev_time.tm_hour){
            Serial.printf("ScheduleEveryHour::run callback()\n");
            callback();
        }
    }
    prev_time = now_time;
}



ScheduleReminder::ScheduleReminder(int hour, int min, String _remind_text)
{
    struct tm now_time = {};

    sched_type = SCHED_REMINDER;
    sched_hour = hour;
    sched_min = min;
    remind_text = _remind_text;

    //If the time has already passed, set the status to executed.
    if (getLocalTime(&now_time)) {
        int sched = sched_hour * 60 + sched_min;
        int now = now_time.tm_hour * 60 + now_time.tm_min;

        if(now >= sched){
            is_executed = true;
            destroy = true;
        }else{
            is_executed = false;
        }
    }
    else{
        Serial.printf("failed to get time\n");
        is_executed = false;
    }

}

#include "llm/ChatGPT/FunctionCall.h"
void ScheduleReminder::run(struct tm now_time)
{
    int sched = sched_hour * 60 + sched_min;
    int now = now_time.tm_hour * 60 + now_time.tm_min;

    if(now >= sched){
        if(!is_executed){
            Serial.printf("ScheduleReminder::run callback()\n");

            String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
            playMP3SD(fname.c_str());
            robot->speech(remind_text + "、をリマインドします");

            is_executed = true;
            destroy = true;

        }
    }


}

String ScheduleReminder::get_time_string()
{
    return String(sched_hour) + String(":") + String(sched_min);
}

ScheduleIntervalMinute::ScheduleIntervalMinute(int _interval_min, void (*func)(void), int _start_hour, int _end_hour)
{
    struct tm now_time = {};

    sched_type = SCHED_INTERVAL_MINUTE;
    interval_min = _interval_min;
    start_hour = _start_hour;
    end_hour = _end_hour;
    callback = func;

    getLocalTime(&now_time);
    prev_time = now_time;

}

void ScheduleIntervalMinute::run(struct tm now_time)
{
    if(start_hour <= now_time.tm_hour && now_time.tm_hour <= end_hour){
        int diff = now_time.tm_min - prev_time.tm_min;
        if(diff < 0){
            diff = now_time.tm_min + (60 - prev_time.tm_min);
        }

        if(diff >= interval_min){
            Serial.printf("ScheduleIntervalMinute::run callback()\n");
            callback();
            prev_time = now_time;
        }

    }
}

