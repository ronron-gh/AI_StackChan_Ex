#include <M5Unified.h>
#include "share/Mutex.h"

SemaphoreHandle_t mutexAudio;

void initMutex(void)
{
    mutexAudio = xSemaphoreCreateMutex();  // セマフォをMutexとして定義
}

void enterMutexAudio(void)
{
    xSemaphoreTake(mutexAudio, portMAX_DELAY);
}

void exitMutexAudio(void)
{
    xSemaphoreGive(mutexAudio);
}