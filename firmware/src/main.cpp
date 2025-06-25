#include <Arduino.h>
//#include <FS.h>
#include <SD.h>
#include <SPIFFS.h>
#include "SDUtil.h"
#include <M5Unified.h>
#include <nvs.h>
#include <Avatar.h>
#include "StackchanExConfig.h" 
#include "Robot.h"
#include "mod/ModManager.h"
#include "mod/ModBase.h"
#include "mod/AiStackChan/AiStackChanMod.h"
#include "mod/Pomodoro/PomodoroMod.h"
#include "mod/PhotoFrame/PhotoFrameMod.h"
#include "mod/StatusMonitor/StatusMonitorMod.h"
#include "mod/VolumeSetting/VolumeSettingMod.h"

#include "driver/PlayMP3.h"   //lipSync

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
//#include <ESP32WebServer.h>
//#include <ESPmDNS.h>
#include <ESP8266FtpServer.h>

#define REALTIME_API
#ifdef REALTIME_API
#define DEBUG_WEBSOCKETS
#define DEBUG_ESP_PORT Serial
#include <WebSocketsClient.h>
#include "rootCA/rootCACertificate.h"
#endif

//#include <deque>

#if defined(ENABLE_WAKEWORD)
#include "driver/WakeWord.h"
#include "driver/WakeWordIndex.h"
#endif

#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "llm/ChatHistory.h"

#include "WebAPI.h"

#if defined( ENABLE_CAMERA )
#include "driver/Camera.h"
#endif    //ENABLE_CAMERA

#include "driver/WatchDog.h"
#include "SDUpdater.h"

StackchanExConfig system_config;
Robot* robot;
bool isOffline = false;


// NTP接続情報　NTP connection information.
const char* NTPSRV      = "ntp.jst.mfeed.ad.jp";    // NTPサーバーアドレス NTP server address.
const long  GMT_OFFSET  = 9 * 3600;                 // GMT-TOKYO(時差９時間）9 hours time difference.
const int   DAYLIGHT_OFFSET = 0;                    // サマータイム設定なし No daylight saving time setting

/// 関数プロトタイプ宣言 /// 
void check_heap_free_size(void);
void check_heap_largest_free_block(void);

//bool servo_home = false;
bool servo_home = true;

using namespace m5avatar;
Avatar avatar;
const Expression expressions_table[] = {
  Expression::Neutral,
  Expression::Happy,
  Expression::Sleepy,
  Expression::Doubt,
  Expression::Sad,
  Expression::Angry
};

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial



#ifdef REALTIME_API
WebSocketsClient webSocket;

//#include <base64.h>
#include "libb64/cdecode.h"
//String audio_delta1 = R"({"type":"response.audio.delta","event_id":"event_BiKJq19lARGrZNAFZrZD4","response_id":"resp_BiKJpDdSMQtdshxQy3rLd","item_id":"item_BiKJpbx1CnEus5VUwQoDA","output_index":0,"content_index":0,"delta":"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"})";
//String audio_delta2 = R"({"type":"response.audio.delta","event_id":"event_BiKJqNH8yZpbEsSD94woG","response_id":"resp_BiKJpDdSMQtdshxQy3rLd","item_id":"item_BiKJpbx1CnEus5VUwQoDA","output_index":0,"content_index":0,"delta":"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"})";

//DynamicJsonDocument msgDoc(1024*10);
SpiRamJsonDocument msgDoc(0);

char session_update[] =
      "{"
        "\"type\": \"session.update\","
        "\"session\": {"
          //"\"turn_detection\": null,"
          "\"input_audio_transcription\": {"
            "\"model\": \"whisper-1\","
            "\"language\": \"ja\""
          "},"
          //"\"voice\":\"alloy\","
          "\"voice\":\"sage\","
          //"\"output_audio_format\":\"g711_ulaw\","    //pcm16よりだいぶ軽いので使いたかったが、再生できるライブラリがなかった
          "\"instructions\": \"You are an AI robot named Stack-chan. Please speak in Japanese.\""
        "}"
      "}";

extern bool realtime_recording;
extern bool realtime_api_debug_log;

extern uint8_t* audioBuf[];   // Base64をデコードして得た音声データを格納するバッファ。再生直後に更新すると音が切れたのでダブルバッファとした
extern int nextBufIdx;     // 次回データを格納するダブルバッファの面（0 or 1）

