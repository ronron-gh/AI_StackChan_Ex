#include "Robot.h"
#include "tts/WebVoiceVoxTTS.h"
#include "tts/ElevenLabsTTS.h"
#include "tts/OpenAITTS.h"
#include "tts/UAquesTalkTTS.h"
#include "tts/ModuleLLMTTS.h"
#include "stt/CloudSpeechClient.h"
#include "stt/Whisper.h"
#include "stt/ModuleLLMASR.h"
#include "stt/ModuleLLMWhisper.h"
#include "driver/ModuleLLM.h"
#include "llm/LLMBase.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/RealtimeChatGPT.h"
#include "llm/Gemini/GeminiLive.h"
#include "llm/ModuleLLM/ChatModuleLLM.h"
#include "llm/ModuleLLMFncl/ChatModuleLLMFncl.h"
#include "llm/Grok/Grok.h"
#include "Avatar.h"

using namespace m5avatar;

extern Avatar avatar;
extern bool servo_home;

void asyncTtsStreamTask(void *arg) {
  Serial.println("TTS stream task created");
  Robot* pRt = (Robot*)arg;
  while(1){
    if(pRt->llm->getOutputTextQueueSize() != 0){
      pRt->asyncPlaying = true;
      String ttsText = pRt->llm->getOutputText();
      pRt->tts->stream(ttsText);
      pRt->asyncPlaying = false;
    }
    delay(1);
  }
}

Robot::Robot(StackchanExConfig& config) : m_config(config)
{
#ifdef USE_SERVO
  servo = new ServoCustom();
  servo->begin(config.getServoInfo(AXIS_X)->pin, config.getServoInfo(AXIS_X)->start_degree,
              config.getServoInfo(AXIS_X)->offset,
              config.getServoInfo(AXIS_Y)->pin, config.getServoInfo(AXIS_Y)->start_degree,
              config.getServoInfo(AXIS_Y)->offset,
              (ServoType)config.getServoType());
#endif

  M5.Power.setExtOutput(!config.getUseTakaoBase());

#if defined(REALTIME_API)
  initRtLLM(config);
  #if defined(REALTIME_API_WITH_TTS)
    initTTS(config);
    invokeAsyncTtsStreamTask();
  #endif
#else
  initLLM(config);
  initTTS(config);
  initSTT(config);
#endif
}

void Robot::initLLM(StackchanExConfig& config){
  int llm_type = config.getExConfig().llm.type;
  api_keys_s* api_key = config.getAPISetting();

  llm_param_t llm_param;
  llm_param.api_key = api_key->ai_service;
  llm_param.llm_conf = config.getExConfig().llm;

  switch(llm_type){
  case LLM_TYPE_CHATGPT:
    llm = new ChatGPT(llm_param);
    break;

  case LLM_TYPE_GROK:
    llm = new Grok(llm_param);
    break;

  case LLM_TYPE_MODULE_LLM:
#if defined(USE_LLM_MODULE)
    llm = new ChatModuleLLM(llm_param);
#else
    Serial.println("ModuleLLM is not enabled.");
    llm = nullptr;
#endif
    break;

  case LLM_TYPE_MODULE_LLM_FNCL:
#if defined(USE_LLM_MODULE)
    llm = new ChatModuleLLMFncl(llm_param);
#else
    Serial.println("ModuleLLM is not enabled.");
    llm = nullptr;
#endif
    break;

  default:
    Serial.println("Unknown LLM type. Defaulting to Grok.");
    llm = new Grok(llm_param);
    break;
  }
}

void Robot::initRtLLM(StackchanExConfig& config){
#if defined(REALTIME_API)
  int llm_type = config.getExConfig().llm.type;
  api_keys_s* api_key = config.getAPISetting();
  llm_param_t llm_param;
  llm_param.api_key = api_key->ai_service;
  llm_param.llm_conf = config.getExConfig().llm;

  switch(llm_type){
  case LLM_TYPE_CHATGPT:
    llm = new RealtimeChatGPT(llm_param);
    break;
  case LLM_TYPE_GEMINI:
    llm = new GeminiLive(llm_param);
    break;
  default:
    llm = nullptr;
  }
#endif
}

void Robot::initSTT(StackchanExConfig& config){
  int stt_type = config.getExConfig().stt.type;
  api_keys_s* api_key = config.getAPISetting();
  stt_param_t stt_param;
  stt_param.api_key = api_key->stt;
  stt_param.stt_conf = config.getExConfig().stt;

  switch(stt_type){
  case STT_TYPE_GOOGLE:
    stt = new CloudSpeechClient(stt_param);
    break;
  case STT_TYPE_OPENAI_WHISPER:
    stt = new Whisper(stt_param);
    break;
  default:
    stt = nullptr;
  }
}

void Robot::initTTS(StackchanExConfig& config){
  int tts_type = config.getExConfig().tts.type;
  api_keys_s* api_key = config.getAPISetting();
  tts_param_t tts_param;
  tts_param.api_key = api_key->tts;
  tts_param.model = config.getExConfig().tts.model;
  tts_param.voice = config.getExConfig().tts.voice;

  switch(tts_type){
  case TTS_TYPE_WEB_VOICEVOX:
    tts = new WebVoiceVoxTTS(tts_param);
    break;
  case TTS_TYPE_ELEVENLABS:
    tts = new ElevenLabsTTS(tts_param);
    break;
  case TTS_TYPE_OPENAI:
    tts_param.api_key = api_key->ai_service;
    tts = new OpenAITTS(tts_param);
    break;
  default:
    tts = nullptr;
  }
}

void Robot::speech(String text)
{
  if(text != ""){
    servo_home = false;
    avatar.setExpression(Expression::Happy);
    tts->stream(text);
    avatar.setExpression(Expression::Neutral);
    servo_home = true;
  }
}

void Robot::invokeAsyncTtsStreamTask()
{
  asyncPlaying = false;
  xTaskCreate(asyncTtsStreamTask, "asyncTtsStreamTask", 5*1024, this, 2, NULL);
}

String Robot::listen()
{
  return stt->speech_to_text();
}

void Robot::chat(String text, const char *base64_buf)
{
  llm->chat(text, base64_buf);
}