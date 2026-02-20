// =============================================================================
// SmartMiFanAsync - Core Module (Phase 5: File Split)
// =============================================================================
// Contains: Global state, crypto, utilities, JSON parsing, model lookup
// =============================================================================

#include "SmartMiFanInternal.h"

namespace SmartMiFanInternal {

// =========================
// Global State Definitions
// =========================

SmartMiFanDiscoveredDevice g_discoveredFans[kMaxSmartMiFans];
size_t g_discoveredFanCount = 0;
WiFiUDP* g_udpContext = nullptr;

// Soft-active overrides (application-level retry logic)
bool g_softActive[kMaxSmartMiFans] = {false};

FanErrorCallback g_errorCallback = nullptr;

FastConnectConfigEntry g_fastConnectConfig[kMaxFastConnectFans];
size_t g_fastConnectConfigCount = 0;

#if SMART_MI_FAN_FAST_CONNECT_ENABLED
bool g_useFastConnect = true;
#else
bool g_useFastConnect = false;
#endif

FastConnectValidationCallback g_fastConnectCallback = nullptr;

SmartConnectContext g_smartConnectContext;
FastConnectValidationCallback g_originalFastConnectCallback = nullptr;

DiscoveryContext g_discoveryContext;
QueryContext g_queryContext;

// Shared static buffers (RAM optimization)
uint8_t g_sharedUdpBuffer[512];
uint8_t g_sharedPlainBuffer[512];
uint8_t g_sharedCipherBuffer[256];
uint8_t g_sharedQueryKey[16];
uint8_t g_sharedQueryIv[16];

uint32_t g_msgId = 1;

// Supported models list
const char* kSupportedModels[] = {
    "zhimi.fan.za5",
    "zhimi.fan.v2",
    "zhimi.fan.v3",
    "zhimi.fan.za4",
    "zhimi.fan.za3",
    "xiaomi.fan.p76",
    "dmaker.fan.1c",
    "dmaker.fan.p5",
    "dmaker.fan.p8",
    "dmaker.fan.p9",
    "dmaker.fan.p10",
    "dmaker.fan.p11",
    "dmaker.fan.p15",
    "dmaker.fan.p18",
    "dmaker.fan.p30",
    "dmaker.fan.p33",
    "dmaker.fan.p220",
};

// =========================
// Context Accessor Implementations
// =========================

uint8_t* DiscoveryContext::queryKey() { return g_sharedQueryKey; }
uint8_t* DiscoveryContext::queryIv() { return g_sharedQueryIv; }
uint8_t* DiscoveryContext::queryCipher() { return g_sharedCipherBuffer; }

void DiscoveryContext::reset() {
  state = DiscoveryState::IDLE;
  startTime = 0;
  discoveryMs = 0;
  tokens = nullptr;
  tokenCount = 0;
  currentTokenIndex = 0;
  currentCandidateIndex = 0;
  candidateCount = 0;
  udp = nullptr;
  lastHelloSend = 0;
  helloSent = false;
  queryStartTime = 0;
  querySent = false;
  queryCipherLen = 0;
  memset(&currentQueryCandidate, 0, sizeof(currentQueryCandidate));
  currentQueryToken = nullptr;
}

uint8_t* QueryContext::queryKey() { return g_sharedQueryKey; }
uint8_t* QueryContext::queryIv() { return g_sharedQueryIv; }
uint8_t* QueryContext::queryCipher() { return g_sharedCipherBuffer; }

void QueryContext::reset() {
  state = QueryState::IDLE;
  udp = nullptr;
  targetIp = IPAddress();
  tokenHex = nullptr;
  startTime = 0;
  helloSent = false;
  lastHelloSend = 0;
  queryStartTime = 0;
  querySent = false;
  queryCipherLen = 0;
  memset(&candidate, 0, sizeof(candidate));
}

// =========================
// UDP Helpers
// =========================

void discardUdpPacket(WiFiUDP* udp) {
  if (!udp) return;
  uint8_t discard[64];
  while (udp->available()) {
    udp->read(discard, min((int)sizeof(discard), udp->available()));
  }
}

// =========================
// Crypto Functions
// =========================

void md5(const uint8_t* in, size_t len, uint8_t out[16]) {
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, in, len);
  mbedtls_md5_finish(&ctx, out);
  mbedtls_md5_free(&ctx);
}

