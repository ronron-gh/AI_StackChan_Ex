#include <Arduino.h>
#include "share/Version.h"
#include "mod/ModManager.h"
#include "StatusMonitorMod.h"
#include "Robot.h"
#include <WiFi.h>
#include <Avatar.h>

using namespace m5avatar;

/// 外部参照 ///
extern Avatar avatar;
extern void sw_tone();

///////////////

namespace {
const uint16_t STATUS_BG = 0x0841;
const uint16_t STATUS_PANEL = 0xffff;
const uint16_t STATUS_LINE = 0xbdf7;
const uint16_t STATUS_TEXT = 0x0000;
const uint16_t STATUS_MUTED = 0x7bef;
const uint16_t STATUS_PRIMARY = 0x0471;
const int32_t STATUS_SUBWINDOW_WIDTH = 320;
const int32_t TAB_MARGIN_X = 6;
const int32_t TAB_Y = 6;
const int32_t TAB_H = 28;
const int32_t TAB_GAP = 4;
const int32_t TAB_W = (STATUS_SUBWINDOW_WIDTH - (TAB_MARGIN_X * 2) - TAB_GAP) / 2;

void drawTextLines(M5Canvas *spi, const String& text, int32_t x, int32_t y, int32_t lineHeight)
{
  int start = 0;
  int cursorY = y;
  while(start <= text.length()){
    int end = text.indexOf('\n', start);
    if(end < 0){
      end = text.length();
    }
    String line = text.substring(start, end);
    spi->drawString(line, x, cursorY);
    cursorY += lineHeight;
    if(end >= text.length()){
      break;
    }
    start = end + 1;
  }
}
}

StatusMonitorMod::StatusMonitorMod(void)
{
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);
  box_TabSystem.setupBox(TAB_MARGIN_X, TAB_Y, TAB_W, TAB_H);
  box_TabAiService.setupBox(TAB_MARGIN_X + TAB_W + TAB_GAP, TAB_Y, TAB_W, TAB_H);
  current_page_no = 0;
}

void StatusMonitorMod::init(void)
{
  update(current_page_no);
  avatar.set_isSubWindowEnable(true);
}


void StatusMonitorMod::pause(void)
{
  avatar.set_isSubWindowEnable(false);
}

void StatusMonitorMod::drawSubWindow(M5Canvas *spi, BoundingRect rect,
                                     DrawContext *ctx, void *userData)
{
  if(userData == nullptr){
    return;
  }
  static_cast<StatusMonitorMod*>(userData)->drawStatusMonitor(spi, rect, ctx);
}

String StatusMonitorMod::buildSystemStatus()
{
  String str = "";
  char tmp[256];

  byte mac[6];
  esp_efuse_mac_get_default(mac);

  //str += "======== System status ========\n";
  sprintf(tmp, "Firmware Version:%s\n", FW_VERSION);
  str += tmp;
  sprintf(tmp, "Wifi:\n  IP addr:%s\n  MAC addr:%02x:%02x:%02x:%02x:%02x:%02x\n",
                  WiFi.localIP().toString().c_str(),
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
  str += tmp;
  sprintf(tmp, "Heap largest free block:\n  DMA:%d\n  SPIRAM:%d\n",
                  heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) );
  str += tmp;
#if 0
  sprintf(tmp, "Prompt buffer usage:\n  %d / %d [byte]\n",
                  robot->llm->chat_doc.memoryUsage(),
                  robot->llm->chat_doc.capacity() );
  str += tmp;
#endif
  sprintf(tmp, "Battery level:  %d %%\n", M5.Power.getBatteryLevel());
  str += tmp;
  return str;
}

const char* StatusMonitorMod::getAiServiceName(int llm_type)
{
  switch(llm_type){
    case LLM_TYPE_CHATGPT:
      return "OpenAI Realtime";
    case LLM_TYPE_GEMINI:
      return "Gemini Live";
    case LLM_TYPE_CUSTOM_OPENAI:
      return "Custom OpenAI";
    case LLM_TYPE_MODULE_LLM:
      return "Module LLM";
    case LLM_TYPE_MODULE_LLM_FNCL:
      return "Module LLM Function";
    default:
      return "Unknown";
  }
}

