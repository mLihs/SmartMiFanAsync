/*
 * MultipleFansWebServer Example
 * 
 * This example demonstrates:
 * - Smart Connect mode (Fast Connect + Discovery fallback)
 * - ESPAsyncWebServer with REST API and WebSocket
 * - State-machine pattern with HTTP actions and WebSocket events
 * - Multi-file architecture for better maintainability
 * 
 * API Design:
 * - HTTP (fetch) for Actions + Settings
 * - POST /api/action/... for starting/stopping/committing things
 * - GET /api/settings + PUT /api/settings for config read/save
 * - GET /api/state for snapshot of all state machines
 * - WebSocket /ws for live state, progress, events
 * - WS pushes: stateChanged, progress, telemetry, error, log
 * 
 * Performance:
 * - WS: throttle/batch (max 10-20 Updates/s, "only on change")
 * - HTTP: reliable & debuggable
 * 
 * Hardware: ESP32
 * 
 * Author: Martin Lihs
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <SmartMiFanAsync.h>
#include <DebugLog.h>

#include "webserver.h"
#include "api_handlers.h"
#include "websocket_handler.h"
#include "state_machine.h"
#include "web_ui.h"
#include "config.h"

// ------ WiFi Configuration ------
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
// --------------------------------

WiFiUDP fanUdp;

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
  
  LOGI_F("\n=== SmartMiFanAsync MultipleFansWebServer Example ===\n");
  
  // Initialize state machine
  StateMachine::init();
  
  // Initialize preferences (load saved settings)
  // This is done via initPreferences() in api_handlers.cpp, but we can also call it here
  // to ensure settings are loaded before any API calls
  
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
  
  // Initialize web server
  WebServer::init();
  LOGI_F("Web server started on port 80");
  
  // Configure Fast Connect
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Configuring Fast Connect with %zu fans...", FAST_CONNECT_FAN_COUNT);
    LOGI(buffer);
  }
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    StateMachine::setState(StateMachine::State::ERROR);
    return;
  }
  
  // Start Smart Connect
  LOGI_F("Starting Smart Connect...");
  if (SmartMiFanAsync_startSmartConnect(fanUdp, 5000)) {
    LOGI_F("Smart Connect started");
    StateMachine::startScan();
  } else {
    LOGE_F("Failed to start Smart Connect");
    StateMachine::setState(StateMachine::State::ERROR);
  }
}

void loop() {
  // Update Smart Connect
  if (StateMachine::getState() == StateMachine::State::SCANNING) {
    if (SmartMiFanAsync_isSmartConnectInProgress()) {
      SmartMiFanAsync_updateSmartConnect();
      
      // Send progress updates via WebSocket
      static unsigned long lastProgressUpdate = 0;
      if (millis() - lastProgressUpdate > 500) {
        lastProgressUpdate = millis();
        SmartConnectState state = SmartMiFanAsync_getSmartConnectState();
        const char* jobId = StateMachine::getCurrentJobId();
        WebSocketHandler::sendProgress(jobId, stateToString(state));
      }
    } else if (SmartMiFanAsync_isSmartConnectComplete()) {
      LOGI_F("\n=== Smart Connect Complete ===\n");
      SmartMiFanAsync_printDiscoveredFans();
      
      size_t count = 0;
      const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
      LOGI_F("Found %zu fan(s) total", count);
      
      if (count > 0) {
        LOGI_F("Performing handshake with all fans...");
        
        // DEBUG: Ensure all discovered fans are enabled by default
        LOGD_F("Ensuring all discovered fans are enabled...");
        for (size_t i = 0; i < count; i++) {
          bool wasEnabled = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
          SmartMiFanAsync_setFanEnabled(static_cast<uint8_t>(i), true);
          FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
          const char* stateStr = (partState == FanParticipationState::ACTIVE) ? "ACTIVE" : 
                                 (partState == FanParticipationState::INACTIVE) ? "INACTIVE" : "ERROR";
          LOGD_F("  Fan[%zu]: enabled=%s->true, participation=%s, ready=%s", 
                 i, wasEnabled ? "true" : "false", stateStr, fans[i].ready ? "true" : "false");
        }
        
        if (SmartMiFanAsync_handshakeAllOrchestrated()) {
          LOGI_F("Handshake successful");
          
          // DEBUG: Check fan states after handshake
          LOGD_F("Fan states after handshake:");
          for (size_t i = 0; i < count; i++) {
            FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
            bool enabled = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
            const char* stateStr = (partState == FanParticipationState::ACTIVE) ? "ACTIVE" : 
                                   (partState == FanParticipationState::INACTIVE) ? "INACTIVE" : "ERROR";
            LOGD_F("  Fan[%zu]: enabled=%s, participation=%s, ready=%s", 
                   i, enabled ? "true" : "false", stateStr, fans[i].ready ? "true" : "false");
          }
          
          StateMachine::setState(StateMachine::State::READY);
          WebSocketHandler::sendStateChanged("READY");
        } else {
          LOGW_F("Some fans failed handshake");
          
          // DEBUG: Check fan states even after failed handshake
          LOGD_F("Fan states after handshake (some failed):");
          for (size_t i = 0; i < count; i++) {
            FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
            bool enabled = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
            const char* stateStr = (partState == FanParticipationState::ACTIVE) ? "ACTIVE" : 
                                   (partState == FanParticipationState::INACTIVE) ? "INACTIVE" : "ERROR";
            LOGD_F("  Fan[%zu]: enabled=%s, participation=%s, ready=%s", 
                   i, enabled ? "true" : "false", stateStr, fans[i].ready ? "true" : "false");
          }
          
          StateMachine::setState(StateMachine::State::READY); // Continue anyway
          WebSocketHandler::sendStateChanged("READY");
        }
      } else {
        LOGW_F("No fans found");
        StateMachine::setState(StateMachine::State::READY); // Continue anyway
        WebSocketHandler::sendStateChanged("READY");
      }
    } else {
      LOGE_F("Smart Connect failed or error");
      StateMachine::setState(StateMachine::State::ERROR);
      WebSocketHandler::sendError("Smart Connect failed");
    }
  }
  
  // Update state machine
  StateMachine::update();
  
  // Send periodic telemetry updates (throttled by WebSocketHandler)
  if (StateMachine::getState() == StateMachine::State::READY) {
    WebSocketHandler::updateTelemetry();
  }
  
  // Clean up disconnected WebSocket clients
  WebSocketHandler::cleanup();
  
  delay(10);
}

// Helper function to convert SmartConnectState to string
const char* stateToString(SmartConnectState state) {
  switch (state) {
    case SmartConnectState::IDLE: return "IDLE";
    case SmartConnectState::VALIDATING_FAST_CONNECT: return "VALIDATING_FAST_CONNECT";
    case SmartConnectState::STARTING_DISCOVERY: return "STARTING_DISCOVERY";
    case SmartConnectState::DISCOVERING: return "DISCOVERING";
    case SmartConnectState::COMPLETE: return "COMPLETE";
    case SmartConnectState::ERROR: return "ERROR";
    default: return "UNKNOWN";
  }
}

