#include <Arduino.h>
#include <M5Unified.h>
#include <SPIFFS.h>
#include <Avatar.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCACertificate.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "ChatGPT.h"
#include "../ChatHistory.h"
#include "FunctionCall.h"
#include "MCPClient.h"
#include "Robot.h"

using namespace m5avatar;
extern Avatar avatar;


const String json_ChatString = 
"{\"model\": \"gpt-4o\","
  "\"messages\": [{\"role\": \"system\", \"content\": \"\"},"     // ユーザーが設定するロール
                  "{\"role\": \"system\", \"content\": \"\"},"    // システム用のロール
                  "{\"role\": \"system\", \"content\": \"User Info: \"}],"  // 長期記憶の要約
  "\"functions\": [],"
  "\"function_call\":\"auto\""
"}";


ChatGPT::ChatGPT(llm_param_t param, int _promptMaxSize)
  : LLMBase(param, _promptMaxSize)
{
  M5.Lcd.println("MCP Servers:");
  for(int i=0; i<param.llm_conf.nMcpServers; i++){
    mcpClient[i] = new MCPClient(param.llm_conf.mcpServer[i].url, 
                                  param.llm_conf.mcpServer[i].port);
    
    if(mcpClient[i]->isConnected()){
      M5.Lcd.println(param.llm_conf.mcpServer[i].name);
    }
  }

  fnCall = new FunctionCall(param, this, mcpClient);
  fnCall->init_func_call_settings(robot->m_config);

  enableMemory(param.llm_conf.enableMemory);
  if(enableMemory()){
    Serial.println("Memory is enabled");
    M5.Lcd.println("Memory is enabled");
  }

  if(promptMaxSize != 0){
    load_role();
  }
  else{
    Serial.println("Prompt buffer is disabled");
  }
}


bool ChatGPT::init_chat_doc(const char *data)
{
  DeserializationError error = deserializeJson(chat_doc, data);
  if (error) {
    Serial.println("DeserializationError");

    String json_str; //= JSON.stringify(chat_doc);
    serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
    Serial.println(json_str);

    return false;
  }
  String json_str; //= JSON.stringify(chat_doc);
  serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
//  Serial.println(json_str);
  return true;
}