int base64_decode(const char* input, int size, char* output)
{
	/* keep track of our decoded position */
	char* c = output;
	/* store the number of bytes decoded by a single call */
	int cnt = 0;
	/* we need a decoder state */
	base64_decodestate s;
	
	/*---------- START DECODING ----------*/
	/* initialise the decoder state */
	base64_init_decodestate(&s);
	/* decode the input data */
	cnt = base64_decode_block(input, strlen(input), c, &s);
	c += cnt;
	/* note: there is no base64_decode_blockend! */
	/*---------- STOP DECODING  ----------*/
	
	/* we want to print the decoded data, so null-terminate it: */
	*c = 0;
	
	return cnt;
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*) mem;
	Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		Serial.printf("%02X ", *src);
		src++;
	}
	Serial.printf("\n");
}


void streamAudioDelta(String& delta)
{
  int base64Size = delta.length();
  Serial.printf("audio base64 size: %d byte\n", base64Size);
  //char* buf = nullptr;                // デコード後のPCM16を格納するバッファ
  //buf = (char*)malloc(base64Size);    // デコード結果はBASE64データのサイズよりも小さくなる
  //if(buf == nullptr){
  //  Serial.println("base64_decode: malloc failed.");
  //}
  uint8_t* buf = audioBuf[nextBufIdx];
  int len = base64_decode(delta.c_str(), base64Size, (char*)buf);
  Serial.printf("audio pcm16 size: %d byte\n", len);
  
  while (M5.Speaker.isPlaying()) { /*vTaskDelay(1);*/ }
  M5.Speaker.playRaw((int16_t*)buf, len/2, 24000, false);
  nextBufIdx ^= 1;  //ダブルバッファを切り替え

}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  String msgType, delta;
  DeserializationError error;
	switch(type) {
		case WStype_DISCONNECTED:
			Serial.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			Serial.printf("[WSc] Connected to url: %s\n", payload);

			// send message to server when Connected
			webSocket.sendTXT(session_update);
			break;
		case WStype_TEXT:
			//Serial.printf("[WSc] get text: %s\n", payload);
			Serial.printf("[WSc] text size: %d\n", strlen((char*)payload));

      error = deserializeJson(msgDoc, payload);
      if (error) {
        Serial.printf("WebSocket Event: JSON deserialization error %d\n", error.code());
      }

      msgType = msgDoc["type"].as<String>();
			Serial.printf("[WSc] text type: %s\n", msgType.c_str());
      if(msgType.equals("response.audio_transcript.delta")){
        delta = msgDoc["delta"].as<String>();
        Serial.printf("[WSc] delta: %s\n", delta.c_str());
      }
      else if(msgType.equals("response.audio.delta")){
        delta = msgDoc["delta"].as<String>();
        streamAudioDelta(delta);
      }
      else if(msgType.equals("input_audio_buffer.committed")){
        realtime_recording = false;
        M5.Mic.end();
        M5.Speaker.begin();
        Serial.printf("[WSc] input audio committed\n");
      }
      else if(msgType.equals("response.done")){
        realtime_recording = true;
        //realtime_api_debug_log = true;
        while (M5.Speaker.isPlaying()) { /*vTaskDelay(1);*/ }
        M5.Speaker.end();
        M5.Mic.begin();
        for(int i=0; i<2; i++){
          memset(audioBuf[i], 0, 100 * 1024);
        }
        Serial.printf("[WSc] response.done\n");
      }
      else if(msgType.equals("rate_limits.updated")){
        //Serial.printf("[WSc] payload: %s\n", payload);
      }

			break;
		case WStype_BIN:
			Serial.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
 			Serial.printf("[WSc] payload: %s\n", payload);		
      break;
    default:
			Serial.printf("[WSc] Unknown event\n");
      //Serial.printf("[WSc] payload: %s\n", payload);
      break;
	}

  //check_heap_largest_free_block();

}

#endif  //REALTIME_API



// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}


