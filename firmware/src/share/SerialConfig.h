#ifndef _SERIAL_CONFIG_H
#define _SERIAL_CONFIG_H

// Serial 経由 YAML 設定機能
// HOST > :CONFIG GET ex|basic|sec  → yaml 全文 + :END で返す
// HOST > :CONFIG SET ex|basic|sec  → 次の :END までを yaml として SD へ保存
// HOST > :CONFIG HELP              → コマンド一覧
// HOST > :CONFIG RESTART           → ESP.restart()
//
// loop() から定期的に serial_config_poll() を呼ぶ。
void serial_config_poll();

#endif  // _SERIAL_CONFIG_H
