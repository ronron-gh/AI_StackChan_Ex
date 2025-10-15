#include "FunctionCall.h"
#include <Avatar.h>
#include "Robot.h"
#include <AudioGeneratorMP3.h>
#include "driver/AudioOutputM5Speaker.h"
#include <HTTPClient.h>
#include <SD.h>
//#include <EMailSender.h>
#include "MailClient.h"
#include "driver/WakeWord.h"
#include "Robot.h"
#include "Scheduler.h"
#include "StackchanExConfig.h" 
#include "SDUtil.h"
#include "MCPClient.h"
using namespace m5avatar;



// 外部参照
extern Avatar avatar;
extern AudioGeneratorMP3 *mp3;
extern bool servo_home;
extern void sw_tone();

static String avatarText;

// メモ機能関連
String note = "";

// 天気予報取得機能関連
//   city IDはsetup()でSDカードのファイルから読み込む。
//   city IDの定義はここを見てください。https://weather.tsukumijima.net/primary_area.xml
String weatherUrl = "http://weather.tsukumijima.net/api/forecast/city/";
String weatherCityID = "";

// 最新ニュース取得機能関連
//   API keyはsetup()でSDカードのファイルから読み込む。
String newsApiUrl = "https://newsapi.org/v2/top-headlines?country=jp&apiKey=";
String newsApiKey = "";

// タイマー機能関連
TimerHandle_t xAlarmTimer;
bool alarmTimerCallbacked = false;
void alarmTimerCallback(TimerHandle_t xTimer);
void powerOffTimerCallback(TimerHandle_t xTimer);

//static String timer(int32_t time, const char* action);
//static String timer_change(int32_t time);


// Function Call関連の初期化
void init_func_call_settings(StackchanExConfig& system_config)
{
  newsApiKey = system_config.getExConfig().news.apikey;
  weatherCityID = system_config.getExConfig().weather.city_id;
  authMailAdr = system_config.getExConfig().mail.account;
  authAppPass = system_config.getExConfig().mail.app_pwd;
  toMailAdr = system_config.getExConfig().mail.to_addr;

  /// メモがあるか確認
  {
    String filename = String(APP_DATA_PATH) + String(FNAME_NOTEPAD);
    char buf[512];
    if(read_sd_file(filename.c_str(), buf, sizeof(buf))){
      note = String(buf);
      Serial.printf("Notepad: %s\n", buf);
    }
  }
}


const String json_Functions =
"["
  "{"
    "\"name\": \"timer\","
    "\"description\": \"指定した時間が経過したら、指定した動作を実行する。指定できる動作はalarmとshutdown。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"time\":{"
          "\"type\": \"integer\","
          "\"description\": \"指定したい時間。単位は秒。\""
        "},"
        "\"action\":{"
          "\"type\": \"string\","
          "\"description\": \"指定したい動作。alarmは音で通知。shutdownは電源OFF。\","
          "\"enum\": [\"alarm\", \"shutdown\"]"
        "}"
      "},"
      "\"required\": [\"time\", \"action\"]"
    "}"
  "},"
  "{"
    "\"name\": \"timer_change\","
    "\"description\": \"実行中のタイマーの設定時間を変更する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"time\":{"
          "\"type\": \"integer\","
          "\"description\": \"変更後の時間。単位は秒。0の場合はタイマーをキャンセルする。\""
        "}"
      "},"
      "\"required\": [\"time\"]"
    "}"
  "},"
  "{"
    "\"name\": \"get_date\","
    "\"description\": \"今日の日付を取得する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"get_time\","
    "\"description\": \"現在の時刻を取得する。タイマー設定したり時刻表を調べたりする前にこの関数で現在時刻を調べる。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"get_week\","
    "\"description\": \"今日が何曜日かを取得する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
#if !defined(USE_EXTENSION_FUNCTIONS)
  "}"
