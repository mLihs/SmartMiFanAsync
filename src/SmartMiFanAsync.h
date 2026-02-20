#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

// =========================
// Library Version
// =========================
#define SMART_MI_FAN_ASYNC_VERSION "1.8.2"
#define SMART_MI_FAN_ASYNC_VERSION_MAJOR 1
#define SMART_MI_FAN_ASYNC_VERSION_MINOR 8
#define SMART_MI_FAN_ASYNC_VERSION_PATCH 2

// =========================
// Minimal compile-time logging (Serial)
// Variant A: GEN / HOT / NET
// Prefix: FAN
// =========================
//
// Enable via build flags or uncomment:
     #define FAN_DEBUG_GEN
//   #define FAN_DEBUG_HOT
//   #define FAN_DEBUG_NET
//

#if defined(__has_include)
  #if __has_include("DebugConfig.h")
    #include "DebugConfig.h"
  #elif __has_include(<DebugConfig.h>)
    #include <DebugConfig.h>
  #endif
#endif

#if defined(FAN_DEBUG_GEN)
  #define FAN_LOGI_F(...) do { Serial.printf("[I] " __VA_ARGS__); Serial.println(); } while(0)
  #define FAN_LOGW_F(...) do { Serial.printf("[W] " __VA_ARGS__); Serial.println(); } while(0)
  #define FAN_LOGE_F(...) do { Serial.printf("[E] " __VA_ARGS__); Serial.println(); } while(0)
#else
  #define FAN_LOGI_F(...) do {} while(0)
  #define FAN_LOGW_F(...) do {} while(0)
  #define FAN_LOGE_F(...) do {} while(0)
#endif

#if defined(FAN_DEBUG_HOT)
  #define FAN_LOGHOT_F(...) do { Serial.printf("[H] " __VA_ARGS__); Serial.println(); } while(0)
#else
  #define FAN_LOGHOT_F(...) do {} while(0)
#endif

#if defined(FAN_DEBUG_NET)
  #define FAN_LOGNET_F(...) do { Serial.printf("[N] " __VA_ARGS__); Serial.println(); } while(0)
#else
  #define FAN_LOGNET_F(...) do {} while(0)
#endif

// Fast Connect Mode (Optional)
// Enable Fast Connect mode at compile-time (default: disabled)
// Can also be enabled/disabled at runtime via SmartMiFanAsync_setFastConnectEnabled()
#ifndef SMART_MI_FAN_FAST_CONNECT_ENABLED
#define SMART_MI_FAN_FAST_CONNECT_ENABLED 0
#endif

// =========================
// Phase 4: Handshake TTL (Self-Healing)
// =========================
// Handshake cache expires after this time (ms)
// After TTL, next command triggers fresh handshake
// Prevents "sticky" invalid states from transient UDP issues
#ifndef SMART_MI_FAN_HANDSHAKE_TTL_MS
#define SMART_MI_FAN_HANDSHAKE_TTL_MS 60000  // 60 seconds default
#endif

/* Example: Async discovery mode
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "your-ssid";
const char *WIFI_PASS = "your-pass";
const char *TOKENS[] = {
  "32-character-token-for-fan-1",
  //"optional-token-for-fan-2"
};
const size_t TOKEN_COUNT = sizeof(TOKENS) / sizeof(TOKENS[0]);

WiFiUDP fanUdp;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    FAN_LOGHOT_F(".");
  }
  FAN_LOGI_F("WiFi connected");

  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 3000);
}

void loop() {
  // Update discovery (non-blocking)
  if (SmartMiFanAsync_updateDiscovery()) {
    // Discovery still in progress
    FAN_LOGHOT_F("Discovering...");
  } else if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Discovery finished
    SmartMiFanAsync_printDiscoveredFans();
    
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    if (count > 0) {
      if (SmartMiFanAsync_handshakeAll()) {
        SmartMiFanAsync_setPowerAll(true);
        SmartMiFanAsync_setSpeedAll(45);
      }
    }
  }
  
  // Your other code continues here...
}
*/

enum class DiscoveryState {
  IDLE,
  SENDING_HELLO,
  COLLECTING_CANDIDATES,
  QUERYING_DEVICES,
  COMPLETE,
  ERROR,
  TIMEOUT
};

enum class QueryState {
  IDLE,
  WAITING_HELLO,
  SENDING_QUERY,
  WAITING_RESPONSE,
  COMPLETE,
  ERROR,
  TIMEOUT
};

enum class SmartConnectState {
  IDLE,
  VALIDATING_FAST_CONNECT,
  STARTING_DISCOVERY,
  DISCOVERING,
  COMPLETE,
  ERROR
};

