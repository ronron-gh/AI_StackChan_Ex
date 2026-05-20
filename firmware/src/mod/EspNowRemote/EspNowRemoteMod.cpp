#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#include <Avatar.h>
#include "Robot.h"
#include "EspNowRemoteMod.h"

using namespace m5avatar;

extern Avatar avatar;
extern Robot* robot;
extern volatile bool espnow_remote_servo_override;

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

namespace {

constexpr uint8_t ESPNOW_REMOTE_CHANNEL = 1;
constexpr size_t ESPNOW_REMOTE_MAX_DATA_LEN = 250;
constexpr int ESPNOW_REMOTE_STD_PAYLOAD_LEN = 8;
constexpr int ESPNOW_REMOTE_WRAPPED_PAYLOAD_OFFSET = 20;
constexpr int16_t ESPNOW_REMOTE_YAW_MIN = -1280;
constexpr int16_t ESPNOW_REMOTE_YAW_MAX = 1280;
constexpr int16_t ESPNOW_REMOTE_PITCH_MIN = 0;
constexpr int16_t ESPNOW_REMOTE_PITCH_MAX = 900;
constexpr int ESPNOW_REMOTE_SERVO_X_MIN = -45;
constexpr int ESPNOW_REMOTE_SERVO_X_MAX = 45;
constexpr int ESPNOW_REMOTE_SERVO_Y_MIN = -30;
constexpr int ESPNOW_REMOTE_SERVO_Y_MAX = 0;

portMUX_TYPE g_recv_mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t g_recv_data[ESPNOW_REMOTE_MAX_DATA_LEN];
uint8_t g_recv_mac[6];
int g_recv_len = 0;
bool g_has_recv_data = false;

void storeReceivedData(const uint8_t* mac, const uint8_t* data, int len)
{
  if(data == nullptr || len <= 0){
    return;
  }

  if(len > static_cast<int>(ESPNOW_REMOTE_MAX_DATA_LEN)){
    len = ESPNOW_REMOTE_MAX_DATA_LEN;
  }

  portENTER_CRITICAL(&g_recv_mux);
  if(mac != nullptr){
    memcpy(g_recv_mac, mac, sizeof(g_recv_mac));
  }else{
    memset(g_recv_mac, 0, sizeof(g_recv_mac));
  }
  memcpy(g_recv_data, data, len);
  g_recv_len = len;
  g_has_recv_data = true;
  portEXIT_CRITICAL(&g_recv_mux);
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
  const uint8_t* mac = nullptr;
  if(info != nullptr){
    mac = info->src_addr;
  }
  storeReceivedData(mac, data, len);
}
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len)
{
  storeReceivedData(mac, data, len);
}
#endif

String dataToHexString(const uint8_t* data, int len)
{
  String hex;
  for(int i = 0; i < len; i++){
    if(i > 0){
      hex += " ";
    }
    if(data[i] < 0x10){
      hex += "0";
    }
    hex += String(data[i], HEX);
  }
  hex.toUpperCase();
  return hex;
}

int mapClamped(int value, int in_min, int in_max, int out_min, int out_max)
{
  if(value < in_min){
    value = in_min;
  }else if(value > in_max){
    value = in_max;
  }

  return out_min + ((value - in_min) * (out_max - out_min)) / (in_max - in_min);
}

}  // namespace

EspNowRemoteMod::EspNowRemoteMod(void)
{
}

void EspNowRemoteMod::init(void)
{
  avatar.setSpeechText("ESP-NOW Receiver");
  avatar.updateSubWindowTxt("ESP-NOW Receiver\nChannel: 1\nServo control enabled", 0, 0, 240, 80);
  avatar.set_isSubWindowEnable(true);
  delay(3000);
  avatar.setSpeechText("");
  avatar.set_isSubWindowEnable(false);

  espnow_remote_servo_override = true;

  espnow_started = startEspNowReceiver();
  if(!espnow_started){
    avatar.updateSubWindowTxt("ESP-NOW init failed\nSee serial monitor", 0, 0, 220, 80);
  }
}

void EspNowRemoteMod::pause(void)
{
  espnow_remote_servo_override = false;
  stopEspNowReceiver();
  avatar.set_isSubWindowEnable(false);
  avatar.setSpeechText("");
}

void EspNowRemoteMod::idle(void)
{
  handleReceivedData();
}

