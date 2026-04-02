#ifndef _MUTEX_H
#define _MUTEX_H
#include <M5Unified.h>

extern SemaphoreHandle_t mutexAudio;

void initMutex(void);
void enterMutexAudio(void);
void exitMutexAudio(void);

#endif