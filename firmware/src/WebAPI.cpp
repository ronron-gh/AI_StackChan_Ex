#include <ESP32WebServer.h>
#include <nvs.h>
#include <Preferences.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <SD.h>
#include "WebAPI.h"
#include "Avatar.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "Robot.h"
#include "share/Version.h"
#include "share/PersonalityPresets.h"

using namespace m5avatar;
extern Avatar avatar;
extern uint8_t m5spk_virtual_channel;
extern String STT_API_KEY;

ESP32WebServer server(80);

// C++11 multiline string constants are neato...
static const char HEAD[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="UTF-8">
  <title>AIｽﾀｯｸﾁｬﾝ</title>
</head>)KEWL";

static const char APIKEY_HTML[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>APIキー設定</title>
  </head>
  <body>
    <h1>APIキー設定</h1>
    <form>
      <label for="role1">OpenAI API Key</label>
      <input type="text" id="openai" name="openai" oninput="adjustSize(this)"><br>
      <label for="role2">VoiceVox API Key</label>
      <input type="text" id="voicevox" name="voicevox" oninput="adjustSize(this)"><br>
      <label for="role3">Speech to Text API Key</label>
      <input type="text" id="sttapikey" name="sttapikey" oninput="adjustSize(this)"><br>
      <button type="button" onclick="sendData()">送信する</button>
    </form>
    <script>
      function adjustSize(input) {
        input.style.width = ((input.value.length + 1) * 8) + 'px';
      }
      function sendData() {
        // FormDataオブジェクトを作成
        const formData = new FormData();

        // 各ロールの値をFormDataオブジェクトに追加
        const openaiValue = document.getElementById("openai").value;
        if (openaiValue !== "") formData.append("openai", openaiValue);

        const voicevoxValue = document.getElementById("voicevox").value;
        if (voicevoxValue !== "") formData.append("voicevox", voicevoxValue);

        const sttapikeyValue = document.getElementById("sttapikey").value;
        if (sttapikeyValue !== "") formData.append("sttapikey", sttapikeyValue);

	    // POSTリクエストを送信
	    const xhr = new XMLHttpRequest();
	    xhr.open("POST", "/apikey_set");
	    xhr.onload = function() {
	      if (xhr.status === 200) {
	        alert("データを送信しました！");
	      } else {
	        alert("送信に失敗しました。");
	      }
	    };
	    xhr.send(formData);
	  }
	</script>
  </body>
</html>)KEWL";

#if 0
static const char ROLE_HTML[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html>
<head>
	<title>ロール設定</title>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<style>
		textarea {
			width: 80%;
			height: 200px;
			resize: both;
		}
	</style>
</head>
<body>
	<h1>ロール設定</h1>
	<form onsubmit="postData(event)">
		<label for="textarea">ここにロールを記述してください。:</label><br>
		<textarea id="textarea" name="textarea"></textarea><br><br>
		<input type="submit" value="Submit">
	</form>
	<script>
		function postData(event) {
			event.preventDefault();
			const textAreaContent = document.getElementById("textarea").value.trim();
//			if (textAreaContent.length > 0) {
				const xhr = new XMLHttpRequest();
				xhr.open("POST", "/role_set", true);
				xhr.setRequestHeader("Content-Type", "text/plain;charset=UTF-8");
			// xhr.onload = () => {
			// 	location.reload(); // 送信後にページをリロード
			// };
			xhr.onload = () => {
				document.open();
				document.write(xhr.responseText);
				document.close();
			};
				xhr.send(textAreaContent);
//        document.getElementById("textarea").value = "";
				alert("Data sent successfully!");
//			} else {
//				alert("Please enter some text before submitting.");
//			}
		}
	</script>
</body>
</html>)KEWL";
#endif

#define IMPORT_FILE(section, filename, symbol) \
static constexpr const char* filename_##symbol = filename; \
extern const uint8_t symbol[], sizeof_##symbol[]; \
asm(\
  ".section " #section "\n"\
  ".balign 4\n"\
  ".global " #symbol "\n"\
  #symbol ":\n"\
  ".incbin \"incbin/" filename "\"\n"\
  ".global sizeof_" #symbol "\n"\
  ".set sizeof_" #symbol ", . - " #symbol "\n"\
  ".balign 4\n"\
  ".section \".text\"\n")

