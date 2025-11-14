#if defined(USE_LLM_MODULE)

#include <Arduino.h>
#include <M5Unified.h>
#include "ModuleLLM.h"

M5ModuleLLM module_llm;

String kws_work_id;
String asr_work_id;
String vad_work_id;
String whisper_work_id;
String tts_work_id;
String llm_work_id;
String language;

void module_llm_setup(module_llm_param_t param)
{
  language = "en_US";   //日本語に対応していないモデルがあるので、ここを"ja"にしても日本語設定にはなりません。
                        //日本語にする場合はモデル毎に"ja"を設定します。

  /* Init module serial port */
  Serial1.begin(115200, SERIAL_8N1, param.rxPin, param.txPin);  // ※Serial2はSCSサーボで使っているためSerial1に変更
  
  /* Init module */
  module_llm.begin(&Serial1);

  /* Make sure module is connected */
  M5.Display.printf(">> Check ModuleLLM connection..\n");
  while (1) {
    if (module_llm.checkConnection()) {
      break;
    }
  }

  /* Reset ModuleLLM */
  M5.Display.printf(">> Reset ModuleLLM..\n");
  Serial.printf(">> Reset ModuleLLM..\n");
  module_llm.sys.reset();

  /* Setup Audio module */
  M5.Display.printf(">> Setup audio..\n");
  Serial.printf(">> Setup audio..\n");
  module_llm.audio.setup();

  /* Setup KWS module and save returned work id */
  if(param.enableKWS){
    M5.Display.printf(">> Setup kws..\n");
    Serial.printf(">> Setup kws..\n");
    m5_module_llm::ApiKwsSetupConfig_t kws_config;
    kws_config.kws = param.wake_up_keyword;
    kws_work_id    = module_llm.kws.setup(kws_config, "kws_setup", language);
    if(kws_work_id == ""){
      M5.Display.printf("kws setup timeout\n");
      Serial.printf("kws setup timeout\n");
    }
  }

  /* Setup ASR module and save returned work id */
  if(param.enableASR){
    M5.Display.printf(">> Setup asr..\n");
    Serial.printf(">> Setup asr..\n");
    m5_module_llm::ApiAsrSetupConfig_t asr_config;
    asr_config.input = {"sys.pcm", kws_work_id};
    asr_work_id      = module_llm.asr.setup(asr_config, "asr_setup", language);
    if(asr_work_id == ""){
      M5.Display.printf("asr setup timeout\n");
      Serial.printf("asr setup timeout\n");
    }
  }

  /* Setup Whisper module and save returned work id */
  if(param.enableWhisper){
    /* Setup VAD module and save returned work id */
    M5.Display.printf(">> Setup vad..\n");
    Serial.printf(">> Setup vad..\n");
    m5_module_llm::ApiVadSetupConfig_t vad_config;
    vad_config.input = {"sys.pcm", kws_work_id};
    vad_work_id      = module_llm.vad.setup(vad_config, "vad_setup");

    /* Setup Whisper module and save returned work id */
    M5.Display.printf(">> Setup whisper (%s)..\n", param.whisper_model.c_str());
    Serial.printf(">> Setup whisper (%s)..\n", param.whisper_model.c_str());
    m5_module_llm::ApiWhisperSetupConfig_t whisper_config;
    whisper_config.input    = {"sys.pcm", kws_work_id, vad_work_id};
    //whisper_config.language = "en";
    //whisper_config.language = "zh";
    whisper_config.language = "ja";
    if(param.whisper_model != ""){
      //Whisperのモデルは別途インストールが必要。未指定の場合はデフォルトのwhisper-tiny
      whisper_config.model = param.whisper_model;
    }
    whisper_work_id = module_llm.whisper.setup(whisper_config, "whisper_setup");
    if(whisper_work_id == ""){
      M5.Display.printf("whisper setup timeout\n");
      Serial.printf("whisper setup timeout\n");
    }
  }

  /* Setup LLM module and save returned work id */
  if(param.enableLLM){
    M5.Display.printf(">> Setup llm (%s)..\n", param.m5llm_config.model.c_str());
    Serial.printf(">> Setup llm (%s)..\n", param.m5llm_config.model.c_str());
    llm_work_id = module_llm.llm.setup(param.m5llm_config);
    if(llm_work_id == ""){
      M5.Display.printf("llm setup timeout\n");
      Serial.printf("llm setup timeout\n");
    }
  }

  /* Setup TTS module and save returned work id */
  if(param.enableTTS){
    if(param.model.equals("melotts-ja-jp")){
      // Note: MeloTTSは別途モデルのインストールが必要
      //
      M5.Display.printf(">> Setup melotts(ja)..\n");
      Serial.printf(">> Setup melotts(ja)..\n");
      m5_module_llm::ApiMelottsSetupConfig_t melotts_config;
      melotts_config.model = param.model;
      melotts_config.input = {llm_work_id, kws_work_id};
      tts_work_id = module_llm.melotts.setup(melotts_config, "tts_setup", "ja_JP");
    }else{
      M5.Display.printf(">> Setup tts..\n");
      Serial.printf(">> Setup tts..\n");
      m5_module_llm::ApiTtsSetupConfig_t tts_config;
      tts_config.input = {llm_work_id, kws_work_id};
      tts_work_id = module_llm.tts.setup(tts_config, "tts_setup");
    }
    if(tts_work_id == ""){
      M5.Display.printf("tts setup timeout\n");
      Serial.printf("tts setup timeout\n");
    }
  }

  //M5.Display.printf(">> Setup ok\n>> Say \"%s\" to wakeup\n", wake_up_keyword.c_str());
  Serial.printf(">> Setup ok\n");
}


