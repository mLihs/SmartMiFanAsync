// =============================================================================
// SmartMiFanAsync - Client Module (Phase 5: File Split)
// =============================================================================
// Contains: SmartMiFanAsyncClient class implementation
// =============================================================================

#include "SmartMiFanInternal.h"

using namespace SmartMiFanInternal;

// Global client instance
SmartMiFanAsyncClient SmartMiFanAsync;

SmartMiFanAsyncClient::SmartMiFanAsyncClient()
    : _udp(nullptr),
      _fanAddress(),
      _deviceTimestamp(0),
      _ready(false),
      _handshakeValid(false),
      _lastHandshakeMillis(0),
      _globalSpeed(30),
      _modelType(FanModelType::UNKNOWN) {
  memset(_token, 0, sizeof(_token));
  memset(_key, 0, sizeof(_key));
  memset(_iv0, 0, sizeof(_iv0));
  memset(_deviceId, 0, sizeof(_deviceId));
  memset(_model, 0, sizeof(_model));
}

bool SmartMiFanAsyncClient::begin(WiFiUDP &udp, const IPAddress &fanAddress, const uint8_t token[16]) {
  _udp = &udp;
  _udp->begin(0);
  setFanAddress(fanAddress);
  if (token != nullptr) {
    if (token != _token) {
      memmove(_token, token, 16);
    }
    deriveKeyIv();
  }
  memset(_deviceId, 0, sizeof(_deviceId));
  _deviceTimestamp = 0;
  _ready = false;
  _handshakeValid = false;
  _lastHandshakeMillis = 0;
  return handshake();
}

bool SmartMiFanAsyncClient::begin(WiFiUDP &udp, const char *fanIp, const uint8_t token[16]) {
  IPAddress addr;
  if (!addr.fromString(fanIp)) {
    return false;
  }
  return begin(udp, addr, token);
}

bool SmartMiFanAsyncClient::handshake(uint32_t timeoutMs) {
  using namespace SmartMiFanInternal;
  
  if (_udp == nullptr || !_fanAddress) {
    return false;
  }

  // Check cache first - if handshake is valid AND within TTL, reuse it
  if (_handshakeValid && _ready) {
    uint32_t age = millis() - _lastHandshakeMillis;
    if (age < SMART_MI_FAN_HANDSHAKE_TTL_MS) {
      return true;
    }
    FAN_LOGI_F("Handshake cache expired (age=%lu ms), refreshing", age);
  }

  _ready = false;
  _handshakeValid = false;
  
  if (_udp) {
    _udp->begin(0);
  }

  uint8_t hello[32] = {0x21, 0x31, 0x00, 0x20};
  memset(hello + 4, 0xFF, 28);

  uint32_t lastSend = 0;
  uint32_t start = millis();
  bool wrongSourceIpSeen = false;
  
  while (millis() - start < timeoutMs) {
    uint32_t now = millis();
    if (lastSend == 0 || (now - lastSend) >= 500) {
      _udp->beginPacket(_fanAddress, kMiioPort);
      _udp->write(hello, sizeof(hello));
      _udp->endPacket();
      lastSend = now;
    }
    int len = _udp->parsePacket();
    if (len > 0) {
      IPAddress sender = _udp->remoteIP();
      if (sender != _fanAddress) {
        if (!wrongSourceIpSeen) {
          wrongSourceIpSeen = true;
          int fanIndex = findFanIndexByIp(_fanAddress);
          if (fanIndex >= 0) {
            g_discoveredFans[fanIndex].lastError = MiioErr::WRONG_SOURCE_IP;
            g_discoveredFans[fanIndex].ready = false;
            // DBG_FAN_TIMEOUT: log unexpected response sender during handshake
            FAN_LOGW_F("[DBG_FAN_TIMEOUT] Handshake wrong source IP: fanIndex=%d ip=%d.%d.%d.%d t=%lums",
                       fanIndex, sender[0], sender[1], sender[2], sender[3], (unsigned long)millis());
            emitErrorCallback(static_cast<uint8_t>(fanIndex), _fanAddress, FanOp::Handshake, 
                            MiioErr::WRONG_SOURCE_IP, millis() - start, false);
          }
        }
        discardUdpPacket(_udp);  // Safe discard instead of flush()
        continue;
      }
      
      if (len == 32) {
        uint8_t buf[32];
        _udp->read(buf, 32);
        memcpy(_deviceId, buf + 8, 4);
        _deviceTimestamp = (uint32_t(buf[12]) << 24) | (uint32_t(buf[13]) << 16) |
                           (uint32_t(buf[14]) << 8) | uint32_t(buf[15]);
        _ready = true;
        _handshakeValid = true;
        _lastHandshakeMillis = millis();
        
        int fanIndex = findFanIndexByIp(_fanAddress);
        if (fanIndex >= 0) {
          g_discoveredFans[fanIndex].ready = true;
          g_discoveredFans[fanIndex].lastError = MiioErr::OK;
        }
        
        return true;
      } else {
        discardUdpPacket(_udp);  // Safe discard instead of flush()
      }
    }
    yield();
  }

  _ready = false;
  _handshakeValid = false;
  int fanIndex = findFanIndexByIp(_fanAddress);
  if (fanIndex >= 0) {
    g_discoveredFans[fanIndex].ready = false;
    g_discoveredFans[fanIndex].lastError = MiioErr::TIMEOUT;
    // DBG_FAN_TIMEOUT: log handshake timeout
    FAN_LOGW_F("[DBG_FAN_TIMEOUT] Handshake timeout: fanIndex=%d ip=%d.%d.%d.%d timeoutMs=%lu t=%lums",
               fanIndex, _fanAddress[0], _fanAddress[1], _fanAddress[2], _fanAddress[3],
               (unsigned long)timeoutMs, (unsigned long)millis());
    emitErrorCallback(static_cast<uint8_t>(fanIndex), _fanAddress, FanOp::Handshake, 
                    MiioErr::TIMEOUT, timeoutMs, false);
  }
  return false;
}

