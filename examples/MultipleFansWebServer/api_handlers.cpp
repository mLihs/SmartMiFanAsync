#include "api_handlers.h"
#include "state_machine.h"
#include "websocket_handler.h"
#include "config.h"
#include <SmartMiFanAsync.h>
#include <DebugLog.h>
#include <WiFiUdp.h>
#include <Preferences.h>

extern WiFiUDP fanUdp;

// Settings storage (in a real application, use EEPROM or NVS)
struct Settings {
  uint8_t globalSpeed;
  bool fanEnabled[16]; // Max 16 fans
  bool initialized;
};

static Settings settings = {30, {true}, false};

// Preferences for persistent storage
static Preferences preferences;
static bool preferencesInitialized = false;

void initPreferences() {
  if (!preferencesInitialized) {
    preferences.begin("fanctrl", false);
    preferencesInitialized = true;
    
    // Load saved settings
    settings.globalSpeed = preferences.getUChar("globalSpeed", 30);
    for (size_t i = 0; i < 16; i++) {
      char key[16];
      snprintf(key, sizeof(key), "fanEnabled%zu", i);
      settings.fanEnabled[i] = preferences.getBool(key, true);
    }
    settings.initialized = true;
  }
}

void ApiHandlers::sendTextResponse(AsyncWebServerRequest *request, int code, const char* text) {
  request->send(code, "text/plain", text);
}

void ApiHandlers::sendJsonResponse(AsyncWebServerRequest *request, int code, const JsonDocument& doc) {
  // Calculate required size
  size_t jsonSize = measureJson(doc);
  if (jsonSize == 0) {
    request->send(code, "application/json", "{}");
    return;
  }
  
  // Allocate buffer on heap - ESPAsyncWebServer will copy it
  // We need to keep it alive until send() completes, so use a static buffer pool
  // For now, use a reasonable max size and allocate
  if (jsonSize > 4096) jsonSize = 4096; // Limit to prevent OOM
  
  // Use String but ensure it's copied properly by ESPAsyncWebServer
  // ESPAsyncWebServer's send() with String parameter makes a copy internally
  String response;
  response.reserve(jsonSize + 1);
  serializeJson(doc, response);
  
  // Send - ESPAsyncWebServer will make its own copy of the String's buffer
  request->send(code, "application/json", response);
  // String destructor will free the buffer after send() has copied it
}

void ApiHandlers::sendErrorResponse(AsyncWebServerRequest *request, int code, const char* error) {
  DynamicJsonDocument doc(256);
  doc["error"] = error;
  sendJsonResponse(request, code, doc);
}

bool ApiHandlers::parseBoolParam(AsyncWebServerRequest *request, const char* param, bool* out) {
  if (request->hasParam(param, true)) {
    String val = request->getParam(param, true)->value();
    *out = (val == "true" || val == "1" || val == "on");
    return true;
  } else if (request->hasParam(param)) {
    String val = request->getParam(param)->value();
    *out = (val == "true" || val == "1" || val == "on");
    return true;
  }
  return false;
}

bool ApiHandlers::parseIntParam(AsyncWebServerRequest *request, const char* param, int* out, int min, int max) {
  if (request->hasParam(param, true)) {
    String val = request->getParam(param, true)->value();
    int value = val.toInt();
    if (value >= min && value <= max) {
      *out = value;
      return true;
    }
  } else if (request->hasParam(param)) {
    String val = request->getParam(param)->value();
    int value = val.toInt();
    if (value >= min && value <= max) {
      *out = value;
      return true;
    }
  }
  return false;
}

