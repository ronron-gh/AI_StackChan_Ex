#ifndef _PLAY_MP3_H
#define _PLAY_MP3_H

#include <Arduino.h>
#include <M5Unified.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <functional>
#include "AudioFileSourceHTTPSStream.h"
#include "AudioOutputM5Speaker.h"

extern uint8_t m5spk_virtual_channel;

// out は AudioOutputM5Speaker.h で extern 宣言済み
extern AudioGeneratorMP3 *mp3;
extern AudioFileSourceBuffer *buff;
extern AudioFileSourceHTTPSStream *file;
extern int preallocateBufferSize;
extern uint8_t *preallocateBuffer;

// 再生開始/終了時のリスナー（presentation 側で表情・サーボ制御に使う）。
// 設定しない場合は何も呼ばれない。driver 層から Avatar.h を直接 include
// しないよう、フック方式で切り出している。
namespace PlayMP3 {
  using EventFn = std::function<void()>;
  void set_event_listeners(EventFn on_start, EventFn on_stop);
}

extern void mp3_init(void);
extern void playMP3(AudioFileSourceBuffer *buff);
extern bool playMP3SPIFFS(const char *filename);
extern bool playMP3SD(const char *filename);

#endif