// =========================
// Phase 4: TTL-aware Handshake Methods
// =========================

bool SmartMiFanAsyncClient::ensureHandshake(uint32_t ttlMs, uint32_t timeoutMs) {
  if (isHandshakeValid(ttlMs)) {
    return true;
  }
  return handshake(timeoutMs);
}

bool SmartMiFanAsyncClient::isHandshakeValid(uint32_t ttlMs) const {
  if (!_handshakeValid || !_ready) {
    return false;
  }
  uint32_t age = millis() - _lastHandshakeMillis;
  return (age < ttlMs);
}

void SmartMiFanAsyncClient::invalidateHandshake() {
  _ready = false;
  _handshakeValid = false;
  _lastHandshakeMillis = 0;
}

uint32_t SmartMiFanAsyncClient::getHandshakeAge() const {
  if (!_handshakeValid || !_ready) {
    return 0;
  }
  return millis() - _lastHandshakeMillis;
}

bool SmartMiFanAsyncClient::queryInfo(char *outModel, size_t modelSize,
                                       char *outFwVer, size_t fwSize,
                                       char *outHwVer, size_t hwSize,
                                       uint32_t *outDid,
                                       uint32_t timeoutMs) {
  using namespace SmartMiFanInternal;
  
  if (!_ready || !_udp) {
    return false;
  }
  
  const char *cmd = "{\"id\":1,\"method\":\"miIO.info\",\"params\":[]}";
  size_t cmdLen = strlen(cmd);
  
  size_t extra0 = 1;
  size_t raw = cmdLen + extra0;
  size_t pad = 16 - (raw % 16);
  size_t padLen = raw + pad;
  
  uint8_t plainPadded[256];
  memcpy(plainPadded, cmd, cmdLen);
  plainPadded[cmdLen] = 0x00;
  memset(plainPadded + cmdLen + 1, static_cast<uint8_t>(pad), pad);
  
  uint8_t cipher[256];
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, _key, 128);
  uint8_t iv[16];
  memcpy(iv, _iv0, sizeof(iv));
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, padLen, iv, plainPadded, cipher);
  mbedtls_aes_free(&aes);
  
  uint32_t ts = _deviceTimestamp + 1;
  MiioHeader header;
  header.magic = to_be16(0x2131);
  header.length = to_be16(32 + padLen);
  header.unknown = 0;
  memcpy(header.device_id, _deviceId, 4);
  header.ts_be = to_be32(ts);
  
  uint8_t tmp[16 + 16 + 256];
  memcpy(tmp, &header, 16);
  memcpy(tmp + 16, _token, 16);
  memcpy(tmp + 32, cipher, padLen);
  md5(tmp, 16 + 16 + padLen, header.checksum);
  
  _udp->beginPacket(_fanAddress, kMiioPort);
  _udp->write(reinterpret_cast<uint8_t *>(&header), 32);
  _udp->write(cipher, padLen);
  _udp->endPacket();
  
  _deviceTimestamp = ts;
  
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    int len = _udp->parsePacket();
    if (len > 32) {
      IPAddress sender = _udp->remoteIP();
      if (sender == _fanAddress) {
        uint8_t buffer[512];
        if (len <= static_cast<int>(sizeof(buffer))) {
          int readLen = _udp->read(buffer, len);
          if (readLen == len) {
            size_t payloadLen = len - 32;
            uint8_t *respCipher = buffer + 32;
            
            uint8_t plain[512];
            mbedtls_aes_init(&aes);
            mbedtls_aes_setkey_dec(&aes, _key, 128);
            memcpy(iv, _iv0, sizeof(iv));
            if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, payloadLen, iv, respCipher, plain) == 0) {
              mbedtls_aes_free(&aes);
              
              size_t plainLen = pkcs7Unpad(plain, payloadLen);
              plain[plainLen] = '\0';
              
              char model[24] = {0};
              char fw[16] = {0};
              char hw[16] = {0};
              
              if (jsonExtractString(reinterpret_cast<char *>(plain), "model", model, sizeof(model))) {
                safeCopyStr(_model, sizeof(_model), model);
                cacheModelType();
                
                if (outModel && modelSize > 0) {
                  safeCopyStr(outModel, modelSize, model);
                }
                
                jsonExtractString(reinterpret_cast<char *>(plain), "fw_ver", fw, sizeof(fw));
                jsonExtractString(reinterpret_cast<char *>(plain), "hw_ver", hw, sizeof(hw));
                
                if (outFwVer && fwSize > 0) {
                  safeCopyStr(outFwVer, fwSize, fw);
                }
                if (outHwVer && hwSize > 0) {
                  safeCopyStr(outHwVer, hwSize, hw);
                }
                
                if (outDid) {
                  char idBuffer[24];
                  if (jsonExtractString(reinterpret_cast<char *>(plain), "did", idBuffer, sizeof(idBuffer))) {
                    *outDid = strtoul(idBuffer, nullptr, 10);
                  } else {
                    const char *didKeyNumeric = "\"did\":";
                    const char *p = strstr(reinterpret_cast<char *>(plain), didKeyNumeric);
                    if (p) {
                      p += strlen(didKeyNumeric);
                      while (*p == ' ' || *p == '\t') ++p;
                      size_t lenDigits = 0;
                      while (isdigit(static_cast<unsigned char>(p[lenDigits])) && lenDigits < sizeof(idBuffer) - 1) {
                        idBuffer[lenDigits] = p[lenDigits];
                        ++lenDigits;
                      }
                      idBuffer[lenDigits] = '\0';
                      if (lenDigits > 0) {
                        *outDid = strtoul(idBuffer, nullptr, 10);
                      }
                    }
                  }
                }
                
                return true;
              }
            } else {
              mbedtls_aes_free(&aes);
            }
          }
        }
      } else {
        discardUdpPacket(_udp);  // Safe discard instead of flush()
      }
    } else if (len > 0) {
      discardUdpPacket(_udp);  // Safe discard instead of flush()
    }
    yield();
  }
  
  return false;
}