IMPORT_FILE(.rodata, "index.html", index_html);
IMPORT_FILE(.rodata, "personalize.html", personalize_html);
IMPORT_FILE(.rodata, "personalize.js", personalize_js);
IMPORT_FILE(.rodata, "dashboard.html", dashboard_html);
IMPORT_FILE(.rodata, "dashboard.js", dashboard_js);
IMPORT_FILE(.rodata, "settings.html", settings_html);
IMPORT_FILE(.rodata, "settings.js", settings_js);
IMPORT_FILE(.rodata, "present.html", present_html);
IMPORT_FILE(.rodata, "present.js", present_js);


void handleRoot() {
  // トップは index.html（Dashboard/Personalize へのナビ）
  server.send_P(200, "text/html", (const char*)index_html, (size_t)sizeof_index_html);
}

void handle_personalize_html() {
  server.send_P(200, "text/html", (const char*)personalize_html, (size_t)sizeof_personalize_html);
}

void handle_personalize_js() {
  server.send_P(200, "application/javascript", (const char*)personalize_js, (size_t)sizeof_personalize_js);
}

void handle_dashboard_html() {
  server.send_P(200, "text/html", (const char*)dashboard_html, (size_t)sizeof_dashboard_html);
}

void handle_dashboard_js() {
  server.send_P(200, "application/javascript", (const char*)dashboard_js, (size_t)sizeof_dashboard_js);
}

// Mute モード（main.cpp 側で実装）
extern bool is_muted();
extern void set_mute(bool m);
extern void led_set(uint32_t rgb);
extern void led_off();
extern void sw_tone();
extern void alarm_tone();

// YAML 設定ファイルの読み書き
// パス検証（許可: ex / basic / sec のみ）
static const char* path_for_config(const String& type) {
  if (type == "ex")    return "/app/AiStackChanEx/SC_ExConfig.yaml";
  if (type == "basic") return "/yaml/SC_BasicConfig.yaml";
  if (type == "sec")   return "/yaml/SC_SecConfig.yaml";
  return nullptr;
}

// 秘密値（password / apikey 系）を ***UNCHANGED*** に置換する
// シンプルな行単位処理。"key:" + 値（引用符あり/なし）に対応。
static String mask_secrets(const String& yaml) {
  String out;
  out.reserve(yaml.length() + 64);
  int start = 0;
  while (start <= (int)yaml.length()) {
    int eol = yaml.indexOf('\n', start);
    String line = (eol < 0) ? yaml.substring(start) : yaml.substring(start, eol);
    String trimmed = line; trimmed.trim();
    String lower = trimmed; lower.toLowerCase();
    bool is_secret =
      lower.startsWith("password:") || lower.startsWith("password :") ||
      lower.startsWith("stt:") || lower.startsWith("stt :") ||
      lower.startsWith("aiservice:") || lower.startsWith("aiservice :") ||
      lower.startsWith("tts:") || lower.startsWith("tts :");
    // ただし tts: は ExConfig 側にも別用途で出てくる（type/voice/model のセクション）
    // SecConfig の "tts: \"...\"" 形式の場合だけマスクしたい → コロン直後の値が引用符ありかで判定
    if (is_secret) {
      int colon = line.indexOf(':');
      if (colon >= 0) {
        // コロン以降の値部分を取り出し、ダブルクォート文字列ならマスク
        String value = line.substring(colon + 1);
        String value_trimmed = value; value_trimmed.trim();
        if (value_trimmed.startsWith("\"") && value_trimmed.length() > 2) {
          // インデント保持してマスク
          String indent = line.substring(0, colon);
          out += indent + ": \"***UNCHANGED***\"";
          if (eol >= 0) out += "\n";
          if (eol < 0) break;
          start = eol + 1;
          continue;
        }
      }
    }
    out += line;
    if (eol >= 0) out += "\n";
    if (eol < 0) break;
    start = eol + 1;
  }
  return out;
}

