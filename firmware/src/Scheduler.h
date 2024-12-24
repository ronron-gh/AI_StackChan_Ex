#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <Arduino.h>

#define MAX_SCHED_NUM   (50)

typedef enum e_sched_type
{
    SCHED_EVERY_DAY,
    SCHED_EVERY_HOUR,
    SCHED_REMINDER,
    SCHED_INTERVAL_MINUTE
} SCHED_TYPE;

class ScheduleBase{
protected:
    SCHED_TYPE sched_type;      //子クラスの判別用（本当はtypeinfoを使いたいがビルドに失敗した）
    struct tm prev_time;
    void (*callback)(void);

public:
    bool destroy;
    ScheduleBase();
    virtual void run(struct tm now_time);
    SCHED_TYPE get_sched_type(){ return sched_type; };

};


class ScheduleEveryDay: public ScheduleBase{
private:
    int sched_hour;
    int sched_min;
    int is_executed;
public:
    ScheduleEveryDay(int hour, int min, void (*func)(void));
    void run(struct tm now_time);
};

class ScheduleEveryHour: public ScheduleBase{
private:
    int start_hour;
    int end_hour;
public:
    ScheduleEveryHour(void (*func)(void), int _start_hour, int _end_hour);
    void run (struct tm now_time);
};


class ScheduleReminder: public ScheduleBase{
private:
    int sched_hour;
    int sched_min;
    int is_executed;
    String remind_text;
public:
    ScheduleReminder(int hour, int min, String _remind_text);
    void run(struct tm now_time);
    String get_time_string();
    String get_remind_string(){ return remind_text; };
};


class ScheduleIntervalMinute: public ScheduleBase{
private:
    int interval_min;
    int start_hour;
    int end_hour;
    int is_executed;
    String remind_text;
public:
    ScheduleIntervalMinute(int _interval_min, void (*func)(void), int _start_hour, int _end_hour);
    void run(struct tm now_time);
};


extern void add_schedule(ScheduleBase* schedule);
extern ScheduleBase* get_schedule(int idx);
extern void run_schedule(void);



#endif