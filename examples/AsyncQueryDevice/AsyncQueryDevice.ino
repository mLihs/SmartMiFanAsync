/*
 * AsyncQueryDevice Example
 * 
 * This example demonstrates how to query a single SmartMi fan device
 * asynchronously using its IP address and token.
 * 
 * This is useful when you already know the IP address of your fan
 * and want to verify it's a supported device and get its information.
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

// ------ Fan Configuration ------
const char *FAN_IP = "192.168.1.100";  // Replace with your fan's IP address
const char *TOKEN_HEX = "11111111111111111111111111111111"; // 32 hex characters
// --------------------------------

WiFiUDP fanUdp;
bool queryStarted = false;

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
  
  LOGI_F("\n=== SmartMiFanAsync Query Device Example ===\n");
  
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
  
  // Convert IP string to IPAddress
  IPAddress fanIp;
  if (!fanIp.fromString(FAN_IP)) {
    LOGE_F("Invalid IP address format");
    while (true) {
      delay(1000);
    }
  }
  
  // Start async query (non-blocking)
  LOGI_F("Querying device at IP: %s", FAN_IP);
  if (SmartMiFanAsync_startQueryDevice(fanUdp, fanIp, TOKEN_HEX)) {
    queryStarted = true;
    LOGI_F("Query started successfully");
  } else {
    LOGE_F("Failed to start query (another query may be in progress)");
  }
}

void loop() {
  // Update query state machine (non-blocking)
  if (queryStarted && SmartMiFanAsync_isQueryInProgress()) {
    SmartMiFanAsync_updateQueryDevice();
    
    // Print progress
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
      lastPrint = millis();
      QueryState state = SmartMiFanAsync_getQueryState();
      switch (state) {
        case QueryState::WAITING_HELLO:
          LOGD_F(".");
          break;
        case QueryState::SENDING_QUERY:
          LOGD_F("\nSending device info query...");
          break;
        case QueryState::WAITING_RESPONSE:
          LOGD_F(".");
          break;
        default:
          break;
      }
    }
  }
  
  // Check if query is complete
  if (queryStarted && SmartMiFanAsync_isQueryComplete()) {
    queryStarted = false;
    
    LOGI_F("\n=== Query Complete ===\n");
    SmartMiFanAsync_printDiscoveredFans();
    
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    
    if (count > 0) {
      LOGI_F("\nDevice information:");
      const auto &fan = fans[0];
      LOGI_F("  Model: %s", fan.model);
      LOGI_F("  IP: %d.%d.%d.%d", fan.ip[0], fan.ip[1], fan.ip[2], fan.ip[3]);
      LOGI_F("  Device ID: %lu", fan.did);
      LOGI_F("  Firmware: %s", fan.fw_ver);
      LOGI_F("  Hardware: %s", fan.hw_ver);
    }
  }
  
  // Check for errors
  if (queryStarted) {
    QueryState state = SmartMiFanAsync_getQueryState();
    if (state == QueryState::ERROR || state == QueryState::TIMEOUT) {
      queryStarted = false;
      if (state == QueryState::TIMEOUT) {
        LOGE_F("\nQuery failed: Timeout");
      } else {
        LOGE_F("\nQuery failed: Error");
      }
      LOGW_F("Make sure:");
      LOGW_F("  1. Fan is powered on and connected to WiFi");
      LOGW_F("  2. IP address is correct");
      LOGW_F("  3. Token is correct (32 hex characters)");
      LOGW_F("  4. Fan is on the same network");
    }
  }
  
  delay(10); // Small delay to prevent watchdog issues
}

