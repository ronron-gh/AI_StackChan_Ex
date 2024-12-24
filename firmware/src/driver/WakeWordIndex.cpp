#if 0   //ウェイクワード数がMAXの時に登録すると古いものから消すということをやりたいが、一旦保留。
#include <Arduino.h>
#include <M5Unified.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include "WakeWordIndex.h"

DynamicJsonDocument jsonWWIndex(1024);

String jsonWWIndexTemplate = 
"{"
"\"wakewords\": ["
  "{"
    "\"idx\": 0,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 1,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 2,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 3,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 4,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 5,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 6,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 7,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 8,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "},"
  "{"
    "\"idx\": 9,"
    "\"timestamp\": 0,"
    "\"protected\": 0,"
    "\"explain\": \"\""
  "}"
"]"
"}";


bool initWWIndexFromTemplate()
{
    bool ret = true;

    Serial.println("Init by Templete");

    DeserializationError error = deserializeJson(jsonWWIndex, jsonWWIndexTemplate.c_str());
    if(error){
        Serial.println("Failed to deserialize JSON from Templete");
        ret = false;
    }
    return ret;
}

bool initWWIndex()
{
    bool ret = true;

    Serial.println("Init Wakeword Index");
    
    if(SPIFFS.begin(true)){
    
        if (SPIFFS.exists(WWINDEX_FILENAME)) {
        
            File file = SPIFFS.open(WWINDEX_FILENAME, "r");
            if(file){
                DeserializationError error = deserializeJson(jsonWWIndex, file);
                if(error){
                    Serial.println("Failed to deserialize JSON from SPIFFS");
                    ret = initWWIndexFromTemplate();
                }
            } else{
                Serial.println("Failed to open JSON on SPIFFS");
                ret = initWWIndexFromTemplate();
            }
        } else{
            Serial.println("JSON does not exist on SPIFFS");
            ret = initWWIndexFromTemplate();
        }

    } else {
        Serial.println("An Error has occurred while mounting SPIFFS");
        ret = initWWIndexFromTemplate();
    }

    String json_str; 
    serializeJsonPretty(jsonWWIndex, json_str);  // 文字列をシリアルポートに出力する
    Serial.println(json_str);
    return ret;
}

#endif