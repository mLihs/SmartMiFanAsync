/*
 * BasicAsyncDiscovery Example
 * 
 * This example demonstrates how to use the SmartMiFanAsync library to
 * discover SmartMi fans asynchronously (non-blocking).
 * 
 * The discovery process runs in the background, allowing your main loop
 * to continue executing other tasks.
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

// ------ Fan Tokens ------
// Add your fan tokens here (32 hex characters each)
const char *TOKENS[] = {
  "44444444444444444444444444444444"
  // Add more tokens as needed
};
const size_t TOKEN_COUNT = sizeof(TOKENS) / sizeof(TOKENS[0]);
// --------------------------------

WiFiUDP fanUdp;
bool discoveryStarted = false;

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
  
  LOGI_F("\n=== SmartMiFanAsync Basic Discovery Example ===\n");
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Reset discovered fans list
  SmartMiFanAsync_resetDiscoveredFans();
  
  // Start async discovery (non-blocking)
  LOGI_F("Starting async discovery...");
  if (SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000)) {
    discoveryStarted = true;
    LOGI_F("Discovery started successfully");
    LOGI_F("Discovery will run for 5 seconds");
  } else {
    LOGE_F("Failed to start discovery");
  }
}

void loop() {
  // Update discovery state machine (non-blocking)
  if (discoveryStarted && SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
    
    // Print progress every second
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();
      DiscoveryState state = SmartMiFanAsync_getDiscoveryState();
      switch (state) {
        case DiscoveryState::SENDING_HELLO:
          LOGD_F("Discovery state: Sending hello packets...");
          break;
        case DiscoveryState::COLLECTING_CANDIDATES:
          LOGD_F("Discovery state: Collecting candidates...");
          break;
        case DiscoveryState::QUERYING_DEVICES:
          LOGD_F("Discovery state: Querying devices...");
          break;
        default:
          break;
      }
    }
  }
  
  // Check if discovery is complete
  if (discoveryStarted && SmartMiFanAsync_isDiscoveryComplete()) {
    discoveryStarted = false;
    
    LOGI_F("\n=== Discovery Complete ===\n");
    SmartMiFanAsync_printDiscoveredFans();
    
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    
    if (count == 0) {
      LOGW_F("\nNo SmartMi fans found.");
      LOGW_F("Make sure:");
      LOGW_F("  1. Fans are powered on and connected to WiFi");
      LOGW_F("  2. Tokens are correct (32 hex characters)");
      LOGW_F("  3. Fans are on the same network");
    } else {
      LOGI_F("\nFound %zu fan(s)", count);
    }
  }
  
  // Your other code can run here without blocking
  // For example, you could:
  // - Read sensors
  // - Update displays
  // - Handle user input
  // - etc.
  
  delay(10); // Small delay to prevent watchdog issues
}