bool SmartMiFanAsyncClient::setPower(bool on) {
  return miotSetPropertyBool("power", 2, 1, on);
}

bool SmartMiFanAsyncClient::setSpeed(uint8_t percent) {
  using namespace SmartMiFanInternal;
  
  uint8_t p = percent;
  if (p < 1) p = 1;
  if (p > 100) p = 100;
  _globalSpeed = p;
  
  int siid = 6;
  int piid = 8;
  bool useFanLevel = false;
  getSpeedParamsByType(_modelType, siid, piid, useFanLevel);
  
  if (useFanLevel) {
    uint8_t level = 1;
    if (p > 66) level = 3;
    else if (p > 33) level = 2;
    return miotSetPropertyUint("fan_level", siid, piid, level);
  }
  
  return miotSetPropertyUint("fan_speed", siid, piid, p);
}

void SmartMiFanAsyncClient::setGlobalSpeed(uint8_t percent) {
  uint8_t p = percent;
  if (p < 1) p = 1;
  if (p > 100) p = 100;
  _globalSpeed = p;
}

uint8_t SmartMiFanAsyncClient::getGlobalSpeed() const {
  return _globalSpeed;
}

bool SmartMiFanAsyncClient::setTokenFromHex(const char *tokenHex) {
  if (tokenHex == nullptr) return false;
  if (!hexToBytes16(tokenHex, _token)) return false;
  deriveKeyIv();
  return true;
}

