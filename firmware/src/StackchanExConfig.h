#ifndef __STACKCHAN_EX_CONFIG_H__
#define __STACKCHAN_EX_CONFIG_H__

#include <Stackchan_system_config.h>
#include "llm/ChatGPT/MCPClient.h"


#if defined(ARDUINO_M5STACK_Core2)
  // #define DEFAULT_SERVO_PIN_X 13  //Core2 PORT C
  // #define DEFAULT_SERVO_PIN_Y 14
  #define DEFAULT_SERVO_PIN_X 33  //Core2 PORT A
  #define DEFAULT_SERVO_PIN_Y 32
#elif defined( ARDUINO_M5STACK_FIRE )
  #define DEFAULT_SERVO_PIN_X 21
  #define DEFAULT_SERVO_PIN_Y 22
#elif defined( ARDUINO_M5Stack_Core_ESP32 )
  #define DEFAULT_SERVO_PIN_X 21
  #define DEFAULT_SERVO_PIN_Y 22
#elif defined( ARDUINO_M5STACK_CORES3 )
  #define DEFAULT_SERVO_PIN_X 18  //CoreS3 PORT C
  #define DEFAULT_SERVO_PIN_Y 17
#elif defined( ARDUINO_M5STACK_ATOMS3R )
  #define DEFAULT_SERVO_PIN_X 0   //非対応
  #define DEFAULT_SERVO_PIN_Y 0
#endif


//
// AI機能設定 
//
#define LLM_TYPE_CHATGPT                0
#define LLM_TYPE_MODULE_LLM             1
#define LLM_TYPE_MODULE_LLM_FNCL        2
#define LLM_TYPE_GEMINI                 3
#define LLM_TYPE_CUSTOM_OPENAI          4
#define LLM_N_MCP_SERVERS_MAX           10

#define TTS_TYPE_WEB_VOICEVOX           0
#define TTS_TYPE_ELEVENLABS             1
#define TTS_TYPE_OPENAI                 2
#define TTS_TYPE_AQUESTALK              3
#define TTS_TYPE_MODULE_LLM             4

#define STT_TYPE_GOOGLE                 0
#define STT_TYPE_OPENAI_WHISPER         1
#define STT_TYPE_MODULE_LLM_ASR         2
#define STT_TYPE_MODULE_LLM_WHISPER     3

#define WAKEWORD_TYPE_SIMPLEVOX         0
#define WAKEWORD_TYPE_MODULE_LLM_KWS    1


typedef struct LLMConf {
    int type;
    String model = "";
    int nMcpServers;
    mcp_server_s mcpServer[LLM_N_MCP_SERVERS_MAX];
    bool enableMemory;
    String customEndpoint = "";  // Optional URL for LLM_TYPE_CUSTOM_OPENAI. Empty falls back to api.openai.com. http:// works without a CA; https:// requires customRootCAFile and is refused at send time if the CA is missing.
    String customRootCA = "";    // PEM contents of the root CA, loaded from llm.customRootCAFile (path on SD, e.g. "/customRootCA.pem"). Only needed for https:// customEndpoint. Expected format: a text file containing the -----BEGIN CERTIFICATE----- ... -----END CERTIFICATE----- block(s).
} llm_s;

typedef struct TTSConf {
    int type;
    String model;
    String voice;
} tts_s;

typedef struct STTConf {
    int type;
    String model;
} stt_s;

typedef struct WakeWordConf {
    int type;
    String keyword;
} wakeword_s;

typedef struct AudioConf {
    uint8_t speaker_volume;
} audio_s;

typedef struct ModuleLLMConf {
    int8_t rxPin;
    int8_t txPin;
} moduleLLM_s;

typedef struct ExConfig {
    llm_s llm;
    tts_s tts;
    stt_s stt;
    wakeword_s wakeword;
    audio_s audio;
    moduleLLM_s moduleLLM;
} ex_config_s;


// StackchanSystemConfigを継承します。
class StackchanExConfig : public StackchanSystemConfig
{
    protected:
        bool USE_SERVO_ST;      //servo.txtの1行目のパラメータの格納先（このソフトでは未使用）。
        ex_config_s _ex_parameters;
        fs::FS* _extend_fs = nullptr;  // filesystem in use during loadExtendConfig (SD on most boards, SPIFFS on AtomS3R)


    public:
        StackchanExConfig();
        ~StackchanExConfig();

        void loadExtendConfig(fs::FS& fs, const char *yaml_filename, uint32_t yaml_size) override;
        void setExtendSettings(DynamicJsonDocument doc) override;
        void printExtParameters(void) override;

        ex_config_s getExConfig() { return _ex_parameters; }
        void setExConfig(ex_config_s config) { _ex_parameters = config; } 

        void basicConfigNotFoundCallback(void) override;
        void secretConfigNotFoundCallback(void) override;
        void extendConfigNotFoundCallback(void);

};


#endif