// ***UNCHANGED*** を元ファイルの該当行で復元
static String unmask_secrets(const String& new_yaml, const String& old_yaml) {
  // 単純に行単位で対応。***UNCHANGED*** を含む行は old_yaml の同名キーの行で置換
  String out;
  out.reserve(new_yaml.length() + 64);
  int start = 0;
  while (start <= (int)new_yaml.length()) {
    int eol = new_yaml.indexOf('\n', start);
    String line = (eol < 0) ? new_yaml.substring(start) : new_yaml.substring(start, eol);
    if (line.indexOf("***UNCHANGED***") >= 0) {
      // この行のキー名を取り出して old_yaml から探す
      int colon = line.indexOf(':');
      if (colon > 0) {
        String key = line.substring(0, colon + 1);   // 'key:' まで（インデント含む）
        // old_yaml の同じインデント + キー で始まる行を探す
        int p = old_yaml.indexOf(key);
        if (p >= 0) {
          // 行頭である必要があるため簡易チェック
          bool at_line_start = (p == 0) || (old_yaml.charAt(p - 1) == '\n');
          if (at_line_start) {
            int eol_old = old_yaml.indexOf('\n', p);
            String old_line = (eol_old < 0) ? old_yaml.substring(p) : old_yaml.substring(p, eol_old);
            out += old_line;
            if (eol >= 0) out += "\n";
            if (eol < 0) break;
            start = eol + 1;
            continue;
          }
        }
      }
    }
    out += line;
    if (eol >= 0) out += "\n";
    if (eol < 0) break;
    start = eol + 1;
  }
  return out;
}

void handle_config_get() {
  String type = server.arg("type");
  const char* path = path_for_config(type);
  if (!path) { server.send(400, "text/plain", "type must be ex|basic|sec"); return; }
  File f = SD.open(path, FILE_READ);
  if (!f) { server.send(404, "text/plain", "file not found"); return; }
  String body;
  body.reserve(f.size() + 16);
  while (f.available()) body += (char)f.read();
  f.close();
  // 秘密値マスク
  body = mask_secrets(body);
  server.send(200, "text/plain; charset=utf-8", body);
}

void handle_config_post() {
  String type = server.arg("type");
  const char* path = path_for_config(type);
  if (!path) { server.send(400, "text/plain", "type must be ex|basic|sec"); return; }
  String new_body = server.arg("plain");
  if (new_body.length() == 0) { server.send(400, "text/plain", "empty body"); return; }
  // 既存読み込み → UNCHANGED 復元
  String old_body;
  File f = SD.open(path, FILE_READ);
  if (f) {
    old_body.reserve(f.size() + 16);
    while (f.available()) old_body += (char)f.read();
    f.close();
  }
  String to_write = unmask_secrets(new_body, old_body);
  // 書き込み
  File w = SD.open(path, FILE_WRITE);
  if (!w) { server.send(500, "text/plain", "open for write failed"); return; }
  w.print(to_write);
  w.close();
  Serial.printf("Config saved: %s (%d bytes)\n", path, (int)to_write.length());
  server.send(200, "text/plain", "saved");
}

void handle_restart() {
  Serial.println("Restart requested via /api/restart");
  server.send(200, "text/plain", "restarting...");
  delay(500);
  ESP.restart();
}

void handle_mute() {
  // デバッグ用: 受信したリクエストの情報を出力
  Serial.printf("handle_mute: method=%d uri=%s args=%d\n",
                (int)server.method(), server.uri().c_str(), server.args());
  for (uint8_t i = 0; i < server.args(); i++) {
    Serial.printf("  arg[%d]: %s = %s\n",
                  i, server.argName(i).c_str(), server.arg(i).c_str());
  }

  // GET/POST 両対応。value 引数があれば設定、なければ現在状態返却。
  if (server.hasArg("value")) {
    String v = server.arg("value");
    v.toLowerCase();
    bool new_value = (v == "true" || v == "1" || v == "on");
    set_mute(new_value);
    Serial.printf("handle_mute: value='%s' -> mute=%d\n", v.c_str(), (int)new_value);
  } else {
    Serial.println("handle_mute: no value arg (status only)");
  }

  String body = String("{\"mute\":") + (is_muted() ? "true" : "false") + "}";
  server.send(200, "application/json", body);
}

// Web から音量変更（NVS で再起動後も保持）
void handle_volume() {
  Serial.printf("handle_volume: method=%d args=%d\n",
                (int)server.method(), server.args());

  if (server.hasArg("value")) {
    int v = server.arg("value").toInt();
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    if (robot) robot->spk_volume = (uint8_t)v;
    M5.Speaker.setVolume((uint8_t)v);
    // NVS 永続化（再起動後も保持）
    Preferences prefs;
    if (prefs.begin("aistackchan", false)) {
      prefs.putUChar("volume", (uint8_t)v);
      prefs.end();
    }
    Serial.printf("Volume set via Web: %d\n", v);
  }

  uint8_t cur = robot ? robot->spk_volume : 0;
  String body = String("{\"volume\":") + cur + ",\"max\":255}";
  server.send(200, "application/json", body);
}

