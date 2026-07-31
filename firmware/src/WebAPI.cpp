#include <ESP32WebServer.h>
#include <nvs.h>
#include <SD.h>
#include <SPIFFS.h>
#include "WebAPI.h"
#include "Avatar.h"
#include "StackchanExConfig.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "Robot.h"

using namespace m5avatar;
extern Avatar avatar;
extern uint8_t m5spk_virtual_channel;
extern String STT_API_KEY;
extern StackchanExConfig system_config;

ESP32WebServer server(80);

// C++11 multiline string constants are neato...
static const char HEAD[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="UTF-8">
  <title>AIｽﾀｯｸﾁｬﾝ</title>
</head>)KEWL";


#define IMPORT_FILE(section, filename, symbol) \
static constexpr const char* filename_##symbol = filename; \
extern const uint8_t symbol[], sizeof_##symbol[]; \
asm(\
  ".section " #section "\n"\
  ".balign 4\n"\
  ".global " #symbol "\n"\
  #symbol ":\n"\
  ".incbin \"incbin/" filename "\"\n"\
  ".global sizeof_" #symbol "\n"\
  ".set sizeof_" #symbol ", . - " #symbol "\n"\
  ".balign 4\n"\
  ".section \".text\"\n")

IMPORT_FILE(.rodata, "home.html", home_html);
IMPORT_FILE(.rodata, "config.html", config_html);
IMPORT_FILE(.rodata, "config.js", config_js);
IMPORT_FILE(.rodata, "personalize.html", personalize_html);
IMPORT_FILE(.rodata, "personalize.js", personalize_js);

static const char* SPIFFS_SEC_CONFIG_PATH = "/SC_SecConfig.yaml";
static const char* SPIFFS_BASIC_CONFIG_PATH = "/SC_BasicConfig.yaml";
static const char* SPIFFS_EX_CONFIG_PATH = "/SC_ExConfig.yaml";

String read_text_file(fs::FS& fs, const char* path)
{
  File file = fs.open(path, FILE_READ);
  if(!file){
    return "";
  }
  String data = file.readString();
  file.close();
  return data;
}

bool write_text_file_atomic(fs::FS& fs, const char* path, const String& data, String* error)
{
  String temp_path = String(path) + ".tmp";
  File temp = fs.open(temp_path.c_str(), FILE_WRITE);
  if(!temp){
    if(error != nullptr){
      *error = String("Cannot open temp file: ") + temp_path;
    }
    return false;
  }
  size_t written = temp.print(data);
  temp.flush();
  temp.close();
  if(written != data.length()){
    fs.remove(temp_path.c_str());
    if(error != nullptr){
      *error = String("Failed to write full data: ") + path;
    }
    return false;
  }
  if(fs.exists(path)){
    fs.remove(path);
  }
  if(!fs.rename(temp_path.c_str(), path)){
    fs.remove(temp_path.c_str());
    if(error != nullptr){
      *error = String("Failed to rename temp file: ") + path;
    }
    return false;
  }
  return true;
}

bool parse_yaml_file(fs::FS& fs, const char* path, DynamicJsonDocument& doc)
{
  String yaml = read_text_file(fs, path);
  if(yaml.length() == 0){
    return false;
  }
  return !deserializeYml(doc, yaml.c_str());
}

String quote_yaml_string(const String& value)
{
  String quoted = "\"";
  for(size_t i = 0; i < value.length(); i++){
    char c = value.charAt(i);
    if(c == '\\' || c == '"'){
      quoted += '\\';
    }
    quoted += c;
  }
  quoted += "\"";
  return quoted;
}

String json_string_or_empty(JsonVariantConst value)
{
  if(value.is<const char*>()){
    return value.as<String>();
  }
  return "";
}

int json_int_or(JsonVariantConst value, int default_value)
{
  if(value.is<int>()){
    return value.as<int>();
  }
  return default_value;
}

bool json_bool_or(JsonVariantConst value, bool default_value)
{
  if(value.is<bool>()){
    return value.as<bool>();
  }
  return default_value;
}

