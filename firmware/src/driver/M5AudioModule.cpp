#if defined(USE_AUDIO_MODULE)

#include "driver/M5AudioModule.h"

AudioI2c device;
ES8388 es8388(&Wire, SYS_I2C_SDA_PIN, SYS_I2C_SCL_PIN);

void initAudioModule(void)
{
  device.begin(&Wire, SYS_I2C_SDA_PIN, SYS_I2C_SCL_PIN);
  device.setHPMode(AUDIO_HPMODE_NATIONAL);
  device.setMICStatus(AUDIO_MIC_OPEN);
  device.setRGBBrightness(100);
  if (!es8388.init()) Serial.println("Init Fail");
  es8388.setADCInput(ADC_INPUT_LINPUT1_RINPUT1);
  es8388.setMicGain(MIC_GAIN_24DB);
  es8388.setADCVolume(100);
  es8388.setDACVolume(40);
  es8388.setDACOutput(DAC_OUTPUT_OUT1);
  es8388.setBitsSample(ES_MODULE_ADC, BIT_LENGTH_16BITS);
  es8388.setSampleRate(SAMPLE_RATE_44K);
}

#endif