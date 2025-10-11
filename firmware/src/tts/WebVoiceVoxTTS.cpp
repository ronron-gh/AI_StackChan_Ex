#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "rootCA/WebVoiceVoxRootCA.h"
#include "WebVoiceVoxTTS.h"
#include "SpiRamJsonDocument.h"
#include "driver/PlayMP3.h"

//String VOICEVOX_API_KEY = "";
//String TTS_SPEAKER_NO = "3";
//String TTS_SPEAKER = "&speaker=";
//String TTS_PARMS = TTS_SPEAKER + TTS_SPEAKER_NO;


String WebVoiceVoxTTS::https_get(const char* url, const char* root_ca) {
  String payload = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
//      if (https.begin(*client, "https://api.tts.quest/v1/voicevox/?text=こんにちは世界！)) {  // HTTPS
//      if (https.begin(*client, "https://jigsaw.w3.org/HTTP/connection.html")) {  // HTTPS&speaker=1"
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  return payload;
}

bool WebVoiceVoxTTS::voicevox_tts_json_status(const char* url, const char* json_key, const char* root_ca) {
  bool json_data = false;
  DynamicJsonDocument doc(1000);
  String payload = https_get(url, root_ca);
  if(payload != ""){
    Serial.println(payload);
 //    StaticJsonDocument<1000> doc;
//    JsonObject object = doc.as();
    DeserializationError error = deserializeJson(doc, payload.c_str());
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return json_data;
    }
    json_data = doc[json_key];
//    Serial.println(json_data);
  }
  return json_data;
}

//String tts_status_url;
String WebVoiceVoxTTS::voicevox_tts_url(const char* url, const char* root_ca) {
  String tts_url = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.println(payload);
            //StaticJsonDocument<1000> doc;
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, payload.c_str());
            if (error) {
              Serial.print(F("deserializeJson() failed: "));
              Serial.println(error.f_str());
              return tts_url;
            }
  String json_string;
  serializeJsonPretty(doc, json_string);
  Serial.println("====================");
  Serial.println(json_string);
  Serial.println("====================");

            if(!doc["success"]) return tts_url;
//            const char* mp3url = doc["mp3DownloadUrl"];
            const char* mp3url = doc["mp3StreamingUrl"];
            Serial.println(mp3url);
            Serial.print("isApiKeyValid:");
            if(doc["isApiKeyValid"]) Serial.println("OK");
            else Serial.println("NG");
            tts_url = String(mp3url);

            // const char* status_url = doc["audioStatusUrl"];
            // Serial.println(status_url);
            // //tts_status_url = String(status_url);
            // if(voicevox_tts_json_status(status_url, "isAudioError ", root_ca))  return tts_url;
            // while(!voicevox_tts_json_status(status_url, "isAudioReady", root_ca)) delay(1);
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
      // End extra scoping block
    }  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
  return tts_url;
}

String WebVoiceVoxTTS::URLEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";

  while (*msg != '\0') {
    if ( ('a' <= *msg && *msg <= 'z')
         || ('A' <= *msg && *msg <= 'Z')
         || ('0' <= *msg && *msg <= '9')
         || *msg  == '-' || *msg == '_' || *msg == '.' || *msg == '~' ) {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 0xf];
    }
    msg++;
  }
  return encodedMsg;
}

String WebVoiceVoxTTS::getStreamUrl(String text){
  String tts_url = String("https://api.tts.quest/v3/voicevox/synthesis?key=")+ param.api_key
                    + String("&text=") +  URLEncode(text.c_str())
                    + String("&speaker=") + param.voice;
  String URL = voicevox_tts_url(tts_url.c_str(), root_ca);
  Serial.println(tts_url);
  return URL;
}

void WebVoiceVoxTTS::stream(String text){
  String URL = getStreamUrl(text);
  if(URL == ""){
    Serial.println("failed to get stream URL.");
    return;
  }

  httpsStream = new AudioFileSourceHTTPSStream(URL.c_str(), root_ca);
  buff = new AudioFileSourceBuffer(httpsStream, preallocateBuffer, preallocateBufferSize);

  playMP3(buff);
  delete httpsStream;
  delete buff;
}