bool validate_mcp_servers(JsonObjectConst llm, String* error)
{
  JsonVariantConst value = llm["mcpServers"];
  if(value.isNull()){
    return true;
  }
  if(!value.is<JsonArrayConst>()){
    *error = "llm.mcpServers must be an array";
    return false;
  }
  JsonArrayConst servers = value.as<JsonArrayConst>();
  if(servers.size() > LLM_N_MCP_SERVERS_MAX){
    *error = "llm.mcpServers must contain at most " + String(LLM_N_MCP_SERVERS_MAX) + " servers";
    return false;
  }
  int index = 0;
  for(JsonVariantConst item : servers){
    if(!item.is<JsonObjectConst>()){
      *error = "llm.mcpServers[" + String(index) + "] must be an object";
      return false;
    }
    JsonObjectConst server = item.as<JsonObjectConst>();
    if(!server["name"].is<const char*>() || server["name"].as<String>().length() == 0){
      *error = "llm.mcpServers[" + String(index) + "].name is required";
      return false;
    }
    if(!server["disabled"].is<bool>()){
      *error = "llm.mcpServers[" + String(index) + "].disabled must be true or false";
      return false;
    }
    if(!server["url"].is<const char*>() || server["url"].as<String>().length() == 0){
      *error = "llm.mcpServers[" + String(index) + "].url is required";
      return false;
    }
    if(!server["port"].is<int>()){
      *error = "llm.mcpServers[" + String(index) + "].port must be an integer";
      return false;
    }
    int port = server["port"].as<int>();
    if(port < 1 || port > 65535){
      *error = "llm.mcpServers[" + String(index) + "].port must be from 1 to 65535";
      return false;
    }
    index++;
  }
  return true;
}

String build_secret_yaml(JsonObjectConst sec)
{
  JsonObjectConst wifi = sec["wifi"];
  JsonObjectConst apikey = sec["apikey"];
  String yaml;
  yaml += "wifi:\n";
  yaml += "  ssid: " + quote_yaml_string(json_string_or_empty(wifi["ssid"])) + "\n";
  yaml += "  password: " + quote_yaml_string(json_string_or_empty(wifi["password"])) + "\n";
  yaml += "apikey:\n";
  yaml += "  aiservice: " + quote_yaml_string(json_string_or_empty(apikey["aiservice"])) + "\n";
  yaml += "  tts: " + quote_yaml_string(json_string_or_empty(apikey["tts"])) + "\n";
  yaml += "  stt: " + quote_yaml_string(json_string_or_empty(apikey["stt"])) + "\n";
  return yaml;
}

String build_basic_yaml(JsonObjectConst basic)
{
  JsonObjectConst servo = basic["servo"];
  String servo_type = json_string_or_empty(basic["servo_type"]);
  if(servo_type.length() == 0){
    servo_type = "PWM";
  }
  String yaml;
  yaml += "servo:\n";
  yaml += "  pin:\n";
  yaml += "    x: " + String(json_int_or(servo["pin"]["x"], 33)) + "\n";
  yaml += "    y: " + String(json_int_or(servo["pin"]["y"], 32)) + "\n";
  yaml += "  offset:\n";
  yaml += "    x: " + String(json_int_or(servo["offset"]["x"], 0)) + "\n";
  yaml += "    y: " + String(json_int_or(servo["offset"]["y"], 0)) + "\n";
  yaml += "  center:\n";
  yaml += "    x: " + String(json_int_or(servo["center"]["x"], 90)) + "\n";
  yaml += "    y: " + String(json_int_or(servo["center"]["y"], 90)) + "\n";
  yaml += "  lower_limit:\n";
  yaml += "    x: " + String(json_int_or(servo["lower_limit"]["x"], 0)) + "\n";
  yaml += "    y: " + String(json_int_or(servo["lower_limit"]["y"], 60)) + "\n";
  yaml += "  upper_limit:\n";
  yaml += "    x: " + String(json_int_or(servo["upper_limit"]["x"], 180)) + "\n";
  yaml += "    y: " + String(json_int_or(servo["upper_limit"]["y"], 90)) + "\n";
  yaml += "takao_base: ";
  yaml += json_bool_or(basic["takao_base"], false) ? "true\n" : "false\n";
  yaml += "servo_type: " + quote_yaml_string(servo_type) + "\n";
  return yaml;
}

String build_extend_yaml(JsonObjectConst ex)
{
  JsonObjectConst llm = ex["llm"];
  String yaml;
  yaml += "llm:\n";
  yaml += "  type: " + String(json_int_or(llm["type"], LLM_TYPE_CHATGPT)) + "\n";
  yaml += "  enableMemory: ";
  yaml += json_bool_or(llm["enableMemory"], false) ? "true\n" : "false\n";
  JsonArrayConst servers = llm["mcpServers"].as<JsonArrayConst>();
  if(servers.size() == 0){
    yaml += "  mcpServers: []\n";
  }else{
    yaml += "  mcpServers:\n";
    for(JsonObjectConst server : servers){
      yaml += "    - name: " + quote_yaml_string(server["name"].as<String>()) + "\n";
      yaml += "      disabled: ";
      yaml += server["disabled"].as<bool>() ? "true\n" : "false\n";
      yaml += "      url: " + quote_yaml_string(server["url"].as<String>()) + "\n";
      yaml += "      port: " + String(server["port"].as<int>()) + "\n";
    }
  }
  return yaml;
}

