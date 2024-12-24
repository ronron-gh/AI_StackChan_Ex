#ifndef _PLAY_MP3_H
#define _PLAY_MP3_H

#include <Arduino.h>
#include <M5Unified.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include "AudioFileSourceHTTPSStream.h"
#include "AudioOutputM5Speaker.h"

extern uint8_t m5spk_virtual_channel;

extern AudioOutputM5Speaker out;
//extern AudioOutputM5Speaker *out;
extern AudioGeneratorMP3 *mp3;
extern AudioFileSourceBuffer *buff;
extern AudioFileSourceHTTPSStream *file;
extern int preallocateBufferSize;
extern uint8_t *preallocateBuffer;

extern void mp3_init(void);
extern void playMP3(AudioFileSourceBuffer *buff);
extern bool playMP3SPIFFS(const char *filename);
extern bool playMP3SD(const char *filename);

#endif