bool hexToBytes16Helper(const char* hex, uint8_t out16[16]) {
  if (hex == nullptr) return false;
  for (int i = 0; i < 16; ++i) {
    char c1 = hex[i * 2];
    char c2 = hex[i * 2 + 1];
    auto nib = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
      if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      return -1;
    };
    int n1 = nib(c1);
    int n2 = nib(c2);
    if (n1 < 0 || n2 < 0) return false;
    out16[i] = static_cast<uint8_t>((n1 << 4) | n2);
  }
  return true;
}

void computeKeyIv(const uint8_t token[16], uint8_t key[16], uint8_t iv[16]) {
  md5(token, 16, key);
  uint8_t tmp[32];
  memcpy(tmp, key, 16);
  memcpy(tmp + 16, token, 16);
  md5(tmp, 32, iv);
}

// =========================
// Model Helpers
// =========================

bool isSupportedModel(const char* model) {
  if (!model || model[0] == '\0') return false;
  
  // Fast prefix check - all supported models have these prefixes
  if (strncmp(model, "zhimi.fan.", 10) == 0) return true;
  if (strncmp(model, "dmaker.fan.", 11) == 0) return true;
  if (strncmp(model, "xiaomi.fan.", 11) == 0) return true;
  
  return false;
}

// Suffix hash constants for O(1) model lookup
constexpr uint32_t SUFFIX_ZA5 = ('z' << 16) | ('a' << 8) | '5';
constexpr uint32_t SUFFIX_ZA4 = ('z' << 16) | ('a' << 8) | '4';
constexpr uint32_t SUFFIX_ZA3 = ('z' << 16) | ('a' << 8) | '3';
constexpr uint32_t SUFFIX_V3  = ('.') << 16 | ('v' << 8) | '3';
constexpr uint32_t SUFFIX_V2  = ('.') << 16 | ('v' << 8) | '2';
constexpr uint32_t SUFFIX_1C  = ('.') << 16 | ('1' << 8) | 'c';
constexpr uint32_t SUFFIX_P5  = ('.') << 16 | ('p' << 8) | '5';
constexpr uint32_t SUFFIX_P8  = ('.') << 16 | ('p' << 8) | '8';
constexpr uint32_t SUFFIX_P9  = ('.') << 16 | ('p' << 8) | '9';
constexpr uint32_t SUFFIX_P10 = ('p' << 16) | ('1' << 8) | '0';
constexpr uint32_t SUFFIX_P11 = ('p' << 16) | ('1' << 8) | '1';
constexpr uint32_t SUFFIX_P15 = ('p' << 16) | ('1' << 8) | '5';
constexpr uint32_t SUFFIX_P18 = ('p' << 16) | ('1' << 8) | '8';
constexpr uint32_t SUFFIX_P30 = ('p' << 16) | ('3' << 8) | '0';
constexpr uint32_t SUFFIX_P33 = ('p' << 16) | ('3' << 8) | '3';
constexpr uint32_t SUFFIX_P76 = ('p' << 16) | ('7' << 8) | '6';
constexpr uint32_t SUFFIX_220 = ('2' << 16) | ('2' << 8) | '0';

inline uint32_t modelSuffixHash(const char* model) {
  if (!model) return 0;
  size_t len = strlen(model);
  if (len < 3) return 0;
  const char* s = model + len - 3;
  return (static_cast<uint32_t>(s[0]) << 16) | 
         (static_cast<uint32_t>(s[1]) << 8) | 
         static_cast<uint32_t>(s[2]);
}

