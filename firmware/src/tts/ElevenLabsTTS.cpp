#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "rootCA/ElevenLabsRootCA.h"
#include "ElevenLabsTTS.h"
#include "SpiRamJsonDocument.h"
#include "driver/PlayMP3.h"
#include "AudioFileSourceHttpsPostStream.h"


static String dataTemplate = 
"{"
    "\"text\": \"dummy\","
    "\"model_id\": \"eleven_multilingual_v2\","
    "\"voice_settings\": {"
        "\"stability\": 0.5,"
        "\"similarity_boost\": 0.8,"
        "\"style\": 0.0,"
        "\"use_speaker_boost\": true"
    "}"
"}";


void ElevenLabsTTS::stream(String text){
  String tts_url = String("https://api.elevenlabs.io/v1/text-to-speech/")
                    + param.voice + String("/stream")
                    + String("?optimize_streaming_latency=0")
                    + String("&output_format=mp3_44100_128");
  Serial.printf("ElevenLabs URL: %s\n", tts_url.c_str());

  AudioFileSourceHttpsPostStream *stream = new AudioFileSourceHttpsPostStream(tts_url.c_str(), root_ca_elevenlabs);
  stream->addHeader("content-type", "application/json");
  stream->addHeader("Accept", "audio/mpeg");
  stream->addHeader("xi-api-key", param.api_key);
  Serial.println(dataTemplate);
  DynamicJsonDocument doc(2000);
  DeserializationError error = deserializeJson(doc, dataTemplate.c_str());
  if (error) {
    Serial.println("DeserializationError");
    String jsonStr;
    serializeJsonPretty(doc, jsonStr);  // 文字列をシリアルポートに出力する
    Serial.println(jsonStr);
    return;
  }
  doc["text"] = text;
  if(param.model != 0){
    doc["model_id"] = param.model;
  }
  String jsonStr;
  serializeJson(doc, jsonStr);
  stream->addData(jsonStr);
  stream->open(tts_url.c_str());

  AudioFileSourceBuffer *buff = new AudioFileSourceBuffer(stream, preallocateBuffer, preallocateBufferSize);
  playMP3(buff);
  delete stream;
  delete buff;
}
