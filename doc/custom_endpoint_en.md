# How to Use an OpenAI-Compatible Endpoint

Instead of ChatGPT, you can use a server with an OpenAI-compatible API (Chat Completions) as the LLM, such as llama.cpp, Ollama, or LM Studio.

- [Overview](#overview)
- [YAML settings](#yaml-settings)
- [Using an HTTPS endpoint (root CA certificate)](#using-an-https-endpoint-root-ca-certificate)
- [Behavior and limitations](#behavior-and-limitations)

## Overview
Set the llm type to 4 in SC_ExConfig.yaml and configure model (required) and customEndpoint. This keeps the same mechanism as ChatGPT (type 0) and only switches the destination to your OpenAI-compatible server. If you run the LLM server on a PC at home, you can use the AI conversation features without sending your conversations to the cloud.

## YAML settings
SD card folder：/app/AiStackChanEx  
File name：SC_ExConfig.yaml

```yaml
llm:
  type: 4                                                          # 4:OpenAI Compatible Endpoint
  model: "Gemma-4-31B-it"                                          # Model name used by the server (required)
  customEndpoint: "http://192.168.X.XXX:8080/v1/chat/completions"  # Chat Completions URL
```

- model: Required. If left blank, requests are refused. Note that model can also be specified for type 0 (ChatGPT), in which case it overrides the default gpt-4o.
- customEndpoint: Specify the Chat Completions URL including the path (/v1/chat/completions). An https:// URL requires the root CA certificate settings described in the next section. If left blank, api.openai.com is used as usual.
- The value of aiservice in SC_SecConfig.yaml is sent as the Authorization: Bearer header. If your server does not require authentication, any string is fine.

## Using an HTTPS endpoint (root CA certificate)
When using an https:// customEndpoint, a root CA certificate is required to verify the server certificate. Save the root CA certificate in PEM format as a text file on the SD card and set its path in customRootCAFile. If it is not set, requests are refused (there is no fallback to api.openai.com).

```yaml
llm:
  type: 4
  model: "Gemma-4-31B-it"
  customEndpoint: "https://my-llm.example.com/v1/chat/completions"
  customRootCAFile: "/customRootCA.pem"
```

Example of the certificate file contents (PEM format with line breaks preserved):
```
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqh...
-----END CERTIFICATE-----
```

To trust multiple root CAs, specify a list of paths in customRootCAFiles (it can be combined with customRootCAFile). The certificates from each file are concatenated, and all of them are trusted as root CAs.

```yaml
  customRootCAFiles:
    - "/customRootCA.pem"
    - "/customRootCA2.pem"
```

> AtomS3R does not support SD cards, so write the certificate files to SPIFFS in the same way as the YAML files. See [here](./atoms3r.md) for how to write them (Japanese).

> The customEndpoint and the number of loaded certificates are printed in the serial log at startup, which can be used to check the settings.

## Behavior and limitations
- Streaming responses (SSE) are not supported. For type 4, stream: false is explicitly set in the request.
- If the settings are incomplete, no request is sent and Stack-chan shows an error in a speech bubble (an error is also printed in the serial log at startup).
  - model is blank → "モデル未設定" (model not set)
  - customEndpoint is https:// but no root CA is set → "CA未設定" (CA not set)
