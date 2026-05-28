#include "AudioTone.h"
#include "MuteMode.h"
#include "../share/Mutex.h"
#include <M5Unified.h>

void sw_tone()
{
  if (is_muted()) return;
  enterMutexAudio();
  M5.Mic.end();
  M5.Speaker.begin();
  delay(300);     // AtomS3R はこの delay がないと鳴らないことがある
  // ソド の上昇 2 音（ピロッ）
  M5.Speaker.tone(784, 70);    // G5
  delay(90);
  M5.Speaker.tone(1047, 110);  // C6
  delay(200);

  M5.Speaker.end();
  M5.Mic.begin();
  exitMutexAudio();
}

void alarm_tone()
{
  if (is_muted()) return;
  enterMutexAudio();
  M5.Mic.end();
  M5.Speaker.begin();

  for (int i = 0; i < 5; i++) {
    M5.Speaker.tone(1200, 50);
    delay(100);
    M5.Speaker.tone(1200, 50);
    delay(100);
    M5.Speaker.tone(1200, 50);
    delay(1000);
  }

  M5.Speaker.end();
  M5.Mic.begin();
  exitMutexAudio();
}
