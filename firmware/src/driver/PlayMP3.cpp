#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPIFFS.h>
#include <AudioOutput.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include "AudioFileSourceHTTPSStream.h"
#include "AudioFileSourceSD.h"
#include "AudioFileSourceSPIFFS.h"
#include "AudioOutputM5Speaker.h"
#include "PlayMP3.h"
#include "Avatar.h"
#include "../share/Mutex.h"

using namespace m5avatar;

extern Avatar avatar;
extern bool servo_home;

/// set M5Speaker virtual channel (0-7)
//static constexpr uint8_t m5spk_virtual_channel = 0;
uint8_t m5spk_virtual_channel = 0;

AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);
AudioGeneratorMP3 *mp3;

int preallocateBufferSize = 30*1024;
uint8_t *preallocateBuffer;




void mp3_init(void)
{
    mp3 = new AudioGeneratorMP3();
    //out = new AudioOutputM5Speaker(&M5.Speaker, m5spk_virtual_channel);

    //TTS MP3用バッファ （PSRAMから確保される）
    preallocateBuffer = (uint8_t *)malloc(preallocateBufferSize);
    if (!preallocateBuffer) {
        M5.Display.printf("FATAL ERROR:  Unable to preallocate %d bytes for app\n", preallocateBufferSize);
        for (;;) { delay(1000); }
    }

    audioLogger = &Serial;
}

// I2S リソース管理を強化した playMP3。連続会話モードで 2 ターン目以降に
// "register I2S object to platform failed" が出る問題への対応 (#1)。
//
// 改善点:
//   1. enterMutexAudio() で sw_tone/alarm_tone と排他制御
//   2. Mic.end → Speaker.begin 間に短い delay（DMA 解放を待つ）
//   3. Speaker.begin() / Mic.begin() の戻り値をチェック、失敗時はリトライ
//   4. 失敗・成功を Serial にログ
void playMP3(AudioFileSourceBuffer *buff){

  enterMutexAudio();

  // --- Mic → Speaker 切替 ---
  M5.Mic.end();
  delay(30);   // I2S DMA を解放させる

  bool spk_ok = M5.Speaker.begin();
  if (!spk_ok) {
    Serial.println("[playMP3] Speaker.begin() failed - retry after 200ms");
    delay(200);
    spk_ok = M5.Speaker.begin();
    if (!spk_ok) {
      Serial.println("[playMP3] Speaker.begin() failed twice - abort");
      // Mic を復帰させてから抜ける
      delay(50);
      M5.Mic.begin();
      exitMutexAudio();
      return;
    }
  }
  delay(50);   // DMA がストリーミング可能になるまで待機

  // --- MP3 再生 ---
  mp3->begin(buff, &out);
  Serial.println("mp3 start");

  while(mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("mp3 stop");
    }
    delay(1);
  }

  // --- Speaker → Mic 切替 ---
  M5.Speaker.end();
  delay(30);   // I2S DMA を解放させる

  bool mic_ok = M5.Mic.begin();
  if (!mic_ok) {
    Serial.println("[playMP3] Mic.begin() failed - retry after 200ms");
    delay(200);
    mic_ok = M5.Mic.begin();
    if (!mic_ok) {
      Serial.println("[playMP3] Mic.begin() failed twice - giving up (mic disabled)");
    }
  }

  exitMutexAudio();
}

bool playMP3SPIFFS(const char *filename)
{
  bool result;

  if (SPIFFS.exists(filename)) {
    AudioFileSourceSPIFFS *file_mp3 = new AudioFileSourceSPIFFS(filename);
    Serial.println("Open mp3");
    
    if( !file_mp3->isOpen() ){
      delete file_mp3;
      file_mp3 = nullptr;
      Serial.println("failed to open mp3 file");
      result = false;
    }
    else{
      AudioFileSourceBuffer *buff = new AudioFileSourceBuffer(file_mp3, preallocateBuffer, preallocateBufferSize);
      avatar.setExpression(Expression::Happy);
      servo_home = false;

      playMP3(buff);
      
      avatar.setExpression(Expression::Neutral);
      servo_home = true;

      delete file_mp3;
      delete buff;
      result = true;
    }
  }else{
    Serial.println("mp3 file is not exist");
    result = false;
  }
  return result;
}


bool playMP3SD(const char *filename)
{
  bool result;

  if (SD.exists(filename)) {

    AudioFileSourceSD *file_mp3 = new AudioFileSourceSD(filename);
    Serial.println("Open mp3");
    
    if( !file_mp3->isOpen() ){
      delete file_mp3;
      //file_mp3 = nullptr;
      Serial.println("failed to open mp3 file");
      result = false;
    }
    else{
      AudioFileSourceBuffer *buff = new AudioFileSourceBuffer(file_mp3, preallocateBuffer, preallocateBufferSize);
      avatar.setExpression(Expression::Happy);
      servo_home = false;

      playMP3(buff);
      
      avatar.setExpression(Expression::Neutral);
      servo_home = true;

      delete file_mp3;
      delete buff;
      result = true;
    }
  }else{
    Serial.println("mp3 file is not exist");
    result = false;
  }

  return result;
}