// 前方宣言（json_escape は後段で定義）
static String json_escape(const String& s);

// YAML 内の wakeword.type と wakeword.keyword を書き換える小ヘルパ。
// 行ベースで安全に編集。`wakeword:` ブロック内（2 スペースインデント）の
// `type:` と `keyword:` を新しい値で置換し、ブロックが無ければ末尾に追加。
static String update_wakeword_in_yaml(const String& yaml, int new_type, const String& new_keyword) {
  String out;
  out.reserve(yaml.length() + 64);
  bool in_wakeword = false;
  bool type_replaced = false;
  bool keyword_replaced = false;

  int start = 0;
  while (start <= (int)yaml.length()) {
    int eol = yaml.indexOf('\n', start);
    String line = (eol < 0) ? yaml.substring(start) : yaml.substring(start, eol);

    String trimmed = line; trimmed.trim();
    if (trimmed.startsWith("wakeword:")) {
      in_wakeword = true;
      out += line;
      if (eol >= 0) out += "\n";
      if (eol < 0) break;
      start = eol + 1;
      continue;
    }

    if (in_wakeword) {
      // インデントが 0 or トップレベルキーに戻ったらブロック終了
      bool block_end = !line.isEmpty() && line.charAt(0) != ' ' && line.charAt(0) != '\t';
      if (block_end) {
        // ブロック内に type / keyword が無ければ追加してから抜ける
        if (!type_replaced) {
          out += "  type: " + String(new_type) + "\n";
          type_replaced = true;
        }
        if (!keyword_replaced) {
          out += "  keyword: \"" + new_keyword + "\"\n";
          keyword_replaced = true;
        }
        in_wakeword = false;
      } else {
        // ブロック内の type / keyword を置換
        if (trimmed.startsWith("type:")) {
          out += "  type: " + String(new_type);
          if (eol >= 0) out += "\n";
          type_replaced = true;
          if (eol < 0) break;
          start = eol + 1;
          continue;
        }
        if (trimmed.startsWith("keyword:")) {
          out += "  keyword: \"" + new_keyword + "\"";
          if (eol >= 0) out += "\n";
          keyword_replaced = true;
          if (eol < 0) break;
          start = eol + 1;
          continue;
        }
      }
    }

    out += line;
    if (eol >= 0) out += "\n";
    if (eol < 0) break;
    start = eol + 1;
  }

  // wakeword ブロック自体が無かった場合は末尾に追加
  if (!type_replaced || !keyword_replaced) {
    if (!out.endsWith("\n")) out += "\n";
    out += "\nwakeword:\n";
    out += "  type: " + String(new_type) + "\n";
    out += "  keyword: \"" + new_keyword + "\"\n";
  }

  return out;
}

// Wakeword 設定: GET で現在値、POST で更新（SD YAML を上書き、即時反映には再起動必要）
void handle_wakeword() {
  // POST or GET ?type=...&keyword=... で更新
  bool changed = false;
  if (server.hasArg("type") || server.hasArg("keyword")) {
    int new_type = server.hasArg("type") ? server.arg("type").toInt()
                                          : (robot ? robot->m_config.getExConfig().wakeword.type : 0);
    if (new_type < 0) new_type = 0;
    if (new_type > 1) new_type = 1;
    String new_keyword = server.hasArg("keyword") ? server.arg("keyword")
                                                   : (robot ? robot->m_config.getExConfig().wakeword.keyword : String(""));
    // 改行・引用符を除去
    new_keyword.replace("\"", "");
    new_keyword.replace("\n", "");
    new_keyword.replace("\r", "");

    const char* path = "/app/AiStackChanEx/SC_ExConfig.yaml";
    File f = SD.open(path, FILE_READ);
    if (!f) { server.send(500, "text/plain", "SC_ExConfig.yaml not found"); return; }
    String yaml;
    yaml.reserve(f.size() + 16);
    while (f.available()) yaml += (char)f.read();
    f.close();

    String new_yaml = update_wakeword_in_yaml(yaml, new_type, new_keyword);

    File w = SD.open(path, FILE_WRITE);
    if (!w) { server.send(500, "text/plain", "open for write failed"); return; }
    w.print(new_yaml);
    w.close();
    Serial.printf("Wakeword updated via Web: type=%d keyword=%s\n", new_type, new_keyword.c_str());
    changed = true;
  }

  int cur_type = robot ? robot->m_config.getExConfig().wakeword.type : 0;
  String cur_kw = robot ? robot->m_config.getExConfig().wakeword.keyword : String("");

  String body = "{";
  body += "\"type\":" + String(cur_type) + ",";
  body += "\"keyword\":\"" + json_escape(cur_kw) + "\",";
  body += "\"changed\":" + String(changed ? "true" : "false") + ",";
  body += "\"types\":[\"SimpleVox\",\"ModuleLLM-KWS\"],";
  body += "\"restart_required\":" + String(changed ? "true" : "false");
  body += "}";
  server.send(200, "application/json", body);
}

