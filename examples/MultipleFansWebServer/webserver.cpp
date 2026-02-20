#include "webserver.h"
#include "api_handlers.h"
#include "websocket_handler.h"
#include "web_ui.h"

AsyncWebServer* WebServer::server = nullptr;

void WebServer::init() {
  server = new AsyncWebServer(80);
  
  // Setup WebSocket
  WebSocketHandler::init(server);
  
  // STEP 2: Atomic Key/Value Settings API
  server->on("/api/settings/get", HTTP_GET, ApiHandlers::handleGetSettingsGet);
  server->on("/api/settings/set", HTTP_POST, ApiHandlers::handlePostSettingsSet);
  server->on("/api/settings/list", HTTP_GET, ApiHandlers::handleGetSettingsList);
  
  // STEP 6: Legacy Settings API - DEPRECATED (kept for backward compatibility)
  // Use /api/settings/get, /api/settings/set, /api/settings/list instead
  server->on("/api/settings", HTTP_GET, ApiHandlers::handleGetSettings);
  server->on("/api/settings", HTTP_PUT, 
    [](AsyncWebServerRequest *request){}, 
    NULL, 
    ApiHandlers::handlePutSettings);
  
  // STEP 1: Action API - NO JSON, query params only
  // POST /api/action/scan/start - Start scan (STEP 1.2)
  server->on("/api/action/scan/start", HTTP_POST, ApiHandlers::handlePostActionScanStart);
  
  // POST /api/action/power - Set power for all fans
  server->on("/api/action/power", HTTP_POST, ApiHandlers::handlePostActionPower);
  
  // POST /api/action/speed - Set speed for all fans
  server->on("/api/action/speed", HTTP_POST, ApiHandlers::handlePostActionSpeed);
  
  // POST /api/action/fan-enabled - Set fan enabled state
  server->on("/api/action/fan-enabled", HTTP_POST, ApiHandlers::handlePostActionFanEnabled);
  
  // GET /api/state - Get snapshot of all state machines
  server->on("/api/state", HTTP_GET, ApiHandlers::handleGetState);
  
  // Serve web page
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", WEB_UI_HTML);
  });
  
  server->begin();
}

AsyncWebServer* WebServer::getServer() {
  return server;
}

