#pragma once

#include <ESPAsyncWebServer.h>

class WebServer {
public:
  static void init();
  static AsyncWebServer* getServer();
  
private:
  static AsyncWebServer* server;
};