FanModelType modelStringToType(const char* model) {
  if (!model || model[0] == '\0') return FanModelType::UNKNOWN;
  
  uint32_t suffix = modelSuffixHash(model);
  
  switch (suffix) {
    case SUFFIX_ZA5: return FanModelType::ZHIMI_FAN_ZA5;
    case SUFFIX_ZA4: return FanModelType::ZHIMI_FAN_ZA4;
    case SUFFIX_ZA3: return FanModelType::ZHIMI_FAN_ZA4;
    case SUFFIX_V3:
    case SUFFIX_V2:  return FanModelType::ZHIMI_FAN_V3;
    case SUFFIX_1C:  return FanModelType::DMAKER_FAN_1C;
    case SUFFIX_P5:  return FanModelType::DMAKER_FAN_P5;
    case SUFFIX_P8:
    case SUFFIX_P9:  return FanModelType::DMAKER_FAN_P9;
    case SUFFIX_P10:
    case SUFFIX_P18: return FanModelType::DMAKER_FAN_P10;
    case SUFFIX_P11:
    case SUFFIX_P15:
    case SUFFIX_P30:
    case SUFFIX_P33: return FanModelType::DMAKER_FAN_P11;
    case SUFFIX_P76: return FanModelType::XIAOMI_FAN_P76;
    case SUFFIX_220: return FanModelType::DMAKER_FAN_P11;
    default:         return FanModelType::UNKNOWN;
  }
}

void getSpeedParamsByType(FanModelType type, int& siid, int& piid, bool& useFanLevel) {
  useFanLevel = false;
  switch (type) {
    case FanModelType::DMAKER_FAN_1C:
      siid = 2; piid = 2; useFanLevel = true; return;
    case FanModelType::DMAKER_FAN_P9:
      siid = 2; piid = 11; return;
    case FanModelType::DMAKER_FAN_P10:
      siid = 2; piid = 10; return;
    case FanModelType::DMAKER_FAN_P11:
    case FanModelType::DMAKER_FAN_P5:
      siid = 2; piid = 6; return;
    case FanModelType::XIAOMI_FAN_P76:
      siid = 2; piid = 5; return;
    case FanModelType::ZHIMI_FAN_ZA5:
    case FanModelType::ZHIMI_FAN_ZA4:
    case FanModelType::ZHIMI_FAN_V3:
      siid = 6; piid = 8; return;
    case FanModelType::UNKNOWN:
    default:
      siid = 6; piid = 8; return;
  }
}

// Suffix key macros for O(1) lookup (replaces strcmp chain)
#define SKEY2(a,b) (((uint16_t)(a)<<8)|(b))
#define SKEY3(a,b,c) (((uint32_t)(a)<<16)|((uint32_t)(b)<<8)|(c))

bool getSpeedParams(const char* model, int& siid, int& piid, bool& useFanLevel) {
  useFanLevel = false;
  siid = 2;
  
  if (!model || model[0] == '\0') {
    siid = 6;
    piid = 8;
    return true;
  }
  
  if (strncmp(model, "dmaker.fan.", 11) == 0) {
    const char* s = model + 11;
    size_t slen = strlen(s);
    
    if (slen == 2) {
      uint16_t key = SKEY2(s[0], s[1]);
      switch (key) {
        case SKEY2('1','c'): siid = 2; piid = 2; useFanLevel = true; return true;
        case SKEY2('p','9'): siid = 2; piid = 11; return true;
      }
    } else if (slen == 3) {
      uint32_t key = SKEY3(s[0], s[1], s[2]);
      switch (key) {
        case SKEY3('p','1','0'):
        case SKEY3('p','1','8'): siid = 2; piid = 10; return true;
        case SKEY3('p','1','1'):
        case SKEY3('p','1','5'):
        case SKEY3('p','3','3'): siid = 2; piid = 6; return true;
      }
    }
    siid = 2; piid = 6; return true;
  }
  
  if (strncmp(model, "zhimi.fan.", 10) == 0) {
    siid = 6; piid = 8; return true;
  }

  if (strncmp(model, "xiaomi.fan.", 11) == 0) {
    siid = 2; piid = 5; return true;
  }
  
  siid = 6;
  piid = 8;
  return false;
}