// Step 2: Error Classification
enum class MiioErr {
  OK,
  TIMEOUT,            // no response from device
  WRONG_SOURCE_IP,    // UDP response from unexpected IP
  DECRYPT_FAIL,       // AES decrypt failed (likely token or stale handshake)
  INVALID_RESPONSE    // decrypted but malformed or unexpected payload
};

// Step 2: Fan Operation Context
enum class FanOp {
  Handshake,
  SendCommand,
  ReceiveResponse,
  HealthCheck
};

// System State (Project-Level API Contract)
// This enum is part of the public API for project-level system state management.
// The library never sets or changes system state internally; it only exposes
// hooks for project code to integrate with system state transitions.
// Projects define their own state machines using this enum.
enum class SystemState {
  ACTIVE,   // BLE sensors connected OR Web/UI interaction OR first outgoing fan command
  IDLE,     // no BLE sensors, no UI interaction, system remains awake
  SLEEP     // prolonged inactivity, miio transport is inactive
};

// Step 3: Fan Participation State
// Each fan has a participation state derived from:
// - user/project intent (ACTIVE/INACTIVE)
// - technical readiness (ERROR derived ONLY from lastError != OK)
// Note: ready==false does NOT mean ERROR - it means 'not handshaked yet'
enum class FanParticipationState {
  ACTIVE,     // default, participates in control
  INACTIVE,   // excluded by project/user
  ERROR       // not available (derived from lastError != OK)
};

// =========================
// Fan Model Type (Performance Optimization)
// =========================
// Cached model type for O(1) lookup instead of strcmp chain
enum class FanModelType : uint8_t {
  UNKNOWN = 0,
  ZHIMI_FAN_ZA5,      // zhimi.fan.za5 (Smartmi Standing Fan 3)
  ZHIMI_FAN_ZA4,      // zhimi.fan.za4 (Smartmi Standing Fan 2S)
  ZHIMI_FAN_V3,       // zhimi.fan.v3 (Smartmi Standing Fan 2)
  DMAKER_FAN_1C,      // dmaker.fan.1c (Mi Smart Standing Fan 1C) - uses fan_level
  DMAKER_FAN_P5,      // dmaker.fan.p5
  DMAKER_FAN_P9,      // dmaker.fan.p9
  DMAKER_FAN_P10,     // dmaker.fan.p10, p18
  DMAKER_FAN_P11,     // dmaker.fan.p11, p15, p33
  XIAOMI_FAN_P76,     // xiaomi.fan.p76
};

// =========================
// String Utilities (Memory Safe, No Heap)
// =========================

// Safe string copy - faster than strncpy (no null-padding), always null-terminated
inline void safeCopyStr(char* dest, size_t destSize, const char* src) {
  if (destSize == 0 || src == nullptr) return;
  size_t srcLen = strlen(src);
  size_t copyLen = (srcLen < destSize - 1) ? srcLen : destSize - 1;
  memcpy(dest, src, copyLen);
  dest[copyLen] = '\0';
}

