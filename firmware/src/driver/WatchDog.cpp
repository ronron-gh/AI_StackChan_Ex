#include <Arduino.h>
#include "WatchDog.h"

#define WATCHDOG_TIMER_SEC  (180)
TimerHandle_t xWatchDogTimer;


void watchdog_callback(TimerHandle_t _xTimer){
  Serial.println("Watch dog timeout");
  esp_restart();
}

void init_watchdog(void)
{
    xWatchDogTimer = xTimerCreate("WatchDogTimer", WATCHDOG_TIMER_SEC * 1000, pdFALSE, 0, watchdog_callback);

}

void reset_watchdog(void)
{
    xTimerChangePeriod(xWatchDogTimer, WATCHDOG_TIMER_SEC * 1000, 0);
}