#else
  "},"
  "{"
    "\"name\": \"reminder\","
    "\"description\": \"指定した時間にリマインドする。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"hour\":{"
          "\"type\": \"integer\","
          "\"description\": \"hour (0-23)\""
        "},"
        "\"min\":{"
          "\"type\": \"integer\","
          "\"description\": \"minute\""
        "},"
        "\"text\":{"
          "\"type\": \"string\","
          "\"description\": \"リマインドする内容\""
        "}"
      "},"
      "\"required\": [\"hour\",\"min\",\"text\"]"
    "}"
  "},"
  "{"
    "\"name\": \"ask\","
    "\"description\": \"依頼を実行するのに必要な情報を会話の相手に質問する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"text\":{"
          "\"type\": \"string\","
          "\"description\": \"質問の内容。\""
        "}"
      "}"
    "}"
  "},"
  "{"
    "\"name\": \"save_note\","
    "\"description\": \"メモを保存する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"text\":{"
          "\"type\": \"string\","
          "\"description\": \"メモの内容。\""
        "}"
      "},"
      "\"required\": [\"text\"]"
    "}"
  "},"
  "{"
    "\"name\": \"read_note\","
    "\"description\": \"メモを読み上げる。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"delete_note\","
    "\"description\": \"メモを消去する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"get_bus_time\","
    "\"description\": \"次のバス（または電車）の時刻を取得する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"nNext\":{"
          "\"type\": \"integer\","
          "\"description\": \"次の発車時刻を取得する場合は0にする。次の次の発車時刻を取得する場合は1にする。\""
        "}"
      "},"
      "\"required\": [\"nNext\"]"
    "}"
  "},"
  "{"
    "\"name\": \"send_mail\","
    "\"description\": \"メールを送信する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"message\":{"
          "\"type\": \"string\","
          "\"description\": \"メールの内容。\""
        "}"
      "}"
    "}"
  "},"
  "{"
    "\"name\": \"read_mail\","
    "\"description\": \"受信メールを読み上げる。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
#if defined(ARDUINO_M5STACK_CORES3)
  "{"
    "\"name\": \"register_wakeword\","
    "\"description\": \"ウェイクワードを登録する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"wakeword_enable\","
    "\"description\": \"ウェイクワードを有効化する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"delete_wakeword\","
    "\"description\": \"ウェイクワードを削除する。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {"
        "\"idx\":{"
          "\"type\": \"integer\","
          "\"description\": \"削除するウェイクワードの番号。\""
        "}"
      "},"
      "\"required\": [\"idx\"]"
    "}"
  "},"
#endif  //defined(ARDUINO_M5STACK_CORES3)
  "{"
    "\"name\": \"get_news\","
    "\"description\": \"最新のニュースを取得して読み上げる。\","
    "\"parameters\": {"
      "\"type\":\"object\","
      "\"properties\": {}"
    "}"
  "},"
  "{"
    "\"name\": \"get_weathers\","
    "\"description\": \"天気予報を取得。\","
    "\"parameters\":  {"
      "\"type\":\"object\","
      "\"properties\": {},"
      "\"required\": []"
    "}"
  "}"
#endif //if defined(USE_EXTENSION_FUNCTIONS)
"]";


void alarmTimerCallback(TimerHandle_t _xTimer){
  xAlarmTimer = NULL;
  Serial.println("時間になりました。");
  avatar.setSpeechText("");
  alarmTimerCallbacked = true;
}

void powerOffTimerCallback(TimerHandle_t _xTimer){
  xAlarmTimer = NULL;
  Serial.println("おやすみなさい。");
  avatar.setSpeechText("おやすみなさい。");
  delay(2000);
  avatar.setSpeechText("");
  M5.Power.powerOff();
}

String timer(int32_t time, const char* action){
  String response = "";

  if(xAlarmTimer != NULL){
    response = "別のタイマーを実行中です。";
  }
  else{
    if(strcmp(action, "alarm") == 0){
      xAlarmTimer = xTimerCreate("Timer", time * 1000, pdFALSE, 0, alarmTimerCallback);
      if(xAlarmTimer != NULL){
        xTimerStart(xAlarmTimer, 0);
        response = String("アラーム設定成功。") ;
      }
      else{
        response = "タイマーの設定が失敗しました。";
      }
    }
    else if(strcmp(action, "shutdown") == 0){
      xAlarmTimer = xTimerCreate("Timer", time * 1000, pdFALSE, 0, powerOffTimerCallback);
      if(xAlarmTimer != NULL){
        xTimerStart(xAlarmTimer, 0);
        response = String(time) + "秒後にパワーオフします。";
      }
      else{
        response = "タイマー設定が失敗しました。";
      }
    }
  }

  Serial.println(response);
  return response;
}

String timer_change(int32_t time){
  String response = "";
  if(time == 0){
    xTimerDelete(xAlarmTimer, 0);
    xAlarmTimer = NULL;
    response = "タイマーをキャンセルしました。";
  }
  else{
    xTimerChangePeriod(xAlarmTimer, time * 1000, 0);
    response = "タイマーの設定時間を" + String(time) + "秒に変更しました。";
  }

  return response;
}