void SmartMiFanAsyncClient::setToken(const uint8_t token[16]) {
  if (token == nullptr) return;
  if (token != _token) {
    memmove(_token, token, 16);
  }
  deriveKeyIv();
}

void SmartMiFanAsyncClient::setFanAddress(const IPAddress &fanAddress) {
  _fanAddress = fanAddress;
  _ready = false;
  _handshakeValid = false;
}

void SmartMiFanAsyncClient::setModel(const char *model) {
  using namespace SmartMiFanInternal;
  
  if (model) {
    safeCopyStr(_model, sizeof(_model), model);
    cacheModelType();
  } else {
    _model[0] = '\0';
    _modelType = FanModelType::UNKNOWN;
  }
}

void SmartMiFanAsyncClient::cacheModelType() {
  using namespace SmartMiFanInternal;
  _modelType = modelStringToType(_model);
}

bool SmartMiFanAsyncClient::miotSetPropertyUint(const char * /*name*/, int siid, int piid, int value) {
  using namespace SmartMiFanInternal;
  
  if (_udp == nullptr) return false;
  if (!handshake()) return false;

  char json[196];
  snprintf(json, sizeof(json),
           "{\"id\":%u,\"method\":\"set_properties\",\"params\":[{\"siid\":%d,\"piid\":%d,\"value\":%d}]}",
           g_msgId++, siid, piid, value);

  uint8_t cipher[256];
  size_t clen = encryptPayload(reinterpret_cast<const uint8_t *>(json), strlen(json), cipher, sizeof(cipher));
  if (clen == 0) return false;

  MiioHeader header{};
  header.magic = to_be16(0x2131);
  header.length = to_be16(32 + static_cast<uint16_t>(clen));
  header.unknown = 0;
  memcpy(header.device_id, _deviceId, sizeof(_deviceId));
  uint32_t ts = _deviceTimestamp + 1;
  header.ts_be = to_be32(ts);

  uint8_t tmp[16 + 16 + sizeof(cipher)];
  memcpy(tmp, &header, 16);
  memcpy(tmp + 16, _token, 16);
  memcpy(tmp + 32, cipher, clen);
  md5(tmp, 16 + 16 + clen, header.checksum);
  _deviceTimestamp = ts;

  _udp->beginPacket(_fanAddress, kMiioPort);
  _udp->write(reinterpret_cast<uint8_t *>(&header), 32);
  _udp->write(cipher, clen);
  _udp->endPacket();

  uint32_t start = millis();
  bool wrongSourceIpSeen = false;
  bool responseReceived = false;
  
  while (millis() - start < 1500) {
    int len = _udp->parsePacket();
    if (len > 0) {
      IPAddress sender = _udp->remoteIP();
      if (sender == _fanAddress) {
        discardUdpPacket(_udp);  // Safe discard of response
        responseReceived = true;
        
        int fanIndex = findFanIndexByIp(_fanAddress);
        if (fanIndex >= 0) {
          g_discoveredFans[fanIndex].ready = true;
          g_discoveredFans[fanIndex].lastError = MiioErr::OK;
        }
        break;
      } else {
        if (!wrongSourceIpSeen) {
          wrongSourceIpSeen = true;
          int fanIndex = findFanIndexByIp(_fanAddress);
          if (fanIndex >= 0) {
            g_discoveredFans[fanIndex].lastError = MiioErr::WRONG_SOURCE_IP;
            g_discoveredFans[fanIndex].ready = false;
            // DBG_FAN_TIMEOUT: log unexpected response sender during set_properties (uint)
            FAN_LOGW_F("[DBG_FAN_TIMEOUT] setProperty(uint) wrong source IP: fanIndex=%d ip=%d.%d.%d.%d t=%lums",
                       fanIndex, sender[0], sender[1], sender[2], sender[3], (unsigned long)millis());
            emitErrorCallback(static_cast<uint8_t>(fanIndex), _fanAddress, FanOp::ReceiveResponse, 
                            MiioErr::WRONG_SOURCE_IP, millis() - start, false);
          }
        }
        discardUdpPacket(_udp);  // Safe discard instead of flush()
      }
    }
    yield();
  }
  
  if (!responseReceived) {
    int fanIndex = findFanIndexByIp(_fanAddress);
    if (fanIndex >= 0) {
      g_discoveredFans[fanIndex].ready = false;
      g_discoveredFans[fanIndex].lastError = MiioErr::TIMEOUT;
      // DBG_FAN_TIMEOUT: log timeout waiting for set_properties (uint) response
      FAN_LOGW_F("[DBG_FAN_TIMEOUT] setProperty(uint) timeout: fanIndex=%d ip=%d.%d.%d.%d timeoutMs=%u t=%lums",
                 fanIndex, _fanAddress[0], _fanAddress[1], _fanAddress[2], _fanAddress[3], 1500,
                 (unsigned long)millis());
      emitErrorCallback(static_cast<uint8_t>(fanIndex), _fanAddress, FanOp::ReceiveResponse, 
                      MiioErr::TIMEOUT, 1500, false);
    }
    return false;
  }

  return true;
}

