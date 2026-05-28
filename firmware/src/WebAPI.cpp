#include <ESP32WebServer.h>
#include <nvs.h>
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
  body += "\"mute\":"; body += (is_muted() ? "true" : "false");

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