bool EspNowRemoteMod::startEspNowReceiver(void)
{
  Serial.println("[EspNowRemote] Starting ESP-NOW receiver");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);

  esp_err_t channel_result = esp_wifi_set_channel(ESPNOW_REMOTE_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if(channel_result != ESP_OK){
    Serial.printf("[EspNowRemote] esp_wifi_set_channel failed: %d\n", channel_result);
    return false;
  }

  esp_err_t init_result = esp_now_init();
  if(init_result != ESP_OK){
    Serial.printf("[EspNowRemote] esp_now_init failed: %d\n", init_result);
    return false;
  }

  esp_now_unregister_recv_cb();
  esp_err_t cb_result = esp_now_register_recv_cb(onEspNowRecv);
  if(cb_result != ESP_OK){
    Serial.printf("[EspNowRemote] esp_now_register_recv_cb failed: %d\n", cb_result);
    return false;
  }

  Serial.printf("[EspNowRemote] Receiver ready on channel %u\n", ESPNOW_REMOTE_CHANNEL);
  return true;
}

void EspNowRemoteMod::stopEspNowReceiver(void)
{
  if(!espnow_started){
    return;
  }

  esp_now_unregister_recv_cb();
  esp_now_deinit();
  espnow_started = false;
  Serial.println("[EspNowRemote] ESP-NOW receiver stopped");
}

void EspNowRemoteMod::handleReceivedData(void)
{
  uint8_t data[ESPNOW_REMOTE_MAX_DATA_LEN];
  uint8_t mac[6];
  int len = 0;

  portENTER_CRITICAL(&g_recv_mux);
  if(g_has_recv_data){
    len = g_recv_len;
    memcpy(data, g_recv_data, len);
    memcpy(mac, g_recv_mac, sizeof(mac));
    g_has_recv_data = false;
  }
  portEXIT_CRITICAL(&g_recv_mux);

  if(len <= 0){
    return;
  }

  received_count++;
  String hex = dataToHexString(data, len);
  Serial.printf("[EspNowRemote] #%lu from %02X:%02X:%02X:%02X:%02X:%02X len=%d data=%s\n",
                static_cast<unsigned long>(received_count),
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                len, hex.c_str());

  int payload_offset = -1;
  if(len >= ESPNOW_REMOTE_WRAPPED_PAYLOAD_OFFSET + ESPNOW_REMOTE_STD_PAYLOAD_LEN){
    payload_offset = ESPNOW_REMOTE_WRAPPED_PAYLOAD_OFFSET;
  }else if(len >= ESPNOW_REMOTE_STD_PAYLOAD_LEN){
    payload_offset = 0;
  }

  if(payload_offset >= 0){
    const uint8_t* payload = data + payload_offset;
    uint8_t target_id = payload[0];
    int16_t yaw = static_cast<int16_t>(payload[1] | (payload[2] << 8));
    int16_t pitch = static_cast<int16_t>(payload[3] | (payload[4] << 8));
    int16_t speed = static_cast<int16_t>(payload[5] | (payload[6] << 8));
    bool laser = (payload[7] != 0);
    Serial.printf("[EspNowRemote] decoded offset=%d target=%u yaw=%d pitch=%d speed=%d laser=%s\n",
                  payload_offset, target_id, yaw, pitch, speed, laser ? "on" : "off");
    applyServoControl(yaw, pitch, speed);
  }
}

void EspNowRemoteMod::applyServoControl(int16_t yaw, int16_t pitch, int16_t speed)
{
#ifdef USE_SERVO
  if(robot == nullptr || robot->servo == nullptr){
    Serial.println("[EspNowRemote] servo is not ready");
    return;
  }

  int servo_x = mapClamped(yaw,
                           ESPNOW_REMOTE_YAW_MIN,
                           ESPNOW_REMOTE_YAW_MAX,
                           ESPNOW_REMOTE_SERVO_X_MAX,
                           ESPNOW_REMOTE_SERVO_X_MIN);
  int servo_y = mapClamped(pitch,
                           ESPNOW_REMOTE_PITCH_MIN,
                           ESPNOW_REMOTE_PITCH_MAX,
                           ESPNOW_REMOTE_SERVO_Y_MAX,
                           ESPNOW_REMOTE_SERVO_Y_MIN);
  uint32_t millis_for_move = (speed < 0) ? 0 : static_cast<uint32_t>(speed);

  //robot->servo->moveTo(servo_x, servo_y, millis_for_move);
  robot->servo->moveTo(servo_x, servo_y, 100);
  Serial.printf("[EspNowRemote] servo x=%d y=%d move_ms=%lu\n",
                servo_x, servo_y, static_cast<unsigned long>(millis_for_move));
#endif
}