// 性格プリセット: GET でリスト + 現在の userRole、POST ?id=... で適用
void handle_personality() {
  // 適用
  bool applied = false;
  String applied_id = "";
  if (server.hasArg("id")) {
    String id = server.arg("id");
    const PersonalityPreset* p = PersonalityPresets::find_by_id(id.c_str());
    if (!p) { server.send(400, "text/plain", "unknown preset id"); return; }
    if (!robot || !robot->llm) { server.send(500, "text/plain", "robot/llm not ready"); return; }
    if (!robot->llm->save_userRole(String(p->role))) {
      server.send(500, "text/plain", "save_userRole failed");
      return;
    }
    applied = true;
    applied_id = id;
    Serial.printf("Personality preset applied: %s\n", id.c_str());
  }

  // レスポンス: list + current
  String cur_role = (robot && robot->llm) ? robot->llm->get_userRole() : String("");
  String body = "{\"presets\":[";
  const PersonalityPreset* p = PersonalityPresets::list();
  bool first = true;
  while (p->id != nullptr) {
    if (!first) body += ",";
    body += "{";
    body += "\"id\":\""; body += p->id; body += "\",";
    body += "\"name\":\""; body += p->name; body += "\",";
    body += "\"emoji\":\""; body += p->emoji; body += "\",";
    body += "\"description\":\""; body += p->description; body += "\"";
    body += "}";
    first = false;
    p++;
  }
  body += "],";
  body += "\"current_role\":\""; body += json_escape(cur_role); body += "\",";
  body += "\"applied\":" + String(applied ? "true" : "false");
  if (applied) {
    body += ",\"applied_id\":\""; body += applied_id; body += "\"";
  }
  body += "}";
  server.send(200, "application/json", body);
}

// LED テスト: ?r=NN&g=NN&b=NN（値なしは消灯）
void handle_led_test() {
  int r = server.arg("r").toInt();
  int g = server.arg("g").toInt();
  int b = server.arg("b").toInt();
  if (r < 0) r = 0; if (r > 255) r = 255;
  if (g < 0) g = 0; if (g > 255) g = 255;
  if (b < 0) b = 0; if (b > 255) b = 255;
  uint32_t rgb = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  if (rgb == 0) led_off();
  else          led_set(rgb);
  Serial.printf("LED test via Web: r=%d g=%d b=%d\n", r, g, b);
  String body = String("{\"r\":") + r + ",\"g\":" + g + ",\"b\":" + b + "}";
  server.send(200, "application/json", body);
}

// サーボテスト: ?x=NN&y=NN（中立からのオフセット度）
void handle_servo_test() {
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  // 安全な範囲にクランプ（±45 度）
  if (x < -45) x = -45; if (x > 45) x = 45;
  if (y < -45) y = -45; if (y > 45) y = 45;
  if (robot && robot->servo) {
    robot->servo->moveTo(x, y, 500);
  }
  Serial.printf("Servo test via Web: x=%d y=%d\n", x, y);
  String body = String("{\"x\":") + x + ",\"y\":" + y + "}";
  server.send(200, "application/json", body);
}

// 音テスト: ?type=sw|alarm（既存の関数を呼ぶ）
void handle_sound_test() {
  String t = server.arg("type");
  if (t.length() == 0) t = "sw";
  Serial.printf("Sound test via Web: type=%s\n", t.c_str());
  if (t == "alarm") alarm_tone();
  else              sw_tone();
  server.send(200, "application/json", String("{\"type\":\"") + t + "\"}");
}

