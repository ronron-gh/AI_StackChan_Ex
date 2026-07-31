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

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
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
Robot* robot = nullptr;
bool isOffline = false;
bool isWebServerEnabled = false;
bool isConfigPortalMode = false;


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

static const char* SPIFFS_EX_CONFIG_PATH = "/SC_ExConfig.yaml";
static const char* SPIFFS_SEC_CONFIG_PATH = "/SC_SecConfig.yaml";
static const char* SPIFFS_BASIC_CONFIG_PATH = "/SC_BasicConfig.yaml";
static const char* SD_EX_CONFIG_PATH = "/app/AiStackChanEx/SC_ExConfig.yaml";
static const char* SD_SEC_CONFIG_PATH = "/yaml/SC_SecConfig.yaml";
static const char* SD_BASIC_CONFIG_PATH = "/yaml/SC_BasicConfig.yaml";
static const char* CONFIG_AP_SSID_PREFIX = "StackChanEx-Config";
static const char* CONFIG_AP_PASSWORD = "stackchan";
static const char* CONFIG_PORTAL_URL = "http://192.168.4.1/";

enum class WifiFallbackMode {
  ConfigAp,
  Offline
};

struct ScreenButton {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

bool is_point_in_button(int32_t x, int32_t y, const ScreenButton& button)
{
  return x >= button.x && x < (button.x + button.w)
      && y >= button.y && y < (button.y + button.h);
}

void draw_centered_text(const char* text, int32_t x, int32_t y, int32_t w)
{
  int32_t text_width = M5.Display.textWidth(text);
  M5.Display.setCursor(x + ((w - text_width) / 2), y);
  M5.Display.print(text);
}

void draw_wifi_fallback_button(const ScreenButton& button, uint16_t fill_color, uint16_t border_color, const char* title, const char* caption)
{
  M5.Display.fillRoundRect(button.x, button.y, button.w, button.h, 10, fill_color);
  M5.Display.drawRoundRect(button.x, button.y, button.w, button.h, 10, border_color);
  M5.Display.setTextColor(TFT_WHITE, fill_color);
  M5.Display.setTextSize(1);
  draw_centered_text(title, button.x, button.y + 22, button.w);
  draw_centered_text(caption, button.x, button.y + button.h - 24, button.w);
}

String generate_config_ap_ssid()
{
  char ssid[32];
  uint32_t suffix = esp_random() % 1000000;
  snprintf(ssid, sizeof(ssid), "%s-%06u", CONFIG_AP_SSID_PREFIX, (unsigned)suffix);
  return String(ssid);
}

bool fs_file_exists(fs::FS& fs, const char* path)
{
  if(!fs.exists(path)){
    Serial.printf("%s not found\n", path);
    return false;
  }
  File file = fs.open(path, FILE_READ);
  if(!file){
    Serial.printf("%s exists but cannot open\n", path);
    return false;
  }
  bool exists = !file.isDirectory() && file.size() > 0;
  Serial.printf("%s %s size:%u\n", path, exists ? "exist" : "invalid", (unsigned)file.size());
  file.close();
  return exists;
}

bool has_required_config_files(fs::FS& fs, const char* ex_path, const char* sec_path, const char* basic_path)
{
  return fs_file_exists(fs, ex_path)
      && fs_file_exists(fs, sec_path)
      && fs_file_exists(fs, basic_path);
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
    } else {
      robot->servo->moveToOrigin();
    }
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

bool Wifi_connection_check() {
  unsigned long start_millis = millis();

  while (WiFi.status() != WL_CONNECTED) {
    M5.Display.print(".");
    Serial.print(".");
    delay(1000);
    // 5秒以上接続できなかったら抜ける
    if ( 5000 < (millis() - start_millis) ) {
      //break;
      return false;
    }
  }
  return true;
}

bool connect_wifi_from_yaml(wifi_s* wifi_info)
{
  if(wifi_info == nullptr || wifi_info->ssid.length() == 0){
    Serial.println("WiFi SSID is not configured.");
    return false;
  }

  Serial.printf("Connecting to WiFi SSID: %s\n", wifi_info->ssid.c_str());
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_info->ssid.c_str(), wifi_info->password.c_str());
  return Wifi_connection_check();
}