void handleRoot() {
  //Serial.println("handleRoot");
  //server.send(200, "text/plain", "hello from m5stack!");
  server.send_P(200, "text/html", (const char*)home_html, (size_t)sizeof_home_html);
}

void handle_config_html() {
  server.send_P(200, "text/html", (const char*)config_html, (size_t)sizeof_config_html);
}

void handle_config_js() {
  server.send_P(200, "application/javascript", (const char*)config_js, (size_t)sizeof_config_js);
}

void handle_personalize_html() {
  server.send_P(200, "text/html", (const char*)personalize_html, (size_t)sizeof_personalize_html);
}

void handle_personalize_js() {
  server.send_P(200, "application/javascript", (const char*)personalize_js, (size_t)sizeof_personalize_js);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
//  server.send(404, "text/plain", message);
  server.send(404, "text/html", String(HEAD) + String("<body>") + message + String("</body>"));
}

void handle_speech() {
  if(robot == nullptr){
    server.send(503, "text/plain", String("Robot is not ready"));
    return;
  }
  String message = server.arg("say");
  String speaker = server.arg("voice");
  //if(speaker != "") {
  //  TTS_PARMS = TTS_SPEAKER + speaker;
  //}
  Serial.println(message);
  ////////////////////////////////////////
  // 音声の発声
  ////////////////////////////////////////
  //avatar.setExpression(Expression::Happy);
  robot->speech(message);
  server.send(200, "text/plain", String("OK"));
}

void handle_chat() {
  if(robot == nullptr){
    server.send(503, "text/plain", String("Robot is not ready"));
    return;
  }
  static String response = "";
  // tts_parms_no = 1;
  String text = server.arg("text");
  String speaker = server.arg("voice");
  //if(speaker != "") {
  //  TTS_PARMS = TTS_SPEAKER + speaker;
  //}

  robot->chat(text);

  server.send(200, "text/html", String(HEAD)+String("<body>")+response+String("</body>"));
}

void handle_role_set() {
  if(robot == nullptr || robot->llm == nullptr){
    server.send(503, "text/plain", String("LLM is not ready"));
    return;
  }
  String html = "";

  // POST以外は拒否
  if (server.method() != HTTP_POST) {
    return;
  }
  String role = server.arg("plain");

  // JSONデータをSPIFFSに保存
  if(robot->llm->save_userRole(role)){
    server.send(200, "text/plain", String("Role set successful"));
  }
  else{
    //html = "Failed to save role to SPIFFS.";
    server.send(500, "text/plain", String("Role set failed"));
  }
};

void handle_role_get() {
  if(robot == nullptr || robot->llm == nullptr){
    server.send(503, "text/plain", String("LLM is not ready"));
    return;
  }

  Serial.println("http request: handle_role_get");
  Serial.println(robot->llm->get_userRole());
  server.send(200, "text/plain", robot->llm->get_userRole());
};

void handle_memory_get() {
  if(robot == nullptr || robot->llm == nullptr){
    server.send(503, "text/plain", String("LLM is not ready"));
    return;
  }
  Serial.println("http request: handle_memory_get");
  Serial.println(robot->llm->get_userInfo());
  server.send(200, "text/plain", robot->llm->get_userInfo());
};

void handle_memory_clear() {
  if(robot == nullptr || robot->llm == nullptr){
    server.send(503, "text/plain", String("LLM is not ready"));
    return;
  }
  Serial.println("http request: handle_memory_clear");
  bool result = robot->llm->clear_userInfo();
  if(result){
    server.send(200, "text/plain", String("Memory clear successful"));
  }else{
    server.send(500, "text/plain", String("Memory clear failed"));
  }
};

void handle_face() {
  String expression = server.arg("expression");
  expression = expression + "\n";
  Serial.println(expression);
  switch (expression.toInt())
  {
    case 0: avatar.setExpression(Expression::Neutral); break;
    case 1: avatar.setExpression(Expression::Happy); break;
    case 2: avatar.setExpression(Expression::Sleepy); break;
    case 3: avatar.setExpression(Expression::Doubt); break;
    case 4: avatar.setExpression(Expression::Sad); break;
    case 5: avatar.setExpression(Expression::Angry); break;  
  } 
  server.send(200, "text/plain", String("OK"));
}