void ChatGPT::load_role(){
  String role = "";
  String userInfo = "User Info: ";
  String systemRole = "";
  Serial.println("Load role from SPIFFS.");
  if(enableMemory()){
    systemRole = systemRole_memory;
  }else{
    systemRole = systemRole_noMemory;
  }

  if(load_system_prompt_from_spiffs()){
    role = String((const char*)systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"]);
    //Serial.printf("role length: %d\n", role.length());
    if (role == "") {
      Serial.println("SPIFFS user role is empty. set default role.");
      role = defaultRole;
    }

    userInfo = String((const char*)systemPrompt["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"]);
    //Serial.println(userInfo);
    int idx = userInfo.indexOf("User Info");
    if(idx < 0 || !enableMemory()){
      userInfo = "User Info: ";
    }
  }else{
    // load_system_prompt_from_spiffs()内でSPIFFSからの取得失敗かつ
    // デフォルトのシステムプロンプト設定に失敗した場合（通常起こり得ない）。
    role = defaultRole;
    userInfo = "User Info: ";
  }

  init_chat_doc(json_ChatString.c_str());   // chat_docを初期化

  chat_doc["messages"][SYSTEM_PROMPT_INDEX_USER_ROLE]["content"] = role;
  chat_doc["messages"][SYSTEM_PROMPT_INDEX_SYSTEM_ROLE]["content"] = systemRole;
  chat_doc["messages"][SYSTEM_PROMPT_INDEX_USER_INFO]["content"] = userInfo;

  /*
   * MCP tools listをfunctionとして挿入
   */
  for(int s=0; s<param.llm_conf.nMcpServers; s++){
    if(!mcpClient[s]->isConnected()){
      continue;
    }

    for(int t=0; t<mcpClient[s]->nTools; t++){
      chat_doc["functions"].add(mcpClient[s]->toolsListDoc["result"]["tools"][t]);
    }
  }

  /*
   * FunctionCall.cppで定義したfunctionを挿入
   */
  SpiRamJsonDocument functionsDoc(1024*10);
  DeserializationError error = deserializeJson(functionsDoc, json_Functions.c_str());
  if (error) {
    Serial.println("load_role: JSON deserialization error");
  }

  int nFuncs = functionsDoc.size();
  for(int i=0; i<nFuncs; i++){
    chat_doc["functions"].add(functionsDoc[i]);
  }

  /*
   * InitBuffer(会話履歴を挿入する前のプロンプト)を初期化
   */
  serializeJson(chat_doc, InitBuffer);
  String json_str; 
  serializeJsonPretty(chat_doc, json_str);  // 文字列をシリアルポートに出力する
  Serial.println("Initialized prompt:");
  Serial.println(json_str);
}


String ChatGPT::https_post_json(const char* url, const char* json_string, const char* root_ca) {
  String payload = "";
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client -> setCACert(root_ca);
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
      https.setTimeout( 65000 ); 
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, url)) {  // HTTPS
        Serial.print("[HTTPS] POST...\n");
        // start connection and send HTTP header
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Authorization", String("Bearer ") + param.api_key);
        int httpCode = https.POST((uint8_t *)json_string, strlen(json_string));
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] POST... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
            Serial.println("//////////////");
            Serial.println(payload);
            Serial.println("//////////////");
          }
        } else {
          Serial.printf("[HTTPS] POST... failed, error: %s\n", https.errorToString(httpCode).c_str());
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


#define MAX_REQUEST_COUNT  (10)
void ChatGPT::chat(String text, const char *base64_buf) {
  static String response = "";
  String calledFunc = "";
  //String funcCallMode = "auto";
  bool image_flag = false;

  //Serial.println(InitBuffer);
  //init_chat_doc(InitBuffer.c_str());

  // 質問をチャット履歴に追加
  if(base64_buf == NULL){
    chatHistory.push_back(String("user"), String(""), text);
  }
  else{
    //画像が入力された場合は第2引数を"image"にして識別する
    chatHistory.push_back(String("user"), String("image"), text);
  }

  // functionの実行が要求されなくなるまで繰り返す
  for (int reqCount = 0; reqCount < MAX_REQUEST_COUNT; reqCount++)
  {
    init_chat_doc(InitBuffer.c_str());

    //if(reqCount == (MAX_REQUEST_COUNT - 1)){
    //  funcCallMode = String("none");
    //}

    for (int i = 0; i < chatHistory.get_size(); i++)
    {
      JsonArray messages = chat_doc["messages"];
      JsonObject systemMessage1 = messages.createNestedObject();

      if(chatHistory.get_role(i).equals(String("function"))){
        //Function Callingの場合
        systemMessage1["role"] = chatHistory.get_role(i);
        systemMessage1["name"] = chatHistory.get_funcName(i);
        systemMessage1["content"] = chatHistory.get_content(i);
      }
      else if(chatHistory.get_funcName(i).equals(String("image"))){
        //画像がある場合
        //このようなJSONを作成する
        // messages=[
        //      {"role": "user", "content": [
        //          {"type": "text", "text": "この三角形の面積は？"},
        //          {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{base64_image}"}}
        //      ]}
        //  ],

        String image_url_str = String("data:image/jpeg;base64,") + String(base64_buf); 

        systemMessage1["role"] = chatHistory.get_role(i);
        JsonObject content_text = systemMessage1["content"].createNestedObject();
        content_text["type"] = "text";
        content_text["text"] = chatHistory.get_content(i);
        JsonObject content_image = systemMessage1["content"].createNestedObject();
        content_image["type"] = "image_url";
        content_image["image_url"]["url"] = image_url_str.c_str();

        //次回以降は画像の埋め込みをしないよう、識別用の文字列"image"を消す
        chatHistory.set_funcName(i, "");
      }
      else{
        systemMessage1["role"] = chatHistory.get_role(i);
        systemMessage1["content"] = chatHistory.get_content(i);
      }

    }

    String json_string;
    serializeJson(chat_doc, json_string);

    //serializeJsonPretty(chat_doc, json_string);
    Serial.println("====================");
    Serial.println(json_string);
    Serial.println("====================");

    response = execChatGpt(json_string, calledFunc);


    if(calledFunc == ""){   // Function Callなし ／ Function Call繰り返しの完了
      chatHistory.push_back(String("assistant"), String(""), response);   // 返答をチャット履歴に追加
      robot->speech(response);
      break;
    }
    else{   // Function Call繰り返し中。ループを継続
      chatHistory.push_back(String("function"), calledFunc, response);   // 返答をチャット履歴に追加   
    }

  }

  //チャット履歴の容量を圧迫しないように、functionロールを削除する
  chatHistory.clean_function_role();
}

String ChatGPT::execChatGpt(String json_string, String& calledFunc) {
  String response = "";
  avatar.setExpression(Expression::Doubt);
  avatar.setSpeechFont(&fonts::efontJA_16);
  avatar.setSpeechText("考え中…");
  String ret = https_post_json("https://api.openai.com/v1/chat/completions", json_string.c_str(), root_ca_openai);
  avatar.setExpression(Expression::Neutral);
  avatar.setSpeechText("");
  Serial.println(ret);
  if(ret != ""){
    DynamicJsonDocument doc(2000);
    DeserializationError error = deserializeJson(doc, ret.c_str());
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      avatar.setExpression(Expression::Sad);
      avatar.setSpeechText("エラーです");
      response = "エラーです";
      delay(1000);
      avatar.setSpeechText("");
      avatar.setExpression(Expression::Neutral);
    }else{
      const char* data = doc["choices"][0]["message"]["content"];
      
      // content = nullならfunction call
      if(data == 0){
        const char* name = doc["choices"][0]["message"]["function_call"]["name"];
        const char* args = doc["choices"][0]["message"]["function_call"]["arguments"];
        calledFunc = String(name);
        //avatar.setSpeechFont(&fonts::efontJA_12);
        //avatar.setSpeechText(name);
        response = fnCall->exec_calledFunc(name, args);
      }
      else{
        Serial.println(data);
        response = String(data);
        std::replace(response.begin(),response.end(),'\n',' ');
        calledFunc = String("");
      }
    }
  } else {
    avatar.setExpression(Expression::Sad);
    avatar.setSpeechFont(&fonts::efontJA_16);
    avatar.setSpeechText("わかりません");
    response = "わかりません";
    delay(1000);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);
  }
  return response;
}