WifiFallbackMode select_wifi_fallback_mode()
{
#if defined(ARDUINO_M5STACK_ATOMS3R)
  return WifiFallbackMode::ConfigAp;
#endif

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  draw_centered_text("Wi-Fi connection failed", 0, 18, M5.Display.width());
  draw_centered_text("Choose how to continue", 0, 48, M5.Display.width());

  const int32_t margin = 16;
  const int32_t gap = 12;
  const int32_t button_y = 88;
  const int32_t button_h = 92;
  const int32_t button_w = (M5.Display.width() - (margin * 2) - gap) / 2;
  const ScreenButton config_ap_button = { margin, button_y, button_w, button_h };
  const ScreenButton offline_button = { margin + button_w + gap, button_y, button_w, button_h };
  draw_wifi_fallback_button(config_ap_button, TFT_DARKCYAN, TFT_CYAN, "Config AP", "BtnA");
  draw_wifi_fallback_button(offline_button, TFT_DARKGREY, TFT_LIGHTGREY, "Offline", "BtnC");

  M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  while(true){
    M5.update();
    if(M5.BtnA.wasPressed()){
      return WifiFallbackMode::ConfigAp;
    }
    if(M5.BtnC.wasPressed()){
      return WifiFallbackMode::Offline;
    }
#if defined(ARDUINO_M5STACK_Core2) || defined( ARDUINO_M5STACK_CORES3 )
    if(M5.Touch.getCount()){
      auto touch = M5.Touch.getDetail();
      if(touch.wasPressed()){
        if(is_point_in_button(touch.x, touch.y, config_ap_button)){
          return WifiFallbackMode::ConfigAp;
        }
        if(is_point_in_button(touch.x, touch.y, offline_button)){
          return WifiFallbackMode::Offline;
        }
      }
    }
#endif
    delay(20);
  }
}

void show_config_portal_qr(const String& url, bool ap_mode, const String& ap_ssid = String())
{
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setCursor(0, 0);
  if(ap_mode){
    M5.Display.printf("SSID: %s\n", ap_ssid.c_str());
    M5.Display.printf("PASS: %s\n", CONFIG_AP_PASSWORD);
  }else{
    M5.Display.println("Config web");
  }
  M5.Display.println(url);
#if !defined(ARDUINO_M5STACK_ATOMS3R)
  int32_t qr_size = min(M5.Display.width(), M5.Display.height()) - 80;
  if(qr_size < 120){
    qr_size = min(M5.Display.width(), M5.Display.height()) - 20;
  }
  int32_t qr_x = (M5.Display.width() - qr_size) / 2;
  int32_t qr_y = (M5.Display.height() - qr_size) / 2;
  if(qr_y < 70){
    qr_y = 70;
  }
  M5.Display.qrcode(url, qr_x, qr_y, qr_size, 5);
#endif
}

void start_config_portal()
{
  String ap_ssid = generate_config_ap_ssid();
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid.c_str(), CONFIG_AP_PASSWORD);
  Serial.printf("Config AP started. SSID: %s URL: %s\n", ap_ssid.c_str(), CONFIG_PORTAL_URL);
  show_config_portal_qr(CONFIG_PORTAL_URL, true, ap_ssid);
  init_web_server();
  isOffline = true;
  isWebServerEnabled = true;
  isConfigPortalMode = true;
}

void start_config_web_server_sta()
{
  String url = "http://" + WiFi.localIP().toString() + "/";
  Serial.printf("Config web server started. URL: %s\n", url.c_str());
  show_config_portal_qr(url, false);
  init_web_server();
  isOffline = true;
  isWebServerEnabled = true;
  isConfigPortalMode = true;
}

void show_config_portal_avatar_qr()
{
  String url = CONFIG_PORTAL_URL;
  avatar.updateSubWindowQrcode(url, 78);
  avatar.setSpeechText("Config: http://192.168.4.1/");
  avatar.set_isSubWindowEnable(true);
  avatar.setScale(0.7);
  avatar.setFaceOffsetY(-80);
}

bool load_system_config(bool sd_available)
{
  bool use_sd_config = false;
  bool full_config_loaded = false;
#if defined(ARDUINO_M5STACK_ATOMS3R)
  use_sd_config = false;
#else
  if(sd_available){
    use_sd_config = has_required_config_files(SD, SD_EX_CONFIG_PATH, SD_SEC_CONFIG_PATH, SD_BASIC_CONFIG_PATH);
  }
#endif

  if(use_sd_config){
    Serial.println("Loading config from SD.");
    system_config.loadConfig(SD, SD_EX_CONFIG_PATH);
    full_config_loaded = true;
  }else if(has_required_config_files(SPIFFS, SPIFFS_EX_CONFIG_PATH, SPIFFS_SEC_CONFIG_PATH, SPIFFS_BASIC_CONFIG_PATH)){
    Serial.println("Loading config from SPIFFS.");
    system_config.loadConfig(SPIFFS, SPIFFS_EX_CONFIG_PATH, 2048,
                                      SPIFFS_SEC_CONFIG_PATH, 2048,
                                      SPIFFS_BASIC_CONFIG_PATH, 2048);
    full_config_loaded = true;
  }else{
    Serial.println("Config files are incomplete. Loading only SC_SecConfig.yaml if available.");
    if(sd_available && fs_file_exists(SD, SD_SEC_CONFIG_PATH)){
      system_config.loadSecretConfigYaml(SD, SD_SEC_CONFIG_PATH);
    }else if(fs_file_exists(SPIFFS, SPIFFS_SEC_CONFIG_PATH)){
      system_config.loadSecretConfigYaml(SPIFFS, SPIFFS_SEC_CONFIG_PATH);
    }
  }
  return full_config_loaded;
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
  enterMutexAudio();
  M5.Mic.end();
  M5.Speaker.begin();
  delay(300);     // AtomS3Rはこのdelayがないと鳴らないときがある
  M5.Speaker.tone(1000, 100);
  delay(500);

  M5.Speaker.end();
  M5.Mic.begin();
  exitMutexAudio();
}
  
