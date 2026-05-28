#ifndef _APP_MUTE_MODE_H
#define _APP_MUTE_MODE_H

// 外出時等に TTS / 効果音を一時停止するための揮発フラグ。
// 再起動でリセットされる（永続化しない）。
//
// Robot::speech / sw_tone / alarm_tone / TTS 各実装が is_muted() を
// チェックして発声をスキップする。
bool is_muted();
void set_mute(bool m);

#endif  // _APP_MUTE_MODE_H
