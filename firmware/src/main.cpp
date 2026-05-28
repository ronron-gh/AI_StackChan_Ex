#include <Arduino.h>
//#include <FS.h>
#include <SD.h>
#include <SPIFFS.h>
#include "share/Version.h"
#include "share/Mutex.h"
#include "share/SDUtil.h"
#include "share/DefaultParams.h"
#include <M5Unified.h>
#include <nvs.h>
#include <Preferences.h>
#include <Avatar.h>
#include <faces/CatFace.h>
#include "StackchanExConfig.h" 
#include "Robot.h"
#include "mod/ModManager.h"
#include "mod/ModBase.h"
#include "mod/AiStackChan/AiStackChanMod.h"
#include "mod/AiStackChan/RealtimeAiMod.h"
#include "mod/Pomodoro/PomodoroMod.h"
#include "mod/PhotoFrame/PhotoFrameMod.h"
#include "mod/StatusMonitor/StatusMonitorMod.h"
#include "mod/VolumeSetting/VolumeSettingMod.h"
#include "mod/QRdisplay/QRdisplayMod.h"
#include "mod/EspNowRemote/EspNowRemoteMod.h"

#include "driver/PlayMP3.h"   //lipSync
#include "driver/TapDetect.h"
#include "driver/HeadPetDetect.h"
#include "driver/IdleMotion.h"
#include "share/Phrases.h"
#include "share/SerialConfig.h"

#define FASTLED_INTERNAL  // 起動バナーログを抑制
#include <FastLED.h>

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include <ESP8266FtpServer.h>

#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "llm/ChatHistory.h"
#include "llm/Gemini/GeminiLive.h"

#include "WebAPI.h"

#if defined( ENABLE_CAMERA )
#include "driver/Camera.h"
#endif    //ENABLE_CAMERA

#include "driver/WatchDog.h"
#include "SDUpdater.h"
#include "DebugTools.h"

#if defined(USE_AUDIO_MODULE)
#include "driver/M5AudioModule.h"
#endif

StackchanExConfig system_config;
Robot* robot;
bool isOffline = false;

// Mute モード（外出時等に TTS / 効果音をスキップ）
// 揮発性: 再起動でリセット
static volatile bool g_mute = false;
bool is_muted() { return g_mute; }
void set_mute(bool m) {
  g_mute = m;
  Serial.printf("Mute mode: %s\n", m ? "ON" : "OFF");
}

// M5StackChan サーボベース搭載 WS2812C RGB LED (PY32 IOExpander 経由で制御)
// LED は robot->servo (= ServoCustom) の PY32 を介して操作する
void led_init() {
  // robot 初期化前は何もしない。led_set/off 側で robot を見て呼ぶ
}

// LED 自動消灯タイマー（TTS ハング時の保険）
// TTS 再生は main loop をブロックするため、別 FreeRTOS タスクで監視する
static volatile unsigned long g_led_off_at_ms = 0;
static const uint32_t LED_AUTO_OFF_MS = 20UL * 1000UL;  // 20秒

// 呼吸アニメ用
static volatile bool g_led_breathing = false;
static volatile uint32_t g_led_breath_base_rgb = 0;
static unsigned long g_led_breath_start_ms = 0;
static const uint32_t LED_BREATH_PERIOD_MS = 1500;  // 周期 1.5秒

void led_set(uint32_t rgb) {
  if (!robot || !robot->servo) return;
  g_led_breathing = false;   // 呼吸モード解除（固定色）
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;
  robot->servo->fillLeds(r, g, b);
  g_led_off_at_ms = millis() + LED_AUTO_OFF_MS;
  Serial.printf("led_set(0x%06X), auto_off_at=%lu\n", rgb, g_led_off_at_ms);
}

// 呼吸アニメ点灯（応答処理中など、「動いてる感」を出したい時用）
// 応答処理は TTS 含めて 30 秒以上かかることがあるため auto-off は 60 秒に延長
void led_breath(uint32_t rgb) {
  if (!robot || !robot->servo) return;
  g_led_breath_base_rgb = rgb;
  g_led_breath_start_ms = millis();
  g_led_breathing = true;
  g_led_off_at_ms = millis() + 60UL * 1000UL;  // 呼吸時は 60 秒
  Serial.printf("led_breath(0x%06X)\n", rgb);
}

