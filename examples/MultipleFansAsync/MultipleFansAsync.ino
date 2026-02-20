/*
 * MultipleFansAsync Example
 * 
 * This example demonstrates how to:
 * 1. Discover multiple SmartMi fans asynchronously
 * 2. Control all discovered fans
 * 3. Handle multiple operations without blocking
 * 
 * This is useful for applications that need to control multiple fans
 * while also performing other tasks (e.g., web server, sensor reading).
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
// Add tokens for all your fans
/*const char *TOKENS[] = {
  "44444444444444444444444444444444",
  "11111111111111111111111111111111",
  "22222222222222222222222222222222",
  "33333333333333333333333333333333"
  // Add more tokens as needed
};*/

const char *TOKENS[] = {
  "22222222222222222222222222222222",
  "33333333333333333333333333333333"
  // Add more tokens as needed
};


const size_t TOKEN_COUNT = sizeof(TOKENS) / sizeof(TOKENS[0]);
// --------------------------------

WiFiUDP fanUdp;

enum class AppState {
  DISCOVERING,
  CONTROLLING,
  IDLE
};

AppState appState = AppState::DISCOVERING;
unsigned long lastControlUpdate = 0;
bool fansOn = false;
uint8_t currentSpeed = 30;

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
  
  LOGI_F("\n=== SmartMiFanAsync Multiple Fans Example ===\n");
  
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
  
  // Start async discovery
  LOGI_F("Starting async discovery...");
  if (SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000)) {
    appState = AppState::DISCOVERING;
    LOGI_F("Discovery started (5 second timeout)");
  } else {
    LOGE_F("Failed to start discovery");
    appState = AppState::IDLE;
  }
}

void loop() {
  // Handle discovery state machine
  if (appState == AppState::DISCOVERING) {
    if (SmartMiFanAsync_isDiscoveryInProgress()) {
      SmartMiFanAsync_updateDiscovery();
      
      // Print progress every second
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 1000) {
        lastPrint = millis();
        DiscoveryState state = SmartMiFanAsync_getDiscoveryState();
        switch (state) {
          case DiscoveryState::SENDING_HELLO:
            LOGD_F("Discovery: Sending hello...");
            break;
          case DiscoveryState::COLLECTING_CANDIDATES:
            LOGD_F("Discovery: Collecting...");
            break;
          case DiscoveryState::QUERYING_DEVICES:
            LOGD_F("Discovery: Querying...");
            break;
          default:
            break;
        }
      }
    } else if (SmartMiFanAsync_isDiscoveryComplete()) {
      // Discovery finished
      LOGI_F("\n=== Discovery Complete ===\n");
      SmartMiFanAsync_printDiscoveredFans();
      
      size_t count = 0;
      const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
      
      if (count == 0) {
        LOGW_F("No fans found. Retrying in 10 seconds...");
        delay(10000);
        SmartMiFanAsync_resetDiscoveredFans();
        if (SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000)) {
          appState = AppState::DISCOVERING;
        }
      } else {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Found %zu fan(s)", count);
        LOGI(buffer);
        
        // Perform handshake with all fans
        LOGI_F("\nPerforming handshake with all fans...");
        if (SmartMiFanAsync_handshakeAll()) {
          LOGI_F("Handshake successful");
          appState = AppState::CONTROLLING;
          lastControlUpdate = millis();
        } else {
          LOGE_F("Handshake failed");
          appState = AppState::IDLE;
        }
      }
    } else {
      // Discovery failed or timed out
      LOGE_F("\nDiscovery failed or timed out");
      appState = AppState::IDLE;
    }
  }
  
  // Control fans (example: toggle power and change speed every 10 seconds)
  if (appState == AppState::CONTROLLING) {
    if (millis() - lastControlUpdate > 10000) {
      lastControlUpdate = millis();
      
      // Toggle power
      fansOn = !fansOn;
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "\nSetting all fans power: %s", fansOn ? "ON" : "OFF");
      LOGI(buffer);
      SmartMiFanAsync_setPowerAll(fansOn);
      
      if (fansOn) {
        // Change speed (random between 20-80)
        currentSpeed = 20 + (esp_random() % 61);
        snprintf(buffer, sizeof(buffer), "Setting all fans speed: %u", currentSpeed);
        LOGI(buffer);
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

