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


String json_ChatString = 
"{\"model\": \"gpt-4o\","
"\"messages\": [{\"role\": \"user\", \"content\": \"\"}],"
"\"functions\": [],"
"\"function_call\":\"auto\""
"}";

ChatGPT::ChatGPT(llm_param_t param, int _promptMaxSize)
  : LLMBase(param, _promptMaxSize)
{
  M5.Lcd.println("MCP Servers:");
  for(int i=0; i<param.llm_conf.nMcpServers; i++){
    mcp_client[i] = new MCPClient(param.llm_conf.mcpServer[i].url, 
                                  param.llm_conf.mcpServer[i].port);
    
    if(mcp_client[i]->isConnected()){
      M5.Lcd.println(param.llm_conf.mcpServer[i].name);
    }
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

bool ChatGPT::save_role(){
  InitBuffer="";
  serializeJson(chat_doc, InitBuffer);
  Serial.println("InitBuffer = " + InitBuffer);

  // SPIFFSをマウントする
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  // JSONファイルを作成または開く
  File file = SPIFFS.open("/data.json", "w");
  if(!file){
    Serial.println("Failed to open file for writing");
    return false;
  }

  // JSONデータをシリアル化して書き込む
  serializeJson(chat_doc, file);
  file.close();
  return true;
}

void ChatGPT::load_role(){

  if(SPIFFS.begin(true)){
    File file = SPIFFS.open("/data.json", "r");
    if(file){
      DeserializationError error = deserializeJson(chat_doc, file);
      if(error){
        Serial.println("Failed to deserialize JSON. Init doc by default.");
        init_chat_doc(json_ChatString.c_str());
      }
      else{
        //const char* role = chat_doc["messages"][1]["content"];
        String role = String((const char*)chat_doc["messages"][1]["content"]);
        
        //Serial.println(role);

        if (role != "") {
          init_chat_doc(json_ChatString.c_str());
          JsonArray messages = chat_doc["messages"];
          JsonObject systemMessage1 = messages.createNestedObject();
          systemMessage1["role"] = "system";
          systemMessage1["content"] = role;
          //serializeJson(chat_doc, InitBuffer);
        } else {
          init_chat_doc(json_ChatString.c_str());
        }
      }

    } else {
      Serial.println("Failed to open file for reading");
      init_chat_doc(json_ChatString.c_str());
    }

  } else {
    Serial.println("An Error has occurred while mounting SPIFFS");
    init_chat_doc(json_ChatString.c_str());
  }


  /*
   * MCP tools listをfunctionとして挿入
   */
  for(int s=0; s<param.llm_conf.nMcpServers; s++){
    if(!mcp_client[s]->isConnected()){
      continue;
    }

    for(int t=0; t<mcp_client[s]->nTools; t++){
      chat_doc["functions"].add(mcp_client[s]->toolsListDoc["result"]["tools"][t]);
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
   * InitBufferを初期化
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

        //avatar.setSpeechFont(&fonts::efontJA_12);
        //avatar.setSpeechText(name);
        response = exec_calledFunc(name, args);
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


String ChatGPT::exec_calledFunc(const char* name, const char* args){
  String response = "";

  Serial.println(name);
  Serial.println(args);

  DynamicJsonDocument argsDoc(256);
  DeserializationError error = deserializeJson(argsDoc, args);
  if (error) {
    Serial.print(F("deserializeJson(arguments) failed: "));
    Serial.println(error.f_str());
    avatar.setExpression(Expression::Sad);
    avatar.setSpeechText("エラーです");
    response = "エラーです";
    delay(1000);
    avatar.setSpeechText("");
    avatar.setExpression(Expression::Neutral);
  }else{

    //関数名がいずれかのMCPサーバに属するかを検索し、ヒットしたらリクエストを送信する
    for(int s=0; s<param.llm_conf.nMcpServers; s++){
      if(mcp_client[s]->search_tool(String(name))){
        DynamicJsonDocument tool_params(512);
        tool_params["name"] = String(name);
        tool_params["arguments"] = argsDoc;
        response = mcp_client[s]->mcp_call_tool(tool_params);
        goto END;
      }
    }


    if(strcmp(name, "timer") == 0){
      const int time = argsDoc["time"];
      const char* action = argsDoc["action"];
      Serial.printf("time:%d\n",time);
      Serial.println(action);

      response = timer(time, action);
    }
    else if(strcmp(name, "timer_change") == 0){
      const int time = argsDoc["time"];
      response = timer_change(time);    
    }
    else if(strcmp(name, "get_date") == 0){
      response = get_date();    
    }
    else if(strcmp(name, "get_time") == 0){
      response = get_time();    
    }
    else if(strcmp(name, "get_week") == 0){
      response = get_week();    
    }
#if defined(USE_EXTENSION_FUNCTIONS)
    else if(strcmp(name, "reminder") == 0){
      const int hour = argsDoc["hour"];
      const int min = argsDoc["min"];
      const char* text = argsDoc["text"];
      response = reminder(hour, min, text);
    }
    else if(strcmp(name, "ask") == 0){
      const char* text = argsDoc["text"];
      Serial.println(text);
      response = ask(text);
    }
    else if(strcmp(name, "save_note") == 0){
      const char* text = argsDoc["text"];
      Serial.println(text);
      response = save_note(text);
    }
    else if(strcmp(name, "read_note") == 0){
      response = read_note();    
    }
    else if(strcmp(name, "delete_note") == 0){
      response = delete_note();    
    }
    else if(strcmp(name, "get_bus_time") == 0){
      const int nNext = argsDoc["nNext"];
      Serial.printf("nNext:%d\n",nNext);   
      response = get_bus_time(nNext);    
    }
    else if(strcmp(name, "send_mail") == 0){
      const char* text = argsDoc["message"];
      Serial.println(text);
      response = send_mail(text);
    }
    else if(strcmp(name, "read_mail") == 0){
      response = read_mail();    
    }
#if defined(ARDUINO_M5STACK_CORES3)
    else if(strcmp(name, "register_wakeword") == 0){
      response = register_wakeword();    
    }
    else if(strcmp(name, "wakeword_enable") == 0){
      response = wakeword_enable();    
    }
    else if(strcmp(name, "delete_wakeword") == 0){
      const int idx = argsDoc["idx"];
      Serial.printf("idx:%d\n",idx);   
      response = delete_wakeword(idx);    
    }
#endif  //defined(ARDUINO_M5STACK_CORES3)
#if !defined(MCP_BRAVE_SEARCH)
    else if(strcmp(name, "get_news") == 0){
      response = get_news();    
    }
#endif
    else if(strcmp(name, "get_weathers") == 0){
      response = get_weathers();    
    }
#endif  //if defined(USE_EXTENSION_FUNCTIONS)

  }

END:
  return response;
}