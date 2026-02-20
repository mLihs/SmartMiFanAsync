#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

class ApiHandlers {
public:
  // Settings API (atomic Key/Value)
  static void handleGetSettingsGet(AsyncWebServerRequest *request); // GET /api/settings/get?key=<key>
  static void handlePostSettingsSet(AsyncWebServerRequest *request); // POST /api/settings/set (form: key=...&value=...)
  static void handleGetSettingsList(AsyncWebServerRequest *request); // GET /api/settings/list
  
  // Legacy Settings API (for backward compatibility, will be removed in STEP 6)
  static void handleGetSettings(AsyncWebServerRequest *request);
  static void handlePutSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
  
  // Action API (NO JSON - query params or form-urlencoded only)
  static void handlePostActionScanStart(AsyncWebServerRequest *request); // POST /api/action/scan/start
  static void handlePostActionPower(AsyncWebServerRequest *request); // POST /api/action/power?power=true|false
  static void handlePostActionSpeed(AsyncWebServerRequest *request); // POST /api/action/speed?speed=<1-100>
  static void handlePostActionFanEnabled(AsyncWebServerRequest *request); // POST /api/action/fan-enabled?fanIndex=<n>&enabled=true|false
  
  // State API (JSON output OK - server → client snapshot)
  static void handleGetState(AsyncWebServerRequest *request);
  
private:
  // Helper methods
  static void sendTextResponse(AsyncWebServerRequest *request, int code, const char* text);
  static void sendJsonResponse(AsyncWebServerRequest *request, int code, const JsonDocument& doc);
  static void sendErrorResponse(AsyncWebServerRequest *request, int code, const char* error);
  
  // Settings helpers
  static bool parseBoolParam(AsyncWebServerRequest *request, const char* param, bool* out);
  static bool parseIntParam(AsyncWebServerRequest *request, const char* param, int* out, int min, int max);
};

