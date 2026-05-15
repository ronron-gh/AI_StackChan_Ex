#pragma once

#include "../LLMBase.h"
#include <WiFiClientSecure.h>

class Grok : public LLMBase {
public:
  explicit Grok(llm_param_t param);
  ~Grok();

  void chat(String text, const char* base64_buf = NULL) override;
  String listen() override;

private:
  WiFiClientSecure client;
  String apiKey;
  String model;

  bool sendRequest(const String& payload);
  String buildPayload(const String& userMessage);
};