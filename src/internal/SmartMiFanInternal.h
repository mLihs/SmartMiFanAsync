// =============================================================================
// SmartMiFanAsync - Internal Header (Phase 5: File Split)
// =============================================================================
// This header contains internal types, constants, and forward declarations
// shared between all implementation modules. NOT part of the public API.
// =============================================================================

#pragma once

#include "../SmartMiFanAsync.h"

#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

#include "mbedtls/aes.h"
#include "mbedtls/md5.h"

namespace SmartMiFanInternal {

// =========================
// Protocol Constants
// =========================
constexpr uint16_t kMiioPort = 54321;
constexpr size_t kMaxSmartMiFans = 16;
constexpr size_t kMaxFastConnectFans = 4;

// =========================
// Internal Structures
// =========================

#pragma pack(push, 1)
struct MiioHeader {
  uint16_t magic;
  uint16_t length;
  uint32_t unknown;
  uint8_t device_id[4];
  uint32_t ts_be;
  uint8_t checksum[16];
};
#pragma pack(pop)

struct DiscoveryCandidate {
  IPAddress ip;
  uint8_t deviceId[4];
  uint32_t timestamp;
};

struct FastConnectConfigEntry {
  IPAddress ip;
  char token[33];  // Hex token string (32 chars + null terminator)
  char model[24];  // Optional: Fan model (skips queryInfo if set)
  bool enabled;    // Entry enabled/disabled
};

// Smart Connect Context
struct SmartConnectContext {
  SmartConnectState state;
  WiFiUDP* udp;
  unsigned long discoveryMs;
  const char* failedTokens[kMaxFastConnectFans];
  size_t failedTokenCount;
  bool fastConnectValidated;
  
  void reset() {
    state = SmartConnectState::IDLE;
    udp = nullptr;
    discoveryMs = 0;
    failedTokenCount = 0;
    fastConnectValidated = false;
    memset(failedTokens, 0, sizeof(failedTokens));
  }
};

// Async Discovery Context (uses shared buffers for crypto)
struct DiscoveryContext {
  DiscoveryState state;
  unsigned long startTime;
  unsigned long discoveryMs;
  const char* const* tokens;
  size_t tokenCount;
  size_t currentTokenIndex;
  size_t currentCandidateIndex;
  DiscoveryCandidate candidates[kMaxSmartMiFans];
  size_t candidateCount;
  WiFiUDP* udp;
  unsigned long lastHelloSend;
  bool helloSent;
  
  // For async miio.info query
  DiscoveryCandidate currentQueryCandidate;
  const char* currentQueryToken;
  size_t queryCipherLen;
  MiioHeader queryHeader;
  unsigned long queryStartTime;
  bool querySent;
  
  // Accessors to shared buffers (defined in Core module)
  uint8_t* queryKey();
  uint8_t* queryIv();
  uint8_t* queryCipher();
  
  void reset();
};

// Async Query Context (uses shared buffers for crypto)
struct QueryContext {
  QueryState state;
  WiFiUDP* udp;
  IPAddress targetIp;
  const char* tokenHex;
  DiscoveryCandidate candidate;
  unsigned long startTime;
  bool helloSent;
  unsigned long lastHelloSend;
  
  // For async miio.info query
  size_t queryCipherLen;
  MiioHeader queryHeader;
  unsigned long queryStartTime;
  bool querySent;
  
  // Accessors to shared buffers
  uint8_t* queryKey();
  uint8_t* queryIv();
  uint8_t* queryCipher();
  
