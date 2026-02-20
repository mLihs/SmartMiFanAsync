/**
 * @file HeapFragmentationTest.ino
 * @brief Example to evaluate heap fragmentation and usage caused by SmartMiFanAsync
 * 
 * This example is based on MultipleFansSmartConnect and adds heap monitoring
 * to measure fragmentation and usage at different stages:
 * - Start (before initialization)
 * - After WiFi connection
 * - After Fast Connect configuration
 * - After Smart Connect completion
 * - After Handshake
 * - During operation (continuous monitoring)
 * 
 * It prints heap statistics in the format:
 * [Heap] TICK 07:26:02: free=110136, largest=57332, frag=47.9%, drift=70096, minFree=60324
 * 
 * Hardware: ESP32
 * 
 * Required libraries:
 * - SmartMiFanAsync
 * - WiFi (ESP32 Arduino Core)
 * - WiFiUDP (ESP32 Arduino Core)
 * 
 * Usage:
 * 1. Update WIFI_SSID and WIFI_PASS below
 * 2. Update fastConnectFans array with your fan configurations
 * 3. Upload to ESP32
 * 4. Open Serial Monitor at 115200 baud
 * 5. Observe heap statistics printed every 30 seconds
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
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.104", "33333333333333333333333333333333", "xiaomi.fan.p76"},
  {"192.168.1.106", "22222222222222222222222222222222"}, // Model: dmaker.fan.p33
  // Add more fans as needed
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

// Heap monitoring configuration
const uint32_t HEAP_TICK_INTERVAL_MS = 30000;  // Print every 30 seconds

// Baseline heap (captured at start)
uint32_t baselineFree = 0;
uint32_t baselineLargest = 0;

// Heap snapshots at different stages
uint32_t afterWifiFree = 0;
uint32_t afterWifiLargest = 0;

uint32_t afterFastConnectConfigFree = 0;
uint32_t afterFastConnectConfigLargest = 0;

uint32_t afterSmartConnectFree = 0;
uint32_t afterSmartConnectLargest = 0;

uint32_t afterHandshakeFree = 0;
uint32_t afterHandshakeLargest = 0;

// State tracking
bool smartConnectComplete = false;
bool handshakeComplete = false;

// Helper function to format uptime as HH:MM:SS
void formatUptime(uint32_t uptimeMs, char* buffer, size_t bufferSize) {
  uint32_t totalSeconds = uptimeMs / 1000;
  uint32_t hours = totalSeconds / 3600;
  uint32_t minutes = (totalSeconds % 3600) / 60;
  uint32_t seconds = totalSeconds % 60;
  
  snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu", hours, minutes, seconds);
}

// Helper function to calculate fragmentation percentage
float calcFragmentation(uint32_t largest, uint32_t free) {
  if (free == 0) return 0.0f;
  return (1.0f - ((float)largest / (float)free)) * 100.0f;
}

// Print heap statistics in the requested format
void printHeapTick(const char* phase = nullptr) {
  uint32_t now = millis();
  uint32_t free = ESP.getFreeHeap();
  uint32_t largest = ESP.getMaxAllocHeap();
  uint32_t minFree = ESP.getMinFreeHeap();
  
  // Calculate fragmentation
  float frag = calcFragmentation(largest, free);
  
  // Calculate drift from baseline
  int32_t drift = baselineFree > 0 ? (int32_t)(baselineFree - free) : 0;
  
  // Format uptime
  char uptimeBuf[12];
  formatUptime(now, uptimeBuf, sizeof(uptimeBuf));
  
  // Print in the exact format requested
  Serial.printf("[Heap] TICK %s: free=%u, largest=%u, frag=%.1f%%, drift=%d, minFree=%u",
                uptimeBuf, free, largest, frag, drift, minFree);
  
  // Add phase indicator if provided
  if (phase) {
    Serial.printf(" [%s]", phase);
  }
  Serial.println();
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
  
  // Capture heap after WiFi connection
  afterWifiFree = ESP.getFreeHeap();
  afterWifiLargest = ESP.getMaxAllocHeap();
  int32_t wifiCost = (int32_t)(baselineFree - afterWifiFree);
  float wifiFrag = calcFragmentation(afterWifiLargest, afterWifiFree);
  
  Serial.println("");
  Serial.printf("[Heap] AFTER_WIFI: free=%u, largest=%u, frag=%.1f%%, cost=%d\n",
                afterWifiFree, afterWifiLargest, wifiFrag, wifiCost);
  Serial.println("");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
  Serial.println("");
  Serial.println("========================================");
  Serial.println("SmartMiFanAsync Heap Fragmentation Test");
  Serial.println("========================================");
  Serial.println("");
  
  // Capture baseline heap BEFORE any initialization
  baselineFree = ESP.getFreeHeap();
  baselineLargest = ESP.getMaxAllocHeap();
  
  Serial.printf("[Heap] BASELINE (before initialization): free=%u, largest=%u\n", 
                baselineFree, baselineLargest);
  Serial.println("");
  
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
  
  // Capture heap before Fast Connect configuration
  uint32_t beforeFastConnectFree = ESP.getFreeHeap();
  uint32_t beforeFastConnectLargest = ESP.getMaxAllocHeap();
  
  if (!SmartMiFanAsync_setFastConnectConfig(fastConnectFans, FAST_CONNECT_FAN_COUNT)) {
    LOGE_F("Failed to set Fast Connect configuration");
    appState = AppState::IDLE;
    return;
  }
  
  // Capture heap after Fast Connect configuration
  afterFastConnectConfigFree = ESP.getFreeHeap();
  afterFastConnectConfigLargest = ESP.getMaxAllocHeap();
  int32_t fastConnectCost = (int32_t)(beforeFastConnectFree - afterFastConnectConfigFree);
  float fastConnectFrag = calcFragmentation(afterFastConnectConfigLargest, afterFastConnectConfigFree);
  
  Serial.println("");
  Serial.printf("[Heap] AFTER_FAST_CONNECT_CONFIG: free=%u, largest=%u, frag=%.1f%%, cost=%d\n",
                afterFastConnectConfigFree, afterFastConnectConfigLargest, fastConnectFrag, fastConnectCost);
  Serial.println("");
  
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
  
  Serial.println("");
  Serial.println("Starting continuous heap monitoring...");
  Serial.println("Heap statistics will be printed every 30 seconds");
  Serial.println("");
  Serial.println("Format: [Heap] TICK HH:MM:SS: free=..., largest=..., frag=...%, drift=..., minFree=... [PHASE]");
  Serial.println("");
  Serial.println("Phases:");
  Serial.println("  [WiFi] = WiFi connected, before Fast Connect config");
  Serial.println("  [FastConnect] = Fast Connect configured, Smart Connect in progress");
  Serial.println("  [SmartConnect] = Smart Connect complete, before Handshake");
  Serial.println("  [Handshake] = Handshake complete, fans ready");
  Serial.println("  [Operating] = Fans in operation");
  Serial.println("");
  
  // Print first tick immediately
  printHeapTick("FastConnect");
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
      if (!smartConnectComplete) {
        smartConnectComplete = true;
        
        LOGI_F("\n=== Smart Connect Complete ===\n");
        SmartMiFanAsync_printDiscoveredFans();
        
        // Capture heap after Smart Connect
        afterSmartConnectFree = ESP.getFreeHeap();
        afterSmartConnectLargest = ESP.getMaxAllocHeap();
        int32_t smartConnectCost = (int32_t)(afterFastConnectConfigFree - afterSmartConnectFree);
        float smartConnectFrag = calcFragmentation(afterSmartConnectLargest, afterSmartConnectFree);
        
        Serial.println("");
        Serial.printf("[Heap] AFTER_SMART_CONNECT: free=%u, largest=%u, frag=%.1f%%, cost=%d\n",
                      afterSmartConnectFree, afterSmartConnectLargest, smartConnectFrag, smartConnectCost);
        Serial.println("");
        
        // Control all discovered fans
        size_t count = 0;
        const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
        LOGI_F("Found %zu fan(s) total", count);
        
        if (count > 0) {
          LOGI_F("Performing handshake with all fans...");
          
          // Capture heap before handshake
          uint32_t beforeHandshakeFree = ESP.getFreeHeap();
          uint32_t beforeHandshakeLargest = ESP.getMaxAllocHeap();
          
          if (SmartMiFanAsync_handshakeAll()) {
            LOGI_F("Handshake successful");
            
            // Capture heap after handshake
            afterHandshakeFree = ESP.getFreeHeap();
            afterHandshakeLargest = ESP.getMaxAllocHeap();
            int32_t handshakeCost = (int32_t)(beforeHandshakeFree - afterHandshakeFree);
            float handshakeFrag = calcFragmentation(afterHandshakeLargest, afterHandshakeFree);
            
            Serial.println("");
            Serial.printf("[Heap] AFTER_HANDSHAKE: free=%u, largest=%u, frag=%.1f%%, cost=%d\n",
                          afterHandshakeFree, afterHandshakeLargest, handshakeFrag, handshakeCost);
            Serial.println("");
            
            // Calculate total cost from Smart Connect
            int32_t totalCostFromSmartConnect = (int32_t)(afterSmartConnectFree - afterHandshakeFree);
            Serial.printf("[Heap] HANDSHAKE_OVERHEAD (vs Smart Connect): cost=%d bytes\n",
                          totalCostFromSmartConnect);
            Serial.println("");
            
            // Print summary of all costs
            Serial.println("========================================");
            Serial.println("Heap Cost Summary:");
            Serial.println("========================================");
            int32_t wifiCost = (int32_t)(baselineFree - afterWifiFree);
            int32_t fastConnectCost = (int32_t)(afterWifiFree - afterFastConnectConfigFree);
            int32_t smartConnectCost = (int32_t)(afterFastConnectConfigFree - afterSmartConnectFree);
            handshakeCost = (int32_t)(afterSmartConnectFree - afterHandshakeFree);
            int32_t totalCost = (int32_t)(baselineFree - afterHandshakeFree);
            
            Serial.printf("WiFi:           %+6d bytes\n", wifiCost);
            Serial.printf("Fast Connect:   %+6d bytes\n", fastConnectCost);
            Serial.printf("Smart Connect:  %+6d bytes\n", smartConnectCost);
            Serial.printf("Handshake:      %+6d bytes\n", handshakeCost);
            Serial.printf("----------------------------------------\n");
            Serial.printf("TOTAL:          %+6d bytes\n", totalCost);
            Serial.println("========================================");
            Serial.println("");
            
            appState = AppState::CONTROLLING;
            lastControlUpdate = millis();
            handshakeComplete = true;
          } else {
            LOGW_F("Some fans failed handshake");
            appState = AppState::IDLE;
          }
        } else {
          LOGW_F("No fans found");
          appState = AppState::IDLE;
        }
      }
    } else {
      // Smart Connect failed or error
      LOGE_F("\nSmart Connect failed or error");
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
        snprintf(buffer, sizeof(buffer), "Setting all fans speed: %u%%", currentSpeed);
        LOGI(buffer);
        SmartMiFanAsync_setSpeedAll(currentSpeed);
      }
    }
  }
  
  // Print heap statistics periodically
  static uint32_t lastTickMs = 0;
  uint32_t now = millis();
  
  if (now - lastTickMs >= HEAP_TICK_INTERVAL_MS) {
    lastTickMs = now;
    
    // Determine current phase
    const char* phase = nullptr;
    if (handshakeComplete && appState == AppState::CONTROLLING) {
      phase = "Operating";
    } else if (smartConnectComplete) {
      phase = "Handshake";
    } else if (appState == AppState::CONNECTING) {
      phase = "FastConnect";
    } else {
      phase = "WiFi";
    }
    
    printHeapTick(phase);
  }
  
  delay(10); // Small delay to prevent watchdog issues
}
