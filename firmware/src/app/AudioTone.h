#ifndef _APP_AUDIO_TONE_H
#define _APP_AUDIO_TONE_H

// 効果音用の小さな PCM tone 再生。Mute 中はスキップする。
// 内部で M5.Mic.end → M5.Speaker.begin → tone → end の I2S 切替を
// enterMutexAudio() で排他しながら行う。

// 通知音（ピロッ）: G5 → C6 の 2 音
void sw_tone();

// アラーム音: 1200Hz パルスを 5 回
void alarm_tone();

#endif  // _APP_AUDIO_TONE_H