// JSON 文字列エスケープ（". \\ 制御文字 を最小限処理）
static String json_escape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if ((uint8_t)c < 0x20) { out += "\\u00"; char buf[4]; snprintf(buf, sizeof(buf), "%02x", (uint8_t)c); out += buf; }
    else out += c;
  }
  return out;
}

static String trim_to(const String& s, size_t n) {
  if (s.length() <= n) return s;
  return s.substring(0, n) + String("…");
}

void handle_status_json() {
  String body;
  body.reserve(1024);
  body += "{";

  // System
  body += "\"system\":{";
  body += "\"fw_version\":\""; body += FW_VERSION; body += "\",";
  body += "\"uptime_ms\":"; body += String((unsigned long)millis()); body += ",";
  body += "\"chip\":\""; body += String(ESP.getChipModel()); body += "\",";
  body += "\"cpu_mhz\":"; body += String(ESP.getCpuFreqMHz()); body += ",";
  body += "\"free_heap\":"; body += String((unsigned long)ESP.getFreeHeap()); body += ",";
  body += "\"min_heap\":"; body += String((unsigned long)ESP.getMinFreeHeap());
  body += "},";

  // Power
  body += "\"power\":{";
  body += "\"battery_level\":"; body += String(M5.Power.getBatteryLevel()); body += ",";
  body += "\"charging\":"; body += (M5.Power.isCharging() ? "true" : "false"); body += ",";
  body += "\"voltage_mv\":"; body += String(M5.Power.getBatteryVoltage());
  body += "},";

  // Network
  body += "\"network\":{";
  body += "\"ssid\":\""; body += json_escape(WiFi.SSID()); body += "\",";
  body += "\"ip\":\""; body += WiFi.localIP().toString(); body += "\",";
  body += "\"rssi\":"; body += String(WiFi.RSSI()); body += ",";
  body += "\"mac\":\""; body += WiFi.macAddress(); body += "\"";
  body += "},";

  // Storage
  body += "\"storage\":{";
  body += "\"spiffs_total\":"; body += String((unsigned long)SPIFFS.totalBytes()); body += ",";
  body += "\"spiffs_used\":"; body += String((unsigned long)SPIFFS.usedBytes()); body += ",";
  body += "\"sd_total\":"; body += String((unsigned long long)SD.totalBytes()); body += ",";
  body += "\"sd_used\":"; body += String((unsigned long long)SD.usedBytes());
  body += "},";

  // Config（API キーは含めない）
  ex_config_s ex = robot ? robot->m_config.getExConfig() : ex_config_s{};
  body += "\"config\":{";
  body += "\"llm_type\":"; body += String(ex.llm.type); body += ",";
  body += "\"tts_type\":"; body += String(ex.tts.type); body += ",";
  body += "\"tts_voice\":\""; body += json_escape(ex.tts.voice); body += "\",";
  body += "\"tts_model\":\""; body += json_escape(ex.tts.model); body += "\",";
  body += "\"stt_type\":"; body += String(ex.stt.type); body += ",";
  body += "\"wakeword_type\":"; body += String(ex.wakeword.type);
  body += "},";

  // Role / Memory（先頭 200 文字まで）
  String role = (robot && robot->llm) ? robot->llm->get_userRole() : String("");
  String memory = (robot && robot->llm) ? robot->llm->get_userInfo() : String("");
  body += "\"role\":\""; body += json_escape(trim_to(role, 200)); body += "\",";
  body += "\"memory\":\""; body += json_escape(trim_to(memory, 200)); body += "\",";

  // Mute モード
  body += "\"mute\":"; body += (is_muted() ? "true" : "false"); body += ",";

  // Volume
  body += "\"volume\":"; body += String(robot ? (int)robot->spk_volume : 0);

  body += "}";
  server.send(200, "application/json", body);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
//  server.send(404, "text/plain", message);
  server.send(404, "text/html", String(HEAD) + String("<body>") + message + String("</body>"));
}

void handle_speech() {
  String message = server.arg("say");
  String speaker = server.arg("voice");
  //if(speaker != "") {
  //  TTS_PARMS = TTS_SPEAKER + speaker;
  //}
  Serial.println(message);
  ////////////////////////////////////////
  // 音声の発声
  ////////////////////////////////////////
  //avatar.setExpression(Expression::Happy);
  robot->speech(message);
  server.send(200, "text/plain", String("OK"));
}