String get_date(){
  String response = "";
  struct tm timeInfo; 

  if (getLocalTime(&timeInfo)) {                            // timeinfoに現在時刻を格納
    response = String(timeInfo.tm_year + 1900) + "年"
               + String(timeInfo.tm_mon + 1) + "月"
               + String(timeInfo.tm_mday) + "日";
  }
  else{
    response = "時刻取得に失敗しました。";
  }
  return response;
}

String get_time(){
  String response = "";
  struct tm timeInfo; 

  if (getLocalTime(&timeInfo)) {                            // timeinfoに現在時刻を格納
    response = String(timeInfo.tm_hour) + "時" + String(timeInfo.tm_min) + "分";
  }
  else{
    response = "時刻取得に失敗しました。";
  }
  return response;
}


String get_week(){
  String response = "";
  struct tm timeInfo; 
  const char week[][8] = {"日", "月", "火", "水", "木", "金", "土"};

  if (getLocalTime(&timeInfo)) {                            // timeinfoに現在時刻を格納
    response = String(week[timeInfo.tm_wday]) + "曜日";
  }
  else{
    response = "時刻取得に失敗しました。";
  }
  return response;
}

#if defined(USE_EXTENSION_FUNCTIONS)

String reminder(int hour, int min, const char* text){
  String response = "";
  int ret;
  
  Serial.println("reminder");
  Serial.printf("%d:%d\n", hour, min);
  Serial.println(text);

  add_schedule(new ScheduleReminder(hour, min, String(text)));
  
  //response = String("Reminder setting successful");
  response = String(String("リマインドの設定成功。")
                    + String(hour) + ":" + String(min) + " "
                    + String(text));
  
  avatarText = String(hour) + ":" + String(min) + String("に設定しました");
  avatar.setSpeechText(avatarText.c_str());
  delay(2000);
  return response;
}


String ask(const char* text){

  bool prev_servo_home = servo_home;
//#ifdef USE_SERVO
  servo_home = true;
//#endif
  avatar.setExpression(Expression::Happy);
  robot->speech(String(text));
  sw_tone();
  avatar.setSpeechText("どうぞ話してください");
  String ret = robot->listen();
//#ifdef USE_SERVO
  servo_home = prev_servo_home;
//#endif
  Serial.println("音声認識終了");
  if(ret != "") {
    Serial.println(ret);

  } else {
    Serial.println("音声認識失敗");
    avatar.setExpression(Expression::Sad);
    avatar.setSpeechText("聞き取れませんでした");
    delay(2000);

  } 

  avatar.setSpeechText("");
  avatar.setExpression(Expression::Neutral);
  //M5.Speaker.begin();

  return ret;
}


String save_note(const char* text){
  String response = "";
  String filename = String(APP_DATA_PATH) + String(FNAME_NOTEPAD);
  
  if (SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    auto fs = SD.open(filename.c_str(), FILE_WRITE, true);

    if(fs) {
      if(note == ""){
        note = String(text);
      }
      else{
        note = note + "\n" + String(text);
      }
      Serial.println(note);
      fs.write((uint8_t*)note.c_str(), note.length());
      fs.close();
      SD.end();
      //response = "Note saved successfully";
      response = String("メモの保存成功。メモの内容：" + String(text));
    }
    else{
      response = "メモの保存に失敗しました";
      SD.end();
    }

  }
  else{
    response = "メモの保存に失敗しました";
  }
  return response;
}

String read_note(){
  String response = "";
  if(note == ""){
    response = "メモはありません";
  }
  else{
    response = "メモの内容は次の通り。" + note;
  }
  return response;
}

String delete_note(){
  String response = "";
  String filename = String(APP_DATA_PATH) + String(FNAME_NOTEPAD);


  if (SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    auto fs = SD.open(filename.c_str(), FILE_WRITE, true);

    if(fs) {
      note = "";
      fs.write((uint8_t*)note.c_str(), note.length());
      fs.close();
      SD.end();
      response = "メモを消去しました";
    }
    else{
      response = "メモの消去に失敗しました";
      SD.end();
    }
  }
  else{
    response = "メモの消去に失敗しました";
  }
  return response;
}


