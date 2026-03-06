#if defined(USE_AUDIO_MODULE)
#ifndef _M5_AUDIO_MODULE_H
#define _M5_AUDIO_MODULE_H

#include "audio_i2c.hpp"
#include "es8388.hpp"
#include "driver/i2s.h"

// for Core2
#define SYS_I2C_SDA_PIN  21
#define SYS_I2C_SCL_PIN  22
#define SYS_I2S_MCLK_PIN 0
#define SYS_I2S_SCLK_PIN 19
#define SYS_I2S_LRCK_PIN 27
#define SYS_I2S_DOUT_PIN 2
#define SYS_I2S_DIN_PIN  34
#define SYS_SPI_MISO_PIN 38
#define SYS_SPI_MOSI_PIN 23
#define SYS_SPI_CLK_PIN  18
#define SYS_SPI_CS_PIN   4

extern AudioI2c device;
extern ES8388 es8388;

void initAudioModule(void);

#endif  //_M5_AUDIO_MODULE_H
#endif  //USE_AUDIO_MODULE