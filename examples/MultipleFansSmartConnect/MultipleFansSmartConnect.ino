/*
 * MultipleFansSmartConnect Example
 * 
 * This example demonstrates Smart Connect mode, which combines:
 * 1. Fast Connect (direct IP connection) - tried first
 * 2. Async Discovery (network discovery) - used for fans that failed Fast Connect
 * 
 * Smart Connect is ideal when you know some fan IPs but want automatic
 * discovery as a fallback for fans that couldn't be reached directly.
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
// Some fans may have wrong IPs or be unreachable - Smart Connect will
// automatically use Discovery for any fans that fail Fast Connect
/*
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.100", "11111111111111111111111111111111"},
  {"192.168.1.101", "22222222222222222222222222222222"},  // May fail, will use Discovery
  {"192.168.1.102", "33333333333333333333333333333333"},
  {"192.168.1.103", "44444444444444444444444444444444"}
};*/

SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.104", "33333333333333333333333333333333"}, // Model: dmaker.fan.p33
  {"192.168.1.106", "22222222222222222222222222222222"} // not Existing IP and Token
};

const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;

enum class AppState {
  CONNECTING,
  CONTROLLING,
  IDLE
};

AppState appState = AppState::CONNECTING;
unsigned long lastControlUpdate = 0;
bool fansOn = false;
uint8_t currentSpeed = 30;

// Control cadence and target range
const uint32_t CONTROL_INTERVAL_MS = 3000;
const uint8_t BASE_SPEED = 90;
const uint8_t SPEED_JITTER = 5; // 90% +/- 5%

const char* miioErrToStr(MiioErr err) {
  switch (err) {
    case MiioErr::OK: return "OK";
    case MiioErr::TIMEOUT: return "TIMEOUT";
    case MiioErr::WRONG_SOURCE_IP: return "WRONG_SOURCE_IP";
    case MiioErr::DECRYPT_FAIL: return "DECRYPT_FAIL";
    case MiioErr::INVALID_RESPONSE: return "INVALID_RESPONSE";
    default: return "UNKNOWN";
  }
}

void logHealthCheck() {
  // Health check all fans (blocking handshake per fan)
  bool ok = SmartMiFanAsync_healthCheckAll(1500);
  size_t count = 0;
  SmartMiFanAsync_getDiscoveredFans(count);
  LOGI_F("[HealthCheck] t=%lums ok=%s fans=%u", (unsigned long)millis(), ok ? "true" : "false", (unsigned)count);
  for (size_t i = 0; i < count; i++) {
    bool ready = SmartMiFanAsync_isFanReady(static_cast<uint8_t>(i));
    MiioErr err = SmartMiFanAsync_getFanLastError(static_cast<uint8_t>(i));
    FanParticipationState ps = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    LOGI_F("[HealthCheck] fan[%u] ready=%s err=%s state=%d",
           (unsigned)i, ready ? "true" : "false", miioErrToStr(err), (int)ps);
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
  
  LOGI_F("\n=== SmartMiFanAsync Smart Connect Example ===\n");
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Configure Fast Connect (required for Smart Connect)
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Configuring Fast Connect with %zu fans...", FAST_CONNECT_FAN_COUNT);
    LOGI(buffer);
  }
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    appState = AppState::IDLE;
    return;
  }
  
  // Start Smart Connect (tries Fast Connect first, then Discovery for failures)
  LOGI_F("Starting Smart Connect...");
  LOGI_F("  Phase 1: Validating Fast Connect fans...");
  LOGI_F("  Phase 2: Discovering failed fans (if any)...");
  
  if (SmartMiFanAsync_startSmartConnect(fanUdp, 5000)) {
    LOGI_F("Smart Connect started");
    appState = AppState::CONNECTING;
  } else {
    LOGE_F("Failed to start Smart Connect");
    appState = AppState::IDLE;
  }
}

void loop() {
  if (appState == AppState::CONNECTING) {
    if (SmartMiFanAsync_isSmartConnectInProgress()) {
      SmartMiFanAsync_updateSmartConnect();
      
      // Print progress every second
      static unsigned long lastPrint = 0;
      if (millis() - lastPrint > 1000) {
        lastPrint = millis();
        SmartConnectState state = SmartMiFanAsync_getSmartConnectState();
        switch (state) {
          case SmartConnectState::VALIDATING_FAST_CONNECT:
            LOGD_F("Smart Connect: Validating Fast Connect...");
            break;
          case SmartConnectState::STARTING_DISCOVERY:
            LOGI_F("Smart Connect: Starting Discovery for failed fans...");
            break;
          case SmartConnectState::DISCOVERING:
            LOGD_F("Smart Connect: Discovering...");
            break;
          default:
            break;
        }
      }
    } else if (SmartMiFanAsync_isSmartConnectComplete()) {
      // Smart Connect finished
      LOGI_F("\n=== Smart Connect Complete ===\n");
      SmartMiFanAsync_printDiscoveredFans();
      
      // Control all discovered fans
      size_t count = 0;
      const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
      LOGI_F("Found %zu fan(s) total", count);
      
      if (count > 0) {
        LOGI_F("Performing handshake with all fans...");
        if (SmartMiFanAsync_handshakeAll()) {
          LOGI_F("Handshake successful");
          appState = AppState::CONTROLLING;
          lastControlUpdate = millis();
        // Ensure fans are ON before starting speed updates
        fansOn = true;
        SmartMiFanAsync_setPowerAll(true);
        } else {
          LOGW_F("Some fans failed handshake");
          appState = AppState::IDLE;
        }
      } else {
        LOGW_F("No fans found");
        appState = AppState::IDLE;
      }
    } else {
      // Smart Connect failed or error
      LOGE_F("\nSmart Connect failed or error");
      appState = AppState::IDLE;
    }
  }
  
  // Control fans: every 3 seconds set random speed near 90% and run health check
  if (appState == AppState::CONTROLLING) {
    if (millis() - lastControlUpdate > CONTROL_INTERVAL_MS) {
      lastControlUpdate = millis();

      // Random speed around 90% (grenzer Bereich)
      int jitter = (int)(esp_random() % (SPEED_JITTER * 2 + 1)) - (int)SPEED_JITTER;
      int speed = (int)BASE_SPEED + jitter;
      if (speed < 1) speed = 1;
      if (speed > 100) speed = 100;
      currentSpeed = static_cast<uint8_t>(speed);

      LOGI_F("\nSetting all fans speed: %u%% (t=%lums)", currentSpeed, (unsigned long)millis());
      SmartMiFanAsync_setSpeedAll(currentSpeed);

      // Health check with debug log
      logHealthCheck();
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