String get_bus_time(int nNext){
  String response = "";
  String filename = "";
  int now;
  int nNextCnt = 0;
  struct tm timeInfo;

  if (getLocalTime(&timeInfo)) {                            // timeinfoに現在時刻を格納
    Serial.printf("現在時刻 %02d:%02d  曜日 %d\n", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_wday);
    now = timeInfo.tm_hour * 60 + timeInfo.tm_min;
    
    switch(timeInfo.tm_wday){
      case 0:   //日
        filename = String(APP_DATA_PATH) + (FNAME_BUS_TIMETABLE_HOLIDAY);
        break;
      case 1:   //月
      case 2:   //火
      case 3:   //水
      case 4:   //木
      case 5:   //金
        filename = String(APP_DATA_PATH) + (FNAME_BUS_TIMETABLE);
        break;
      case 6:   //土
        filename = String(APP_DATA_PATH) + (FNAME_BUS_TIMETABLE_SAT);
        break;
    }

    if (SD.begin(GPIO_NUM_4, SPI, 25000000)) {
      auto fs = SD.open(filename.c_str(), FILE_READ);

      if(fs) {
        int hour, min;
        size_t max = 8;
        char buf[max];

        for(int line=0; line<200; line++){
          int len = fs.readBytesUntil(0x0a, (uint8_t*)buf, max);
          if(len == 0){
            Serial.printf("End of file. total line : %d\n", line);
            response = "最後の発車時刻を過ぎました。";
            break;
          }
          else{
            sscanf(buf, "%d:%d", &hour, &min);
            Serial.printf("%03d %02d:%02d\n", line, hour, min);

            int table = hour * 60 + min;
            if(now < table){
              if(nNextCnt == nNext){
                response = String(hour) + "時" + String(min) + "分";
                break;
              }
              else{
                nNextCnt ++;
              }
            }

          }
        }
        fs.close();

      } else {
        Serial.println("Failed to SD.open().");    
        response = "時刻表の読み取りに失敗しました。";  
      }

      SD.end();
    }
    else{
      response = "時刻表の読み取りに失敗しました。";
    }
  }
  else{
    response = "現在時刻の取得に失敗しました。";
  }

  return response;
}


//メッセージをメールで送信する関数
// ※EMailSenderライブラリのインクルード、及びplatformio.iniのlib_depsでの宣言を有効化してください。
String send_mail(String msg) {
  String response = "";
  EMailSender::EMailMessage message;

  if (authMailAdr != "") {

    EMailSender emailSend(authMailAdr.c_str(), authAppPass.c_str());

    message.subject = "スタックチャンからの通知";
    message.message = msg;
    EMailSender::Response resp = emailSend.send(toMailAdr.c_str(), message);

    if(resp.status == true){
      response = "メール送信成功";
    }
    else{
      response = "メール送信失敗";
    }

  }
  else{
    response = "メールアカウント情報のエラー";
  }


  return response;
}

//受信したメールを読み上げる
String read_mail(void) {
  String response = "";

  if(recvMessages.size() > 0){
    response = String(recvMessages[0]);
    recvMessages.pop_front();
    prev_nMail = recvMessages.size();
  }
  else{
    response = "受信メールはありません。";
  }
  
  return response;
}

#if defined(ARDUINO_M5STACK_CORES3)
bool register_wakeword_required = false;
String register_wakeword(void){
  String response = "ウェイクワードを登録します。合図の後にウェイクワードを発声してください。";
  register_wakeword_required = true;
  return response;
}

bool wakeword_enable_required = false;
String wakeword_enable(void){
  String response = "ウェイクワードを有効化しました。";
  wakeword_enable_required = true;
  return response;
}


String delete_wakeword(int idx){
  SPIFFS.begin(true);
  String filename = filename_base + String(idx) + String(".bin");
  if (SPIFFS.exists(filename.c_str()))
  {
    SPIFFS.remove(filename.c_str());
    delete_mfcc(idx);
  }
  String response = String("ウェイクワード#") + String(idx) + String("を削除しました。");
  return response;
}
#endif  //defined(ARDUINO_M5STACK_CORES3)