void lipSync(void *args)
{
  float gazeX, gazeY;
  int level = 0;
  DriveContext *ctx = (DriveContext *)args;
  Avatar *avatar = ctx->getAvatar();
  for (;;)
  {
#ifdef REALTIME_API
    level = abs(*audioBuf[nextBufIdx ^ 1]) * 50;
#else
    level = robot->tts->getLevel();
#endif
    if(level<100) level = 0;
    if(level > 15000)
    {
      level = 15000;
    }
    float open = (float)level/15000.0;
    avatar->setMouthOpenRatio(open);
    avatar->getGaze(&gazeY, &gazeX);
    avatar->setRotation(gazeX * 5);
    delay(50);
  }
}


void servo(void *args)
{
  float gazeX, gazeY;
  DriveContext *ctx = (DriveContext *)args;
  Avatar *avatar = ctx->getAvatar();
  for (;;)
  {
#ifdef USE_SERVO
    if(!servo_home)
    {
      avatar->getGaze(&gazeY, &gazeX);
      robot->servo->moveToGaze((int)(15.0 * gazeX), (int)(10.0 * gazeY));
    } else {
      robot->servo->moveToOrigin();
    }
#endif
    delay(50);
  }
}


//void Wifi_setup() {
bool Wifi_connection_check() {
  unsigned long start_millis = millis();

  // 前回接続時情報で接続する
  while (WiFi.status() != WL_CONNECTED) {
    M5.Display.print(".");
    Serial.print(".");
    delay(500);
    // 10秒以上接続できなかったら抜ける
    if ( 10000 < (millis() - start_millis) ) {
      //break;
      return false;
    }
  }
  return true;

#if 0
  M5.Display.println("");
  Serial.println("");
  // 未接続の場合にはSmartConfig待受
  if ( WiFi.status() != WL_CONNECTED ) {
    WiFi.mode(WIFI_STA);
    WiFi.beginSmartConfig();
    M5.Display.println("Waiting for SmartConfig");
    Serial.println("Waiting for SmartConfig");
    while (!WiFi.smartConfigDone()) {
      delay(500);
      M5.Display.print("#");
      Serial.print("#");
      // 30秒以上接続できなかったら抜ける
      if ( 30000 < millis() ) {
        Serial.println("");
        Serial.println("Reset");
        ESP.restart();
      }
    }

    // Wi-fi接続
    M5.Display.println("");
    Serial.println("");
    M5.Display.println("Waiting for WiFi");
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      M5.Display.print(".");
      Serial.print(".");
      // 60秒以上接続できなかったら抜ける
      if ( 60000 < millis() ) {
        Serial.println("");
        Serial.println("Reset");
        ESP.restart();
      }
    }
  }
#endif
}

void time_sync(const char* ntpsrv, long gmt_offset, int daylight_offset) {
  struct tm timeInfo; 
  char buf[60];

  configTime(gmt_offset, daylight_offset, ntpsrv);          // NTPサーバと同期

  if (getLocalTime(&timeInfo)) {                            // timeinfoに現在時刻を格納
    Serial.print("NTP : ");                                 // シリアルモニターに表示
    Serial.println(ntpsrv);                                 // シリアルモニターに表示

    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d\n",     // 表示内容の編集
    timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
    timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

    Serial.println(buf);                                    // シリアルモニターに表示
  }
  else {
    Serial.print("NTP Sync Error ");                        // シリアルモニターに表示
  }
}



ModBase* init_mod(void)
{
  ModBase* mod;
  if(!isOffline || robot->isAllOfflineService()){
    add_mod(new AiStackChanMod(isOffline));
  }
  //add_mod(new PomodoroMod(isOffline));
  //add_mod(new PhotoFrameMod(isOffline));
  add_mod(new StatusMonitorMod());
  add_mod(new VolumeSettingMod());
  mod = get_current_mod();
  mod->init();
  return mod;
}


void sw_tone()
{
  M5.Mic.end();
  M5.Speaker.begin();

  M5.Speaker.tone(1000, 100);
  delay(500);

  M5.Speaker.end();
  M5.Mic.begin();
}
  
void alarm_tone()
{
  M5.Mic.end();
  M5.Speaker.begin();

  for(int i=0; i<5; i++){
    M5.Speaker.tone(1200, 50);
    delay(100);
    M5.Speaker.tone(1200, 50);
    delay(100);
    M5.Speaker.tone(1200, 50);
    delay(1000);  
  }

  M5.Speaker.end();
  M5.Mic.begin();
}