void handle_config_get() {
  DynamicJsonDocument response(8192);
  bool has_sec = SPIFFS.exists(SPIFFS_SEC_CONFIG_PATH);
  bool has_basic = SPIFFS.exists(SPIFFS_BASIC_CONFIG_PATH);
  bool has_ex = SPIFFS.exists(SPIFFS_EX_CONFIG_PATH);
  response["source"] = (has_sec || has_basic || has_ex) ? "spiffs" : "none";
  response["complete"] = has_sec && has_basic && has_ex;

  JsonObject sec = response["sec"].to<JsonObject>();
  JsonObject wifi = sec["wifi"].to<JsonObject>();
  JsonObject apikey = sec["apikey"].to<JsonObject>();
  wifi["ssid"] = "";
  wifi["password"] = "";
  apikey["aiservice"] = "";
  apikey["tts"] = "";
  apikey["stt"] = "";
  DynamicJsonDocument sec_doc(2048);
  if(parse_yaml_file(SPIFFS, SPIFFS_SEC_CONFIG_PATH, sec_doc)){
    wifi["ssid"] = sec_doc["wifi"]["ssid"].as<String>();
    wifi["password"] = sec_doc["wifi"]["password"].as<String>();
    apikey["aiservice"] = sec_doc["apikey"]["aiservice"].as<String>();
    apikey["tts"] = sec_doc["apikey"]["tts"].as<String>();
    apikey["stt"] = sec_doc["apikey"]["stt"].as<String>();
  }else{
    secret_config_s* secret = system_config.getSecretSetting();
    if(secret != nullptr){
      wifi["ssid"] = secret->wifi_info.ssid;
      wifi["password"] = secret->wifi_info.password;
      apikey["aiservice"] = secret->api_key.ai_service;
      apikey["tts"] = secret->api_key.tts;
      apikey["stt"] = secret->api_key.stt;
    }
  }

  JsonObject basic = response["basic"].to<JsonObject>();
  JsonObject servo = basic["servo"].to<JsonObject>();
  servo["pin"]["x"] = 33;
  servo["pin"]["y"] = 32;
  servo["offset"]["x"] = 0;
  servo["offset"]["y"] = 0;
  servo["center"]["x"] = 90;
  servo["center"]["y"] = 90;
  servo["lower_limit"]["x"] = 0;
  servo["lower_limit"]["y"] = 60;
  servo["upper_limit"]["x"] = 180;
  servo["upper_limit"]["y"] = 90;
  basic["takao_base"] = false;
  basic["servo_type"] = "PWM";
  DynamicJsonDocument basic_doc(2048);
  if(parse_yaml_file(SPIFFS, SPIFFS_BASIC_CONFIG_PATH, basic_doc)){
    servo["pin"]["x"] = basic_doc["servo"]["pin"]["x"] | 33;
    servo["pin"]["y"] = basic_doc["servo"]["pin"]["y"] | 32;
    servo["offset"]["x"] = basic_doc["servo"]["offset"]["x"] | 0;
    servo["offset"]["y"] = basic_doc["servo"]["offset"]["y"] | 0;
    servo["center"]["x"] = basic_doc["servo"]["center"]["x"] | 90;
    servo["center"]["y"] = basic_doc["servo"]["center"]["y"] | 90;
    servo["lower_limit"]["x"] = basic_doc["servo"]["lower_limit"]["x"] | 0;
    servo["lower_limit"]["y"] = basic_doc["servo"]["lower_limit"]["y"] | 60;
    servo["upper_limit"]["x"] = basic_doc["servo"]["upper_limit"]["x"] | 180;
    servo["upper_limit"]["y"] = basic_doc["servo"]["upper_limit"]["y"] | 90;
    basic["takao_base"] = basic_doc["takao_base"] | false;
    basic["servo_type"] = basic_doc["servo_type"].as<String>().length() > 0
                         ? basic_doc["servo_type"].as<String>()
                         : String("PWM");
  }

  JsonObject ex = response["ex"].to<JsonObject>();
  JsonObject llm = ex["llm"].to<JsonObject>();
  llm["type"] = LLM_TYPE_CHATGPT;
  llm["enableMemory"] = false;
  JsonArray mcp_servers = llm["mcpServers"].to<JsonArray>();
  DynamicJsonDocument ex_doc(4096);
  if(parse_yaml_file(SPIFFS, SPIFFS_EX_CONFIG_PATH, ex_doc)){
    llm["type"] = ex_doc["llm"]["type"] | LLM_TYPE_CHATGPT;
    llm["enableMemory"] = ex_doc["llm"]["enableMemory"] | false;
    JsonArrayConst stored_servers = ex_doc["llm"]["mcpServers"].as<JsonArrayConst>();
    int count = 0;
    for(JsonObjectConst stored_server : stored_servers){
      if(count >= LLM_N_MCP_SERVERS_MAX){
        break;
      }
      JsonObject server = mcp_servers.createNestedObject();
      server["name"] = stored_server["name"].as<String>();
      server["disabled"] = stored_server["disabled"] | false;
      server["url"] = stored_server["url"].as<String>();
      server["port"] = stored_server["port"] | 0;
      count++;
    }
  }else{
    ex_config_s ex_config = system_config.getExConfig();
    if(ex_config.llm.type == LLM_TYPE_CHATGPT || ex_config.llm.type == LLM_TYPE_GEMINI){
      llm["type"] = ex_config.llm.type;
    }
    llm["enableMemory"] = ex_config.llm.enableMemory;
    for(int i = 0; i < ex_config.llm.nMcpServers && i < LLM_N_MCP_SERVERS_MAX; i++){
      JsonObject server = mcp_servers.createNestedObject();
      server["name"] = ex_config.llm.mcpServer[i].name;
      server["disabled"] = ex_config.llm.mcpServer[i].disabled;
      server["url"] = ex_config.llm.mcpServer[i].url;
      server["port"] = ex_config.llm.mcpServer[i].port;
    }
  }

  String json;
  serializeJson(response, json);
  server.send(200, "application/json", json);
}

