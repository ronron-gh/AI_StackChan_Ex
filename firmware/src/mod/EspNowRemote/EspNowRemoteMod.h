#ifndef _ESPNOW_REMOTE_MOD_H
#define _ESPNOW_REMOTE_MOD_H

#include <Arduino.h>
#include "mod/ModBase.h"

class EspNowRemoteMod: public ModBase {
private:
    bool espnow_started = false;
    uint32_t received_count = 0;

    bool startEspNowReceiver(void);
    void stopEspNowReceiver(void);
    void reconnectWiFi(void);
    void handleReceivedData(void);
    void applyServoControl(int16_t yaw, int16_t pitch, int16_t speed);

public:
    EspNowRemoteMod(void);
    void init(void);
    void pause(void);
    void idle(void);
};

#endif  // _ESPNOW_REMOTE_MOD_H