// STEP 2: Atomic Key/Value Settings API
void ApiHandlers::handleGetSettingsGet(AsyncWebServerRequest *request) {
  LOGD_F("[API] Get settings command received");
  if (!request->hasParam("key")) {
    LOGD_F("[API] Get settings missing 'key' parameter");
    sendTextResponse(request, 400, "ERR:Missing 'key' parameter");
    return;
  }
  
  String key = request->getParam("key")->value();
  LOGD_F("[API] Get settings key: %s", key.c_str());
  
  if (key == "globalSpeed") {
    char response[16];
    snprintf(response, sizeof(response), "%u", settings.globalSpeed);
    sendTextResponse(request, 200, response);
  } else if (key.startsWith("fan.")) {
    // Parse fan.0.enabled format
    int dot1 = key.indexOf('.');
    int dot2 = key.indexOf('.', dot1 + 1);
    if (dot1 >= 0 && dot2 > dot1) {
      int fanIndex = key.substring(dot1 + 1, dot2).toInt();
      String setting = key.substring(dot2 + 1);
      
      if (setting == "enabled" && fanIndex >= 0 && fanIndex < 16) {
        sendTextResponse(request, 200, settings.fanEnabled[fanIndex] ? "true" : "false");
        return;
      }
    }
    sendTextResponse(request, 404, "ERR:Setting not found");
  } else {
    sendTextResponse(request, 404, "ERR:Setting not found");
  }
}

void ApiHandlers::handlePostSettingsSet(AsyncWebServerRequest *request) {
  LOGD_F("[API] Set settings command received");
  // Parse form-urlencoded body: key=...&value=...
  if (!request->hasParam("key", true) || !request->hasParam("value", true)) {
    LOGD_F("[API] Set settings missing 'key' or 'value' parameter");
    sendTextResponse(request, 400, "ERR:Missing 'key' or 'value' parameter");
    return;
  }
  
  String key = request->getParam("key", true)->value();
  String value = request->getParam("value", true)->value();
  LOGD_F("[API] Set settings: key=%s, value=%s", key.c_str(), value.c_str());
  
  initPreferences();
  
  if (key == "globalSpeed") {
    int speed = value.toInt();
    // STEP 5D: Robust validation
    if (speed < 1 || speed > 100) {
      LOGD_F("[API] Set settings rejected - invalid speed: %d (must be 1-100)", speed);
      sendTextResponse(request, 400, "ERR:Speed must be between 1 and 100");
      return;
    }
    LOGD_F("[API] Setting globalSpeed to %d", speed);
    settings.globalSpeed = (uint8_t)speed;
    preferences.putUChar("globalSpeed", settings.globalSpeed);
    LOGD_F("[API] Settings saved successfully: globalSpeed = %d", settings.globalSpeed);
    sendTextResponse(request, 200, "OK");
  } else if (key.startsWith("fan.")) {
    // Parse fan.0.enabled format
    int dot1 = key.indexOf('.');
    int dot2 = key.indexOf('.', dot1 + 1);
    if (dot1 >= 0 && dot2 > dot1) {
      int fanIndex = key.substring(dot1 + 1, dot2).toInt();
      String setting = key.substring(dot2 + 1);
      
      if (setting == "enabled" && fanIndex >= 0 && fanIndex < 16) {
        bool enabled = (value == "true" || value == "1" || value == "on");
        LOGD_F("[API] Setting fan[%d].enabled to %s", fanIndex, enabled ? "true" : "false");
        settings.fanEnabled[fanIndex] = enabled;
        SmartMiFanAsync_setFanEnabled((uint8_t)fanIndex, enabled);
        
        char prefKey[16];
        snprintf(prefKey, sizeof(prefKey), "fanEnabled%u", fanIndex);
        preferences.putBool(prefKey, enabled);
        
        LOGD_F("[API] Settings saved successfully: fan[%d].enabled = %s", fanIndex, enabled ? "true" : "false");
        WebSocketHandler::sendStateChanged("READY");
        sendTextResponse(request, 200, "OK");
        return;
      }
    }
    LOGD_F("[API] Set settings rejected - invalid key format: %s", key.c_str());
    sendTextResponse(request, 400, "ERR:Invalid key format");
  } else {
    LOGD_F("[API] Set settings rejected - unknown key: %s", key.c_str());
    sendTextResponse(request, 400, "ERR:Unknown setting key");
  }
}

void ApiHandlers::handleGetSettingsList(AsyncWebServerRequest *request) {
  LOGD_F("[API] Get settings list command received");
  // Return text/plain list of all settings
  String response = "globalSpeed=";
  response += String(settings.globalSpeed);
  response += "\n";
  
  size_t fanCount = 0;
  SmartMiFanAsync_getDiscoveredFans(fanCount);
  LOGD_F("[API] Listing settings for %zu fans", fanCount);
  for (size_t i = 0; i < fanCount && i < 16; i++) {
    response += "fan.";
    response += String(i);
    response += ".enabled=";
    response += settings.fanEnabled[i] ? "true" : "false";
    response += "\n";
  }
  
  LOGD_F("[API] Settings list returned successfully");
  sendTextResponse(request, 200, response.c_str());
}

