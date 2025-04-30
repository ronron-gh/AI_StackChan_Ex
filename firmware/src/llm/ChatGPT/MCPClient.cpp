#include "MCPClient.h"

//const char* mcpAddr = "192.168.3.105";
//const uint16_t mcpPort = 8000;
HTTPClient http;
WiFiClient stream;
const char* sseUrl = "http://192.168.3.105:8000/sse"; // SSEのエンドポイント
String mcpPostUrl;

String mcpInitJson =
"{"
"\"method\":\"initialize\","
"\"params\":{"
  "\"protocolVersion\":\"2024-11-05\","
    "\"capabilities\":{"
      "\"sampling\":{},"
      "\"roots\":{\"listChanged\":true}"
    "},"
  "\"clientInfo\":{"
    "\"name\":\"mcp\","
    "\"version\":\"0.1.0\""
    "}"
  "},"
"\"jsonrpc\":\"2.0\","
"\"id\":0"
"}";

String InitNotificationJson = 
"{"
"\"method\":\"notifications/initialized\","
"\"jsonrpc\":\"2.0\""
"}";

String toolsListJson = 
"{"
"\"method\":\"tools/list\","
"\"jsonrpc\":\"2.0\","
"\"id\":1"
"}";

#if 0
String toolCallJson = 
"{"
"\"method\":\"tools/call\","
"\"params\":{"
  "\"name\":\"search_calendar_events_by_type\","
  "\"arguments\":{"
  "\"calendar_type\":\"primary\"}"
  "},"
"\"jsonrpc\":\"2.0\","
"\"id\":1"
"}";
#endif

String toolCallJson = 
"{"
"\"method\":\"tools/call\","
"\"params\":{},"
"\"jsonrpc\":\"2.0\","
"\"id\":1"
"}";

bool init_finished = false;
bool init_notification_finished = false;
bool tools_list_requesting = false;
bool tool_calling = false;
bool waiting_tool_response = false;
bool tool_call_complete = false;

String tool_response;

static void pole_stream(const char* mcpAddr, uint16_t mcpPort, DynamicJsonDocument& tool_params);

String mcp_request_tool_call(const char* mcpAddr, uint16_t mcpPort, DynamicJsonDocument& tool_params)
{

    init_finished = false;
    init_notification_finished = false;
    tools_list_requesting = false;
    tool_calling = false;
    waiting_tool_response = false;
    tool_call_complete = false;
  
    if (stream.connect(mcpAddr,mcpPort)) {
      stream.print(String("GET ") + "/sse" + " HTTP/1.1\r\n" +
                   "Host: " + mcpAddr + "\r\n" +
                   "Accept: text/event-stream\r\n" +
                   "Connection: keep-alive\r\n\r\n");
    }
  
    while(!tool_call_complete){
      pole_stream(mcpAddr, mcpPort, tool_params);
      delay(100);
    }
  
    stream.stop();

    return tool_response;
}

void pole_stream(const char* mcpAddr, uint16_t mcpPort, DynamicJsonDocument& tool_params)
{
  if (stream.available()) {
    String line = stream.readStringUntil('\n');
    Serial.println("Received: " + line);
      
    if (line.startsWith("data: ")) {
      String data = line.substring(6);
      data.trim();
      //Serial.println("Received: " + data);
      
      if(!init_finished){
        init_finished = true;
        mcpPostUrl = String("http://") + mcpAddr + ":" + String(mcpPort) + data;
        Serial.printf("MCP POST URL: %s\n", mcpPostUrl.c_str());
        if (http.begin(mcpPostUrl)) {
          http.addHeader("Content-Type", "application/json");
          int httpCode = http.POST((uint8_t *)mcpInitJson.c_str(), mcpInitJson.length());
        
        } else {
          Serial.printf("[HTTP] Unable to connect MCP POST URL\n");
        }

      }
      else if(init_finished && !init_notification_finished){
        Serial.printf("notifications/initialized\n");
        init_notification_finished = true;
        int httpCode = http.POST((uint8_t *)InitNotificationJson.c_str(), InitNotificationJson.length());
#if 0
        if(tool_list){
          Serial.printf("tools list\n");
          httpCode = http.POST((uint8_t *)toolsListJson.c_str(), toolsListJson.length());
        
          if(tool_call){
            tool_calling = true;
          }
        }
        else if(tool_call){
          tool_calling = true;
        }
#else
        //Serial.printf("tools list\n");
        //httpCode = http.POST((uint8_t *)toolsListJson.c_str(), toolsListJson.length());
        //tool_calling = true;

        Serial.printf("tool call\n");
        waiting_tool_response = true;

        DynamicJsonDocument tool_call(512);
        DeserializationError error = deserializeJson(tool_call, toolCallJson.c_str());
        if (error) {
        Serial.println("tool call: JSON deserialization error");
        }

        tool_call["params"] = tool_params;

        String json_str;
        serializeJsonPretty(tool_call, json_str);  // 文字列をシリアルポートに出力する
        Serial.println(json_str);

        httpCode = http.POST((uint8_t *)json_str.c_str(), json_str.length());

#endif
      }
#if 0
      else if(tool_calling){
        Serial.printf("tool call response\n");
        tool_calling = false;
        waiting_tool_response = true;

        DynamicJsonDocument tool_call(512);
        DeserializationError error = deserializeJson(tool_call, toolCallJson.c_str());
        if (error) {
        Serial.println("tool call: JSON deserialization error");
        }

        tool_call["params"] = tool_params;

        String json_str;
        serializeJsonPretty(tool_call, json_str);  // 文字列をシリアルポートに出力する
        Serial.println(json_str);

        int httpCode = http.POST((uint8_t *)json_str.c_str(), toolCallJson.length());
      }
#endif
      else if(waiting_tool_response){
        Serial.printf("tool response received\n");
        waiting_tool_response = false;
        tool_call_complete = true;
        tool_response = data;
      }

    }
  }

}