void setup()
{
  auto cfg = M5.config();

  cfg.external_spk = true;    /// use external speaker (SPK HAT / ATOMIC SPK)
//cfg.external_spk_detail.omit_atomic_spk = true; // exclude ATOMIC SPK
//cfg.external_spk_detail.omit_spk_hat    = true; // exclude SPK HAT
//  cfg.output_power = true;
  cfg.serial_baudrate = 115200;   //M5Unified 0.1.17からデフォルトが0になったため設定
  M5.begin(cfg);

  /// シリアル出力のログレベルを VERBOSEに設定
  //M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);

#if defined(ENABLE_SD_UPDATER)
  // ***** for SD-Updater *********************
  SDU_lobby("AiStackChanEx");
  // ******************************************
#endif

  //auto brightness = M5.Display.getBrightness();
  //Serial.printf("Brightness: %d\n", brightness);

  {
    auto micConfig = M5.Mic.config();
    //micConfig.stereo = false;
    micConfig.sample_rate = 16000;
    M5.Mic.config(micConfig);
  }
  M5.Mic.begin();

  { /// custom setting
    auto spk_cfg = M5.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 64000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;
    //spk_cfg.dma_buf_count = 8;
    //spk_cfg.dma_buf_len = 128;
    M5.Speaker.config(spk_cfg);
  }
  //M5.Speaker.begin();

#ifdef REALTIME_API
  msgDoc = SpiRamJsonDocument(1024*150);
#endif

//#ifdef REALTIME_API
#if 0
  M5.Mic.end();
  M5.Speaker.begin();
  DynamicJsonDocument doc(1024*10);
  DeserializationError error = deserializeJson(doc, audio_delta1.c_str());
  if (error) {
    Serial.println("audio_delta: JSON deserialization error");
  }

  streamAudioDelta(doc);

  error = deserializeJson(doc, audio_delta2.c_str());
  if (error) {
    Serial.println("audio_delta: JSON deserialization error");
  }

  streamAudioDelta(doc);

#if 0
  String delta = doc["delta"].as<String>();
  //Serial.print(delta);
  int base64Size = delta.length();
  Serial.printf("audio base64 size: %d byte\n", base64Size);
  char* buf = nullptr;
  buf = (char*)malloc(base64Size);
  if(buf == nullptr){
    Serial.println("base64_decode: malloc failed.");
  }
  int len = base64_decode(delta.c_str(), base64Size, buf);
  Serial.printf("audio pcm16 size: %d byte\n", len);
  
  while (M5.Speaker.isPlaying()) { vTaskDelay(1); }
  M5.Mic.end();
  M5.Speaker.begin();
  M5.Speaker.playRaw((int16_t*)buf, len/2, 24000, false);
#endif
#endif

//#endif  //REALTIME_API

  M5.Lcd.setTextSize(2);


  /// settings
  if (SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    // この関数ですべてのYAMLファイル(Basic, Secret, Extend)を読み込む
    system_config.loadConfig(SD, "/app/AiStackChanEx/SC_ExConfig.yaml");

    // Wifi設定読み込み
    wifi_s* wifi_info = system_config.getWiFiSetting();
    Serial.printf("\nSSID: %s\n",wifi_info->ssid.c_str());
    Serial.printf("Key: %s\n",wifi_info->password.c_str());

    if(wifi_info->ssid.length() == 0){
      M5.Lcd.print("Can't get WiFi settings. Start offline mode.\n");
      isOffline = true;
    }
    else{
      //WiFi設定を読み込めた場合のみネットワーク関連の設定を行う。

      Serial.println("Connecting to WiFi");
      WiFi.disconnect();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifi_info->ssid.c_str(), wifi_info->password.c_str());
      if(Wifi_connection_check()){
        M5.Lcd.println("\nConnected");
        Serial.printf_P(PSTR("Go to http://"));
        M5.Lcd.print("Go to http://");
        Serial.println(WiFi.localIP());
        M5.Lcd.println(WiFi.localIP());
        delay(1000);
        
        //Webサーバ設定
        init_web_server();
        //FTPサーバ設定（SPIFFS用）
        ftpSrv.begin("stackchan","stackchan");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
        Serial.println("FTP server started");
        M5.Lcd.println("FTP server started");
        //時刻同期
        time_sync(NTPSRV, GMT_OFFSET, DAYLIGHT_OFFSET);

#ifdef REALTIME_API
        webSocket.beginSslWithCA("api.openai.com", 443, "/v1/realtime?model=gpt-4o-realtime-preview-2025-06-03", root_ca_openai);
        // event handler
        webSocket.onEvent(webSocketEvent);

        // use HTTP Basic Authorization this is optional remove if not needed
        //webSocket.setAuthorization("user", "Password");
        webSocket.setAuthorization("Bearer YOUR_API_KEY");
        webSocket.setExtraHeaders("OpenAI-Beta: realtime=v1");

        // try ever 5000 again if connection has failed
        webSocket.setReconnectInterval(5000);

#endif

      }
      else{
        M5.Lcd.print("Can't connect to WiFi. Start offline mode.\n");
        isOffline = true;
        delay(3000);
      }
  
    }

    robot = new Robot(system_config);

    //SD.end();
  } else {
    M5.Lcd.print("Failed to load SD card settings. System reset after 5 seconds.");
    delay(5000);
    ESP.restart();
    //WiFi.begin();
  }
  
  mp3_init();

  //TODO: クラス化してAiStackChanModの初期化に移動したい
  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    // Nothing to initialize here
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    wakeword_init();
#endif
  }

  //mod設定
  init_mod();

