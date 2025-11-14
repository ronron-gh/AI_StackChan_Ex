#if defined(USE_LLM_MODULE)
#pragma once

#include <Arduino.h>
#include <M5ModuleLLM.h>

struct module_llm_param_t
{
  int8_t rxPin;
  int8_t txPin;
  //LLM
  bool enableLLM = false;
  m5_module_llm::ApiLlmSetupConfig_t m5llm_config;
  //KWS
  bool enableKWS = false;
  String wake_up_keyword;  
  //ASR
  bool enableASR = false;
  //Whisper
  bool enableWhisper = false;
  String whisper_model = "";
  //TTS
  bool enableTTS = false;
  String model = "";
};

extern M5ModuleLLM module_llm;
extern String tts_work_id;
extern String llm_work_id;

void module_llm_setup(module_llm_param_t param);
bool check_kws_wakeup();
String wait_for_asr_result();
String wait_for_whisper_result();


#endif