void led_off() {
  if (!robot || !robot->servo) return;
  g_led_breathing = false;
  robot->servo->clearLeds();
  g_led_off_at_ms = 0;
}

// 別タスクで LED watchdog（main loop ブロック時でも動く）+ 呼吸アニメ
// PY32 への I2C 競合を避けるため、breathing 時のみ細かく動く 2 段周期
static void led_watchdog_task(void *param) {
  Serial.println("LED watchdog task started");
  uint32_t heartbeat = 0;
  for (;;) {
    // breathing 中だけ細かく更新（240ms ≒ 1.5sec / 6step）
    // 非 breathing 時は I2C を叩かないよう 1 秒待機
    vTaskDelay(pdMS_TO_TICKS(g_led_breathing ? 240 : 1000));
    heartbeat++;

    // ハングアウトオフ
    unsigned long off_at = g_led_off_at_ms;
    if (off_at != 0 && millis() > off_at) {
      Serial.printf("LED auto-off triggered at %lu\n", millis());
      g_led_breathing = false;
      if (robot && robot->servo) {
        robot->servo->clearLeds();
      }
      g_led_off_at_ms = 0;
      continue;
    }

    // 呼吸アニメ
    if (g_led_breathing && robot && robot->servo) {
      uint32_t base = g_led_breath_base_rgb;
      uint8_t br = (base >> 16) & 0xFF;
      uint8_t bg = (base >> 8) & 0xFF;
      uint8_t bb = base & 0xFF;
      // 0..1 の周期、サインで 0.5..1.0
      float t = (float)((millis() - g_led_breath_start_ms) % LED_BREATH_PERIOD_MS)
                / (float)LED_BREATH_PERIOD_MS;
      float bright = 0.5f + 0.5f * sinf(t * 2.0f * 3.14159265f);
      uint8_t r = (uint8_t)(br * bright);
      uint8_t g = (uint8_t)(bg * bright);
      uint8_t b = (uint8_t)(bb * bright);
      robot->servo->fillLeds(r, g, b);
    }

    // 30秒に1回ハートビートだけログ
    if (heartbeat % 200 == 0) {
      Serial.printf("LED watchdog alive (breathing=%d, off_at=%lu, now=%lu)\n",
                    g_led_breathing ? 1 : 0, g_led_off_at_ms, millis());
    }
  }
}

void led_auto_off_tick() {
  // 互換性のためのスタブ（実体は led_watchdog_task）
}

// idle タイムアウト管理（5分操作なしで省エネモード）
static const uint32_t IDLE_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static unsigned long last_activity_ms = 0;
static bool is_idle_state = false;
extern void sched_fn_sleep(void);
extern void sched_fn_wake(void);

void notify_activity() {
  last_activity_ms = millis();
  if (is_idle_state) {
    sched_fn_wake();
    is_idle_state = false;
  }
}

// 頭撫でモード（画面上部から下方向フリックで検出）の状態のみここで宣言
// 実関数は avatar 宣言後に定義（後段の lipSync 直前あたり）
static bool head_pet_active = false;
static unsigned long head_pet_end_ms = 0;
static const uint32_t HEAD_PET_DURATION_MS = 3000;
void head_pet_trigger();
void head_pet_update();


// NTP接続情報　NTP connection information.
const char* NTPSRV      = "ntp.jst.mfeed.ad.jp";    // NTPサーバーアドレス NTP server address.
const long  GMT_OFFSET  = 9 * 3600;                 // GMT-TOKYO(時差９時間）9 hours time difference.
const int   DAYLIGHT_OFFSET = 0;                    // サマータイム設定なし No daylight saving time setting

//bool servo_home = false;
bool servo_home = true;
volatile bool espnow_remote_servo_override = false;

using namespace m5avatar;
Avatar avatar;
Face* customFace;
const Expression expressions_table[] = {
  Expression::Neutral,
  Expression::Happy,
  Expression::Sleepy,
  Expression::Doubt,
  Expression::Sad,
  Expression::Angry
};

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial


