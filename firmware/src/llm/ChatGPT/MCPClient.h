#ifndef _MCP_CLIENT_H_
#define _MCP_CLIENT_H_

#include <HTTPClient.h>
#include <ArduinoJson.h>

extern String tool_response;

String mcp_request_tool_call(const char* mcpAddr, uint16_t mcpPort, DynamicJsonDocument& tool_params);

#endif