void handle_config_post() {
  if(server.method() != HTTP_POST){
    server.send(405, "text/plain", String("Method Not Allowed"));
    return;
  }

  String body = server.arg("plain");
  String error = "";
  DynamicJsonDocument request(8192);
  DeserializationError parse_error = deserializeJson(request, body);
  if(parse_error){
    server.send(400, "text/plain", String("JSON parse error: ") + parse_error.c_str());
    return;
  }
  if(!validate_mcp_servers(request["ex"]["llm"], &error)){
    server.send(400, "text/plain", error);
    return;
  }

  String sec_yaml = build_secret_yaml(request["sec"]);
  String basic_yaml = build_basic_yaml(request["basic"]);
  String ex_yaml = build_extend_yaml(request["ex"]);

  if(!system_config.saveSecretConfigYaml(SPIFFS, SPIFFS_SEC_CONFIG_PATH, sec_yaml, &error)){
    server.send(400, "text/plain", error.length() > 0 ? error : String("Invalid SC_SecConfig.yaml"));
    return;
  }
  if(!write_text_file_atomic(SPIFFS, SPIFFS_BASIC_CONFIG_PATH, basic_yaml, &error)){
    server.send(500, "text/plain", error.length() > 0 ? error : String("Failed to save SC_BasicConfig.yaml"));
    return;
  }
  if(!write_text_file_atomic(SPIFFS, SPIFFS_EX_CONFIG_PATH, ex_yaml, &error)){
    server.send(500, "text/plain", error.length() > 0 ? error : String("Failed to save SC_ExConfig.yaml"));
    return;
  }

  server.send(200, "application/json", String("{\"status\":\"ok\"}"));
}

void handle_config_restart() {
  server.send(200, "text/plain", String("Restarting"));
  delay(200);
  ESP.restart();
}


void init_web_server(void)
{
  // Files
  //
  server.on("/", handleRoot);
  server.on("/config.html", handle_config_html);
  server.on("/config.js", handle_config_js);
  server.on("/personalize.html", handle_personalize_html);
  server.on("/personalize.js", handle_personalize_js);


  // APIs
  //
  server.on("/speech", handle_speech);
  server.on("/face", handle_face);
  server.on("/chat", handle_chat);
  server.on("/role_set", HTTP_POST, handle_role_set);
  server.on("/role_get", handle_role_get);
  server.on("/memory_get", handle_memory_get);
  server.on("/memory_clear", handle_memory_clear);
  server.on("/config", HTTP_GET, handle_config_get);
  server.on("/config", HTTP_POST, handle_config_post);
  server.on("/config/restart", HTTP_POST, handle_config_restart);

  // Other
  //
  server.onNotFound(handleNotFound);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.begin();
  Serial.println("HTTP server started");
  //M5.Lcd.println("HTTP server started");  
}

void web_server_handle_client(void)
{
  server.handleClient();
}
