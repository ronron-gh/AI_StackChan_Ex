#include "StackchanExConfig.h"
#include "share/SDUtil.h"

StackchanExConfig::StackchanExConfig() {};
StackchanExConfig::~StackchanExConfig() {};


void StackchanExConfig::basicConfigNotFoundCallback(void)
{
    char buf[128], data[128];
    char *endp;
    String SV_ON_OFF = "";

    Serial.printf("Cannot open YAML basic config file. Try to read legacy text file.\n");

    /// Servo
    if(read_sd_file("/servo.txt", buf, sizeof(buf))){
        read_line_from_buf(buf, data);
        SV_ON_OFF = String(data);
        if (SV_ON_OFF == "on" || SV_ON_OFF == "ON"){
            USE_SERVO_ST = true;
        }
        else{
            USE_SERVO_ST = false;
        }
        Serial.printf("Servo ON or OFF: %s\n",data);

        read_line_from_buf(nullptr, data);
        _servo[AXIS_X].pin = strtol(data, &endp, 10);
        Serial.printf("Servo pin X: %d\n", _servo[AXIS_X].pin);

        read_line_from_buf(nullptr, data);
        _servo[AXIS_Y].pin = strtol(data, &endp, 10);
        Serial.printf("Servo pin Y: %d\n", _servo[AXIS_Y].pin);
    }
    else{
        Serial.printf("Cannot open legacy text file. Set default value.\n");
        _servo[AXIS_X].pin = DEFAULT_SERVO_PIN_X;
        _servo[AXIS_Y].pin = DEFAULT_SERVO_PIN_Y;
        Serial.printf("Servo pin X: %d\n", _servo[AXIS_X].pin);
        Serial.printf("Servo pin Y: %d\n", _servo[AXIS_Y].pin);
    }
}

void StackchanExConfig::secretConfigNotFoundCallback(void)
{
    char buf[128], data[128];

    Serial.printf("Cannot open YAML secret config file. Try to read legacy text file.\n");

    /// wifi
    if(read_sd_file("/wifi.txt", buf, sizeof(buf))){
        read_line_from_buf(buf, data);
        _secret_config.wifi_info.ssid = String(data);
        Serial.printf("SSID: %s\n",data);

        read_line_from_buf(nullptr, data);
        _secret_config.wifi_info.password = String(data);
        Serial.printf("Key: %s\n",data);
    }

    /// apikey
    if(read_sd_file("/apikey.txt", buf, sizeof(buf))){
        read_line_from_buf(buf, data);
        _secret_config.api_key.ai_service = String(data);
        Serial.printf("openai: %s\n",data);

        read_line_from_buf(nullptr, data);
        _secret_config.api_key.tts = String(data);
        Serial.printf("voicevox: %s\n",data);

        read_line_from_buf(nullptr, data);
        _secret_config.api_key.stt = String(data);
        Serial.printf("stt: %s\n",data);
    }
}

void StackchanExConfig::loadExtendConfig(fs::FS& fs, const char* yaml_filename, uint32_t yaml_size)
{
    M5_LOGI("----- StackchanExConfig::loadExtendConfig:%s\n", yaml_filename);
    _extend_fs = &fs;
    File file = fs.open(yaml_filename);
    if (file) {
        DynamicJsonDocument doc(yaml_size);
        auto err = deserializeYml( doc, file);
        if (err) {
            M5_LOGE("yaml file read error: %s\n", yaml_filename);
            M5_LOGE("error%s\n", err.c_str());
            extendConfigNotFoundCallback();
        }
        else{
            setExtendSettings(doc);
        }

        serializeJsonPretty(doc, Serial);
        M5_LOGI("");
        printExtParameters();

    }
    else{
        //YAMLファイルオープン失敗の場合は、SDのディレクトリ直下に旧TXTファイルがあれば読み込む
        M5_LOGE("yaml file open error: %s\n", yaml_filename);
        extendConfigNotFoundCallback();
        printExtParameters();
    }
}

void StackchanExConfig::extendConfigNotFoundCallback(void)
{
    M5_LOGE("load extend config from txt files\n");

}