// STEP 6: Legacy endpoint - DEPRECATED, use /api/settings/get and /api/settings/list instead
void ApiHandlers::handleGetSettings(AsyncWebServerRequest *request) {
  initPreferences(); // Ensure preferences are loaded
  
  DynamicJsonDocument doc(1024);
  doc["globalSpeed"] = settings.globalSpeed;
  
  JsonArray fanEnabled = doc.createNestedArray("fanEnabled");
  size_t fanCount = 0;
  SmartMiFanAsync_getDiscoveredFans(fanCount);
  // STEP 5D: Bounds checking
  for (size_t i = 0; i < fanCount && i < 16; i++) {
    fanEnabled.add(SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i)));
  }
  
  sendJsonResponse(request, 200, doc);
}

// STEP 6: Legacy endpoint - DEPRECATED, use /api/settings/set instead
void ApiHandlers::handlePutSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  initPreferences(); // Ensure preferences are loaded
  
  if (index == 0) {
    request->_tempObject = new DynamicJsonDocument(1024);
  }
  
  DynamicJsonDocument *doc = (DynamicJsonDocument *)request->_tempObject;
  
  if (index + len < total) {
    return;
  }
  
  DeserializationError error = deserializeJson(*doc, (char *)data);
  if (error) {
    sendErrorResponse(request, 400, "Invalid JSON");
    delete doc;
    return;
  }
  
  if (doc->containsKey("globalSpeed")) {
    uint8_t speed = (*doc)["globalSpeed"].as<uint8_t>();
    // STEP 5D: Robust validation
    if (speed < 1 || speed > 100) {
      sendErrorResponse(request, 400, "Speed must be between 1 and 100");
      delete doc;
      return;
    }
    settings.globalSpeed = speed;
    preferences.putUChar("globalSpeed", settings.globalSpeed);
  }
  
  if (doc->containsKey("fanEnabled")) {
    JsonArray fanEnabled = (*doc)["fanEnabled"];
    size_t fanCount = 0;
    SmartMiFanAsync_getDiscoveredFans(fanCount);
    // STEP 5D: Bounds checking - ensure we don't exceed array bounds
    size_t maxIter = fanEnabled.size();
    if (maxIter > fanCount) maxIter = fanCount;
    if (maxIter > 16) maxIter = 16;
    
    for (size_t i = 0; i < maxIter; i++) {
      SmartMiFanAsync_setFanEnabled(static_cast<uint8_t>(i), fanEnabled[i].as<bool>());
      settings.fanEnabled[i] = fanEnabled[i].as<bool>();
      char prefKey[16];
      snprintf(prefKey, sizeof(prefKey), "fanEnabled%zu", i);
      preferences.putBool(prefKey, settings.fanEnabled[i]);
    }
  }
  
  settings.initialized = true;
  
  // Send state change event
  WebSocketHandler::sendStateChanged("READY"); // STEP 4: Map to READY instead of SETTINGS_UPDATED
  
  DynamicJsonDocument response(128);
  response["success"] = true;
  sendJsonResponse(request, 200, response);
  delete doc;
}

// STEP 1.2: Scan action - NO JSON, returns text/plain
void ApiHandlers::handlePostActionScanStart(AsyncWebServerRequest *request) {
  LOGD_F("[API] Scan start command received");
  
  // Check if already scanning
  if (StateMachine::getState() == StateMachine::State::SCANNING) {
    LOGD_F("[API] Scan already in progress, returning BUSY");
    sendTextResponse(request, 409, "BUSY");
    return;
  }
  
  // Check if in valid state
  if (StateMachine::getState() != StateMachine::State::READY && 
      StateMachine::getState() != StateMachine::State::IDLE) {
    LOGD_F("[API] Cannot start scan in state: %s", StateMachine::getStateString());
    sendTextResponse(request, 400, "ERR:Cannot start scan in current state");
    return;
  }
  
  LOGD_F("[API] Starting scan...");
  // Start new scan
  StateMachine::startScan();
  
  // Actually start Smart Connect
  extern WiFiUDP fanUdp;
  
  // Reset discovered fans
  SmartMiFanAsync_resetDiscoveredFans();
  
  // Configure Fast Connect if not already done
  SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT);
  
  // Start Smart Connect
  if (SmartMiFanAsync_startSmartConnect(fanUdp, 5000)) {
    LOGD_F("[API] Scan started successfully");
    sendTextResponse(request, 200, "OK");
    WebSocketHandler::sendStateChanged("SCANNING");
  } else {
    LOGD_F("[API] Failed to start Smart Connect");
    StateMachine::setState(StateMachine::State::ERROR);
    sendTextResponse(request, 503, "ERR:Failed to start Smart Connect");
  }
}