void handle_chat() {
  static String response = "";
  // tts_parms_no = 1;
  String text = server.arg("text");
  String speaker = server.arg("voice");
  //if(speaker != "") {
  //  TTS_PARMS = TTS_SPEAKER + speaker;
  //}

  robot->chat(text);

  server.send(200, "text/html", String(HEAD)+String("<body>")+response+String("</body>"));
}

void handle_apikey() {
  // ファイルを読み込み、クライアントに送信する
  server.send(200, "text/html", APIKEY_HTML);
}

#if 0
void handle_apikey_set() {
  // POST以外は拒否
  if (server.method() != HTTP_POST) {
    return;
  }
  // openai
  String openai = server.arg("openai");
  // voicetxt
  String voicevox = server.arg("voicevox");
  // voicetxt
  String sttapikey = server.arg("sttapikey");
 
  OPENAI_API_KEY = openai;
  VOICEVOX_API_KEY = voicevox;
  STT_API_KEY = sttapikey;
  Serial.println(openai);
  Serial.println(voicevox);
  Serial.println(sttapikey);

  uint32_t nvs_handle;
  if (ESP_OK == nvs_open("apikey", NVS_READWRITE, &nvs_handle)) {
    nvs_set_str(nvs_handle, "openai", openai.c_str());
    nvs_set_str(nvs_handle, "voicevox", voicevox.c_str());
    nvs_set_str(nvs_handle, "sttapikey", sttapikey.c_str());
    nvs_close(nvs_handle);
  }
  server.send(200, "text/plain", String("OK"));
}
#endif

void handle_role_set() {
  String html = "";

  // POST以外は拒否
  if (server.method() != HTTP_POST) {
    return;
  }
  String role = server.arg("plain");

  // JSONデータをSPIFFSに保存
  if(robot->llm->save_userRole(role)){
#if 0
    // 整形したJSONデータを出力するHTMLデータを作成する
    serializeJsonPretty(robot->llm->get_chat_doc(), html);
    html = "<html><body><pre>" + html + "</pre></body></html>";
    //Serial.println(html);
#endif
    server.send(200, "text/plain", String("Role set successful"));
  }
  else{
    //html = "Failed to save role to SPIFFS.";
    server.send(500, "text/plain", String("Role set failed"));
  }

  // HTMLデータをシリアルに出力する
  //server.send(200, "text/html", html);
};

void handle_role_get() {
#if 0
  String html = "";
  serializeJsonPretty(robot->llm->get_chat_doc(), html);
  html = "<html><body><pre>" + html + "</pre></body></html>";

  // HTMLデータをシリアルに出力する
  //Serial.println(html);
  server.send(200, "text/html", String(HEAD) + html);
#endif
  Serial.println("http request: handle_role_get");
  Serial.println(robot->llm->get_userRole());
  server.send(200, "text/plain", robot->llm->get_userRole());
};

void handle_memory_get() {
  Serial.println("http request: handle_memory_get");
  Serial.println(robot->llm->get_userInfo());
  server.send(200, "text/plain", robot->llm->get_userInfo());
};

void handle_memory_clear() {
  Serial.println("http request: handle_memory_clear");
  bool result = robot->llm->clear_userInfo();
  if(result){
    server.send(200, "text/plain", String("Memory clear successful"));
  }else{
    server.send(500, "text/plain", String("Memory clear failed"));
  }
};

void handle_face() {
  String expression = server.arg("expression");
  expression = expression + "\n";
  Serial.println(expression);
  switch (expression.toInt())
  {
    case 0: avatar.setExpression(Expression::Neutral); break;
    case 1: avatar.setExpression(Expression::Happy); break;
    case 2: avatar.setExpression(Expression::Sleepy); break;
    case 3: avatar.setExpression(Expression::Doubt); break;
    case 4: avatar.setExpression(Expression::Sad); break;
    case 5: avatar.setExpression(Expression::Angry); break;
    case 6: avatar.setExpression(Expression::Embarrassed); break;
    case 7: avatar.setExpression(Expression::HeartEyes); break;
    case 8: avatar.setExpression(Expression::Surprised); break;
  }
  server.send(200, "text/plain", String("OK"));
}