// 頭撫でモード実装本体（avatar / robot がここで参照可能）
void head_pet_trigger() {
  notify_activity();
  avatar.setExpression(Expression::Embarrassed);
  avatar.setSpeechFont(&fonts::efontJA_16);
  avatar.setSpeechText(phrases::head_pet());
  if (robot && robot->servo) {
    robot->servo->fillLeds(0xFF, 0x60, 0xA0);   // ピンク
    robot->servo->moveTo(0, -20, 400);          // ちょっと上向き
  }
  // サーボ動作の振動を IMU が拾わないよう、復帰までマスク
  headPetMaskFor(HEAD_PET_DURATION_MS + 800);
  head_pet_active = true;
  head_pet_end_ms = millis() + HEAD_PET_DURATION_MS;
}

void head_pet_update() {
  if (head_pet_active && millis() > head_pet_end_ms) {
    avatar.setExpression(Expression::Neutral);
    avatar.setSpeechText("");
    if (robot && robot->servo) {
      robot->servo->clearLeds();
      robot->servo->moveTo(0, 0, 400);
    }
    head_pet_active = false;
  }
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
#ifdef REALTIME_API_WITH_TTS
    level = robot->tts->getLevel();
#else
    level = ((RealtimeLLMBase*)(robot->llm))->getAudioLevel();
#endif
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
    delay(100);
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
    if(espnow_remote_servo_override)
    {
      delay(100);
      continue;
    }

    if(!servo_home)
    {
      avatar->getGaze(&gazeY, &gazeX);
      robot->servo->moveTo((int)(15.0 * gazeX), (int)(10.0 * gazeY));
    }
    // servo_home == true のときは IdleMotion がランダムな動きを担当するため、
    // ここで moveToOrigin() を呼ぶと競合する。何もしない。
#endif
    delay(5000);
  }
}

void battery_check(void *args) {
  DriveContext *ctx = (DriveContext *)args;
  Avatar *avatar = ctx->getAvatar();
  for (;;)
  {
    int32_t batteryLevel = M5.Power.getBatteryLevel();
    if((batteryLevel < 95) && (batteryLevel != 0)){
      avatar->setBatteryIcon(true);
      avatar->setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
    }
    else{
      avatar->setBatteryIcon(false);    
    }
    delay(60000);
  }
}

