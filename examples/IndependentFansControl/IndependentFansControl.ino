/*
 * IndependentFansControl Example
 * 
 * This example demonstrates how to:
 * 1. Discover multiple SmartMi fans asynchronously
 * 2. Control each fan independently with:
 *    - Individual pause times (different for each fan)
 *    - Random speeds for each fan
 * 3. Handle multiple operations without blocking
 * 
 * Each fan operates on its own schedule:
 * - Fan 0: Updates every 5 seconds
 * - Fan 1: Updates every 8 seconds
 * - Fan 2: Updates every 12 seconds
 * - etc. (configurable)
 * 
 * Each fan gets a random speed between 20-80% when turned on.
 * 
 * This is useful for applications that need to control multiple fans
 * with different behaviors while also performing other tasks.
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
const char *TOKENS[] = {
  "22222222222222222222222222222222",
  "33333333333333333333333333333333"
  // Add more tokens as needed
};

const size_t TOKEN_COUNT = sizeof(TOKENS) / sizeof(TOKENS[0]);
// --------------------------------

// ------ Individual Fan Control Configuration ------
// Pause times for each fan (in milliseconds)
// You can customize these values for each fan
const unsigned long FAN_PAUSE_TIMES[] = {
  5000,   // Fan 0: 5 seconds
  8000,   // Fan 1: 8 seconds
  12000,  // Fan 2: 12 seconds
  15000,  // Fan 3: 15 seconds
  10000,  // Fan 4: 10 seconds
  7000,   // Fan 5: 7 seconds
  9000,   // Fan 6: 9 seconds
  11000   // Fan 7: 11 seconds
  // Add more as needed (max 16 fans)
};

// Speed range for random speed generation
const uint8_t MIN_SPEED = 20;
const uint8_t MAX_SPEED = 80;
// --------------------------------

WiFiUDP fanUdp;

enum class AppState {
  DISCOVERING,
  CONTROLLING,
  IDLE
};

AppState appState = AppState::DISCOVERING;

// Individual fan state structure
struct FanControlState {
  unsigned long lastUpdateTime;
  unsigned long pauseTime;
  bool powerOn;
  uint8_t currentSpeed;
  bool initialized;
};

// Maximum number of fans we can control
const size_t MAX_FANS = 16;
FanControlState fanStates[MAX_FANS];

// Helper function to get pause time for a fan (with fallback)
unsigned long getFanPauseTime(size_t fanIndex) {
  if (fanIndex < sizeof(FAN_PAUSE_TIMES) / sizeof(FAN_PAUSE_TIMES[0])) {
    return FAN_PAUSE_TIMES[fanIndex];
  }
  // Default pause time if not specified
  return 10000; // 10 seconds
}

// Helper function to generate random speed
uint8_t generateRandomSpeed() {
  return MIN_SPEED + (esp_random() % (MAX_SPEED - MIN_SPEED + 1));
}

// Initialize fan state
void initializeFanState(size_t fanIndex) {
  if (fanIndex >= MAX_FANS) return;
  
  fanStates[fanIndex].lastUpdateTime = millis();
  fanStates[fanIndex].pauseTime = getFanPauseTime(fanIndex);
  fanStates[fanIndex].powerOn = false;
  fanStates[fanIndex].currentSpeed = generateRandomSpeed();
  fanStates[fanIndex].initialized = true;
  
  char buffer[128];
  snprintf(buffer, sizeof(buffer), 
           "Fan %zu initialized: pause=%lums, initial_speed=%u", 
           fanIndex, fanStates[fanIndex].pauseTime, fanStates[fanIndex].currentSpeed);
  LOGI(buffer);
}

// Helper function to prepare fan context for individual control
bool prepareFanContext(const SmartMiFanDiscoveredDevice &fan) {
  SmartMiFanAsync.attachUdp(fanUdp);
  if (!SmartMiFanAsync.setTokenFromHex(fan.token)) {
    return false;
  }
  SmartMiFanAsync.setFanAddress(fan.ip);
  SmartMiFanAsync.setModel(fan.model);
  return true;
}

// Control individual fan
void controlFan(size_t fanIndex) {
  if (fanIndex >= MAX_FANS || !fanStates[fanIndex].initialized) return;
  
  size_t fanCount = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
  
  if (fanIndex >= fanCount) return;
  
  const auto &fan = fans[fanIndex];
  
  // Check if it's time to update this fan
  unsigned long now = millis();
  if (now - fanStates[fanIndex].lastUpdateTime < fanStates[fanIndex].pauseTime) {
    return; // Not time yet
  }
  
  // Update this fan
  fanStates[fanIndex].lastUpdateTime = now;
  
  // Toggle power
  fanStates[fanIndex].powerOn = !fanStates[fanIndex].powerOn;
  
  // Generate new random speed
  fanStates[fanIndex].currentSpeed = generateRandomSpeed();
  
  // Prepare fan context
  if (!prepareFanContext(fan)) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Failed to prepare context for fan %zu", fanIndex);
    LOGW(buffer);
    return;
  }
  
  // Set power
  char buffer[128];
  snprintf(buffer, sizeof(buffer), 
           "Fan %zu (%d.%d.%d.%d): Power=%s, Speed=%u%%", 
           fanIndex, fan.ip[0], fan.ip[1], fan.ip[2], fan.ip[3],
           fanStates[fanIndex].powerOn ? "ON" : "OFF", 
           fanStates[fanIndex].currentSpeed);
  LOGI(buffer);
  
  SmartMiFanAsync.setPower(fanStates[fanIndex].powerOn);
  
  // Set speed only if power is on
  if (fanStates[fanIndex].powerOn) {
    SmartMiFanAsync.setSpeed(fanStates[fanIndex].currentSpeed);
  }
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
  
  LOGI_F("\n=== SmartMiFanAsync Independent Fans Control Example ===\n");
  
  // Initialize all fan states
  for (size_t i = 0; i < MAX_FANS; i++) {
    fanStates[i].initialized = false;
  }
  
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
          
          // Initialize states for all discovered fans
          LOGI_F("\nInitializing individual fan control states...");
          for (size_t i = 0; i < count && i < MAX_FANS; i++) {
            initializeFanState(i);
          }
          
          appState = AppState::CONTROLLING;
          LOGI_F("\n=== Starting Independent Fan Control ===\n");
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
  
  // Control each fan independently
  if (appState == AppState::CONTROLLING) {
    size_t fanCount = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
    
    // Check and update each fan independently
    for (size_t i = 0; i < fanCount && i < MAX_FANS; i++) {
      controlFan(i);
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
