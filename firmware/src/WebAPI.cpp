#include <ESP32WebServer.h>
#include <nvs.h>
#include "WebAPI.h"
#include "Avatar.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "Robot.h"

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


void handleRoot() {
  server.send(200, "text/plain", "hello from m5stack!");
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

void handle_role() {
  // ファイルを読み込み、クライアントに送信する
  //server.send(200, "text/html", ROLE_HTML);
  server.send(200, "text/html", (const char*)index_html);
}


/**
 * アプリからテキスト(文字列)と共にRoll情報が配列でPOSTされてくることを想定してJSONを扱いやすい形に変更
 * 出力形式をJSONに変更
*/
void handle_role_set() {
  String html = "";

  // POST以外は拒否
  if (server.method() != HTTP_POST) {
    return;
  }
  String role = server.arg("plain");

  // JSONデータをSPIFFSに保存
  if(robot->llm->save_role(role)){
    // 整形したJSONデータを出力するHTMLデータを作成する
    serializeJsonPretty(robot->llm->get_chat_doc(), html);
    html = "<html><body><pre>" + html + "</pre></body></html>";
  }
  else{
    html = "Failed to save role to SPIFFS.";
  }

  // HTMLデータをシリアルに出力する
  //Serial.println(html);
  server.send(200, "text/html", html);
};

// 整形したJSONデータを出力するHTMLデータを作成する
void handle_role_get() {

  String html = "";
  serializeJsonPretty(robot->llm->get_chat_doc(), html);
  html = "<html><body><pre>" + html + "</pre></body></html>";

  // HTMLデータをシリアルに出力する
  //Serial.println(html);
  server.send(200, "text/html", String(HEAD) + html);
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

  server.on("/", handleRoot);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  // And as regular external functions:
  server.on("/speech", handle_speech);
  server.on("/face", handle_face);
  server.on("/chat", handle_chat);
  server.on("/apikey", handle_apikey);
  //server.on("/setting", handle_setting);
  //server.on("/apikey_set", HTTP_POST, handle_apikey_set);
  server.on("/role", handle_role);
  server.on("/role_set", HTTP_POST, handle_role_set);
  server.on("/role_get", handle_role_get);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
  M5.Lcd.println("HTTP server started");  
}

void web_server_handle_client(void)
{
  server.handleClient();
}
