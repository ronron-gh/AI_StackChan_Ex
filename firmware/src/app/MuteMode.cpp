#include "MuteMode.h"
#include <Arduino.h>

static volatile bool g_mute = false;

bool is_muted() { return g_mute; }

void set_mute(bool m) {
  g_mute = m;
  Serial.printf("Mute mode: %s\n", m ? "ON" : "OFF");
}