// 最新のニュースをWeb APIで取得する関数
String get_news(){
  String response = "";
  DynamicJsonDocument doc(2048*10);//ここの数値が小さいと上手く取得できませんでした。

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // ニュースAPIのURL
    String url = newsApiUrl + newsApiKey;

    // HTTPリクエストを送信します
    http.begin(url); // URLをHTTPクライアントに渡す
    int httpCode = http.GET(); // URLはbegin()で渡しているため、GET()メソッドには引数が必要ありません

    // HTTPステータスコードを確認します
    if (httpCode == HTTP_CODE_OK) {
      // レスポンスデータを取得します
      String payload = http.getString();

      // JSONデータをパースして必要な情報を抽出します
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        // ニュースの一覧が空でないことを確認します
        if (doc.containsKey("articles") && doc["articles"].size() > 0) {
          // ニュースの一覧から最初の5件のタイトルを取得します
          for (int j = 0; j < 5; j++) {
            String title = doc["articles"][j]["title"];
            response += title + "\n"; // タイトルを改行してresponseに追加
          }
        } else {
          response = "ニュースが見つかりませんでした。";
        }
      } else {
        response = "JSONの解析に失敗しました。";
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
      }
    } else {
      response = "HTTPリクエストが失敗しました。";
      Serial.printf("HTTP error: %d\n", httpCode);
    }

  //  http.end(); // HTTPセッションを閉じます
  } else {
    response = "WiFiに接続されていません。";
    Serial.println("WiFi is not connected.");
  }

  return response;
}


// 今日の天気をWeb APIで取得する関数
String get_weathers(){
  String payload;
  String response = "";
  DynamicJsonDocument doc(4096);

  if ((WiFi.status() == WL_CONNECTED))
  {
    HTTPClient http;
    //http.begin("http://weather.tsukumijima.net/api/forecast/city/140010");
    http.begin(weatherUrl + weatherCityID);
    int httpCode = http.GET();

    if (httpCode > 0)
    {
      if (httpCode == HTTP_CODE_OK)
      {
        payload = http.getString();
      }
    }
    if (httpCode <= 0)
    {
      Serial.println("Error on HTTP request");
      return "エラーです。";
    }

    deserializeJson(doc, payload);
    String date = doc["publicTimeFormatted"];
    String forecast = doc["description"]["bodyText"];

    Serial.println(date);
    Serial.println(forecast);

    response = forecast;
  }
  return response;
}

#endif  //if defined(USE_EXTENSION_FUNCTIONS)



#ifdef MCP_GOOGLE_CALENDAR

String calendar_search(){
  /*
   *  リクエストのJSONに挿入するツールパラメータのJSONを作成
   */
  DynamicJsonDocument tool_params(512);
  tool_params["name"] = "search_calendar_events";
  //tool_params["arguments"]["calendar_type"] = "primary";

  //String json_str;
  //serializeJsonPretty(tool_params, json_str);  // 文字列をシリアルポートに出力する
  //Serial.println(json_str);

  /*
   *  MCPサーバにリクエスト
   */
  //String result = mcp_call_tool("192.168.3.105", 8000, tool_params);
  String result = mcp_calendar->mcp_call_tool(tool_params);

#if 0
  /*
   *  レスポンスを解析
   */
  DynamicJsonDocument tool_res(1024);
  DeserializationError error = deserializeJson(tool_res, result);
  if (error) {
    Serial.println("DeserializationError");
  }
  //String json_str;
  //serializeJsonPretty(tool_res, json_str);  // 文字列をシリアルポートに出力する
  //Serial.println(json_str);

  DynamicJsonDocument event(1024);
  JsonArray events = tool_res["result"]["content"];
  String events_str;
  for(int i=0; i < events.size(); i++){
    error = deserializeJson(event, events[i]["text"]);
    if (error) {
      Serial.println("DeserializationError");
    }
    //serializeJsonPretty(event, json_str);  // 文字列をシリアルポートに出力する
    //Serial.println(json_str);

    events_str += event["start"].as<String>() + " " + event["summary"].as<String>() + "\n";
  }
  Serial.println(events_str);
  return events_str;
#endif
  return result;
}

#endif  //MCP_GOOGLE_CALENDAR

#ifdef MCP_BRAVE_SEARCH

String web_search(String& query){
  /*
   *  リクエストのJSONに挿入するツールパラメータのJSONを作成
   */
  DynamicJsonDocument tool_params(512);
  tool_params["name"] = "brave_web_search";
  tool_params["arguments"]["query"] = query;

  String json_str;
  serializeJsonPretty(tool_params, json_str);  // 文字列をシリアルポートに出力する
  Serial.println(json_str);

  /*
   *  MCPサーバにリクエスト
   */
  //String result = mcp_call_tool("192.168.3.105", 8003, tool_params);
  String result = mcp_web_search->mcp_call_tool(tool_params);

  return result;
}

#endif  //MCP_BRAVE_SEARCH

