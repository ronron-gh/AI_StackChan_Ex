#ifndef _WEB_API_H
#define _WEB_API_H

#include <Arduino.h>
#include <M5Unified.h>

extern void init_web_server(void);
extern void web_server_handle_client(void);

#endif  //_WEB_API_H