#if defined(USE_LLM_MODULE)

#include <Arduino.h>
#include <M5Unified.h>
#include "ModuleLLM.h"

M5ModuleLLM module_llm;

String kws_work_id;
String asr_work_id;
String tts_work_id;
String llm_work_id;
String language;

void module_llm_setup(module_llm_param_t param)
{
  language = "en_US";

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
  }

  /* Setup ASR module and save returned work id */
  if(param.enableASR){
    M5.Display.printf(">> Setup asr..\n");
    Serial.printf(">> Setup asr..\n");
    m5_module_llm::ApiAsrSetupConfig_t asr_config;
    asr_config.input = {"sys.pcm", kws_work_id};
    asr_work_id      = module_llm.asr.setup(asr_config, "asr_setup", language);
  }

  /* Setup TTS module and save returned work id */
  if(param.enableTTS){
    M5.Display.printf(">> Setup tts..\n");
    Serial.printf(">> Setup tts..\n");
    tts_work_id = module_llm.tts.setup();
  }

  /* Setup LLM module and save returned work id */
  if(param.enableLLM){
    M5.Display.printf(">> Setup llm..\n");
    Serial.printf(">> Setup llm..\n");
    llm_work_id = module_llm.llm.setup(param.m5llm_config);
    if(llm_work_id == ""){
      M5.Display.printf("llm setup timeout\n");
      Serial.printf("llm setup timeout\n");
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
  String asr_result = "";
  int no_response_count = 0;
  
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
          asr_result_prev = asr_result;
          asr_result = doc["data"]["delta"].as<String>();

          //M5.Display.setTextColor(TFT_YELLOW);
          //M5.Display.printf(">> %s\n", asr_result.c_str());
          Serial.printf(">> %s\n", asr_result.c_str());
          break;
        }
      }
    }

    /* Clear handled messages */
    module_llm.msg.responseMsgList.clear();

#if 0   //生成終了時、前回の結果と同じ文字列が返ってくると思われたが、そうとは限らないようなので、単純に取得回数100回でタイムアウトとする
    if(asr_result != ""){
      if(asr_result.length() == asr_result_prev.length()){
        //生成終了時は前回の結果と同じ文字列が返ってくる
        Serial.println("ASR complete.");
        break;
      }
    }
    else{
      no_response_count ++;
    }
#else
    no_response_count ++;
#endif

    if(no_response_count > 100){
      Serial.println("ASR timed out.");
      break;
    }

    delay(10);
  }

  //Serial.println("Break the loop.");
  return asr_result;
}

#endif