bool SmartMiFanAsyncClient::miotSetPropertyBool(const char * /*name*/, int siid, int piid, bool value) {
  using namespace SmartMiFanInternal;
  
  if (_udp == nullptr) return false;
  if (!handshake()) return false;

  char json[196];
  snprintf(json, sizeof(json),
           "{\"id\":%u,\"method\":\"set_properties\",\"params\":[{\"siid\":%d,\"piid\":%d,\"value\":%s}]}",
           g_msgId++, siid, piid, value ? "true" : "false");

  uint8_t cipher[256];
  size_t clen = encryptPayload(reinterpret_cast<const uint8_t *>(json), strlen(json), cipher, sizeof(cipher));
  if (clen == 0) return false;

  MiioHeader header{};
  header.magic = to_be16(0x2131);
  header.length = to_be16(32 + static_cast<uint16_t>(clen));
  header.unknown = 0;
  memcpy(header.device_id, _deviceId, sizeof(_deviceId));
  uint32_t ts = _deviceTimestamp + 1;
  header.ts_be = to_be32(ts);

  uint8_t tmp[16 + 16 + sizeof(cipher)];
  memcpy(tmp, &header, 16);
  memcpy(tmp + 16, _token, 16);
  memcpy(tmp + 32, cipher, clen);
  md5(tmp, 16 + 16 + clen, header.checksum);
  _deviceTimestamp = ts;

  _udp->beginPacket(_fanAddress, kMiioPort);
  _udp->write(reinterpret_cast<uint8_t *>(&header), 32);
  _udp->write(cipher, clen);
  _udp->endPacket();

  uint32_t start = millis();
  bool wrongSourceIpSeen = false;
  bool responseReceived = false;
  
  while (millis() - start < 1500) {
    int len = _udp->parsePacket();
    if (len > 0) {
      IPAddress sender = _udp->remoteIP();
      if (sender == _fanAddress) {
        discardUdpPacket(_udp);  // Safe discard of response
        responseReceived = true;
        
        int fanIndex = findFanIndexByIp(_fanAddress);
        if (fanIndex >= 0) {
          g_discoveredFans[fanIndex].ready = true;
          g_discoveredFans[fanIndex].lastError = MiioErr::OK;
        }
        break;
      } else {
        if (!wrongSourceIpSeen) {
          wrongSourceIpSeen = true;
          int fanIndex = findFanIndexByIp(_fanAddress);
          if (fanIndex >= 0) {
            g_discoveredFans[fanIndex].lastError = MiioErr::WRONG_SOURCE_IP;
            g_discoveredFans[fanIndex].ready = false;
            // DBG_FAN_TIMEOUT: log unexpected response sender during set_properties (bool)
            FAN_LOGW_F("[DBG_FAN_TIMEOUT] setProperty(bool) wrong source IP: fanIndex=%d ip=%d.%d.%d.%d t=%lums",
                       fanIndex, sender[0], sender[1], sender[2], sender[3], (unsigned long)millis());
            emitErrorCallback(static_cast<uint8_t>(fanIndex), _fanAddress, FanOp::ReceiveResponse, 
                            MiioErr::WRONG_SOURCE_IP, millis() - start, false);
          }
        }
        discardUdpPacket(_udp);  // Safe discard instead of flush()
      }
    }
    yield();
  }
  
  if (!responseReceived) {
    int fanIndex = findFanIndexByIp(_fanAddress);
    if (fanIndex >= 0) {
      g_discoveredFans[fanIndex].ready = false;
      g_discoveredFans[fanIndex].lastError = MiioErr::TIMEOUT;
      // DBG_FAN_TIMEOUT: log timeout waiting for set_properties (bool) response
      FAN_LOGW_F("[DBG_FAN_TIMEOUT] setProperty(bool) timeout: fanIndex=%d ip=%d.%d.%d.%d timeoutMs=%u t=%lums",
                 fanIndex, _fanAddress[0], _fanAddress[1], _fanAddress[2], _fanAddress[3], 1500,
                 (unsigned long)millis());
      emitErrorCallback(static_cast<uint8_t>(fanIndex), _fanAddress, FanOp::ReceiveResponse, 
                      MiioErr::TIMEOUT, 1500, false);
    }
    return false;
  }

  return true;
}

