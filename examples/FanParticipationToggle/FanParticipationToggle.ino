/*
 * FanParticipationToggle
 * 
 * This example demonstrates Step 3: Fan Participation State Management
 * 
 * This example shows:
 * 1. Default ACTIVE participation for all fans
 * 2. Explicit exclusion of a fan (set INACTIVE)
 * 3. A fan entering ERROR state via technical failure
 * 4. Only ACTIVE fans receiving commands
 * 
 * Key Concepts:
 * - All fans start in ACTIVE state after registration
 * - INACTIVE is set explicitly by the project (e.g. UI toggle)
 * - ERROR is derived from technical readiness (ready == false OR lastError != OK)
 * - Only ACTIVE fans receive commands
 * 
 * Hardware Requirements:
 * - ESP32 board
 * - WiFi connection
 * 
 * Author: Martin Lihs
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>
#include <DebugLog.h>

// ------ WiFi Configuration ------
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
// --------------------------------

// ------ Fast Connect Configuration ------
// Configure multiple fans for demonstration
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.100", "11111111111111111111111111111111"},   // Fan 1: Valid
  {"192.168.1.101", "22222222222222222222222222222222"},  // Fan 2: Valid
  {"192.168.1.104", "33333333333333333333333333333333"},  // Fan 3: Valid
  {"192.168.1.105", "44444444444444444444444444444444"}   // Fan 4: Wrong IP -> Error 
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;

enum class DemoPhase {
  INITIALIZING,
  SHOWING_DEFAULTS,
  DISABLING_FAN,
  SHOWING_STATES,
  SENDING_COMMANDS,
  IDLE
};

DemoPhase demoPhase = DemoPhase::INITIALIZING;
unsigned long phaseStartTime = 0;

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
  snprintf(buffer, sizeof(buffer), "WiFi connected, IP: %d.%d.%d.%d", 
           localIp[0], localIp[1], localIp[2], localIp[3]);
  LOGI(buffer);
}

void printFanParticipationStates() {
  LOGI_F("\n=== Fan Participation States ===");
  size_t count = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
  
  for (size_t i = 0; i < count; ++i) {
    FanParticipationState state = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    bool userEnabled = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i));
    bool ready = SmartMiFanAsync_isFanReady(static_cast<uint8_t>(i));
    MiioErr lastError = SmartMiFanAsync_getFanLastError(static_cast<uint8_t>(i));
    
    const char* stateStr = "UNKNOWN";
    switch (state) {
      case FanParticipationState::ACTIVE:
        stateStr = "ACTIVE";
        break;
      case FanParticipationState::INACTIVE:
        stateStr = "INACTIVE";
        break;
      case FanParticipationState::ERROR:
        stateStr = "ERROR";
        break;
    }
    
    const char* errorStr = "OK";
    switch (lastError) {
      case MiioErr::OK:
        errorStr = "OK";
        break;
      case MiioErr::TIMEOUT:
        errorStr = "TIMEOUT";
        break;
      case MiioErr::WRONG_SOURCE_IP:
        errorStr = "WRONG_SOURCE_IP";
        break;
      case MiioErr::DECRYPT_FAIL:
        errorStr = "DECRYPT_FAIL";
        break;
      case MiioErr::INVALID_RESPONSE:
        errorStr = "INVALID_RESPONSE";
        break;
    }
    
    LOGI_F("Fan %zu (%d.%d.%d.%d):\n  Participation: %s\n  User Enabled: %s\n  Technical Ready: %s\n  Last Error: %s",
           i, fans[i].ip[0], fans[i].ip[1], fans[i].ip[2], fans[i].ip[3],
           stateStr, userEnabled ? "true" : "false", 
           ready ? "true" : "false", errorStr);
  }
  LOGI_F("===============================\n");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  LOGI_F("\n=== SmartMiFanAsync Fan Participation Toggle Example ===\n");
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Enable Fast Connect mode
  SmartMiFanAsync_setFastConnectEnabled(true);
  
  // Set Fast Connect configuration
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    while (true) {
      delay(1000);
    }
  }
  
  // Reset discovered fans list
  SmartMiFanAsync_resetDiscoveredFans();
  
  // Register fans directly
  if (SmartMiFanAsync_registerFastConnectFans(fanUdp)) {
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    LOGI_F("Registered %zu fan(s)", count);
  }
  
  LOGI_F("\nFan Participation State Demo:");
  LOGI_F("  - All fans start with ACTIVE participation (default)");
  LOGI_F("  - Fan 3 has invalid IP (will enter ERROR state)");
  LOGI_F("  - Fan 1 will be explicitly disabled (INACTIVE)");
  LOGI_F("  - Only ACTIVE fans will receive commands");
  LOGI_F("\nStarting demo...\n");
  
  phaseStartTime = millis();
}

void loop() {
  unsigned long now = millis();
  
  switch (demoPhase) {
    case DemoPhase::INITIALIZING:
      // Wait a moment
      if (now - phaseStartTime > 1000) {
        demoPhase = DemoPhase::SHOWING_DEFAULTS;
        phaseStartTime = now;
      }
      break;
      
    case DemoPhase::SHOWING_DEFAULTS:
      LOGI_F("\nPhase 1: Showing default participation states (all ACTIVE)");
      printFanParticipationStates();
      demoPhase = DemoPhase::DISABLING_FAN;
      phaseStartTime = now;
      break;
      
    case DemoPhase::DISABLING_FAN:
      if (now - phaseStartTime > 2000) {
        LOGI_F("\nPhase 2: Explicitly disabling Fan 1 (setting INACTIVE)");
        SmartMiFanAsync_setFanEnabled(1, false);
        printFanParticipationStates();
        demoPhase = DemoPhase::SHOWING_STATES;
        phaseStartTime = now;
      }
      break;
      
    case DemoPhase::SHOWING_STATES:
      if (now - phaseStartTime > 2000) {
        LOGI_F("\nPhase 3: Attempting handshake to trigger ERROR state for Fan 3");
        // Attempt handshake - Fan 3 will fail (invalid IP) and enter ERROR state
        SmartMiFanAsync_handshakeAllOrchestrated();
        delay(1000);
        printFanParticipationStates();
        demoPhase = DemoPhase::SENDING_COMMANDS;
        phaseStartTime = now;
      }
      break;
      
    case DemoPhase::SENDING_COMMANDS:
      if (now - phaseStartTime > 2000) {
        LOGI_F("\nPhase 4: Sending commands (only ACTIVE fans receive commands)");
        LOGI_F("Expected: Only Fan 0 receives commands (Fan 1=INACTIVE, Fan 2=ERROR, Fan 3=ERROR)");
        
        // Send commands - only ACTIVE fans will receive them
        SmartMiFanAsync_setPowerAllOrchestrated(true);
        delay(1100); // Wait for rate limiting
        SmartMiFanAsync_setSpeedAllOrchestrated(50);
        
        LOGI_F("\nCommands sent. Check logs above to see which fans received commands.");
        demoPhase = DemoPhase::IDLE;
        phaseStartTime = now;
      }
      break;
      
    case DemoPhase::IDLE:
      // Demo complete
      static bool demoCompletePrinted = false;
      if (!demoCompletePrinted) {
        LOGI_F("\n=== Demo Complete ===");
        LOGI_F("Summary:");
        LOGI_F("  - Fan 0: ACTIVE (receives commands)");
        LOGI_F("  - Fan 1: INACTIVE (excluded by user, no commands)");
        LOGI_F("  - Fan 2: ACTIVE (receives commands)");
        LOGI_F("  - Fan 3: ERROR (technical failure, no commands)");
        demoCompletePrinted = true;
      }
      break;
  }
  
  delay(100);
}