// STEP 1: Power action - NO JSON, uses query param
void ApiHandlers::handlePostActionPower(AsyncWebServerRequest *request) {
  LOGD_F("[API] Power command received");
  bool power = false;
  
  // Try query param first
  if (request->hasParam("power", true)) {
    String powerStr = request->getParam("power", true)->value();
    power = (powerStr == "true" || powerStr == "1" || powerStr == "on");
    LOGD_F("[API] Power param (body): %s -> %s", powerStr.c_str(), power ? "ON" : "OFF");
  } else if (request->hasParam("power")) {
    String powerStr = request->getParam("power")->value();
    power = (powerStr == "true" || powerStr == "1" || powerStr == "on");
    LOGD_F("[API] Power param (query): %s -> %s", powerStr.c_str(), power ? "ON" : "OFF");
  } else {
    LOGD_F("[API] Power command missing 'power' parameter");
    sendTextResponse(request, 400, "ERR:Missing 'power' parameter");
    return;
  }
  
  // Check state
  if (StateMachine::getState() != StateMachine::State::READY) {
    LOGD_F("[API] Power command rejected - system not ready (state: %s)", StateMachine::getStateString());
    sendTextResponse(request, 409, "BUSY");
    return;
  }
  
  LOGD_F("[API] Executing power command: %s", power ? "ON" : "OFF");
  
  // DEBUG: Check fan states before sending command
  size_t fanCount = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
  LOGD_F("[API] Checking %zu fans before power command:", fanCount);
  for (size_t i = 0; i < fanCount; i++) {
    FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    bool enabled = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
    const char* stateStr = (partState == FanParticipationState::ACTIVE) ? "ACTIVE" : 
                           (partState == FanParticipationState::INACTIVE) ? "INACTIVE" : "ERROR";
    LOGD_F("[API]   Fan[%zu]: enabled=%s, participation=%s, ready=%s", 
           i, enabled ? "true" : "false", stateStr, fans[i].ready ? "true" : "false");
  }
  
  // Create job and execute
  const char* jobId = StateMachine::createJob("setPower", power ? "true" : "false");
  bool success = SmartMiFanAsync_setPowerAllOrchestrated(power);
  
  StateMachine::completeJob(jobId, success);
  
  // STEP 5: Mark telemetry dirty instead of sending directly from async handler
  WebSocketHandler::markTelemetryDirty();
  
  if (success) {
    LOGD_F("[API] Power command executed successfully: %s", power ? "ON" : "OFF");
    sendTextResponse(request, 200, "OK");
  } else {
    LOGD_F("[API] Power command failed: %s", power ? "ON" : "OFF");
    sendTextResponse(request, 500, "ERR:Failed to set power");
  }
}

