#include "Grok.h"
#include "Robot.h"
#include <Arduino.h>

Grok::Grok(llm_param_t param)
  : LLMBase(param, 8192)   // 8192 is a reasonable default prompt size
{
  apiKey = param.api_key;
  model  = "grok-3-mini";  // Change to "grok-3" for the larger model if desired
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
    Serial.println("[Grok] Connection to api.x.ai failed");
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

  // Read response
  String response = "";
  unsigned long timeout = millis();
  while (client.connected() && (millis() - timeout < 15000)) {
    if (client.available()) {
      response += client.readString();
      timeout = millis();
    }
  }
  client.stop();

  Serial.println("=== Grok Response ===");
  Serial.println(response);

  // Basic content extraction (we'll improve this later)
  int contentStart = response.indexOf("\"content\":\"");
  if (contentStart != -1) {
    contentStart += 11;
    int contentEnd = response.indexOf("\"", contentStart);
    if (contentEnd != -1) {
      String reply = response.substring(contentStart, contentEnd);
      reply.replace("\\n", "\n");
      reply.replace("\\\"", "\"");
      robot->speech(reply);
      return true;
    }
  }

  Serial.println("[Grok] Failed to parse response content");
  return false;
}

bool Grok::chat(const char* text, const char* image) {
  if (image != NULL) {
    Serial.println("[Grok] Image input not yet supported in this driver");
  }

  String payload = buildPayload(String(text));
  return sendRequest(payload);
}

String Grok::listen() {
  // Placeholder for future Grok STT integration
  return "";
}