void alarm_tone()
{
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
  /// シリアル出力のログレベルを VERBOSEに設定
  //M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_VERBOSE);

  auto cfg = M5.config();

#if defined(ARDUINO_M5STACK_ATOMS3R)
  cfg.internal_spk = false;
  cfg.internal_mic = false;
  cfg.external_speaker.atomic_echo = true;
#endif
  cfg.serial_baudrate = 115200;   //M5Unified 0.1.17からデフォルトが0になったため設定
  M5.begin(cfg);

  ///// Debug /////
#if 0
  check_board();
  Wire.begin(); 
  i2c_scan(Wire);
  Wire1.begin(); 
  i2c_scan(Wire1);
#endif
  /////////////////

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
  if(!SPIFFS.begin()){
    M5.Lcd.print("Failed to mount SPIFFS. System reset after 5 seconds.");
    delay(5000);
    ESP.restart();
  }

  bool sd_available = false;
#if !defined(ARDUINO_M5STACK_ATOMS3R)
  sd_available = SD.begin(GPIO_NUM_4, SPI, 25000000);
#endif
  bool full_config_loaded = load_system_config(sd_available);

  // Wifi設定読み込み
  wifi_s* wifi_info = system_config.getWiFiSetting();
  Serial.printf("\nSSID: %s\n", wifi_info->ssid.c_str());
  Serial.printf("Key: %s\n", wifi_info->password.length() > 0 ? "(set)" : "(empty)");

  if(!full_config_loaded){
#if defined(REALTIME_API)
    Serial.println("Full config set is missing. Starting config web flow.");
    if(connect_wifi_from_yaml(wifi_info)){
      start_config_web_server_sta();
    }else{
      start_config_portal();
    }
#else
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Config incomplete.");
    M5.Display.println("");
    M5.Display.println("Missing one or more files:");
    M5.Display.println("SC_BasicConfig.yaml");
    M5.Display.println("SC_SecConfig.yaml");
    M5.Display.println("SC_ExConfig.yaml");
    while(true){
      M5.update();
      delay(100);
    }
#endif
    goto END;
  }

  if(connect_wifi_from_yaml(wifi_info)){
    Serial.println("Successfully connected to Wi-Fi using YAML settings.");
  }else{
    WifiFallbackMode fallback = select_wifi_fallback_mode();
    if(fallback == WifiFallbackMode::ConfigAp){
      start_config_portal();
      goto END;
    }else{
      isOffline = true;
      M5.Lcd.print("Start offline mode.\n");
    }
  }

  if(!isOffline){
    Serial.println(WiFi.localIP());
    M5.Lcd.println(WiFi.localIP());
    delay(1000);

    //Webサーバ設定
    init_web_server();
    isWebServerEnabled = true;
    //FTPサーバ設定（SPIFFS用）
    ftpSrv.begin("stackchan","stackchan");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
    Serial.println("FTP server started");
    M5.Lcd.println("FTP server started");

    //時刻同期
    time_sync(NTPSRV, GMT_OFFSET, DAYLIGHT_OFFSET);
  }

  robot = new Robot(system_config);
  
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

  if(isConfigPortalMode){
    show_config_portal_avatar_qr();
  }

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
  Serial.printf("Speaker volume (set): %d\n", robot->spk_volume);
  M5.Speaker.setVolume(robot->spk_volume);

#if defined(ENABLE_CAMERA)
  camera_init();
  avatar.set_isSubWindowEnable(true);
#endif

#if defined(ENABLE_TAP_DETECT)
  invokeDoubleTapDetectTask();
#endif

  //init_watchdog();

  //ヒープメモリ残量確認(デバッグ用)
  check_heap_free_size();
  check_heap_largest_free_block();

END:
  // Nothing to do.
  return ;
}



void loop()
{
  //get_elapsed_time_micro("loop() start");
  M5.update();
  //get_elapsed_time_micro("M5.update time");
  ModBase* mod = get_current_mod();
  if(mod == nullptr){
    if(isWebServerEnabled){
      web_server_handle_client();
    }
    delay(10);
    return;
  }
  mod->idle();
  //get_elapsed_time_micro("Mod idle time");

  if (M5.BtnA.wasPressed())
  {
    mod->btnA_pressed();
  }

  if (M5.BtnA.pressedFor(2000))
  {
    mod->btnA_longPressed();
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

  if(isWebServerEnabled){
    web_server_handle_client();
  }
  if(!isOffline){
    ftpSrv.handleFTP();
  }

  //get_elapsed_time_micro("Web event process time");
  
  //reset_watchdog();
}