void SmartMiFanAsyncClient::closeSession() {
  _ready = false;
  _handshakeValid = false;
}

void SmartMiFanAsyncClient::attachUdp(WiFiUDP &udp) {
  if (_udp && _udp != &udp) {
    _udp->stop();
  }
  _udp = &udp;
  _ready = false;
  _handshakeValid = false;
}

void SmartMiFanAsyncClient::deriveKeyIv() {
  using namespace SmartMiFanInternal;
  md5(_token, 16, _key);
  uint8_t tmp[32];
  memcpy(tmp, _key, 16);
  memcpy(tmp + 16, _token, 16);
  md5(tmp, 32, _iv0);
}

bool SmartMiFanAsyncClient::hexToBytes16(const char *hex, uint8_t *out16) {
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

size_t SmartMiFanAsyncClient::encryptPayload(const uint8_t *plain, size_t len, uint8_t *out, size_t outCap) {
  const size_t extra0 = 1;
  size_t raw = len + extra0;
  size_t pad = 16 - (raw % 16);
  size_t total = raw + pad;
  if (total > outCap) return 0;

  memcpy(out, plain, len);
  out[len] = 0x00;
  memset(out + len + 1, static_cast<uint8_t>(pad), pad);

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, _key, 128);
  uint8_t iv[16];
  memcpy(iv, _iv0, sizeof(iv));
  for (size_t off = 0; off < total; off += 16) {
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, 16, iv, out + off, out + off);
  }
  mbedtls_aes_free(&aes);
  return total;
}

// Legacy C-style wrappers
bool fan_set_speed(uint8_t percent) {
  return SmartMiFanAsync.setSpeed(percent);
}

bool fan_power(bool on) {
  return SmartMiFanAsync.setPower(on);
}
