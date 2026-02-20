/*
 * MultipleFansFastConnect Example
 * 
 * This example demonstrates how to use Fast Connect mode to register
 * SmartMi fans directly by IP address and token, skipping the discovery process.
 * 
 * Fast Connect mode is useful when you already know the IP addresses and tokens
 * of your fans, allowing for faster initialization without network discovery.
 * 
 * This example shows:
 * 1. How to configure Fast Connect entries (IP, Token, optional DID)
 * 2. How to enable Fast Connect mode
 * 3. How to register fans directly
 * 4. How to control all registered fans
 * 
 * Note: Validation is lazy - it happens on first handshake. If a fan fails
 * validation (wrong IP/token), the handshake will return false for that fan.
 * 
 * IMPORTANT: This example does NOT manage system or fan participation states.
 * - System states (ACTIVE/IDLE/SLEEP) are not managed here
 * - Fan participation states (ACTIVE/INACTIVE/ERROR) are not managed here
 * - All fans are treated as ACTIVE by default
 * - For orchestration examples, see SystemStateOrchestration, FanParticipationToggle, or Prompt3_FullFlow
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
// Define your fans directly by IP address, token, and optional model
// When model is provided, queryInfo() is skipped → faster & more stable connection!
// Common models: "zhimi.fan.za5" (Standing Fan 3), "zhimi.fan.za4" (Standing Fan 2S)
SmartMiFanFastConnectEntry fastConnectFans[] = {
  //{"192.168.1.100", "11111111111111111111111111111111"},                    // Model queried
  //{"192.168.1.101", "22222222222222222222222222222222","dmaker.fan.p33"},    // Model provided (faster!)
  //{"192.168.1.104", "33333333333333333333333333333333", "xiaomi.fan.p76"}  
  {"192.168.1.101", "22222222222222222222222222222222"},    // Model provided (faster!)
  {"192.168.1.104", "33333333333333333333333333333333", "xiaomi.fan.p76"}     // Model provided (faster!)            
  //{"192.168.1.107", "44444444444444444444444444444444"}  
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;

enum class AppState {
  INITIALIZING,
  CONTROLLING,
  IDLE
};

AppState appState = AppState::INITIALIZING;
unsigned long lastControlUpdate = 0;
bool fansOn = false;
uint8_t currentSpeed = 30;

// Fast Connect Validation Callback
// This callback is called once after all Fast Connect fans are validated
// Results can be used as data for discovery mode
void onFastConnectValidationComplete(const SmartMiFanFastConnectResult results[], size_t count) {
  LOGI_F("\n=== Fast Connect Validation Complete ===");
  LOGI_F("Validated %zu fan(s)", count);
  
  for (size_t i = 0; i < count; ++i) {
    if (results[i].success) {
      LOGI_F("  %d.%d.%d.%d - SUCCESS (token: %s)", 
             results[i].ip[0], results[i].ip[1], results[i].ip[2], results[i].ip[3], results[i].token);
    } else {
      LOGW_F("  %d.%d.%d.%d - FAILED (token: %s)", 
             results[i].ip[0], results[i].ip[1], results[i].ip[2], results[i].ip[3], results[i].token);
    }
    
    // Results can be stored/used as discovery mode data:
    // - IP address: results[i].ip
    // - Token: results[i].token
    // - Success status: results[i].success
  }
  LOGI_F("==========================================\n");
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
  LOGI_F("WiFi connected, IP: %d.%d.%d.%d", localIp[0], localIp[1], localIp[2], localIp[3]);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  LOGI_F("\n=== SmartMiFanAsync Fast Connect Example ===\n");
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Enable Fast Connect mode (runtime)
  // Alternatively, you can enable it at compile-time by defining:
  // #define SMART_MI_FAN_FAST_CONNECT_ENABLED 1
  SmartMiFanAsync_setFastConnectEnabled(true);
  
  // Set Fast Connect configuration
  LOGI_F("Configuring Fast Connect with %zu fans...", FAST_CONNECT_FAN_COUNT);
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    appState = AppState::IDLE;
    return;
  }
  
  // Set Fast Connect validation callback (optional)
  // This callback will be called once after all fans are validated
  SmartMiFanAsync_setFastConnectValidationCallback(onFastConnectValidationComplete);
  
  // Reset discovered fans list
  SmartMiFanAsync_resetDiscoveredFans();
  
  // Register fans directly (no discovery, no validation yet - lazy validation)
  LOGI_F("Registering Fast Connect fans...");
  if (SmartMiFanAsync_registerFastConnectFans(fanUdp)) {
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    LOGI_F("Successfully registered %zu fan(s)", count);
    
    // Print registered fans (model info will be empty until first handshake)
    SmartMiFanAsync_printDiscoveredFans();
    
    appState = AppState::CONTROLLING;
  } else {
    LOGE_F("Failed to register Fast Connect fans");
    appState = AppState::IDLE;
  }
  
  // Validate all Fast Connect fans (callback will be called with results)
  // This will validate IP addresses and tokens, and populate device info
  LOGI_F("\nValidating Fast Connect fans...");
  SmartMiFanAsync_validateFastConnectFans(fanUdp);
  
  // Print updated fan info (model, firmware, hardware versions now populated)
  LOGI_F("\nUpdated fan information:");
  SmartMiFanAsync_printDiscoveredFans();
  
  lastControlUpdate = millis();
}

void loop() {
  if (appState == AppState::CONTROLLING) {
    // Control fans (example: toggle power and change speed every 10 seconds)
    if (millis() - lastControlUpdate > 10000) {
      lastControlUpdate = millis();
      
      // Toggle power
      fansOn = !fansOn;
      LOGI_F("\nSetting all fans power: %s", fansOn ? "ON" : "OFF");
      SmartMiFanAsync_setPowerAll(fansOn);
      
      if (fansOn) {
        // Change speed (random between 20-80)
        currentSpeed = 20 + (esp_random() % 61);
        LOGI_F("Setting all fans speed: %u%%", currentSpeed);
        SmartMiFanAsync_setSpeedAll(currentSpeed);
        
      }
    }
  }
  
  // Your other code can run here without blocking
  // For example:
  // - Web server handling
  // - Sensor reading
  // - Display updates
  // - User input handling
  // etc.
  
  delay(10); // Small delay to prevent watchdog issues
}