void StackchanExConfig::setExtendSettings(DynamicJsonDocument doc)
{
    _ex_parameters.llm.type         = doc["llm"]["type"].as<int>();
    _ex_parameters.llm.model        = doc["llm"]["model"].as<String>();
    _ex_parameters.llm.nMcpServers  = doc["llm"]["mcpServers"].size();
    for(int i=0; i<_ex_parameters.llm.nMcpServers; i++){
        _ex_parameters.llm.mcpServer[i].name = doc["llm"]["mcpServers"][i]["name"].as<String>();
        _ex_parameters.llm.mcpServer[i].disabled = doc["llm"]["mcpServers"][i]["disabled"].as<bool>();
        _ex_parameters.llm.mcpServer[i].url = doc["llm"]["mcpServers"][i]["url"].as<String>();
        _ex_parameters.llm.mcpServer[i].port = doc["llm"]["mcpServers"][i]["port"].as<int>();
    }
    _ex_parameters.llm.enableMemory = doc["llm"]["enableMemory"].as<bool>();
    _ex_parameters.llm.customRootCA = "";
    if(_extend_fs != nullptr && doc["llm"]["customRootCAFile"].is<const char*>()){
        String ca_path = doc["llm"]["customRootCAFile"].as<String>();
        if(ca_path.length() > 0){
            File ca_file = _extend_fs->open(ca_path.c_str());
            if(ca_file){
                size_t sz = ca_file.size();
                _ex_parameters.llm.customRootCA.reserve(sz + 1);
                while(ca_file.available()){
                    char chunk[128];
                    int n = ca_file.readBytes(chunk, sizeof(chunk) - 1);
                    if(n <= 0) break;
                    chunk[n] = 0;
                    _ex_parameters.llm.customRootCA += chunk;
                }
                ca_file.close();
            }
            else{
                M5_LOGE("llm customRootCAFile not found: %s", ca_path.c_str());
            }
        }
    }
    if(doc["llm"]["customEndpoint"].is<const char*>()){
        _ex_parameters.llm.customEndpoint = doc["llm"]["customEndpoint"].as<String>();
        if(_ex_parameters.llm.customEndpoint.startsWith("https://") && _ex_parameters.llm.customRootCA.length() == 0){
            // Keep the endpoint so requests are refused at send time rather
            // than silently retargeted to api.openai.com.
            M5_LOGE("llm customEndpoint is https:// but no customRootCAFile was loaded; requests will be refused: %s",
                    _ex_parameters.llm.customEndpoint.c_str());
        }
    }
    else{
        _ex_parameters.llm.customEndpoint = "";
    }
    if((_ex_parameters.llm.type == LLM_TYPE_CUSTOM_OPENAI)
        && (_ex_parameters.llm.model.length() == 0 || _ex_parameters.llm.model == "null")){
        // The OpenAI-compatible endpoint has no default model; surface the
        // misconfiguration at boot. Requests are refused at send time.
        M5_LOGE("llm type is Custom OpenAI (%d) but model is blank; requests will be refused", LLM_TYPE_CUSTOM_OPENAI);
    }

    _ex_parameters.tts.type         = doc["tts"]["type"].as<int>();
    _ex_parameters.tts.model        = doc["tts"]["model"].as<String>();
    _ex_parameters.tts.voice        = doc["tts"]["voice"].as<String>();

    _ex_parameters.stt.type         = doc["stt"]["type"].as<int>();
    _ex_parameters.stt.model        = doc["stt"]["model"].as<String>();

    _ex_parameters.wakeword.type    = doc["wakeword"]["type"].as<int>();
    _ex_parameters.wakeword.keyword = doc["wakeword"]["keyword"].as<String>();

    _ex_parameters.audio.speaker_volume = doc["audio"]["speaker_volume"].as<int>();

    _ex_parameters.moduleLLM.rxPin  = doc["moduleLLM"]["rxPin"].as<int>();
    _ex_parameters.moduleLLM.txPin  = doc["moduleLLM"]["txPin"].as<int>();

}

void StackchanExConfig::printExtParameters(void)
{
    M5_LOGI("llm type: %d", _ex_parameters.llm.type);
    M5_LOGI("llm model: %s", _ex_parameters.llm.model.c_str());
    M5_LOGI("llm nMcpServers: %d", _ex_parameters.llm.nMcpServers);
    for(int i=0; i<_ex_parameters.llm.nMcpServers; i++){
        M5_LOGI("llm mcpServer[%d] name: %s", i, _ex_parameters.llm.mcpServer[i].name.c_str());
        M5_LOGI("llm mcpServer[%d] disabled: %s", i, _ex_parameters.llm.mcpServer[i].disabled ? "true":"false");
        M5_LOGI("llm mcpServer[%d] url: %s", i, _ex_parameters.llm.mcpServer[i].url.c_str());
        M5_LOGI("llm mcpServer[%d] port: %d", i, _ex_parameters.llm.mcpServer[i].port);
    }
    M5_LOGI("llm enableMemory: %s", _ex_parameters.llm.enableMemory ? "true":"false");
    M5_LOGI("llm customEndpoint: %s", _ex_parameters.llm.customEndpoint.c_str());
    M5_LOGI("llm customRootCA: %s", _ex_parameters.llm.customRootCA.length() > 0 ? "(set)" : "(none)");


    M5_LOGI("tts type: %d", _ex_parameters.tts.type);
    M5_LOGI("tts model: %s", _ex_parameters.tts.model.c_str());
    M5_LOGI("tts voice: %s", _ex_parameters.tts.voice.c_str());

    M5_LOGI("stt type: %d", _ex_parameters.stt.type);
    M5_LOGI("stt model: %s", _ex_parameters.stt.model.c_str());

    M5_LOGI("wakeword type: %d", _ex_parameters.wakeword.type);
    M5_LOGI("wakeword keyword: %s", _ex_parameters.wakeword.keyword.c_str());

    M5_LOGI("audio speaker volume: %d", _ex_parameters.audio.speaker_volume);

    M5_LOGI("module llm rxPin: %d", _ex_parameters.moduleLLM.rxPin);
    M5_LOGI("module llm txPin: %d", _ex_parameters.moduleLLM.txPin);
    
}