#undef SKEY2
#undef SKEY3

// =========================
// Fan Storage Helpers
// =========================

bool fanAlreadyStored(uint32_t did, const IPAddress& ip) {
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    if (g_discoveredFans[i].ip == ip) return true;
    if (did != 0 && g_discoveredFans[i].did != 0 && g_discoveredFans[i].did == did) return true;
  }
  return false;
}

void cacheFanCrypto(SmartMiFanDiscoveredDevice& fan) {
  if (fan.cryptoCached) return;
  
  if (!hexToBytes16Helper(fan.token, fan.tokenBytes)) {
    fan.cryptoCached = false;
    return;
  }
  
  computeKeyIv(fan.tokenBytes, fan.cachedKey, fan.cachedIv);
  fan.modelType = modelStringToType(fan.model);
  fan.cryptoCached = true;
}

void appendDiscoveredFan(const SmartMiFanDiscoveredDevice& fan) {
  if (g_discoveredFanCount >= kMaxSmartMiFans) return;
  if (fanAlreadyStored(fan.did, fan.ip)) return;
  g_discoveredFans[g_discoveredFanCount] = fan;
  cacheFanCrypto(g_discoveredFans[g_discoveredFanCount]);
  g_discoveredFanCount++;
}

// =========================
// Error Handling
// =========================

void emitErrorCallback(uint8_t fanIndex, const IPAddress& ip, FanOp operation, 
                       MiioErr error, uint32_t elapsedMs, bool handshakeInvalidated) {
  if (g_errorCallback == nullptr) return;
  
  FanErrorInfo info{};
  info.fanIndex = fanIndex;
  info.ip = ip;
  info.operation = operation;
  info.error = error;
  info.elapsedMs = elapsedMs;
  info.handshakeInvalidated = handshakeInvalidated;
  
  g_errorCallback(info);
}

int findFanIndexByIp(const IPAddress& ip) {
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    if (g_discoveredFans[i].ip == ip) return static_cast<int>(i);
  }
  return -1;
}

// =========================
// Context Preparation
// =========================

bool prepareFanContextCached(const SmartMiFanDiscoveredDevice& fan) {
  if (!g_udpContext) return false;
  if (!fan.cryptoCached) return false;
  
  SmartMiFanAsync.attachUdp(*g_udpContext);
  SmartMiFanAsync.setToken(fan.tokenBytes);
  SmartMiFanAsync.setFanAddress(fan.ip);
  SmartMiFanAsync.setModelType(fan.modelType);
  return true;
}

bool prepareFanContext(const SmartMiFanDiscoveredDevice& fan) {
  if (fan.cryptoCached) {
    return prepareFanContextCached(fan);
  }
  
  if (!g_udpContext) return false;
  SmartMiFanAsync.attachUdp(*g_udpContext);
  if (!SmartMiFanAsync.setTokenFromHex(fan.token)) return false;
  SmartMiFanAsync.setFanAddress(fan.ip);
  SmartMiFanAsync.setModel(fan.model);
  return true;
}

// =========================
// JSON Parsing
// =========================

size_t pkcs7Unpad(uint8_t* buffer, size_t len) {
  if (len == 0) return 0;
  uint8_t pad = buffer[len - 1];
  if (pad == 0 || pad > 16 || pad > len) return len;
  return len - pad;
}

bool jsonExtractString(const char* json, const char* key, char* out, size_t outLen) {
  if (!json || !key || !out || outLen == 0) return false;
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return false;
  start += strlen(pattern);
  const char* end = strchr(start, '"');
  if (!end) return false;
  size_t len = end - start;
  if (len >= outLen) len = outLen - 1;
  memcpy(out, start, len);
  out[len] = '\0';
  return true;
}