  void reset();
};

// Phase 3: Single-Pass miIO.info Parser result
struct MiioInfoFields {
  char model[24];
  char fw_ver[16];
  char hw_ver[16];
  uint32_t did;
  bool modelFound;
};

// Query result enum (internal)
enum class QueryInfoResult {
  IN_PROGRESS,
  SUCCESS,
  FAILED
};

// Shared parameters for miIO query execution (Phase 1)
struct MiioQueryParams {
  WiFiUDP* udp;
  const DiscoveryCandidate* candidate;
  const char* tokenHex;
  uint8_t* queryKey;
  uint8_t* queryIv;
  uint8_t* queryCipher;
  size_t* queryCipherLen;
  MiioHeader* queryHeader;
  unsigned long* queryStartTime;
  bool* querySent;
};

// =========================
// Global State (extern declarations)
// =========================
extern SmartMiFanDiscoveredDevice g_discoveredFans[kMaxSmartMiFans];
extern size_t g_discoveredFanCount;
extern WiFiUDP* g_udpContext;
extern FanErrorCallback g_errorCallback;

extern FastConnectConfigEntry g_fastConnectConfig[kMaxFastConnectFans];
extern size_t g_fastConnectConfigCount;
extern bool g_useFastConnect;
extern FastConnectValidationCallback g_fastConnectCallback;

extern SmartConnectContext g_smartConnectContext;
extern FastConnectValidationCallback g_originalFastConnectCallback;

extern DiscoveryContext g_discoveryContext;
extern QueryContext g_queryContext;

// Shared static buffers
extern uint8_t g_sharedUdpBuffer[512];
extern uint8_t g_sharedPlainBuffer[512];
extern uint8_t g_sharedCipherBuffer[256];
extern uint8_t g_sharedQueryKey[16];
extern uint8_t g_sharedQueryIv[16];

extern uint32_t g_msgId;

// Supported models list
extern const char* kSupportedModels[];

// =========================
// Core Utility Functions
// =========================

// Byte order conversion
inline uint16_t to_be16(uint16_t v) {
  return (v >> 8) | (v << 8);
}

inline uint32_t to_be32(uint32_t v) {
  return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | 
         ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}

// MD5 hash
void md5(const uint8_t* in, size_t len, uint8_t out[16]);

// UDP helpers - safe packet discard (avoids flush() which can discard multiple packets)
void discardUdpPacket(WiFiUDP* udp);

// Crypto helpers
bool hexToBytes16Helper(const char* hex, uint8_t out16[16]);
void computeKeyIv(const uint8_t token[16], uint8_t key[16], uint8_t iv[16]);

// Model/Fan helpers
bool isSupportedModel(const char* model);
FanModelType modelStringToType(const char* model);
void getSpeedParamsByType(FanModelType type, int& siid, int& piid, bool& useFanLevel);
bool getSpeedParams(const char* model, int& siid, int& piid, bool& useFanLevel);
bool fanAlreadyStored(uint32_t did, const IPAddress& ip);

// JSON parsing
size_t pkcs7Unpad(uint8_t* buffer, size_t len);
bool jsonExtractString(const char* json, const char* key, char* out, size_t outLen);
uint32_t jsonExtractUint(const char* json, const char* key);
uint32_t extractDidFromJson(const char* json, const uint8_t deviceId[4]);
bool parseMiioInfoSinglePass(const char* json, MiioInfoFields& out);

// Fan management
void cacheFanCrypto(SmartMiFanDiscoveredDevice& fan);
void appendDiscoveredFan(const SmartMiFanDiscoveredDevice& fan);
bool prepareFanContext(const SmartMiFanDiscoveredDevice& fan);
bool prepareFanContextCached(const SmartMiFanDiscoveredDevice& fan);

// Error handling
void emitErrorCallback(uint8_t fanIndex, const IPAddress& ip, FanOp operation, 
                       MiioErr error, uint32_t elapsedMs, bool handshakeInvalidated);
int findFanIndexByIp(const IPAddress& ip);

// Discovery helpers
bool storeHelloCandidate(const IPAddress& ip, const uint8_t* buffer, size_t len, 
                         DiscoveryCandidate& candidate);
bool candidateExists(const DiscoveryCandidate* candidates, size_t count, const IPAddress& ip);

// Query system (Phase 1 consolidated)
bool sendMiioInfoQuery(MiioQueryParams& p);
QueryInfoResult processMiioResponse(MiioQueryParams& p, bool checkSupportedModel = true);

// Discovery/Query async helpers
QueryInfoResult attemptMiioInfoAsync(DiscoveryContext& ctx);
QueryInfoResult attemptMiioInfoAsync(QueryContext& ctx);

}  // namespace SmartMiFanInternal

// Bring commonly used items into scope for implementation files
using namespace SmartMiFanInternal;
