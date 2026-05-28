#include "SerialConfig.h"
#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <WiFi.h>

namespace {

enum class Mode { Idle, ReceivingYaml };
Mode mode = Mode::Idle;
String type_;       // ex / basic / sec
String yaml_buf;
String line_buf;

const char* path_for(const String& t) {
  if (t == "ex")    return "/app/AiStackChanEx/SC_ExConfig.yaml";
  if (t == "basic") return "/yaml/SC_BasicConfig.yaml";
  if (t == "sec")   return "/yaml/SC_SecConfig.yaml";
  return nullptr;
}

void print_help() {
  Serial.println(F(":CONFIG HELP                       - this help"));
  Serial.println(F(":CONFIG GET <ex|basic|sec>         - get yaml"));
  Serial.println(F(":CONFIG SET <ex|basic|sec>"));
  Serial.println(F("  <yaml lines...>"));
  Serial.println(F("  :END                              - save yaml"));
  Serial.println(F(":CONFIG RESTART                    - reboot device"));
  Serial.println(F(":CONFIG WIFI_SCAN                  - list nearby SSIDs"));
}

void do_wifi_scan() {
  Serial.println(F(":WIFI_SCAN scanning..."));
  // STA モードを有効化してスキャン（接続中でも使える）
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  if (n < 0) {
    Serial.printf(":ERR scanNetworks failed (%d)\n", n);
    return;
  }
  Serial.printf(":WIFI_SCAN found %d networks\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  [%2d] %-32s rssi=%d ch=%d enc=%d\n",
                  i, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                  WiFi.channel(i), (int)WiFi.encryptionType(i));
  }
  Serial.println(F(":END"));
}

void do_get(const String& t) {
  const char* p = path_for(t);
  if (!p) { Serial.println(F(":ERR invalid type")); return; }
  File f = SD.open(p, FILE_READ);
  if (!f) { Serial.println(F(":ERR file not found")); return; }
  Serial.print(F(":BEGIN "));
  Serial.println(t);
  while (f.available()) {
    Serial.write(f.read());
  }
  f.close();
  Serial.println();
  Serial.println(F(":END"));
}

void do_set_begin(const String& t) {
  const char* p = path_for(t);
  if (!p) { Serial.println(F(":ERR invalid type")); return; }
  type_ = t;
  yaml_buf = "";
  mode = Mode::ReceivingYaml;
  Serial.println(F(":READY send yaml ending with :END"));
}

void do_set_commit() {
  const char* p = path_for(type_);
  if (!p) { Serial.println(F(":ERR invalid type (state lost)")); mode = Mode::Idle; return; }
  File w = SD.open(p, FILE_WRITE);
  if (!w) { Serial.println(F(":ERR open for write failed")); mode = Mode::Idle; return; }
  size_t written = w.print(yaml_buf);
  w.close();
  Serial.printf(":OK %u bytes written to %s\n", (unsigned)written, p);
  mode = Mode::Idle;
  yaml_buf = "";
  type_ = "";
}

void handle_command(const String& line) {
  // 受信モード中は :END 以外はバッファに追加
  if (mode == Mode::ReceivingYaml) {
    if (line == ":END") {
      do_set_commit();
    } else {
      yaml_buf += line;
      yaml_buf += '\n';
    }
    return;
  }

  // コマンド判定
  if (!line.startsWith(":CONFIG")) return;
  String rest = line.substring(7);
  rest.trim();

  if (rest == "HELP" || rest.length() == 0) {
    print_help();
  } else if (rest == "RESTART") {
    Serial.println(F(":OK restarting..."));
    delay(200);
    ESP.restart();
  } else if (rest == "WIFI_SCAN") {
    do_wifi_scan();
  } else if (rest.startsWith("GET ")) {
    String t = rest.substring(4); t.trim();
    do_get(t);
  } else if (rest.startsWith("SET ")) {
    String t = rest.substring(4); t.trim();
    do_set_begin(t);
  } else {
    Serial.println(F(":ERR unknown command, try :CONFIG HELP"));
  }
}

}  // namespace

void serial_config_poll() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;   // CR は無視
    if (c == '\n') {
      handle_command(line_buf);
      line_buf = "";
      continue;
    }
    line_buf += c;
    if (line_buf.length() > 4096) {
      // 異常に長い行は捨てる
      line_buf = "";
    }
  }
}