// STEP 1: Speed action - NO JSON, uses query param
void ApiHandlers::handlePostActionSpeed(AsyncWebServerRequest *request) {
  LOGD_F("[API] Speed command received");
  int speed = 0;
  
  // Try query param first
  if (request->hasParam("speed", true)) {
    String speedStr = request->getParam("speed", true)->value();
    speed = speedStr.toInt();
    LOGD_F("[API] Speed param (body): %s -> %d", speedStr.c_str(), speed);
  } else if (request->hasParam("speed")) {
    String speedStr = request->getParam("speed")->value();
    speed = speedStr.toInt();
    LOGD_F("[API] Speed param (query): %s -> %d", speedStr.c_str(), speed);
  } else {
    LOGD_F("[API] Speed command missing 'speed' parameter");
    sendTextResponse(request, 400, "ERR:Missing 'speed' parameter");
    return;
  }
  
  // STEP 5D: Robust speed validation
  if (speed < 1 || speed > 100) {
    LOGD_F("[API] Speed command rejected - invalid value: %d (must be 1-100)", speed);
    sendTextResponse(request, 400, "ERR:Speed must be between 1 and 100");
    return;
  }
  
  // Check state
  if (StateMachine::getState() != StateMachine::State::READY) {
    LOGD_F("[API] Speed command rejected - system not ready (state: %s)", StateMachine::getStateString());
    sendTextResponse(request, 409, "BUSY");
    return;
  }
  
  LOGD_F("[API] Executing speed command: %d%%", speed);
  settings.globalSpeed = (uint8_t)speed;
  
  // Save to preferences
  initPreferences();
  preferences.putUChar("globalSpeed", settings.globalSpeed);
  
  // DEBUG: Check fan states before sending command
  size_t fanCount = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
  LOGD_F("[API] Checking %zu fans before speed command:", fanCount);
  for (size_t i = 0; i < fanCount; i++) {
    FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    bool enabled = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
    const char* stateStr = (partState == FanParticipationState::ACTIVE) ? "ACTIVE" : 
                           (partState == FanParticipationState::INACTIVE) ? "INACTIVE" : "ERROR";
    LOGD_F("[API]   Fan[%zu]: enabled=%s, participation=%s, ready=%s", 
           i, enabled ? "true" : "false", stateStr, fans[i].ready ? "true" : "false");
  }
  
  // Create job and execute
  char speedStr[4];
  snprintf(speedStr, sizeof(speedStr), "%u", speed);
  const char* jobId = StateMachine::createJob("setSpeed", speedStr);
  bool success = SmartMiFanAsync_setSpeedAllOrchestrated((uint8_t)speed);
  
  StateMachine::completeJob(jobId, success);
  
  // STEP 5: Mark telemetry dirty instead of sending directly from async handler
  WebSocketHandler::markTelemetryDirty();
  WebSocketHandler::sendStateChanged("READY"); // STEP 4: Map to READY instead of SPEED_UPDATED
  
  if (success) {
    LOGD_F("[API] Speed command executed successfully: %d%%", speed);
    sendTextResponse(request, 200, "OK");
  } else {
    LOGD_F("[API] Speed command failed: %d%%", speed);
    sendTextResponse(request, 500, "ERR:Failed to set speed");
  }
}