bool check_kws_wakeup()
{
  bool is_wakeup = false;

  /* Update ModuleLLM */
  module_llm.update();

  /* Handle module response messages */
  for (auto& msg : module_llm.msg.responseMsgList) {
    /* If KWS module message */
    if (msg.work_id == kws_work_id) {
      //M5.Display.setTextColor(TFT_GREENYELLOW);
      //M5.Display.printf(">> Keyword detected\n");
      Serial.printf(">> Keyword detected\n");
      is_wakeup = true;
    }
  }

  /* Clear handled messages */
  module_llm.msg.responseMsgList.clear();
  return is_wakeup;
}


String wait_for_asr_result()
{
  String asr_result_prev = "";
  String asr_result_new = "";
  String asr_result = "";
  int no_response_count = 0;
  int after_response_count = 0;
  
  //Serial.println("Waiting for ASR result.");
  while(1){
    /* Update ModuleLLM */
    module_llm.update();

    /* Handle module response messages */
    for (auto& msg : module_llm.msg.responseMsgList) {

      /* If ASR module message */
      if (msg.work_id == asr_work_id) {
        /* Check message object type */
        if (msg.object == "asr.utf-8.stream") {
          /* Parse message json and get ASR result */
          JsonDocument doc;
          deserializeJson(doc, msg.raw_msg);
          asr_result_new = doc["data"]["delta"].as<String>();
          Serial.printf(">> %s\n", asr_result.c_str());
          break;
        }
      }

      /* If ASR module message (Whisper)*/
      if (msg.work_id == whisper_work_id) {
        /* Check message object type */
        if (msg.object == "asr.utf-8") {
          /* Parse message json and get ASR result */
          JsonDocument doc;
          deserializeJson(doc, msg.raw_msg);
          asr_result_new = doc["data"].as<String>();
          Serial.printf(">> %s\n", asr_result.c_str());
        }
      }
    }

    /* Clear handled messages */
    module_llm.msg.responseMsgList.clear();

    if(asr_result_new != ""){
      asr_result_prev = asr_result;
      asr_result = asr_result_new;
      asr_result_new = "";
      if(asr_result.length() == asr_result_prev.length()){
        //生成終了時は前回の結果と同じ文字列が返ってくる
        Serial.println("ASR complete.");
        break;
      }
    }
    else{
      if(asr_result != ""){
        //「生成終了時は前回の結果と同じ文字列が返ってくる」が無い場合のタイムアウト用カウント
        after_response_count ++;
      }
      else{
        no_response_count ++;
      }
    }

    if(after_response_count > 20){
      Serial.println("ASR complete (determined by loop count).");
      break;
    }

    if(no_response_count > 300){
      Serial.println("ASR timed out.");
      break;
    }

    delay(10);
  }

  return asr_result;
}



String wait_for_whisper_result()
{
  String whisper_result = "";
  int no_response_count = 0;
  
  //Serial.println("Waiting for ASR result.");
  while(1){
    /* Update ModuleLLM */
    module_llm.update();

    /* Handle module response messages */
    for (auto& msg : module_llm.msg.responseMsgList) {

      /* If ASR module message (Whisper)*/
      if (msg.work_id == whisper_work_id) {
        /* Check message object type */
        if (msg.object == "asr.utf-8") {
          /* Parse message json and get ASR result */
          JsonDocument doc;
          deserializeJson(doc, msg.raw_msg);
          whisper_result = doc["data"].as<String>();
          Serial.printf(">> %s\n", whisper_result.c_str());
        }
      }
    }

    /* Clear handled messages */
    module_llm.msg.responseMsgList.clear();

    if(whisper_result != ""){
      Serial.println("Whisper complete.");
      break;
    }
    else{
      no_response_count ++;
    }

    if(no_response_count > 3000){
      Serial.println("Whisper timed out.");
      break;
    }

    delay(10);
  }

  return whisper_result;
}

#endif