// IP to string without heap allocation (avoids .toString().c_str())
inline void ipToStr(const IPAddress& ip, char* out, size_t outSize) {
  if (outSize < 16) return;  // Need at least 16 chars for "255.255.255.255\0"
  snprintf(out, outSize, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

struct SmartMiFanDiscoveredDevice {
  IPAddress ip;
  uint32_t did;
  char model[24];
  char token[33];
  char fw_ver[16];
  char hw_ver[16];
  // Step 2: Per-fan readiness state
  bool ready;          // true only after successful handshake
  MiioErr lastError;   // last error encountered
  // Step 3: Fan participation state
  bool userEnabled;   // user/project intent: true = enabled, false = disabled (default: true)
  
  // Phase 3: Cached crypto data (computed once, reused on every command)
  // Saves ~250μs per command (2x MD5 + hex parsing)
  uint8_t tokenBytes[16];    // Parsed token bytes (from hex string)
  uint8_t cachedKey[16];     // AES key derived from token
  uint8_t cachedIv[16];      // AES IV derived from token
  FanModelType modelType;    // Cached model type for O(1) lookup
  bool cryptoCached;         // true if tokenBytes/cachedKey/cachedIv are valid
};

// Fast Connect Configuration Entry
struct SmartMiFanFastConnectEntry {
  const char* ipStr;      // IP address as string (e.g., "192.168.1.100")
  const char* tokenHex;   // 32-character hex token string
  const char* model;      // Optional: Fan model (e.g., "zhimi.fan.za5") - skips queryInfo if set
};

// Fast Connect Validation Result
struct SmartMiFanFastConnectResult {
  IPAddress ip;
  char token[33];     // Hex token string
  bool success;       // true = handshake succeeded, false = failed
};

// Fast Connect Validation Callback
// Called once after all Fast Connect fans are validated
// Parameters: array of results, count of results
typedef void (*FastConnectValidationCallback)(const SmartMiFanFastConnectResult results[], size_t count);

// Step 2: Error Callback
// Error reporting structure - informational only, does NOT affect control flow
struct FanErrorInfo {
  uint8_t fanIndex;
  IPAddress ip;
  FanOp operation;
  MiioErr error;
  uint32_t elapsedMs;
  bool handshakeInvalidated;
};

// Step 2: Error Callback Function Type
// Callback must never block, trigger retries, or modify discovery/smart connect state
typedef void (*FanErrorCallback)(const FanErrorInfo&);

class SmartMiFanAsyncClient {
public:
  SmartMiFanAsyncClient();

  bool begin(WiFiUDP &udp, const IPAddress &fanAddress, const uint8_t token[16]);
  bool begin(WiFiUDP &udp, const char *fanIp, const uint8_t token[16]);

  bool handshake(uint32_t timeoutMs = 2000);
  
  // Phase 4: TTL-aware handshake - ensures valid handshake within TTL
  // Returns true if handshake is valid (cached or freshly performed)
  // Use this instead of handshake() for self-healing behavior
  bool ensureHandshake(uint32_t ttlMs = SMART_MI_FAN_HANDSHAKE_TTL_MS, uint32_t timeoutMs = 2000);
  
  // Check if handshake is currently valid (within TTL)
  bool isHandshakeValid(uint32_t ttlMs = SMART_MI_FAN_HANDSHAKE_TTL_MS) const;
  
  // Invalidate handshake cache (forces fresh handshake on next command)
  void invalidateHandshake();
  
  // Get age of current handshake in milliseconds (0 if invalid)
  uint32_t getHandshakeAge() const;
  
  // Query miio.info to get model, fw_ver, hw_ver - call after successful handshake
  // Returns true if model was successfully retrieved
  // Populates _model internally and optionally outputs fw/hw versions
  bool queryInfo(char *outModel = nullptr, size_t modelSize = 0,
                 char *outFwVer = nullptr, size_t fwSize = 0,
                 char *outHwVer = nullptr, size_t hwSize = 0,
                 uint32_t *outDid = nullptr,
                 uint32_t timeoutMs = 2000);

  bool setPower(bool on);
  bool setSpeed(uint8_t percent);

  void setGlobalSpeed(uint8_t percent);
  uint8_t getGlobalSpeed() const;

  bool setTokenFromHex(const char *tokenHex);
  void setToken(const uint8_t token[16]);
  const uint8_t *getToken() const { return _token; }

  void setFanAddress(const IPAddress &fanAddress);
  IPAddress getFanAddress() const { return _fanAddress; }

  void setModel(const char *model);
  void setModelType(FanModelType type) { _modelType = type; }  // Direct set for cached path
  const char *getModel() const { return _model; }
  FanModelType getModelType() const { return _modelType; }

  bool isReady() const { return _ready; }

  void attachUdp(WiFiUDP &udp);

private:
  bool miotSetPropertyUint(const char *name, int siid, int piid, int value);
  bool miotSetPropertyBool(const char *name, int siid, int piid, bool value);
  void closeSession();
  void deriveKeyIv();
  bool hexToBytes16(const char *hex, uint8_t *out16);
  size_t encryptPayload(const uint8_t *plain, size_t len, uint8_t *out, size_t outCap);
  void cacheModelType();  // Convert model string to enum for O(1) lookup

  WiFiUDP *_udp;
  IPAddress _fanAddress;
  uint8_t _token[16];
  uint8_t _key[16];
  uint8_t _iv0[16];
  uint8_t _deviceId[4];
  uint32_t _deviceTimestamp;
  bool _ready;
  bool _handshakeValid;  // Cache flag: handshake is valid until error
  unsigned long _lastHandshakeMillis;  // Timestamp of last successful handshake
  uint8_t _globalSpeed;
  char _model[24];
  FanModelType _modelType;  // Cached for O(1) speed param lookup
};

extern SmartMiFanAsyncClient SmartMiFanAsync;

bool fan_set_speed(uint8_t percent);
bool fan_power(bool on);

// Async Discovery API
bool SmartMiFanAsync_startDiscovery(WiFiUDP &udp, const char *const tokens[], size_t tokenCount, unsigned long discoveryMs = 3000);
bool SmartMiFanAsync_startDiscovery(WiFiUDP &udp, const char *tokenHex, unsigned long discoveryMs = 3000);
bool SmartMiFanAsync_updateDiscovery();
DiscoveryState SmartMiFanAsync_getDiscoveryState();
bool SmartMiFanAsync_isDiscoveryComplete();
bool SmartMiFanAsync_isDiscoveryInProgress();
void SmartMiFanAsync_cancelDiscovery();

// Async Query API
bool SmartMiFanAsync_startQueryDevice(WiFiUDP &udp, const IPAddress &ip, const char *tokenHex);
bool SmartMiFanAsync_updateQueryDevice();
QueryState SmartMiFanAsync_getQueryState();
bool SmartMiFanAsync_isQueryComplete();
bool SmartMiFanAsync_isQueryInProgress();
void SmartMiFanAsync_cancelQuery();

// Helper functions
void SmartMiFanAsync_resetDiscoveredFans();
const SmartMiFanDiscoveredDevice *SmartMiFanAsync_getDiscoveredFans(size_t &count);
void SmartMiFanAsync_printDiscoveredFans();
bool SmartMiFanAsync_handshakeAll();
bool SmartMiFanAsync_setPowerAll(bool on);
bool SmartMiFanAsync_setSpeedAll(uint8_t percent);

// Fast Connect API (Optional)
bool SmartMiFanAsync_setFastConnectConfig(const SmartMiFanFastConnectEntry entries[], size_t count);
void SmartMiFanAsync_clearFastConnectConfig();
bool SmartMiFanAsync_isFastConnectEnabled();
void SmartMiFanAsync_setFastConnectEnabled(bool enabled);
bool SmartMiFanAsync_registerFastConnectFans(WiFiUDP &udp);
void SmartMiFanAsync_setFastConnectValidationCallback(FastConnectValidationCallback callback);
bool SmartMiFanAsync_validateFastConnectFans(WiFiUDP &udp);

// Smart Connect API (Optional - combines Fast Connect + Discovery)
bool SmartMiFanAsync_startSmartConnect(WiFiUDP &udp, unsigned long discoveryMs = 3000);
bool SmartMiFanAsync_updateSmartConnect();
SmartConnectState SmartMiFanAsync_getSmartConnectState();
bool SmartMiFanAsync_isSmartConnectComplete();
bool SmartMiFanAsync_isSmartConnectInProgress();
void SmartMiFanAsync_cancelSmartConnect();

// Step 2: Error and Health Callback API
void SmartMiFanAsync_setErrorCallback(FanErrorCallback cb);
bool SmartMiFanAsync_isFanReady(uint8_t fanIndex);
MiioErr SmartMiFanAsync_getFanLastError(uint8_t fanIndex);

// Step 2: Health Check API
bool SmartMiFanAsync_healthCheck(uint8_t fanIndex, uint32_t timeoutMs);
bool SmartMiFanAsync_healthCheckAll(uint32_t timeoutMs);

// Step 2: Transport / Sleep Hooks
void SmartMiFanAsync_prepareForSleep(bool closeUdp, bool invalidateHandshake);
void SmartMiFanAsync_softWakeUp();

// Step 3: Fan Participation State API
// Get fan participation state (derived from userEnabled and lastError)
// ERROR state is derived ONLY from lastError != OK (not from ready==false)
FanParticipationState SmartMiFanAsync_getFanParticipationState(uint8_t fanIndex);

// Set user-enabled state for a fan (default: true = ACTIVE)
// Setting to false makes fan INACTIVE (excluded from commands)
// ERROR state is derived automatically from lastError != OK
void SmartMiFanAsync_setFanEnabled(uint8_t fanIndex, bool enabled);
bool SmartMiFanAsync_isFanEnabled(uint8_t fanIndex);

// Soft-active override: keep fan ACTIVE despite lastError != OK.
// Intended for application-level retry logic.
void SmartMiFanAsync_setFanSoftActive(uint8_t fanIndex, bool enabled);

// Step 3: Command Orchestration API
// These functions respect fan participation states:
// - Only ACTIVE fans receive commands
// - INACTIVE and ERROR fans are skipped
// - Commands are sent in deterministic order (Fan 0 → 1 → 2 → 3)
// - Command coalescing: max 1 command per second
bool SmartMiFanAsync_setPowerAllOrchestrated(bool on);
bool SmartMiFanAsync_setSpeedAllOrchestrated(uint8_t percent);
bool SmartMiFanAsync_handshakeAllOrchestrated();