#if 0
void handle_setting() {
  String value = server.arg("volume");
  String led = server.arg("led");
  String speaker = server.arg("speaker");
//  volume = volume + "\n";
  Serial.println(speaker);
  Serial.println(value);
  size_t speaker_no;

  if(speaker != ""){
    speaker_no = speaker.toInt();
    if(speaker_no > 60) {
      speaker_no = 60;
    }
    TTS_SPEAKER_NO = String(speaker_no);
    TTS_PARMS = TTS_SPEAKER + TTS_SPEAKER_NO;
  }

  if(value == "") value = "180";
  size_t volume = value.toInt();
  uint8_t led_onoff = 0;
  uint32_t nvs_handle;
  if (ESP_OK == nvs_open("setting", NVS_READWRITE, &nvs_handle)) {
    if(volume > 255) volume = 255;
    nvs_set_u32(nvs_handle, "volume", volume);
    if(led != "") {
      if(led == "on") led_onoff = 1;
      else  led_onoff = 0;
      nvs_set_u8(nvs_handle, "led", led_onoff);
    }
    nvs_set_u8(nvs_handle, "speaker", speaker_no);

    nvs_close(nvs_handle);
  }
  M5.Speaker.setVolume(volume);
  M5.Speaker.setChannelVolume(m5spk_virtual_channel, volume);
  server.send(200, "text/plain", String("OK"));
}
#endif


void init_web_server(void)
{
  // Files
  //
  server.on("/", handleRoot);
  server.on("/personalize.html", handle_personalize_html);
  server.on("/personalize.js", handle_personalize_js);
  server.on("/dashboard.html", handle_dashboard_html);
  server.on("/dashboard.js", handle_dashboard_js);
  server.on("/api/status", handle_status_json);
  // GET/POST 両対応（value 引数有無で取得/設定を切替）
  server.on("/api/mute", HTTP_GET, handle_mute);
  server.on("/api/mute", HTTP_POST, handle_mute);
  server.on("/api/volume", HTTP_GET, handle_volume);
  server.on("/api/volume", HTTP_POST, handle_volume);
  server.on("/api/wakeword", HTTP_GET, handle_wakeword);
  server.on("/api/wakeword", HTTP_POST, handle_wakeword);
  server.on("/api/personality", HTTP_GET, handle_personality);
  server.on("/api/personality", HTTP_POST, handle_personality);
  // アクション系（テスト用）
  server.on("/api/led", HTTP_POST, handle_led_test);
  server.on("/api/led", HTTP_GET, handle_led_test);     // ブラウザから直接叩けるよう GET も許可
  server.on("/api/servo-test", HTTP_POST, handle_servo_test);
  server.on("/api/servo-test", HTTP_GET, handle_servo_test);
  server.on("/api/sound-test", HTTP_POST, handle_sound_test);
  server.on("/api/sound-test", HTTP_GET, handle_sound_test);
  // YAML 設定編集
  server.on("/api/config", HTTP_GET, handle_config_get);
  server.on("/api/config", HTTP_POST, handle_config_post);
  // 再起動
  server.on("/api/restart", HTTP_POST, handle_restart);
  // Settings ページ
  server.on("/settings.html", []() {
    server.send_P(200, "text/html", (const char*)settings_html, (size_t)sizeof_settings_html);
  });
  server.on("/settings.js", []() {
    server.send_P(200, "application/javascript", (const char*)settings_js, (size_t)sizeof_settings_js);
  });
  // Present ページ（テキスト → 発話）
  server.on("/present.html", []() {
    server.send_P(200, "text/html", (const char*)present_html, (size_t)sizeof_present_html);
  });
  server.on("/present.js", []() {
    server.send_P(200, "application/javascript", (const char*)present_js, (size_t)sizeof_present_js);
  });


  // APIs
  //
  server.on("/speech", handle_speech);
  server.on("/face", handle_face);
  server.on("/chat", handle_chat);
  server.on("/apikey", handle_apikey);
  //server.on("/setting", handle_setting);
  //server.on("/apikey_set", HTTP_POST, handle_apikey_set);
  server.on("/role_set", HTTP_POST, handle_role_set);
  server.on("/role_get", handle_role_get);
  server.on("/memory_get", handle_memory_get);
  server.on("/memory_clear", handle_memory_clear);

  // Other
  //
  server.onNotFound(handleNotFound);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.begin();
  Serial.println("HTTP server started");
  M5.Lcd.println("HTTP server started");  
}

void web_server_handle_client(void)
{
  server.handleClient();
}