uint32_t jsonExtractUint(const char* json, const char* key) {
  if (!json || !key) return 0;

  char asString[24];
  if (jsonExtractString(json, key, asString, sizeof(asString))) {
    return strtoul(asString, nullptr, 10);
  }

  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return 0;
  start += strlen(pattern);

  while (*start == ' ' || *start == '\t') start++;

  char buffer[24];
  size_t len = 0;
  while (start[len] && start[len] != ',' && start[len] != '}' && len < sizeof(buffer) - 1) {
    buffer[len] = start[len];
    ++len;
  }
  while (len > 0 && (buffer[len - 1] == ' ' || buffer[len - 1] == '\t')) {
    --len;
  }
  buffer[len] = '\0';
  return strtoul(buffer, nullptr, 10);
}

uint32_t extractDidFromJson(const char* json, const uint8_t deviceId[4]) {
  if (!json) return 0;
  
  char idBuffer[24];
  if (jsonExtractString(json, "did", idBuffer, sizeof(idBuffer))) {
    return strtoul(idBuffer, nullptr, 10);
  }
  
  const char* didKey = "\"did\":";
  const char* p = strstr(json, didKey);
  if (p) {
    p += strlen(didKey);
    while (*p == ' ' || *p == '\t') ++p;
    if (isdigit(static_cast<unsigned char>(*p))) {
      return strtoul(p, nullptr, 10);
    }
  }
  
  if (deviceId) {
    return (deviceId[0] << 24) | (deviceId[1] << 16) | 
           (deviceId[2] << 8) | deviceId[3];
  }
  
  return 0;
}

bool parseMiioInfoSinglePass(const char* json, MiioInfoFields& out) {
  memset(&out, 0, sizeof(out));
  if (!json) return false;
  
  const char* p = json;
  while (*p) {
    while (*p && *p != '"') ++p;
    if (!*p) break;
    ++p;
    
    bool isModel = (strncmp(p, "model\"", 6) == 0);
    bool isFwVer = (strncmp(p, "fw_ver\"", 7) == 0);
    bool isHwVer = (strncmp(p, "hw_ver\"", 7) == 0);
    bool isDid = (strncmp(p, "did\"", 4) == 0);
    
    if (!isModel && !isFwVer && !isHwVer && !isDid) {
      while (*p && *p != '"') ++p;
      if (*p) ++p;
      continue;
    }
    
    while (*p && *p != ':') ++p;
    if (!*p) break;
    ++p;
    while (*p == ' ' || *p == '\t') ++p;
    
    if (isDid) {
      if (*p == '"') {
        ++p;
        out.did = strtoul(p, nullptr, 10);
      } else if (isdigit(static_cast<unsigned char>(*p))) {
        out.did = strtoul(p, nullptr, 10);
      }
      continue;
    }
    
    if (*p != '"') continue;
    ++p;
    
    const char* valueStart = p;
    while (*p && *p != '"') ++p;
    size_t valueLen = p - valueStart;
    
    if (isModel && valueLen < sizeof(out.model)) {
      memcpy(out.model, valueStart, valueLen);
      out.model[valueLen] = '\0';
      out.modelFound = true;
    } else if (isFwVer && valueLen < sizeof(out.fw_ver)) {
      memcpy(out.fw_ver, valueStart, valueLen);
      out.fw_ver[valueLen] = '\0';
    } else if (isHwVer && valueLen < sizeof(out.hw_ver)) {
      memcpy(out.hw_ver, valueStart, valueLen);
      out.hw_ver[valueLen] = '\0';
    }
  }
  
  return out.modelFound;
}

// =========================
// Discovery Helpers
// =========================

bool storeHelloCandidate(const IPAddress& ip, const uint8_t* buffer, size_t len, 
                         DiscoveryCandidate& candidate) {
  if (len != 32) return false;
  candidate.ip = ip;
  memcpy(candidate.deviceId, buffer + 8, 4);
  candidate.timestamp = (buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15];
  return true;
}

bool candidateExists(const DiscoveryCandidate* candidates, size_t count, const IPAddress& ip) {
  for (size_t i = 0; i < count; ++i) {
    if (candidates[i].ip == ip) return true;
  }
  return false;
}

