/*
 * SystemStateOrchestration
 * 
 * This example demonstrates Step 3: System State Orchestration
 * 
 * This example shows:
 * 1. Explicit project-level transitions between ACTIVE / IDLE / SLEEP
 * 2. Calls to softWakeUp() and prepareForSleep()
 * 3. No implicit or automatic state changes
 * 
 * Key Concepts:
 * - System states (ACTIVE, IDLE, SLEEP) are controlled EXCLUSIVELY by the project
 * - The library never sets or changes system state
 * - Library only exposes hooks (softWakeUp / prepareForSleep)
 * - Transitions are decided ONLY by the project
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
  {"192.168.1.100", "your-32-char-token-hex-here"}
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;

// Step 3: System State (Project-Level)
// System states are controlled EXCLUSIVELY by the project.
// The library does not know the current system state.
SystemState currentSystemState = SystemState::IDLE;
unsigned long lastActivityTime = 0;
const unsigned long IDLE_TIMEOUT_MS = 30000;  // 30 seconds
const unsigned long SLEEP_TIMEOUT_MS = 60000;  // 60 seconds

// Simulated activity sources (in real project: BLE sensors, UI, etc.)
bool simulatedBleActivity = false;
bool simulatedUiActivity = false;

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

// Step 3: Project-level system state management
// This function decides when to transition system states
// The library never calls this - it's purely project logic
void updateSystemState() {
  unsigned long now = millis();
  bool hasActivity = simulatedBleActivity || simulatedUiActivity;
  
  switch (currentSystemState) {
    case SystemState::ACTIVE:
      if (hasActivity) {
        lastActivityTime = now;
      } else {
        // No activity - transition to IDLE after timeout
        if (now - lastActivityTime > IDLE_TIMEOUT_MS) {
          LOGI_F("System: ACTIVE -> IDLE (no activity)");
          currentSystemState = SystemState::IDLE;
          lastActivityTime = now;
        }
      }
      break;
      
    case SystemState::IDLE:
      if (hasActivity) {
        // Activity detected - transition to ACTIVE
        LOGI_F("System: IDLE -> ACTIVE (activity detected)");
        currentSystemState = SystemState::ACTIVE;
        lastActivityTime = now;
        
        // Call library hook for wake-up
        SmartMiFanAsync_softWakeUp();
      } else {
        // Still no activity - transition to SLEEP after longer timeout
        if (now - lastActivityTime > SLEEP_TIMEOUT_MS) {
          LOGI_F("System: IDLE -> SLEEP (prolonged inactivity)");
          currentSystemState = SystemState::SLEEP;
          
          // Call library hook for sleep preparation
          SmartMiFanAsync_prepareForSleep(true, true);  // Close UDP, invalidate handshake
        }
      }
      break;
      
    case SystemState::SLEEP:
      if (hasActivity) {
        // Activity detected - wake up
        LOGI_F("System: SLEEP -> ACTIVE (activity detected)");
        currentSystemState = SystemState::ACTIVE;
        lastActivityTime = now;
        
        // Call library hook for wake-up
        SmartMiFanAsync_softWakeUp();
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  LOGI_F("\n=== SmartMiFanAsync System State Orchestration Example ===\n");
  
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
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Registered %zu fan(s)", count);
    LOGI(buffer);
  }
  
  // Initialize system state
  currentSystemState = SystemState::IDLE;
  lastActivityTime = millis();
  
  LOGI_F("\nSystem State Orchestration Demo:");
  LOGI_F("  - System starts in IDLE state");
  LOGI_F("  - Simulated activity will trigger ACTIVE state");
  LOGI_F("  - Prolonged inactivity will trigger SLEEP state");
  LOGI_F("  - Library hooks (softWakeUp/prepareForSleep) are called by project");
  LOGI_F("\nStarting demo...\n");
}

void loop() {
  // Step 3: Update system state (project-level logic)
  updateSystemState();
  
  // Simulate activity sources (in real project: BLE sensors, UI, etc.)
  // Simulate BLE activity every 10 seconds for first 20 seconds
  static unsigned long lastBleSim = 0;
  if (millis() - lastBleSim > 10000 && millis() < 20000) {
    lastBleSim = millis();
    simulatedBleActivity = true;
    LOGI_F("Simulated: BLE sensor connected");
    delay(100);
    simulatedBleActivity = false;
  }
  
  // Simulate UI activity at 30 seconds
  static bool uiSimulated = false;
  if (!uiSimulated && millis() > 30000 && millis() < 31000) {
    uiSimulated = true;
    simulatedUiActivity = true;
    LOGI_F("Simulated: UI interaction");
    delay(100);
    simulatedUiActivity = false;
  }
  
  // When ACTIVE, demonstrate fan control
  if (currentSystemState == SystemState::ACTIVE) {
    static unsigned long lastCommand = 0;
    if (millis() - lastCommand > 5000) {
      lastCommand = millis();
      
      // First command after wake-up ensures handshake on demand
      LOGI_F("System ACTIVE: Sending fan commands...");
      SmartMiFanAsync_handshakeAllOrchestrated();
      SmartMiFanAsync_setPowerAllOrchestrated(true);
      SmartMiFanAsync_setSpeedAllOrchestrated(50);
    }
  }
  
  // Print system state periodically
  static unsigned long lastStatePrint = 0;
  if (millis() - lastStatePrint > 5000) {
    lastStatePrint = millis();
    const char* stateStr = "UNKNOWN";
    switch (currentSystemState) {
      case SystemState::ACTIVE:
        stateStr = "ACTIVE";
        break;
      case SystemState::IDLE:
        stateStr = "IDLE";
        break;
      case SystemState::SLEEP:
        stateStr = "SLEEP";
        break;
    }
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Current System State: %s (idle for %lu ms)", 
             stateStr, millis() - lastActivityTime);
    LOGI(buffer);
  }
  
  delay(100);
}