String StatusMonitorMod::buildAiServiceStatus()
{
  String str = "";
  //str += "========= AI Service ==========\n";

  if(robot == nullptr){
    str += "Robot is not initialized.\n";
    return str;
  }

  ex_config_s ex_config = robot->m_config.getExConfig();
  str += "Service:\n  ";
  str += getAiServiceName(ex_config.llm.type);
  str += "\n";
  str += "Memory: ";
  str += ex_config.llm.enableMemory ? "Enabled" : "Disabled";
  str += "\n";
  str += "MCP Servers:\n";

  if(ex_config.llm.nMcpServers <= 0){
    str += "  (none)\n";
  }else{
    for(int i=0; i<ex_config.llm.nMcpServers && i<LLM_N_MCP_SERVERS_MAX; i++){
      const mcp_server_s& server = ex_config.llm.mcpServer[i];
      str += "  - ";
      str += server.name.length() > 0 ? server.name : "(unnamed)";
      if(server.disabled){
        str += " [disabled]";
      }
      str += "\n";
      str += "    ";
      str += server.url;
      str += ":";
      str += String(server.port);
      str += "\n";
    }
  }
  return str;
}

void StatusMonitorMod::drawStatusMonitor(M5Canvas *spi, BoundingRect rect,
                                         DrawContext *ctx)
{
  int32_t x = rect.getLeft();
  int32_t y = rect.getTop();
  int32_t w = rect.getWidth();
  int32_t h = rect.getHeight();
  if(w <= 0){
    w = 320;
  }
  if(h <= 0){
    h = 240;
  }

  spi->fillRect(x, y, w, h, STATUS_BG);
  spi->fillRoundRect(x + 2, y + 2, w - 4, h - 4, 6, STATUS_PANEL);
  spi->drawRoundRect(x + 2, y + 2, w - 4, h - 4, 6, STATUS_LINE);
  spi->setFont(&fonts::Font0);  
  spi->setTextSize(1.5);

  const int32_t tabY = y + TAB_Y;
  const int32_t tabH = TAB_H;
  const int32_t tabGap = TAB_GAP;
  const int32_t tabW = (w - (TAB_MARGIN_X * 2) - tabGap) / 2;
  const char* tabNames[TAB_COUNT] = {"System", "AI Service"};
  for(int i = 0; i < TAB_COUNT; i++){
    int32_t tabX = x + TAB_MARGIN_X + (tabW + tabGap) * i;
    bool active = (i == current_page_no);
    uint16_t fill = active ? STATUS_PRIMARY : 0xe71c;
    uint16_t text = active ? TFT_WHITE : STATUS_TEXT;
    spi->fillRoundRect(tabX, tabY, tabW, tabH, 5, fill);
    spi->drawRoundRect(tabX, tabY, tabW, tabH, 5, active ? STATUS_PRIMARY : STATUS_LINE);
    spi->setTextColor(text, fill);
    spi->setTextDatum(middle_center);
    spi->drawString(tabNames[i], tabX + tabW / 2, tabY + tabH / 2);
  }

  int32_t bodyX = x + 10;
  int32_t bodyY = y + 42;
  int32_t bodyW = w - 20;
  int32_t bodyH = h - 50;
  spi->fillRect(bodyX - 2, bodyY - 2, bodyW + 4, bodyH + 4, TFT_WHITE);
  spi->setTextDatum(top_left);
  spi->setTextColor(STATUS_TEXT, TFT_WHITE);
  drawTextLines(spi, current_page_no == 0 ? buildSystemStatus() : buildAiServiceStatus(),
                bodyX, bodyY, 20);
}

void StatusMonitorMod::update(int page_no)
{
  avatar.updateSubWindowCustom(StatusMonitorMod::drawSubWindow, this, 0, 0, 320, 240);
}


void StatusMonitorMod::btnA_pressed(void)
{
  current_page_no--;
  if(current_page_no < 0) current_page_no = TAB_COUNT - 1;
  update(current_page_no);
}

void StatusMonitorMod::btnC_pressed(void)
{
  current_page_no++;
  if(current_page_no >= TAB_COUNT) current_page_no = 0;
  update(current_page_no);
}

void StatusMonitorMod::display_touched(int16_t x, int16_t y)
{

  if (box_BtnA.contain(x, y))
  {

  }

  if (box_BtnC.contain(x, y))
  {
    //sw_tone();
  }

  if (box_TabSystem.contain(x, y))
  {
    current_page_no = 0;
    update(current_page_no);
    return;
  }

  if (box_TabAiService.contain(x, y))
  {
    current_page_no = 1;
    update(current_page_no);
    return;
  }
}


void StatusMonitorMod::idle(void)
{
  update(current_page_no);
}
