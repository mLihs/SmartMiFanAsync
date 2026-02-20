/*
 * ErrorAndHealthCallbackExample
 * 
 * This example demonstrates Step 2 features:
 * - Error classification and callbacks
 * - Per-fan readiness state tracking
 * - Health check API
 * - Transport/sleep hooks
 * 
 * This example shows:
 * 1. How to register an error callback that logs errors
 * 2. How to read per-fan readiness state
 * 3. How to trigger typical error cases (timeout, wrong token, etc.)
 * 4. How to use health check API
 * 5. How to use transport/sleep hooks
 * 
 * Note: This example does NOT implement:
 * - Retries or recovery logic
 * - Discovery fallback
 * - UI or BLE logic
 * 
 * Error callbacks are observational only and do NOT affect control flow.
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
// Define your fans directly by IP address and token
// For demonstration, we'll use some example IPs - replace with your actual fans
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.100", "11111111111111111111111111111111"},           // Fan 1
  {"192.168.1.101", "22222222222222222222222222222222"},          // Fan 2
  {"192.168.1.255", "00000000000000000000000000000000"},          // Fan 3: Invalid IP (will timeout)
  {"192.168.1.104", "wrongtoken1234567890123456789012"}           // Fan 4: Wrong token (will fail decrypt)
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------

WiFiUDP fanUdp;

enum class AppState {
  INITIALIZING,
  TESTING_ERRORS,
  TESTING_HEALTH,
  TESTING_SLEEP,
  IDLE
};

AppState appState = AppState::INITIALIZING;
unsigned long lastTestUpdate = 0;
uint8_t testPhase = 0;

// Step 2: Error Callback
// This callback is called when miio operations encounter errors
// It is observational only - it does NOT affect control flow
void onFanError(const FanErrorInfo& info) {
  LOGI_F("\n=== Fan Error Callback ===");
  LOGI_F("Fan Index: %u", info.fanIndex);
  LOGI_F("IP: %d.%d.%d.%d", info.ip[0], info.ip[1], info.ip[2], info.ip[3]);
  
  // Operation type
  const char* opStr = "Unknown";
  switch (info.operation) {
    case FanOp::Handshake:
      opStr = "Handshake";
      break;
    case FanOp::SendCommand:
      opStr = "SendCommand";
      break;
    case FanOp::ReceiveResponse:
      opStr = "ReceiveResponse";
      break;
    case FanOp::HealthCheck:
      opStr = "HealthCheck";
      break;
  }
  LOGI_F("Operation: %s", opStr);
  
  // Error type
  const char* errStr = "OK";
  switch (info.error) {
    case MiioErr::OK:
      errStr = "OK";
      break;
    case MiioErr::TIMEOUT:
      errStr = "TIMEOUT";
      break;
    case MiioErr::WRONG_SOURCE_IP:
      errStr = "WRONG_SOURCE_IP";
      break;
    case MiioErr::DECRYPT_FAIL:
      errStr = "DECRYPT_FAIL";
      break;
    case MiioErr::INVALID_RESPONSE:
      errStr = "INVALID_RESPONSE";
      break;
  }
  LOGI_F("Error: %s", errStr);
  LOGI_F("Elapsed: %lu ms", info.elapsedMs);
  LOGI_F("Handshake Invalidated: %s", info.handshakeInvalidated ? "true" : "false");
  LOGI_F("===========================\n");
  
  // Note: This callback does NOT:
  // - Trigger retries
  // - Start discovery
  // - Modify Smart Connect state
  // - Change global connection modes
  // All recovery decisions belong to the project logic
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
  
  LOGI_F("\n=== SmartMiFanAsync Error and Health Callback Example ===\n");
  
  // Connect to WiFi
  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    LOGE_F("WiFi not connected, aborting");
    while (true) {
      delay(1000);
    }
  }
  
  // Step 2: Register error callback
  LOGI_F("Registering error callback...");
  SmartMiFanAsync_setErrorCallback(onFanError);
  
  // Enable Fast Connect mode
  SmartMiFanAsync_setFastConnectEnabled(true);
  
  // Set Fast Connect configuration
  LOGI_F("Configuring Fast Connect with %zu fans...", FAST_CONNECT_FAN_COUNT);
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    appState = AppState::IDLE;
    return;
  }
  
  // Reset discovered fans list
  SmartMiFanAsync_resetDiscoveredFans();
  
  // Register fans directly
  LOGI_F("Registering Fast Connect fans...");
  if (SmartMiFanAsync_registerFastConnectFans(fanUdp)) {
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    LOGI_F("Successfully registered %zu fan(s)", count);
    
    // Print registered fans
    SmartMiFanAsync_printDiscoveredFans();
    
    appState = AppState::TESTING_ERRORS;
  } else {
    LOGE_F("Failed to register Fast Connect fans");
    appState = AppState::IDLE;
  }
  
  lastTestUpdate = millis();
}

void loop() {
  if (appState == AppState::TESTING_ERRORS) {
    // Test error cases by attempting handshakes
    // Some fans will timeout (invalid IP) or fail (wrong token)
    if (millis() - lastTestUpdate > 5000) {
      lastTestUpdate = millis();
      
      LOGI_F("\n=== Testing Error Cases ===");
      
      // Attempt handshake with all fans
      // This will trigger error callbacks for fans that fail
      SmartMiFanAsync_handshakeAll();
      
      // Read and display per-fan readiness state
      LOGI_F("\nPer-fan readiness state:");
      size_t count = 0;
      const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
      for (size_t i = 0; i < count; ++i) {
        LOGI_F("  Fan %zu (%d.%d.%d.%d):", 
               i, fans[i].ip[0], fans[i].ip[1], fans[i].ip[2], fans[i].ip[3]);
        
        bool ready = SmartMiFanAsync_isFanReady(static_cast<uint8_t>(i));
        MiioErr lastError = SmartMiFanAsync_getFanLastError(static_cast<uint8_t>(i));
        
        LOGI_F("    Ready: %s", ready ? "true" : "false");
        
        const char* errStr = "OK";
        switch (lastError) {
          case MiioErr::OK:
            errStr = "OK";
            break;
          case MiioErr::TIMEOUT:
            errStr = "TIMEOUT";
            break;
          case MiioErr::WRONG_SOURCE_IP:
            errStr = "WRONG_SOURCE_IP";
            break;
          case MiioErr::DECRYPT_FAIL:
            errStr = "DECRYPT_FAIL";
            break;
          case MiioErr::INVALID_RESPONSE:
            errStr = "INVALID_RESPONSE";
            break;
        }
        LOGI_F("    Last Error: %s", errStr);
      }
      
      appState = AppState::TESTING_HEALTH;
    }
  } else if (appState == AppState::TESTING_HEALTH) {
    // Test health check API
    if (millis() - lastTestUpdate > 5000) {
      lastTestUpdate = millis();
      
      LOGI_F("\n=== Testing Health Check API ===");
      
      // Health check all fans
      LOGI_F("Performing health check on all fans...");
      bool allHealthy = SmartMiFanAsync_healthCheckAll(2000);
      
      LOGI_F("All fans healthy: %s", allHealthy ? "true" : "false");
      
      // Health check individual fan
      LOGI_F("Performing health check on fan 0...");
      bool fan0Healthy = SmartMiFanAsync_healthCheck(0, 2000);
      LOGI_F("Fan 0 healthy: %s", fan0Healthy ? "true" : "false");
      
      appState = AppState::TESTING_SLEEP;
    }
  } else if (appState == AppState::TESTING_SLEEP) {
    // Test transport/sleep hooks
    if (millis() - lastTestUpdate > 5000) {
      lastTestUpdate = millis();
      
      LOGI_F("\n=== Testing Transport/Sleep Hooks ===");
      
      // Prepare for sleep (invalidate handshake cache)
      LOGI_F("Preparing for sleep (invalidate handshake cache)...");
      SmartMiFanAsync_prepareForSleep(false, true);  // Don't close UDP, but invalidate handshake
      
      // Check readiness after sleep preparation
      LOGI_F("Fan readiness after sleep preparation:");
      size_t count = 0;
      const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
      for (size_t i = 0; i < count; ++i) {
        bool ready = SmartMiFanAsync_isFanReady(static_cast<uint8_t>(i));
        LOGI_F("  Fan %zu: %s", i, ready ? "ready" : "not ready");
      }
      
      // Soft wake up
      LOGI_F("Soft wake up...");
      SmartMiFanAsync_softWakeUp();
      
      // Attempt handshake after wake up
      LOGI_F("Attempting handshake after wake up...");
      SmartMiFanAsync_handshakeAll();
      
      appState = AppState::IDLE;
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

