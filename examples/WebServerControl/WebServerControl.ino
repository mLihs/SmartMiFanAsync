/*
 * SmartMiFanAsync - Web Server Control Example
 * 
 * This example demonstrates:
 * - ESPAsyncWebServer with WebSocket and REST API
 * - Web interface to display and control multiple fans
 * - WebSocket for real-time telemetry/events and UI state updates
 * - REST API for actions (power, speed, fan enable/disable) and settings persistence
 * 
 * Features:
 * - Display all connected fans and their state
 * - Set speed for all fans simultaneously (1-100%)
 * - Turn all fans on/off simultaneously
 * - Each fan can be individually set Active/Inactive
 * - Real-time updates via WebSocket
 * - Settings persistence via REST API
 * 
 * Required libraries:
 * - ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)
 * - AsyncTCP (ESP32) - required by ESPAsyncWebServer
 * - WiFi (ESP32 Arduino Core)
 * - ESPmDNS (ESP32 Arduino Core)
 * 
 * Hardware: ESP32
 * 
 * Author: Martin Lihs
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SmartMiFanAsync.h>
#include <DebugLog.h>

// ------ WiFi Configuration ------
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
// --------------------------------

// ------ Fast Connect Configuration ------
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.100", "11111111111111111111111111111111"},
  {"192.168.1.101", "22222222222222222222222222222222"},
  {"192.168.1.102", "33333333333333333333333333333333"},
  {"192.168.1.103", "44444444444444444444444444444444"}
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;
AsyncWebServer server(80);
AsyncWebSocket ws("/telemetry/events");

// Application state
enum class AppState {
  CONNECTING,
  READY,
  ERROR
};

AppState appState = AppState::CONNECTING;
unsigned long lastTelemetryUpdate = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 1000; // Send telemetry every second

// Global fan control state
bool globalPowerState = false;
uint8_t globalSpeed = 30;

// Web page HTML
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>SmartMi Fan Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .header {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
    }
    .status {
      display: inline-block;
      padding: 8px 16px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: 500;
    }
    .status.connected {
      background: #d4edda;
      color: #155724;
    }
    .status.disconnected {
      background: #f8d7da;
      color: #721c24;
    }
    .controls {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .control-group {
      margin-bottom: 25px;
    }
    .control-group:last-child {
      margin-bottom: 0;
    }
    .control-label {
      display: block;
      font-weight: 600;
      margin-bottom: 10px;
      color: #333;
    }
    .button-group {
      display: flex;
      gap: 10px;
    }
    button {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.2s;
      flex: 1;
    }
    button:hover {
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
    }
    button:active {
      transform: translateY(0);
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-danger {
      background: #dc3545;
      color: white;
    }
    .btn-success {
      background: #28a745;
      color: white;
    }
    .slider-container {
      display: flex;
      align-items: center;
      gap: 15px;
    }
    input[type="range"] {
      flex: 1;
      height: 8px;
      border-radius: 5px;
      background: #ddd;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
    }
    input[type="range"]::-moz-range-thumb {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
      border: none;
    }
    .speed-value {
      font-size: 24px;
      font-weight: bold;
      color: #667eea;
      min-width: 60px;
      text-align: center;
    }
    .fans-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
      gap: 20px;
    }
    .fan-card {
      background: white;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .fan-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 15px;
    }
    .fan-title {
      font-size: 18px;
      font-weight: 600;
      color: #333;
    }
    .fan-toggle {
      position: relative;
      width: 50px;
      height: 26px;
    }
    .fan-toggle input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .fan-toggle-slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 26px;
    }
    .fan-toggle-slider:before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    .fan-toggle input:checked + .fan-toggle-slider {
      background-color: #667eea;
    }
    .fan-toggle input:checked + .fan-toggle-slider:before {
      transform: translateX(24px);
    }
    .fan-info {
      margin-bottom: 10px;
    }
    .fan-info-item {
      display: flex;
      justify-content: space-between;
      padding: 5px 0;
      font-size: 14px;
    }
    .fan-info-label {
      color: #666;
    }
    .fan-info-value {
      color: #333;
      font-weight: 500;
    }
    .fan-state {
      display: inline-block;
      padding: 4px 12px;
      border-radius: 12px;
      font-size: 12px;
      font-weight: 500;
      margin-top: 10px;
    }
    .fan-state.active {
      background: #d4edda;
      color: #155724;
    }
    .fan-state.inactive {
      background: #fff3cd;
      color: #856404;
    }
    .fan-state.error {
      background: #f8d7da;
      color: #721c24;
    }
    .empty-state {
      text-align: center;
      padding: 40px;
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🌬️ SmartMi Fan Control</h1>
      <div>
        <span class="status disconnected" id="ws-status">WebSocket: Disconnected</span>
      </div>
    </div>
    
    <div class="controls">
      <div class="control-group">
        <label class="control-label">Global Power Control</label>
        <div class="button-group">
          <button class="btn-success" onclick="setAllPower(true)">Turn All ON</button>
          <button class="btn-danger" onclick="setAllPower(false)">Turn All OFF</button>
        </div>
      </div>
      
      <div class="control-group">
        <label class="control-label">Global Speed Control</label>
        <div class="slider-container">
          <input type="range" id="speed-slider" min="1" max="100" value="30" oninput="updateSpeedDisplay(this.value)" onchange="setAllSpeed(this.value)">
          <span class="speed-value" id="speed-display">30%</span>
        </div>
      </div>
    </div>
    
    <div class="controls">
      <h2 style="margin-bottom: 20px;">Connected Fans</h2>
      <div class="fans-grid" id="fans-container">
        <div class="empty-state">No fans connected</div>
      </div>
    </div>
  </div>
  
  <script>
    var websocket;
    var fans = [];
    
    window.addEventListener('load', onload);
    
    function onload(event) {
      initWebSocket();
      loadSettings();
    }
    
    function initWebSocket() {
      var gateway = `ws://${window.location.hostname}/telemetry/events`;
      console.log('Connecting to WebSocket:', gateway);
      websocket = new WebSocket(gateway);
      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage;
      websocket.onerror = onError;
    }
    
    function onOpen(event) {
      console.log('WebSocket connected');
      document.getElementById('ws-status').className = 'status connected';
      document.getElementById('ws-status').textContent = 'WebSocket: Connected';
    }
    
    function onClose(event) {
      console.log('WebSocket disconnected');
      document.getElementById('ws-status').className = 'status disconnected';
      document.getElementById('ws-status').textContent = 'WebSocket: Disconnected';
      setTimeout(initWebSocket, 2000);
    }
    
    function onError(event) {
      console.error('WebSocket error:', event);
    }
    
    function onMessage(event) {
      try {
        var data = JSON.parse(event.data);
        console.log('Received:', data);
        
        if (data.type === 'telemetry') {
          updateFansDisplay(data.fans);
        } else if (data.type === 'ui_state') {
          updateUIState(data.state);
        }
      } catch (e) {
        console.error('Failed to parse WebSocket message:', e);
      }
    }
    
    function updateFansDisplay(fansData) {
      fans = fansData || [];
      var container = document.getElementById('fans-container');
      
      if (fans.length === 0) {
        container.innerHTML = '<div class="empty-state">No fans connected</div>';
        return;
      }
      
      container.innerHTML = fans.map((fan, index) => {
        var stateClass = fan.participationState === 'ACTIVE' ? 'active' : 
                         fan.participationState === 'INACTIVE' ? 'inactive' : 'error';
        var stateText = fan.participationState === 'ACTIVE' ? 'Active' : 
                        fan.participationState === 'INACTIVE' ? 'Inactive' : 'Error';
        
        return `
          <div class="fan-card">
            <div class="fan-header">
              <div class="fan-title">Fan ${index + 1}</div>
              <label class="fan-toggle">
                <input type="checkbox" ${fan.enabled ? 'checked' : ''} 
                       onchange="setFanEnabled(${index}, this.checked)">
                <span class="fan-toggle-slider"></span>
              </label>
            </div>
            <div class="fan-info">
              <div class="fan-info-item">
                <span class="fan-info-label">IP:</span>
                <span class="fan-info-value">${fan.ip}</span>
              </div>
              <div class="fan-info-item">
                <span class="fan-info-label">Model:</span>
                <span class="fan-info-value">${fan.model || 'Unknown'}</span>
              </div>
              <div class="fan-info-item">
                <span class="fan-info-label">Ready:</span>
                <span class="fan-info-value">${fan.ready ? 'Yes' : 'No'}</span>
              </div>
              <div class="fan-info-item">
                <span class="fan-info-label">DID:</span>
                <span class="fan-info-value">${fan.did || 'N/A'}</span>
              </div>
            </div>
            <div class="fan-state ${stateClass}">${stateText}</div>
          </div>
        `;
      }).join('');
    }
    
    function updateUIState(state) {
      if (state.globalSpeed !== undefined) {
        document.getElementById('speed-slider').value = state.globalSpeed;
        document.getElementById('speed-display').textContent = state.globalSpeed + '%';
      }
    }
    
    function setAllPower(on) {
      fetch('/api/actions/power', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({power: on})
      })
      .then(response => response.json())
      .then(data => {
        console.log('Power set:', data);
      })
      .catch(error => {
        console.error('Error setting power:', error);
      });
    }
    
    function setAllSpeed(speed) {
      fetch('/api/actions/speed', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({speed: parseInt(speed)})
      })
      .then(response => response.json())
      .then(data => {
        console.log('Speed set:', data);
        saveSettings();
      })
      .catch(error => {
        console.error('Error setting speed:', error);
      });
    }
    
    function updateSpeedDisplay(value) {
      document.getElementById('speed-display').textContent = value + '%';
    }
    
    function setFanEnabled(fanIndex, enabled) {
      fetch('/api/actions/fan-enabled', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({fanIndex: fanIndex, enabled: enabled})
      })
      .then(response => response.json())
      .then(data => {
        console.log('Fan enabled set:', data);
        saveSettings();
      })
      .catch(error => {
        console.error('Error setting fan enabled:', error);
      });
    }
    
    function loadSettings() {
      fetch('/api/settings')
        .then(response => response.json())
        .then(data => {
          if (data.globalSpeed !== undefined) {
            document.getElementById('speed-slider').value = data.globalSpeed;
            document.getElementById('speed-display').textContent = data.globalSpeed + '%';
          }
        })
        .catch(error => {
          console.error('Error loading settings:', error);
        });
    }
    
    function saveSettings() {
      var settings = {
        globalSpeed: parseInt(document.getElementById('speed-slider').value)
      };
      
      fetch('/api/settings', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(settings)
      })
      .then(response => response.json())
      .then(data => {
        console.log('Settings saved:', data);
      })
      .catch(error => {
        console.error('Error saving settings:', error);
      });
    }
  </script>
</body>
</html>
)rawliteral";

// Settings storage (in a real application, use EEPROM or NVS)
struct Settings {
  uint8_t globalSpeed;
  bool fanEnabled[16]; // Max 16 fans
  bool initialized;
};

Settings settings = {30, {true, true, true, true}, false};

// Load settings from storage (placeholder - implement with EEPROM/NVS)
void loadSettings() {
  // In a real implementation, load from EEPROM or NVS
  settings.globalSpeed = 30;
  for (size_t i = 0; i < 16; i++) {
    settings.fanEnabled[i] = true;
  }
  settings.initialized = true;
}

// Save settings to storage (placeholder - implement with EEPROM/NVS)
void saveSettings() {
  // In a real implementation, save to EEPROM or NVS
  LOGI_F("Settings saved: globalSpeed=%u", settings.globalSpeed);
}

// Send telemetry data via WebSocket
void sendTelemetry() {
  if (ws.count() == 0) {
    return; // No clients connected
  }
  
  size_t fanCount = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
  
  // Build JSON message
  String json = "{\"type\":\"telemetry\",\"fans\":[";
  
  for (size_t i = 0; i < fanCount; i++) {
    if (i > 0) json += ",";
    
    FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    const char *partStateStr = "ERROR";
    if (partState == FanParticipationState::ACTIVE) {
      partStateStr = "ACTIVE";
    } else if (partState == FanParticipationState::INACTIVE) {
      partStateStr = "INACTIVE";
    }
    
    json += "{";
    json += "\"index\":" + String(i) + ",";
    json += "\"ip\":\"" + fans[i].ip.toString() + "\",";
    json += "\"did\":" + String(fans[i].did) + ",";
    json += "\"model\":\"" + String(fans[i].model) + "\",";
    json += "\"ready\":" + String(fans[i].ready ? "true" : "false") + ",";
    json += "\"enabled\":" + String(SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i)) ? "true" : "false") + ",";
    json += "\"participationState\":\"" + String(partStateStr) + "\"";
    json += "}";
  }
  
  json += "]}";
  
  ws.textAll(json);
}

// Send UI state via WebSocket
void sendUIState() {
  if (ws.count() == 0) {
    return;
  }
  
  String json = "{\"type\":\"ui_state\",\"state\":{";
  json += "\"globalSpeed\":" + String(settings.globalSpeed);
  json += "}}";
  
  ws.textAll(json);
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    LOGI_F("WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
    // Send initial state
    sendTelemetry();
    sendUIState();
  } else if (type == WS_EVT_DISCONNECT) {
    LOGI_F("WebSocket client #%u disconnected", client->id());
  } else if (type == WS_EVT_DATA) {
    // Handle incoming WebSocket data if needed
    // For now, we only send data, not receive commands via WebSocket
  }
}

// REST API: Get settings
void handleGetSettings(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  doc["globalSpeed"] = settings.globalSpeed;
  
  JsonArray fanEnabled = doc.createNestedArray("fanEnabled");
  size_t fanCount = 0;
  SmartMiFanAsync_getDiscoveredFans(fanCount);
  for (size_t i = 0; i < fanCount && i < 16; i++) {
    fanEnabled.add(SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i)));
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

// REST API: Save settings
void handlePostSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    request->_tempObject = new DynamicJsonDocument(1024);
  }
  
  DynamicJsonDocument *doc = (DynamicJsonDocument *)request->_tempObject;
  
  if (index + len < total) {
    // More data to come
    return;
  }
  
  DeserializationError error = deserializeJson(*doc, (char *)data);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    delete doc;
    return;
  }
  
  if (doc->containsKey("globalSpeed")) {
    settings.globalSpeed = (*doc)["globalSpeed"].as<uint8_t>();
    if (settings.globalSpeed < 1) settings.globalSpeed = 1;
    if (settings.globalSpeed > 100) settings.globalSpeed = 100;
  }
  
  if (doc->containsKey("fanEnabled")) {
    JsonArray fanEnabled = (*doc)["fanEnabled"];
    size_t fanCount = 0;
    SmartMiFanAsync_getDiscoveredFans(fanCount);
    for (size_t i = 0; i < fanEnabled.size() && i < fanCount && i < 16; i++) {
      SmartMiFanAsync_setFanEnabled(static_cast<uint8_t>(i), fanEnabled[i].as<bool>());
      settings.fanEnabled[i] = fanEnabled[i].as<bool>();
    }
  }
  
  saveSettings();
  sendUIState();
  
  request->send(200, "application/json", "{\"success\":true}");
  delete doc;
}

// REST API: Set power for all fans
void handlePostPower(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    request->_tempObject = new DynamicJsonDocument(256);
  }
  
  DynamicJsonDocument *doc = (DynamicJsonDocument *)request->_tempObject;
  
  if (index + len < total) {
    return;
  }
  
  DeserializationError error = deserializeJson(*doc, (char *)data);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    delete doc;
    return;
  }
  
  bool power = (*doc)["power"].as<bool>();
  globalPowerState = power;
  
  bool success = SmartMiFanAsync_setPowerAllOrchestrated(power);
  
  // Send telemetry update
  sendTelemetry();
  
  String response = "{\"success\":" + String(success ? "true" : "false") + "}";
  request->send(200, "application/json", response);
  delete doc;
}

// REST API: Set speed for all fans
void handlePostSpeed(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    request->_tempObject = new DynamicJsonDocument(256);
  }
  
  DynamicJsonDocument *doc = (DynamicJsonDocument *)request->_tempObject;
  
  if (index + len < total) {
    return;
  }
  
  DeserializationError error = deserializeJson(*doc, (char *)data);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    delete doc;
    return;
  }
  
  uint8_t speed = (*doc)["speed"].as<uint8_t>();
  if (speed < 1) speed = 1;
  if (speed > 100) speed = 100;
  
  settings.globalSpeed = speed;
  globalSpeed = speed;
  
  bool success = SmartMiFanAsync_setSpeedAllOrchestrated(speed);
  
  saveSettings();
  sendTelemetry();
  sendUIState();
  
  String response = "{\"success\":" + String(success ? "true" : "false") + "}";
  request->send(200, "application/json", response);
  delete doc;
}

// REST API: Set fan enabled state
void handlePostFanEnabled(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    request->_tempObject = new DynamicJsonDocument(256);
  }
  
  DynamicJsonDocument *doc = (DynamicJsonDocument *)request->_tempObject;
  
  if (index + len < total) {
    return;
  }
  
  DeserializationError error = deserializeJson(*doc, (char *)data);
  if (error) {
    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    delete doc;
    return;
  }
  
  uint8_t fanIndex = (*doc)["fanIndex"].as<uint8_t>();
  bool enabled = (*doc)["enabled"].as<bool>();
  
  size_t fanCount = 0;
  SmartMiFanAsync_getDiscoveredFans(fanCount);
  
  if (fanIndex >= fanCount) {
    request->send(400, "application/json", "{\"error\":\"Invalid fan index\"}");
    delete doc;
    return;
  }
  
  SmartMiFanAsync_setFanEnabled(fanIndex, enabled);
  settings.fanEnabled[fanIndex] = enabled;
  
  saveSettings();
  sendTelemetry();
  
  String response = "{\"success\":true}";
  request->send(200, "application/json", response);
  delete doc;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  LOGI_F("Connecting to WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 20000) {
      LOGE_F("WiFi connection failed");
      return;
    }
    LOGD_F(".");
    delay(250);
  }
  IPAddress localIp = WiFi.localIP();
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "WiFi connected, IP: %d.%d.%d.%d", localIp[0], localIp[1], localIp[2], localIp[3]);
  LOGI(buffer);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  LOGI_F("\n=== SmartMiFanAsync Web Server Control Example ===\n");
  
  // Load settings
  loadSettings();
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Setup mDNS
  if (MDNS.begin("smartmi-fan-control")) {
    MDNS.addService("http", "tcp", 80);
    LOGI_F("mDNS started: http://smartmi-fan-control.local");
  }
  
  // Setup WebSocket
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  
  // Setup REST API routes
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostSettings);
  
  server.on("/api/actions/power", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostPower);
  server.on("/api/actions/speed", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostSpeed);
  server.on("/api/actions/fan-enabled", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostFanEnabled);
  
  // Serve web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });
  
  // Start server
  server.begin();
  LOGI_F("Web server started on port 80");
  
  // Configure Fast Connect
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Configuring Fast Connect with %zu fans...", FAST_CONNECT_FAN_COUNT);
    LOGI(buffer);
  }
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    appState = AppState::ERROR;
    return;
  }
  
  // Start Smart Connect
  LOGI_F("Starting Smart Connect...");
  if (SmartMiFanAsync_startSmartConnect(fanUdp, 5000)) {
    LOGI_F("Smart Connect started");
    appState = AppState::CONNECTING;
  } else {
    LOGE_F("Failed to start Smart Connect");
    appState = AppState::ERROR;
  }
}

void loop() {
  // Handle Smart Connect
  if (appState == AppState::CONNECTING) {
    if (SmartMiFanAsync_isSmartConnectInProgress()) {
      SmartMiFanAsync_updateSmartConnect();
    } else if (SmartMiFanAsync_isSmartConnectComplete()) {
      LOGI_F("\n=== Smart Connect Complete ===\n");
      SmartMiFanAsync_printDiscoveredFans();
      
      size_t count = 0;
      const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
      LOGI_F("Found %zu fan(s) total", count);
      
      if (count > 0) {
        LOGI_F("Performing handshake with all fans...");
        if (SmartMiFanAsync_handshakeAllOrchestrated()) {
          LOGI_F("Handshake successful");
          appState = AppState::READY;
          
          // Apply saved settings
          for (size_t i = 0; i < count && i < 16; i++) {
            SmartMiFanAsync_setFanEnabled(static_cast<uint8_t>(i), settings.fanEnabled[i]);
          }
          
          // Set initial speed
          if (settings.globalSpeed > 0) {
            SmartMiFanAsync_setSpeedAllOrchestrated(settings.globalSpeed);
          }
        } else {
          LOGW_F("Some fans failed handshake");
          appState = AppState::READY; // Continue anyway
        }
      } else {
        LOGW_F("No fans found");
        appState = AppState::READY; // Continue anyway
      }
    } else {
      LOGE_F("Smart Connect failed or error");
      appState = AppState::ERROR;
    }
  }
  
  // Send telemetry updates periodically
  if (appState == AppState::READY && millis() - lastTelemetryUpdate > TELEMETRY_INTERVAL_MS) {
    lastTelemetryUpdate = millis();
    sendTelemetry();
  }
  
  // Clean up disconnected WebSocket clients
  ws.cleanupClients();
  
  delay(10);
}

