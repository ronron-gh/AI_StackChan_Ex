#include "Grok.h"
#include "Robot.h"
#include <Arduino.h>

Grok::Grok(llm_param_t param)
  : LLMBase(param, 8192)
{
  apiKey = param.api_key;
  model  = "grok-3-mini";
  client.setInsecure();
}

Grok::~Grok() {}

String Grok::buildPayload(const String& userMessage) {
  String payload = "{";
  payload += "\"model\":\"" + model + "\",";
  payload += "\"messages\":[";
  payload += "{\"role\":\"system\",\"content\":\"" + get_userRole() + "\"},";
  payload += "{\"role\":\"user\",\"content\":\"" + userMessage + "\"}";
  payload += "],";
  payload += "\"temperature\": 0.7";
  payload += "}";
  return payload;
}

bool Grok::sendRequest(const String& payload) {
  if (!client.connect("api.x.ai", 443)) {
    Serial.println("[Grok] Connection failed");
    return false;
  }

  String request = "POST /v1/chat/completions HTTP/1.1\r\n";
  request += "Host: api.x.ai\r\n";
  request += "Authorization: Bearer " + apiKey + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n\r\n";
  request += payload;

  client.print(request);

  String response = "";
  unsigned long timeout = millis();
  while (client.connected() && (millis() - timeout < 15000)) {
    if (client.available()) {
      response += client.readString();
      timeout = millis();
    }
  }
  client.stop();

  int start = response.indexOf("\"content\":\"");
  if (start != -1) {
    start += 11;
    int end = response.indexOf("\"", start);
    if (end != -1) {
      String reply = response.substring(start, end);
      reply.replace("\\n", "\n");
      reply.replace("\\\"", "\"");
      robot->speech(reply);
      return true;
    }
  }
  return false;
}

void Grok::chat(String text, const char* base64_buf) {
  String payload = buildPayload(text);
  sendRequest(payload);
}

String Grok::listen() {
  return "";
}