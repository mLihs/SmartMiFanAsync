/*
 * Prompt3_FullFlow
 * 
 * This example demonstrates Step 3: Full Orchestration Flow
 * 
 * This example shows:
 * 1. System states (ACTIVE, IDLE, SLEEP)
 * 2. Fan participation states (ACTIVE, INACTIVE, ERROR)
 * 3. Error propagation from Step 2
 * 4. Deterministic multi-fan command sending
 * 5. Sleep-on-idle and soft wake-up
 * 
 * This is a comprehensive example that combines:
 * - System state orchestration
 * - Fan participation management
 * - Error callbacks (Step 2)
 * - Command orchestration with coalescing
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
const char *WIFI_SSID = "your-ssid";
const char *WIFI_PASS = "your-password";
// --------------------------------

// ------ Fast Connect Configuration ------
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.100", "valid-token-32-characters-hex"},   // Fan 0: Valid
  {"192.168.1.101", "valid-token-32-characters-hex"},   // Fan 1: Valid
  {"192.168.1.255", "invalid-token-32-characters-hex"}  // Fan 2: Invalid IP (will error)
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;

// Step 3: System State (Project-Level)
SystemState currentSystemState = SystemState::IDLE;
unsigned long lastActivityTime = 0;
const unsigned long IDLE_TIMEOUT_MS = 20000;  // 20 seconds
const unsigned long SLEEP_TIMEOUT_MS = 40000;  // 40 seconds

// Simulated activity sources
bool simulatedBleActivity = false;
bool simulatedUiActivity = false;

// Step 2: Error Callback
void onFanError(const FanErrorInfo& info) {
  // Error callback is observational only - does NOT affect control flow
  const char* opStr = "Unknown";
  switch (info.operation) {
    case FanOp::Handshake: opStr = "Handshake"; break;
    case FanOp::SendCommand: opStr = "SendCommand"; break;
    case FanOp::ReceiveResponse: opStr = "ReceiveResponse"; break;
    case FanOp::HealthCheck: opStr = "HealthCheck"; break;
  }
  
  const char* errStr = "OK";
  switch (info.error) {
    case MiioErr::OK: errStr = "OK"; break;
    case MiioErr::TIMEOUT: errStr = "TIMEOUT"; break;
    case MiioErr::WRONG_SOURCE_IP: errStr = "WRONG_SOURCE_IP"; break;
    case MiioErr::DECRYPT_FAIL: errStr = "DECRYPT_FAIL"; break;
    case MiioErr::INVALID_RESPONSE: errStr = "INVALID_RESPONSE"; break;
  }
  
  LOGI_F("[Error Callback] Fan %u: %s - %s (elapsed: %lu ms, invalidated: %s)",
         info.fanIndex, opStr, errStr, info.elapsedMs,
         info.handshakeInvalidated ? "true" : "false");
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
  LOGI_F("WiFi connected, IP: %d.%d.%d.%d", 
         localIp[0], localIp[1], localIp[2], localIp[3]);
}

// Step 3: Project-level system state management
void updateSystemState() {
  unsigned long now = millis();
  bool hasActivity = simulatedBleActivity || simulatedUiActivity;
  
  switch (currentSystemState) {
    case SystemState::ACTIVE:
      if (hasActivity) {
        lastActivityTime = now;
      } else {
        if (now - lastActivityTime > IDLE_TIMEOUT_MS) {
          LOGI_F("System: ACTIVE -> IDLE");
          currentSystemState = SystemState::IDLE;
          lastActivityTime = now;
        }
      }
      break;
      
    case SystemState::IDLE:
      if (hasActivity) {
        LOGI_F("System: IDLE -> ACTIVE");
        currentSystemState = SystemState::ACTIVE;
        lastActivityTime = now;
        SmartMiFanAsync_softWakeUp();
      } else {
        if (now - lastActivityTime > SLEEP_TIMEOUT_MS) {
          LOGI_F("System: IDLE -> SLEEP");
          currentSystemState = SystemState::SLEEP;
          SmartMiFanAsync_prepareForSleep(true, true);
        }
      }
      break;
      
    case SystemState::SLEEP:
      if (hasActivity) {
        LOGI_F("System: SLEEP -> ACTIVE");
        currentSystemState = SystemState::ACTIVE;
        lastActivityTime = now;
        SmartMiFanAsync_softWakeUp();
      }
      break;
  }
}

void printSystemAndFanStates() {
  const char* sysStateStr = "UNKNOWN";
  switch (currentSystemState) {
    case SystemState::ACTIVE: sysStateStr = "ACTIVE"; break;
    case SystemState::IDLE: sysStateStr = "IDLE"; break;
    case SystemState::SLEEP: sysStateStr = "SLEEP"; break;
  }
  
  LOGI_F("\nSystem State: %s (idle for %lu ms)", 
         sysStateStr, millis() - lastActivityTime);
  
  LOGI_F("Fan Participation States:");
  size_t count = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
  
  for (size_t i = 0; i < count; ++i) {
    FanParticipationState state = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    const char* stateStr = "UNKNOWN";
    switch (state) {
      case FanParticipationState::ACTIVE: stateStr = "ACTIVE"; break;
      case FanParticipationState::INACTIVE: stateStr = "INACTIVE"; break;
      case FanParticipationState::ERROR: stateStr = "ERROR"; break;
    }
    
    LOGI_F("  Fan %zu: %s", i, stateStr);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  LOGI_F("\n=== SmartMiFanAsync Step 3 Full Flow Example ===\n");
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Step 2: Register error callback
  SmartMiFanAsync_setErrorCallback(onFanError);
  
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
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Registered %zu fan(s)", count);
    LOGI(buffer);
  }
  
  // Initialize system state
  currentSystemState = SystemState::IDLE;
  lastActivityTime = millis();
  
  LOGI_F("\nFull Flow Demo:");
  LOGI_F("  - System states: ACTIVE/IDLE/SLEEP");
  LOGI_F("  - Fan participation: ACTIVE/INACTIVE/ERROR");
  LOGI_F("  - Error callbacks (Step 2)");
  LOGI_F("  - Command orchestration with coalescing");
  LOGI_F("  - Sleep/wake hooks");
  LOGI_F("\nStarting demo...\n");
}

void loop() {
  // Step 3: Update system state (project-level logic)
  updateSystemState();
  
  // Simulate activity sources
  // Simulate BLE activity at 5 seconds
  static bool bleSimulated = false;
  if (!bleSimulated && millis() > 5000 && millis() < 5100) {
    bleSimulated = true;
    simulatedBleActivity = true;
    LOGI_F("Simulated: BLE sensor connected");
    delay(100);
    simulatedBleActivity = false;
  }
  
  // Simulate UI activity at 25 seconds
  static bool uiSimulated = false;
  if (!uiSimulated && millis() > 25000 && millis() < 25100) {
    uiSimulated = true;
    simulatedUiActivity = true;
    LOGI_F("Simulated: UI interaction");
    delay(100);
    simulatedUiActivity = false;
  }
  
  // When ACTIVE, demonstrate orchestrated fan control
  if (currentSystemState == SystemState::ACTIVE) {
    static unsigned long lastHandshake = 0;
    static unsigned long lastCommand = 0;
    
    // Perform handshake on all ACTIVE fans
    if (lastHandshake == 0 || millis() - lastHandshake > 10000) {
      lastHandshake = millis();
      LOGI_F("Performing handshake on ACTIVE fans...");
      SmartMiFanAsync_handshakeAllOrchestrated();
      printSystemAndFanStates();
    }
    
    // Send commands to ACTIVE fans (with coalescing)
    if (lastCommand == 0 || millis() - lastCommand > 5000) {
      lastCommand = millis();
      LOGI_F("Sending commands to ACTIVE fans...");
      SmartMiFanAsync_setPowerAllOrchestrated(true);
      delay(1100); // Wait for rate limiting
      SmartMiFanAsync_setSpeedAllOrchestrated(50);
    }
  }
  
  // Demonstrate fan participation toggle
  static bool fan1Disabled = false;
  if (!fan1Disabled && millis() > 15000 && millis() < 15100) {
    fan1Disabled = true;
    LOGI_F("Explicitly disabling Fan 1 (setting INACTIVE)");
    SmartMiFanAsync_setFanEnabled(1, false);
    printSystemAndFanStates();
  }
  
  // Print states periodically
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();
    printSystemAndFanStates();
  }
  
  delay(100);
}