// =========================
// MiIO Query System
// =========================

bool sendMiioInfoQuery(MiioQueryParams& p) {
  if (!p.tokenHex || !p.udp || !p.candidate) return false;
  
  uint8_t token[16];
  if (!hexToBytes16Helper(p.tokenHex, token)) return false;
  
  computeKeyIv(token, p.queryKey, p.queryIv);
  
  const char* json = "{\"id\":1,\"method\":\"miIO.info\",\"params\":[]}";
  size_t len = strlen(json);
  size_t extra0 = 1;
  size_t raw = len + extra0;
  size_t pad = 16 - (raw % 16);
  *p.queryCipherLen = raw + pad;
  if (*p.queryCipherLen > 256) return false;
  
  uint8_t buffer[256];
  memcpy(buffer, json, len);
  buffer[len] = 0x00;
  memset(buffer + len + 1, static_cast<uint8_t>(pad), pad);
  
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, p.queryKey, 128);
  uint8_t iv[16];
  memcpy(iv, p.queryIv, sizeof(iv));
  for (size_t off = 0; off < *p.queryCipherLen; off += 16) {
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 16, iv, buffer + off, buffer + off);
  }
  mbedtls_aes_free(&aes);
  memcpy(p.queryCipher, buffer, *p.queryCipherLen);
  
  p.queryHeader->magic = to_be16(0x2131);
  p.queryHeader->length = to_be16(32 + static_cast<uint16_t>(*p.queryCipherLen));
  p.queryHeader->unknown = 0;
  memcpy(p.queryHeader->device_id, p.candidate->deviceId, 4);
  uint32_t ts = p.candidate->timestamp + 1;
  p.queryHeader->ts_be = to_be32(ts);
  
  uint8_t tmp[16 + 16 + 256];
  memcpy(tmp, p.queryHeader, 16);
  memcpy(tmp + 16, token, 16);
  memcpy(tmp + 32, p.queryCipher, *p.queryCipherLen);
  md5(tmp, 16 + 16 + *p.queryCipherLen, p.queryHeader->checksum);
  
  p.udp->stop();
  p.udp->begin(0);
  p.udp->beginPacket(p.candidate->ip, kMiioPort);
  p.udp->write(reinterpret_cast<uint8_t*>(p.queryHeader), 32);
  p.udp->write(p.queryCipher, *p.queryCipherLen);
  p.udp->endPacket();
  
  *p.querySent = true;
  *p.queryStartTime = millis();
  return true;
}