// WiFi.status() の数値を読みやすい文字列に
static const char* wifi_status_str(wl_status_t s) {
  switch (s) {
    case WL_NO_SHIELD:       return "NO_SHIELD";
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL (該当SSIDが見つからない)";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED (認証失敗・パスワード違い等)";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

bool Wifi_connection_check() {
  unsigned long start_millis = millis();
  wl_status_t last_status = (wl_status_t)255;

  // 前回接続時情報で接続する
  while (WiFi.status() != WL_CONNECTED) {
    wl_status_t s = WiFi.status();
    if (s != last_status) {
      Serial.printf("\n  WiFi status: %d (%s)\n", (int)s, wifi_status_str(s));
      last_status = s;
    }
    M5.Display.print(".");
    Serial.print(".");
    delay(1000);
    // 5秒以上接続できなかったら抜ける
    if ( 5000 < (millis() - start_millis) ) {
      Serial.printf("\n  Final WiFi status: %d (%s)\n", (int)WiFi.status(), wifi_status_str(WiFi.status()));
      return false;
    }
  }
  return true;
}

bool WifiSmartConfig() {
#if defined(USE_LLM_MODULE)
  // LLMモジュール使用時は普通はオフラインが前提のため、Smart Config待ちはしない
  return false;
#else
  unsigned long start_millis = millis();
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  M5.Display.println("Waiting for SmartConfig");
  Serial.println("Waiting for SmartConfig");
  while (!WiFi.smartConfigDone()) {
    delay(1000);
    M5.Display.print("#");
    Serial.print("#");
    // 30秒以上接続できなかったら抜ける
    if ( 30000 < millis() - start_millis) {
      Serial.println("");
      //Serial.println("Reset");
      //ESP.restart();
      return false;
    }
  }
  return true;
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
#if defined(REALTIME_API)
    add_mod(new RealtimeAiMod(isOffline));
#else
    add_mod(new AiStackChanMod(isOffline));
#endif
  }
  add_mod(new StatusMonitorMod());
  add_mod(new VolumeSettingMod());
  //add_mod(new EspNowRemoteMod());
  //add_mod(new PomodoroMod(isOffline));
  //add_mod(new PhotoFrameMod(isOffline));
  //add_mod(new QRdisplayMod());
  mod = get_current_mod();
  mod->init();
  return mod;
}


void sw_tone()
{
  if (is_muted()) return;
  enterMutexAudio();
  M5.Mic.end();
  M5.Speaker.begin();
  delay(300);     // AtomS3Rはこのdelayがないと鳴らないときがある
  // ソド の上昇2音（ピロッ）
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
  exitMutexAudio();
}

void init_mic_spk()
{
#if defined(USE_AUDIO_MODULE)
  initAudioModule();
#endif

  {
    auto micConfig = M5.Mic.config();
    //micConfig.stereo = false;
    micConfig.sample_rate = 16000;
#if defined(USE_AUDIO_MODULE)
    micConfig.pin_data_in = SYS_I2S_DIN_PIN;
    micConfig.pin_bck = SYS_I2S_SCLK_PIN;
    micConfig.pin_mck = SYS_I2S_MCLK_PIN;
    micConfig.pin_ws = SYS_I2S_LRCK_PIN;
#endif
    M5.Mic.config(micConfig);
  }
  M5.Mic.begin();

  { /// custom setting
    auto spk_cfg = M5.Speaker.config();
    /// Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 64000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    spk_cfg.task_pinned_core = APP_CPU_NUM;

#if defined(USE_AUDIO_MODULE)
    spk_cfg.pin_data_out = SYS_I2S_DOUT_PIN;
    spk_cfg.pin_bck = SYS_I2S_SCLK_PIN;
    spk_cfg.pin_mck = SYS_I2S_MCLK_PIN;
    spk_cfg.pin_ws = SYS_I2S_LRCK_PIN;
#endif
    M5.Speaker.config(spk_cfg);
  }
  //M5.Speaker.begin();
}

void setup()
{
  auto cfg = M5.config();

#if defined(ARDUINO_M5STACK_ATOMS3R)
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  cfg.external_speaker.atomic_echo = true;
#endif
  cfg.serial_baudrate = 115200;   //M5Unified 0.1.17からデフォルトが0になったため設定
  M5.begin(cfg);
  M5.Display.setBrightness(255);  // バックライト最大（CoreS3で点灯しない問題への対処）
  led_init();   // GoBottom LED 初期化（起動時消灯）
  // LED watchdog（TTS等で main loop がブロックしても自動消灯する）
  xTaskCreatePinnedToCore(led_watchdog_task, "led_wd", 4096, NULL, 1, NULL, 0);
  Serial.printf("Board: %d, Display: %dx%d\n",
    (int)M5.getBoard(), M5.Display.width(), M5.Display.height());

  /// シリアル出力のログレベルを VERBOSEに設定
  //M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);


#if defined(ARDUINO_M5STACK_ATOMS3R)
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("Ver.%s\n", FW_VERSION);
#else
  M5.Lcd.setFont(&fonts::lgfxJapanGothic_20);
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("AI Stack-chan Ex [・＿・]");
  M5.Lcd.printf("Firmware Version: %s\n", FW_VERSION);
#endif

  initMutex();

#if defined(ENABLE_SD_UPDATER)
  // ***** for SD-Updater *********************
  SDU_lobby("AiStackChanEx");
  // ******************************************
#endif

  //auto brightness = M5.Display.getBrightness();
  //Serial.printf("Brightness: %d\n", brightness);

  init_mic_spk();

  /// settings
#if defined(ARDUINO_M5STACK_ATOMS3R)
  if (SPIFFS.begin()) {
    // この関数ですべてのYAMLファイル(Basic, Secret, Extend)を読み込む
    system_config.loadConfig(SPIFFS, "/SC_ExConfig.yaml", 2048,
                                     "/SC_SecConfig.yaml", 2048,
                                     "/SC_BasicConfig.yaml", 2048);
#else
  // SDカードマウントは起動直後のタイミング問題で失敗することがあるため、最大5回リトライ
  bool sd_ok = false;
  for (int i = 0; i < 5; i++) {
    if (SD.begin(GPIO_NUM_4, SPI, 25000000)) {
      sd_ok = true;
      break;
    }
    Serial.printf("SD mount failed, retry %d/5\n", i + 1);
    delay(500);
  }
  if (sd_ok) {
    // この関数ですべてのYAMLファイル(Basic, Secret, Extend)を読み込む
    system_config.loadConfig(SD, "/app/AiStackChanEx/SC_ExConfig.yaml");
#endif
    // Wifi設定読み込み
    wifi_s* wifi_info = system_config.getWiFiSetting();
    Serial.printf("\nSSID: %s\n",wifi_info->ssid.c_str());
    Serial.printf("Key: %s\n",wifi_info->password.c_str());

    // 前回設定で接続
    Serial.println("Connecting to WiFi");
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    if(Wifi_connection_check()){
      Serial.println("Successfully connected to Wi-Fi using the previous settings.");
    }else{
      // 前回設定での接続に失敗。SDカード設定による接続にトライ。
      Serial.println("The previous WiFi connection failed. Attempting to connect using the SD card settings.");
      if(wifi_info->ssid.length() == 0){
        // SDカード設定の取得に失敗。Smart Configをスタート。
        Serial.println("Can't get WiFi settings. Start Smart Config.");
        if(!WifiSmartConfig()){
          // Smart Config失敗。オフラインモード。
          Serial.println("Smart Config failed. Running in offline mode.");
          isOffline = true;
        }
      }else{
        WiFi.begin(wifi_info->ssid.c_str(), wifi_info->password.c_str());
        if(Wifi_connection_check()){
          // SDカード設定による接続に成功。
          Serial.println("Successfully established a Wi-Fi connection via the SD card settings.");
        }else{
          // メイン Wi-Fi 失敗 → フォールバック SSID を試す
          const auto& fallback = system_config.getExConfig().wifi_fallback;
          bool fallback_ok = false;
          if (fallback.ssid.length() > 0) {
            Serial.printf("Trying fallback Wi-Fi: %s\n", fallback.ssid.c_str());
            WiFi.disconnect();
            WiFi.begin(fallback.ssid.c_str(), fallback.password.c_str());
            if (Wifi_connection_check()) {
              Serial.println("Successfully connected via fallback Wi-Fi.");
              fallback_ok = true;
            } else {
              Serial.println("Fallback Wi-Fi also failed.");
            }
          }
          if (!fallback_ok) {
            // フォールバックも失敗 → Smart Config
            Serial.println("WiFi connection failed. Start Smart Config.");
            if(!WifiSmartConfig()){
              Serial.println("Smart Config failed. Running in offline mode.");
              isOffline = true;
            }
          }
        }
      }
    }

    if(!isOffline){
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
    }else{
      M5.Lcd.print("Can't connect to WiFi. Start offline mode.\n");
    }

    robot = new Robot(system_config);

    // PY32 LED が前回状態を保持しているため、明示的に消灯
    led_off();

    //SD.end();
  } else {
    M5.Lcd.print("Failed to load SD card settings. System reset after 5 seconds.");
    delay(5000);
    ESP.restart();
    //WiFi.begin();
  }
  
  mp3_init();

  //mod設定
  init_mod();

#if defined(ARDUINO_M5STACK_ATOMS3R)
#if defined(CAT_FACE)
  customFace = new CatFace();
  avatar.setFace(customFace);
#endif
  avatar.setScale(0.5);
  avatar.setPosition(-56, -96);
  avatar.init();
#else
  //avatar.init();
  avatar.init(16);
#endif

  avatar.addTask(lipSync, "lipSync", 2048, 2);
  avatar.addTask(servo, "servo", 2048);
  avatar.addTask(battery_check, "battery_check", 2048);
  avatar.setSpeechFont(&fonts::efontJA_16);

  Serial.printf("Speaker volume (yaml): %d\n", system_config.getExConfig().audio.speaker_volume);
  if(0 != system_config.getExConfig().audio.speaker_volume){
    robot->spk_volume = system_config.getExConfig().audio.speaker_volume;
  }else{
    robot->spk_volume = DEFAULT_SPEAKER_VOLUME;
  }
  // NVS に Web 経由で保存された値があれば優先（0 は未設定扱い）
  {
    Preferences prefs;
    if (prefs.begin("aistackchan", true)) {
      uint8_t nvs_vol = prefs.getUChar("volume", 0);
      prefs.end();
      if (nvs_vol > 0) {
        robot->spk_volume = nvs_vol;
        Serial.printf("Speaker volume (NVS override): %d\n", nvs_vol);
      }
    }
  }
  Serial.printf("Speaker volume (set): %d\n", robot->spk_volume);
  M5.Speaker.setVolume(robot->spk_volume);

#if defined(ENABLE_CAMERA)
  camera_init();
  avatar.set_isSubWindowEnable(true);
#endif

#if defined(ENABLE_TAP_DETECT)
  invokeDoubleTapDetectTask();
#endif

  // IMU 加速度センサ経由の頭撫で検出（M5StackChan キット時のみ有効、CoreS3-SE は IMU 非搭載でスキップ）
  invokeHeadPetDetectTask();

  // アイドル時のランダム動作（公式 stackchan の IdleMotionModifier を参考に）
  idle_motion_init();

  //init_watchdog();

  //ヒープメモリ残量確認(デバッグ用)
  check_heap_free_size();
  check_heap_largest_free_block();

}



void loop()
{
  //get_elapsed_time_micro("loop() start");
  M5.update();
  //get_elapsed_time_micro("M5.update time");
  ModBase* mod = get_current_mod();
  mod->idle();
  //get_elapsed_time_micro("Mod idle time");

  // idle タイムアウト判定
  if (!is_idle_state && (millis() - last_activity_ms > IDLE_TIMEOUT_MS)) {
    Serial.println("Idle timeout: entering sleep mode");
    sched_fn_sleep();
    is_idle_state = true;
  }

  // LED 自動消灯（TTS ハングしても 60 秒で消える保険）
  led_auto_off_tick();

  // Serial 経由の yaml 設定コマンドを処理
  serial_config_poll();

  if (M5.BtnA.wasPressed())
  {
    notify_activity();
    mod->btnA_pressed();
  }

  if (M5.BtnA.pressedFor(2000))
  {
    mod->btnA_longPressed();
  }

  if (M5.BtnB.wasPressed())
  {
    notify_activity();
    mod->btnB_pressed();
  }

  if (M5.BtnB.pressedFor(2000))
  {
    mod->btnB_longPressed();
  }

  if (M5.BtnC.wasPressed())
  {
    notify_activity();
    mod->btnC_pressed();
  }

#if defined(ARDUINO_M5STACK_Core2) || defined( ARDUINO_M5STACK_CORES3 )
  auto count = M5.Touch.getCount();
  if (count)
  {
    auto t = M5.Touch.getDetail();
    if (t.wasPressed())
    {
      notify_activity();
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
      else if (dy > 30) {
        // 下方向フリック → 頭撫で
        head_pet_trigger();
      }
    }
  }
  // IMU 加速度で頭撫で検出（複数回スパイク）
  if (headPetDetected()) {
    head_pet_trigger();
  }
  // 頭撫で状態の自動解除
  head_pet_update();

  // アイドル時のランダムサーボ動作（4〜8秒間隔）
  idle_motion_tick();
#endif

#if defined(ENABLE_TAP_DETECT)
  if(doubleTapDetected){
    Serial.println("loop(): Double tap detected");
    mod->doubleTapped(detectedAcc[0], detectedAcc[1], detectedAcc[2]);
    doubleTapDetected = false;
  }

  // Modで重い処理をしている場合はダブルタップ検出を停止する
  if(mod->isBusy()){
    stopDoubleTapDetectTask();
  }else{
    resumeDoubleTapDetectTask();
  }
#endif
  //get_elapsed_time_micro("Callback process time");

  if(!isOffline){
    web_server_handle_client();
    ftpSrv.handleFTP();
  }

  //get_elapsed_time_micro("Web event process time");
  
  //reset_watchdog();
}