// STEP 1: Fan enabled action - NO JSON, uses query params
void ApiHandlers::handlePostActionFanEnabled(AsyncWebServerRequest *request) {
  LOGD_F("[API] Fan enabled command received");
  int fanIndex = -1;
  bool enabled = false;
  
  // Get fanIndex
  if (request->hasParam("fanIndex", true)) {
    String fanIndexStr = request->getParam("fanIndex", true)->value();
    fanIndex = fanIndexStr.toInt();
    LOGD_F("[API] FanIndex param (body): %s -> %d", fanIndexStr.c_str(), fanIndex);
  } else if (request->hasParam("fanIndex")) {
    String fanIndexStr = request->getParam("fanIndex")->value();
    fanIndex = fanIndexStr.toInt();
    LOGD_F("[API] FanIndex param (query): %s -> %d", fanIndexStr.c_str(), fanIndex);
  } else {
    LOGD_F("[API] Fan enabled command missing 'fanIndex' parameter");
    sendTextResponse(request, 400, "ERR:Missing 'fanIndex' parameter");
    return;
  }
  
  // Get enabled
  if (request->hasParam("enabled", true)) {
    String enabledStr = request->getParam("enabled", true)->value();
    enabled = (enabledStr == "true" || enabledStr == "1" || enabledStr == "on");
    LOGD_F("[API] Enabled param (body): %s -> %s", enabledStr.c_str(), enabled ? "true" : "false");
  } else if (request->hasParam("enabled")) {
    String enabledStr = request->getParam("enabled")->value();
    enabled = (enabledStr == "true" || enabledStr == "1" || enabledStr == "on");
    LOGD_F("[API] Enabled param (query): %s -> %s", enabledStr.c_str(), enabled ? "true" : "false");
  } else {
    LOGD_F("[API] Fan enabled command missing 'enabled' parameter");
    sendTextResponse(request, 400, "ERR:Missing 'enabled' parameter");
    return;
  }
  
  // STEP 5D: Bounds & Validation - strict checking
  if (fanIndex < 0 || fanIndex > 15) {
    LOGD_F("[API] Fan enabled command rejected - invalid fan index: %d (must be 0-15)", fanIndex);
    sendTextResponse(request, 400, "ERR:Invalid fan index (0-15)");
    return;
  }
  
  size_t fanCount = 0;
  SmartMiFanAsync_getDiscoveredFans(fanCount);
  
  // STEP 5D: Strict bounds checking against actual fan count
  if (fanCount == 0) {
    LOGD_F("[API] Fan enabled command rejected - no fans discovered");
    sendTextResponse(request, 400, "ERR:No fans discovered");
    return;
  }
  
  if ((size_t)fanIndex >= fanCount) {
    LOGD_F("[API] Fan enabled command rejected - fan index %d out of range (fanCount: %zu)", fanIndex, fanCount);
    sendTextResponse(request, 400, "ERR:Fan index out of range");
    return;
  }
  
  // Check state
  if (StateMachine::getState() != StateMachine::State::READY) {
    LOGD_F("[API] Fan enabled command rejected - system not ready (state: %s)", StateMachine::getStateString());
    sendTextResponse(request, 409, "BUSY");
    return;
  }
  
  LOGD_F("[API] Executing fan enabled command: fan[%d] = %s", fanIndex, enabled ? "enabled" : "disabled");
  SmartMiFanAsync_setFanEnabled((uint8_t)fanIndex, enabled);
  settings.fanEnabled[fanIndex] = enabled;
  
  // Save to preferences
  initPreferences();
  char key[16];
  snprintf(key, sizeof(key), "fanEnabled%u", fanIndex);
  preferences.putBool(key, enabled);
  
  // STEP 5: Mark telemetry dirty instead of sending directly from async handler
  WebSocketHandler::markTelemetryDirty();
  WebSocketHandler::sendStateChanged("READY"); // STEP 4: Map to READY instead of FAN_ENABLED_CHANGED
  
  LOGD_F("[API] Fan enabled command executed successfully: fan[%d] = %s", fanIndex, enabled ? "enabled" : "disabled");
  sendTextResponse(request, 200, "OK");
}

void ApiHandlers::handleGetState(AsyncWebServerRequest *request) {
  // Increase size for multiple fans
  DynamicJsonDocument doc(3072);
  
  // System state
  doc["systemState"] = StateMachine::getStateString();
  const char* jobId = StateMachine::getCurrentJobId();
  doc["currentJobId"] = jobId ? jobId : "";
  
  // Fan state
  size_t fanCount = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
  
  JsonArray fansArray = doc.createNestedArray("fans");
  for (size_t i = 0; i < fanCount; i++) {
    JsonObject fan = fansArray.createNestedObject();
    fan["index"] = i;
    
    // Convert IP to string without creating String object
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", 
             fans[i].ip[0], fans[i].ip[1], fans[i].ip[2], fans[i].ip[3]);
    fan["ip"] = ipStr;
    
    fan["did"] = fans[i].did;
    fan["model"] = fans[i].model;
    fan["ready"] = fans[i].ready;
    fan["enabled"] = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
    
    FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    const char *partStateStr = "ERROR";
    if (partState == FanParticipationState::ACTIVE) {
      partStateStr = "ACTIVE";
    } else if (partState == FanParticipationState::INACTIVE) {
      partStateStr = "INACTIVE";
    }
    fan["participationState"] = partStateStr;
  }
  
  // Settings
  JsonObject settingsObj = doc.createNestedObject("settings");
  settingsObj["globalSpeed"] = settings.globalSpeed;
  
  JsonArray fanEnabled = settingsObj.createNestedArray("fanEnabled");
  for (size_t i = 0; i < fanCount && i < 16; i++) {
    fanEnabled.add(settings.fanEnabled[i]);
  }
  
  sendJsonResponse(request, 200, doc);
}