//  avatar.init();
  avatar.init(16);

  avatar.addTask(lipSync, "lipSync");
  avatar.addTask(servo, "servo");
  avatar.setSpeechFont(&fonts::efontJA_16);

  robot->spk_volume = 120;
  M5.Speaker.setVolume(robot->spk_volume);

#if defined(ENABLE_CAMERA)
  camera_init();
  avatar.set_isSubWindowEnable(true);
#endif

  //init_watchdog();

  //ヒープメモリ残量確認(デバッグ用)
  check_heap_free_size();
  check_heap_largest_free_block();

}



void loop()
{

  M5.update();
  ModBase* mod = get_current_mod();
  
  mod->idle();

#ifdef REALTIME_API
 	webSocket.loop();
#endif

  if (M5.BtnA.wasPressed())
  {
    mod->btnA_pressed();
  }

  if (M5.BtnB.wasPressed())
  {
    mod->btnB_pressed();
  }

  if (M5.BtnB.pressedFor(2000))
  {
    mod->btnB_longPressed();
  }

  if (M5.BtnC.wasPressed())
  {
    mod->btnC_pressed();
  }

#if defined(ARDUINO_M5STACK_Core2) || defined( ARDUINO_M5STACK_CORES3 )
  auto count = M5.Touch.getCount();
  if (count)
  {
    auto t = M5.Touch.getDetail();
    if (t.wasPressed())
    {
      mod->display_touched(t.x, t.y);
    }

    if (t.wasFlicked())
    {
      int16_t dx = t.distanceX();
      int16_t dy = t.distanceY();

      // detect flick right/left
      if(abs(dx) >= abs(dy))
      {
        if(dx > 0){
          //Serial.println("Right flicked");
          change_mod(true);
        }
        else{
          //Serial.println("Left flicked");
          change_mod();
        }
      }
    }
  }
#endif

  if(!isOffline){
    web_server_handle_client();
    ftpSrv.handleFTP();
  }
  
  if(M5.Power.getBatteryLevel() < 95){
    avatar.setBatteryIcon(true);
    avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
  }
  else{
    avatar.setBatteryIcon(false);    
  }

  //reset_watchdog();
  
}


void check_heap_free_size(void){
  Serial.printf("===============================================================\n");
  Serial.printf("Check free heap size\n");
  Serial.printf("===============================================================\n");
  //Serial.printf("esp_get_free_heap_size()                              : %6d\n", esp_get_free_heap_size() );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DMA)               : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_SPIRAM)            : %6d\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_INTERNAL)          : %6d\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) );
  Serial.printf("heap_caps_get_free_size(MALLOC_CAP_DEFAULT)           : %6d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) );

}

void check_heap_largest_free_block(void){
  Serial.printf("===============================================================\n");
  Serial.printf("Check largest free heap block\n");
  Serial.printf("===============================================================\n");
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DMA)      : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)   : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) );
  Serial.printf("heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)  : %6d\n", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) );

}