QueryInfoResult processMiioResponse(MiioQueryParams& p, bool checkSupportedModel) {
  if (!p.udp) return QueryInfoResult::FAILED;
  
  if (millis() - *p.queryStartTime > 2000) {
    return QueryInfoResult::FAILED;
  }
  
  for (int attempt = 0; attempt < 2; ++attempt) {
    int len = p.udp->parsePacket();
    if (len <= 0) return QueryInfoResult::IN_PROGRESS;
    
    IPAddress senderIp = p.udp->remoteIP();
    if (senderIp != p.candidate->ip) {
      if (len > static_cast<int>(sizeof(g_sharedUdpBuffer))) {
        discardUdpPacket(p.udp);  // Safe discard instead of flush()
      } else {
        p.udp->read(g_sharedUdpBuffer, len);
      }
      continue;
    }
    
    if (len > static_cast<int>(sizeof(g_sharedUdpBuffer))) {
      discardUdpPacket(p.udp);  // Safe discard instead of flush()
      return QueryInfoResult::IN_PROGRESS;
    }
    
    int readLen = p.udp->read(g_sharedUdpBuffer, len);
    if (readLen != len || len <= 32) return QueryInfoResult::IN_PROGRESS;
    
    size_t payloadLen = len - 32;
    uint8_t* respCipher = g_sharedUdpBuffer + 32;
    
    if (payloadLen > sizeof(g_sharedPlainBuffer)) return QueryInfoResult::IN_PROGRESS;
    
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, p.queryKey, 128);
    uint8_t iv[16];
    memcpy(iv, p.queryIv, sizeof(iv));
    memcpy(g_sharedPlainBuffer, respCipher, payloadLen);
    if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, payloadLen, iv, respCipher, g_sharedPlainBuffer) != 0) {
      mbedtls_aes_free(&aes);
      return QueryInfoResult::IN_PROGRESS;
    }
    mbedtls_aes_free(&aes);
    
    size_t plainLen = pkcs7Unpad(g_sharedPlainBuffer, payloadLen);
    g_sharedPlainBuffer[plainLen] = '\0';
    
    // Use proven jsonExtractString method (more reliable than single-pass parser)
    char model[24] = {0};
    char fw[16] = {0};
    char hw[16] = {0};
    
    if (!jsonExtractString(reinterpret_cast<char*>(g_sharedPlainBuffer), "model", model, sizeof(model))) {
      return QueryInfoResult::IN_PROGRESS;
    }
    if (checkSupportedModel && !isSupportedModel(model)) {
      return QueryInfoResult::IN_PROGRESS;
    }
    
    // Extract fw_ver and hw_ver (optional fields)
    jsonExtractString(reinterpret_cast<char*>(g_sharedPlainBuffer), "fw_ver", fw, sizeof(fw));
    jsonExtractString(reinterpret_cast<char*>(g_sharedPlainBuffer), "hw_ver", hw, sizeof(hw));
    
    // Extract DID
    uint32_t did = jsonExtractUint(reinterpret_cast<char*>(g_sharedPlainBuffer), "did");
    if (did == 0 && p.candidate->deviceId) {
      did = (p.candidate->deviceId[0] << 24) | (p.candidate->deviceId[1] << 16) | 
            (p.candidate->deviceId[2] << 8) | p.candidate->deviceId[3];
    }
    
    SmartMiFanDiscoveredDevice fan{};
    fan.ip = p.candidate->ip;
    fan.did = did;
    safeCopyStr(fan.model, sizeof(fan.model), model);
    safeCopyStr(fan.token, sizeof(fan.token), p.tokenHex);
    safeCopyStr(fan.fw_ver, sizeof(fan.fw_ver), fw);
    safeCopyStr(fan.hw_ver, sizeof(fan.hw_ver), hw);
    fan.ready = false;
    fan.lastError = MiioErr::OK;
    fan.userEnabled = true;
    
    appendDiscoveredFan(fan);
    return QueryInfoResult::SUCCESS;
  }
  
  return QueryInfoResult::IN_PROGRESS;
}

QueryInfoResult attemptMiioInfoAsync(DiscoveryContext& ctx) {
  MiioQueryParams params = {
    ctx.udp,
    &ctx.currentQueryCandidate,
    ctx.currentQueryToken,
    ctx.queryKey(),
    ctx.queryIv(),
    ctx.queryCipher(),
    &ctx.queryCipherLen,
    &ctx.queryHeader,
    &ctx.queryStartTime,
    &ctx.querySent
  };
  
  if (!ctx.querySent) {
    if (!sendMiioInfoQuery(params)) return QueryInfoResult::FAILED;
    return QueryInfoResult::IN_PROGRESS;
  }
  
  return processMiioResponse(params, true);
}

QueryInfoResult attemptMiioInfoAsync(QueryContext& ctx) {
  MiioQueryParams params = {
    ctx.udp,
    &ctx.candidate,
    ctx.tokenHex,
    ctx.queryKey(),
    ctx.queryIv(),
    ctx.queryCipher(),
    &ctx.queryCipherLen,
    &ctx.queryHeader,
    &ctx.queryStartTime,
    &ctx.querySent
  };
  
  if (!ctx.querySent) {
    if (!sendMiioInfoQuery(params)) return QueryInfoResult::FAILED;
    return QueryInfoResult::IN_PROGRESS;
  }
  
  return processMiioResponse(params, true);
}

}  // namespace SmartMiFanInternal
