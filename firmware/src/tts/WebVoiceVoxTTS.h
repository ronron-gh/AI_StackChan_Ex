#pragma once


#include "TTSBase.h"

class WebVoiceVoxTTS: public TTSBase{
private:
    AudioFileSourceHTTPSStream *httpsStream;
    AudioFileSourceBuffer *buff;

    String URLEncode(const char* msg);
    String voicevox_tts_url(const char* url, const char* root_ca);
    bool voicevox_tts_json_status(const char* url, const char* json_key, const char* root_ca);
    String https_get(const char* url, const char* root_ca);
    String getStreamUrl(String text);

public:
    WebVoiceVoxTTS(tts_param_t param) : TTSBase(param) {};
    void stream(String text);
};

