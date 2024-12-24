#include "CloudSpeechClient.h"
//#include "network_param.h"
#include <base64.h>
#include <ArduinoJson.h>
#include "rootCA/rootCAgoogle.h"

String LANG_CODE = "ja-jp";

namespace {
constexpr char* API_HOST = "speech.googleapis.com";
constexpr int API_PORT = 443;
constexpr char* API_PATH = "/v1/speech:recognize";
}  // namespace

CloudSpeechClient::CloudSpeechClient(stt_param_t param) : client(), STTBase(param) {
  client.setCACert(root_ca_google);
  client.setTimeout( 10000 ); 
  //if (!client.connect(API_HOST, API_PORT)) {
  //  Serial.println("Google STT: Connection failed!");
  //}
}

CloudSpeechClient::~CloudSpeechClient() {
  //client.stop();
}

void CloudSpeechClient::PrintHttpBody2(Audio* audio) {
  String enc = base64::encode(audio->paddedHeader, sizeof(audio->paddedHeader));
  enc.replace("\n", "");  // delete last "\n"
  client.print(enc);      // HttpBody2
  char* wavData = (char*)audio->wavData;
  for (int j = 0; j < audio->record_number; j++) {
    enc = base64::encode((byte*)&wavData[j*audio->record_length*2], audio->record_length*2);
    enc.replace("\n", "");// delete last "\n"
    client.print(enc);    //Serial.print(enc); // HttpBody2
    delay(10);
  } 
//  Serial.printf("PrintHttpBody2=%d",len);
}

String CloudSpeechClient::Transcribe(Audio* audio) {
  String HttpBody1 = "{\"config\":{\"encoding\":\"LINEAR16\",\"sampleRateHertz\":16000,\"languageCode\":\""+LANG_CODE+"\"},\"audio\":{\"content\":\"";
  String HttpBody3 = "\"}}\r\n\r\n";
  int httpBody2Length = (audio->wavDataSize + sizeof(audio->paddedHeader))*4/3;  // 4/3 is from base64 encoding
  String ContentLength = String(HttpBody1.length() + httpBody2Length + HttpBody3.length());
//  Serial.printf("HttpBody1=%d httpBody2Length=%d HttpBody3=%d \n",HttpBody1.length(),httpBody2Length,HttpBody3.length());
  String HttpHeader;
  HttpHeader = String("POST ") + String(API_PATH) + String("?key=") + param.api_key
    + String(" HTTP/1.1\r\nHost: ") + String(API_HOST) + String("\r\nContent-Type: application/json\r\nContent-Length: ") + ContentLength + String("\r\n\r\n");
  client.print(HttpHeader); //Serial.print(HttpHeader);
  client.print(HttpBody1); //Serial.print(HttpBody1);
  PrintHttpBody2(audio);
  client.print(HttpBody3); //Serial.print(HttpBody3);
  while (!client.available());
  // Skip HTTP headers
  char endOfHeaders[] = "\r\n\r\n";
  if (!client.find(endOfHeaders)) {
    Serial.println(F("Invalid response"));
    return String("");
  }
  if(client.available())client.read();
  if(client.available())client.read();
  if(client.available())client.read();

  // Parse JSON object
  StaticJsonDocument<500> jsonBuffer;
  DeserializationError error = deserializeJson(jsonBuffer,client);
//root.prettyPrintTo(Serial); //Serial.println("");
  String result = "";
  if (error) {
    Serial.println("Parsing failed!");
    return result;
  } else {
    String  json_string;
    serializeJsonPretty(jsonBuffer, json_string);
    Serial.println("====================");
    Serial.println(json_string);
    Serial.println("====================");
//  root.prettyPrintTo(Serial);
    const char* text = jsonBuffer["results"][0]["alternatives"][0]["transcript"];
    Serial.print("\n認識結果：");
    if(text) {
      result = String (text);
      Serial.println((char *)text);
    }
    else {
      Serial.println("NG");
    }
  }
  return result;
}

String CloudSpeechClient::speech_to_text(){
  String ret;
  Audio* audio = new Audio();
  Serial.println("\r\nRecord start!\r\n");
  audio->Record();  
  Serial.println("Record end\r\n");
  Serial.println("音声認識開始");
  //avatar.setSpeechText("わかりました");
  if (!client.connect(API_HOST, API_PORT)) {
    Serial.println("Google STT: Connection failed!");
    ret = String("");
  }
  else{
    ret = Transcribe(audio);
    client.stop();
  }
  delete audio;
  return ret;
}
