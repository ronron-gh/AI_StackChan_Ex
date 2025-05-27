#ifndef _MCP_CLIENT_H_
#define _MCP_CLIENT_H_

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"

#define TOOLS_LIST_MAX   (20)

class MCPClient {
private:
    String mcpAddr;
    uint16_t mcpPort;
    bool _isConnected;

    bool init_finished;
    bool init_notification_finished;
    bool tools_list_requesting;
    bool tool_calling;
    bool waiting_tool_response;
    bool request_complete;
    String tool_response;

    String toolNameList[TOOLS_LIST_MAX];

    void pole_stream(String& requestJson);

public:

    SpiRamJsonDocument toolsListDoc;

    MCPClient(String _mcpAddr, uint16_t _mcpPort);
    ~MCPClient();

    bool isConnected(){ return _isConnected; };
    bool search_tool(String name);

    String mcp_list_tools();
    String mcp_call_tool(DynamicJsonDocument& tool_params);

};


#endif