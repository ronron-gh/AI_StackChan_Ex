#include "MCPClient.h"

HTTPClient http;
WiFiClient stream;
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

String toolCallJson = 
"{"
"\"method\":\"tools/call\","
"\"params\":{},"
"\"jsonrpc\":\"2.0\","
"\"id\":1"
"}";

MCPClient::MCPClient(String _mcpAddr, uint16_t _mcpPort)
  : mcpAddr(_mcpAddr), 
    mcpPort(_mcpPort),
    _isConnected(true),
    nTools(0),
    toolsListDoc(SpiRamJsonDocument(1024*10))
{
  Serial.printf("Connecting MCP Server Url:%s, Port:%d\n", _mcpAddr.c_str(), _mcpPort);

  String result = mcp_list_tools();
  //Serial.print(result);

  if(result.equals("")){
    Serial.println("MCPClient: connect error");
    _isConnected = false;
    return;
  }

  result.replace("inputSchema", "parameters");    //OpenAI function calling 仕様に変更
  DeserializationError error = deserializeJson(toolsListDoc, result.c_str());
  if (error) {
    Serial.printf("MCPClient: JSON deserialization error %d\n", error);
  }

  //String json_str;
  //serializeJsonPretty(toolsListDoc["result"]["tools"], json_str);  // 文字列をシリアルポートに出力する
  //Serial.println(json_str);

  nTools = toolsListDoc["result"]["tools"].size();

  if(nTools > TOOLS_LIST_MAX){
    Serial.printf("Warning: number of tools(%d) exceeds list array size(%d)\n.", nTools, TOOLS_LIST_MAX);
    nTools = TOOLS_LIST_MAX;
  }

  Serial.println("Tools list:");
  for(int i=0; i<nTools; i++){
    toolNameList[i] = toolsListDoc["result"]["tools"][i]["name"].as<String>();
    Serial.println(toolNameList[i]);
    //OpenAI function callingの仕様に合わせて"outputSchema"と"_meta"を削除
    toolsListDoc["result"]["tools"][i].remove("outputSchema");
    toolsListDoc["result"]["tools"][i].remove("_meta");
  }

}

bool MCPClient::search_tool(String name)
{
  for(int i=0; i<TOOLS_LIST_MAX; i++){
    if(toolNameList[i].equals(name)){
      return true;
    }
  }
  return false;
}

String MCPClient::mcp_list_tools()
{

  init_finished = false;
  init_notification_finished = false;
  tools_list_requesting = false;
  tool_calling = false;
  waiting_tool_response = false;
  request_complete = false;
  tool_response = String("");

  if (stream.connect(mcpAddr.c_str(), mcpPort)) {
    stream.print(String("GET ") + "/sse" + " HTTP/1.1\r\n" +
                  "Host: " + mcpAddr + "\r\n" +
                  "Accept: text/event-stream\r\n" +
                  "Connection: keep-alive\r\n\r\n");

    while(!request_complete){
      pole_stream(toolsListJson);
      delay(100);
    }

    stream.stop();
    http.end();
  }

  return tool_response;
}

String MCPClient::mcp_call_tool(DynamicJsonDocument& tool_params)
{

  init_finished = false;
  init_notification_finished = false;
  tools_list_requesting = false;
  tool_calling = false;
  waiting_tool_response = false;
  request_complete = false;
  tool_response = String("");

  /*
   *  Call toolリクエストとして送信するJSONを作成
   */
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, toolCallJson.c_str());
  if (error) {
    Serial.println("mcp_call_tool: JSON deserialization error");
  }

  doc["params"] = tool_params;

  String json_str;
  serializeJsonPretty(doc, json_str);
  //Serial.println(json_str);


  if (stream.connect(mcpAddr.c_str(), mcpPort)) {
    stream.print(String("GET ") + "/sse" + " HTTP/1.1\r\n" +
                  "Host: " + mcpAddr + "\r\n" +
                  "Accept: text/event-stream\r\n" +
                  "Connection: keep-alive\r\n\r\n");

    while(!request_complete){
      pole_stream(json_str);
      delay(100);
    }

    stream.stop();
    http.end();
  }

  return tool_response;
}

void MCPClient::pole_stream(String& requestJson)
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

        Serial.printf("send request\n");
        waiting_tool_response = true;
        httpCode = http.POST((uint8_t *)requestJson.c_str(), requestJson.length());

      }
      else if(waiting_tool_response){
        if(data.indexOf("result") != -1){
          Serial.printf("tool response received\n");
          waiting_tool_response = false;
          request_complete = true;
          tool_response = data;
        }
      }
    }
  }

}

