// =============================================================================
// SmartMiFanAsync - Discovery Module (Phase 5: File Split)
// =============================================================================
// Contains: Discovery and Query Device APIs
// =============================================================================

#include "SmartMiFanInternal.h"

using namespace SmartMiFanInternal;

// =========================
// Discovery API
// =========================

void SmartMiFanAsync_resetDiscoveredFans() {
  g_discoveredFanCount = 0;
  // Reset soft-active overrides
  for (size_t i = 0; i < kMaxSmartMiFans; ++i) {
    g_softActive[i] = false;
  }
}

bool SmartMiFanAsync_startDiscovery(WiFiUDP &udp, const char *const tokens[], size_t tokenCount, unsigned long discoveryMs) {
  // Note: g_useFastConnect no longer blocks discovery - Smart Connect needs both
  if (tokens == nullptr || tokenCount == 0) return false;
  if (g_discoveryContext.state != DiscoveryState::IDLE) return false;
  
  g_udpContext = &udp;
  g_discoveryContext.reset();
  g_discoveryContext.udp = &udp;
  g_discoveryContext.tokens = tokens;
  g_discoveryContext.tokenCount = tokenCount;
  g_discoveryContext.discoveryMs = discoveryMs;
  g_discoveryContext.startTime = millis();
  g_discoveryContext.state = DiscoveryState::SENDING_HELLO;
  
  udp.stop();
  udp.begin(0);
  
  uint8_t hello[32] = {0x21, 0x31, 0x00, 0x20};
  memset(hello + 4, 0xFF, 28);
  udp.beginPacket(IPAddress(255, 255, 255, 255), kMiioPort);
  udp.write(hello, sizeof(hello));
  udp.endPacket();
  
  g_discoveryContext.helloSent = true;
  g_discoveryContext.lastHelloSend = millis();
  
  return true;
}

bool SmartMiFanAsync_startDiscovery(WiFiUDP &udp, const char *tokenHex, unsigned long discoveryMs) {
  return SmartMiFanAsync_startDiscovery(udp, &tokenHex, tokenHex ? 1 : 0, discoveryMs);
}

bool SmartMiFanAsync_updateDiscovery() {
  if (g_discoveryContext.state == DiscoveryState::IDLE) return false;
  
  if (g_discoveryContext.state == DiscoveryState::COMPLETE || 
      g_discoveryContext.state == DiscoveryState::ERROR ||
      g_discoveryContext.state == DiscoveryState::TIMEOUT) {
    return false;
  }
  
  // Check timeout for querying phase
  if (g_discoveryContext.state == DiscoveryState::QUERYING_DEVICES) {
    unsigned long minTimeout = g_discoveryContext.discoveryMs * 3;
    unsigned long queryTimeout = g_discoveryContext.discoveryMs + 
                                 (g_discoveryContext.candidateCount * g_discoveryContext.tokenCount * 2500UL);
    if (queryTimeout < minTimeout) queryTimeout = minTimeout;
    if (millis() - g_discoveryContext.startTime > queryTimeout) {
      g_discoveryContext.state = DiscoveryState::TIMEOUT;
      return false;
    }
  }
  
  if (g_discoveryContext.state == DiscoveryState::SENDING_HELLO) {
    unsigned long now = millis();
    if (now - g_discoveryContext.lastHelloSend >= 500) {
      if (g_discoveryContext.udp) {
        uint8_t hello[32] = {0x21, 0x31, 0x00, 0x20};
        memset(hello + 4, 0xFF, 28);
        g_discoveryContext.udp->beginPacket(IPAddress(255, 255, 255, 255), kMiioPort);
        g_discoveryContext.udp->write(hello, sizeof(hello));
        g_discoveryContext.udp->endPacket();
        g_discoveryContext.lastHelloSend = now;
      }
    }
    
    if (g_discoveryContext.udp) {
      int len = g_discoveryContext.udp->parsePacket();
      if (len == 32 && g_discoveryContext.candidateCount < kMaxSmartMiFans) {
        uint8_t buf[32];
        g_discoveryContext.udp->read(buf, 32);
        IPAddress sender = g_discoveryContext.udp->remoteIP();
        if (!candidateExists(g_discoveryContext.candidates, g_discoveryContext.candidateCount, sender)) {
          DiscoveryCandidate candidate{};
          if (storeHelloCandidate(sender, buf, 32, candidate)) {
            g_discoveryContext.candidates[g_discoveryContext.candidateCount++] = candidate;
          }
        }
      } else if (len > 0) {
        discardUdpPacket(g_discoveryContext.udp);  // Safe discard instead of flush()
      }
    }
    
    if (millis() - g_discoveryContext.startTime >= g_discoveryContext.discoveryMs) {
      g_discoveryContext.state = DiscoveryState::QUERYING_DEVICES;
      g_discoveryContext.currentCandidateIndex = 0;
      g_discoveryContext.currentTokenIndex = 0;
    }
    
    return true;
  }
  
  if (g_discoveryContext.state == DiscoveryState::QUERYING_DEVICES) {
    if (g_discoveredFanCount >= kMaxSmartMiFans) {
      g_discoveryContext.state = DiscoveryState::COMPLETE;
      return false;
    }
    
    if (g_discoveryContext.candidateCount == 0 || 
        g_discoveryContext.currentCandidateIndex >= g_discoveryContext.candidateCount) {
      g_discoveryContext.state = DiscoveryState::COMPLETE;
      return false;
    }
    
    const DiscoveryCandidate &candidate = g_discoveryContext.candidates[g_discoveryContext.currentCandidateIndex];
    const char *token = g_discoveryContext.tokens[g_discoveryContext.currentTokenIndex];
    
    if (!g_discoveryContext.querySent) {
      g_discoveryContext.currentQueryCandidate = candidate;
      g_discoveryContext.currentQueryToken = token;
      g_discoveryContext.queryStartTime = 0;
    }
    
    QueryInfoResult result = attemptMiioInfoAsync(g_discoveryContext);
    
    if (result == QueryInfoResult::SUCCESS) {
      g_discoveryContext.querySent = false;
      g_discoveryContext.currentTokenIndex++;
      if (g_discoveryContext.currentTokenIndex >= g_discoveryContext.tokenCount) {
        g_discoveryContext.currentTokenIndex = 0;
        g_discoveryContext.currentCandidateIndex++;
      }
    } else if (result == QueryInfoResult::FAILED) {
      g_discoveryContext.querySent = false;
      g_discoveryContext.currentTokenIndex++;
      if (g_discoveryContext.currentTokenIndex >= g_discoveryContext.tokenCount) {
        g_discoveryContext.currentTokenIndex = 0;
        g_discoveryContext.currentCandidateIndex++;
      }
    }
    
    return true;
  }
  
  return false;
}

DiscoveryState SmartMiFanAsync_getDiscoveryState() {
  return g_discoveryContext.state;
}

bool SmartMiFanAsync_isDiscoveryComplete() {
  return g_discoveryContext.state == DiscoveryState::COMPLETE;
}

bool SmartMiFanAsync_isDiscoveryInProgress() {
  return g_discoveryContext.state != DiscoveryState::IDLE &&
         g_discoveryContext.state != DiscoveryState::COMPLETE &&
         g_discoveryContext.state != DiscoveryState::ERROR &&
         g_discoveryContext.state != DiscoveryState::TIMEOUT;
}

void SmartMiFanAsync_cancelDiscovery() {
  g_discoveryContext.state = DiscoveryState::IDLE;
  g_discoveryContext.reset();
}

// =========================
// Query Device API
// =========================

bool SmartMiFanAsync_startQueryDevice(WiFiUDP &udp, const IPAddress &ip, const char *tokenHex) {
  if (!tokenHex) return false;
  if (g_queryContext.state != QueryState::IDLE) return false;
  
  g_udpContext = &udp;
  g_queryContext.reset();
  g_queryContext.udp = &udp;
  g_queryContext.targetIp = ip;
  g_queryContext.tokenHex = tokenHex;
  g_queryContext.startTime = millis();
  g_queryContext.state = QueryState::WAITING_HELLO;
  
  udp.stop();
  udp.begin(0);
  
  uint8_t hello[32] = {0x21, 0x31, 0x00, 0x20};
  memset(hello + 4, 0xFF, 28);
  udp.beginPacket(ip, kMiioPort);
  udp.write(hello, sizeof(hello));
  udp.endPacket();
  
  g_queryContext.helloSent = true;
  g_queryContext.lastHelloSend = millis();
  
  return true;
}

bool SmartMiFanAsync_updateQueryDevice() {
  if (g_queryContext.state == QueryState::IDLE) return false;
  
  if (g_queryContext.state == QueryState::COMPLETE || 
      g_queryContext.state == QueryState::ERROR ||
      g_queryContext.state == QueryState::TIMEOUT) {
    return false;
  }
  
  if (g_queryContext.state == QueryState::WAITING_HELLO) {
    if (millis() - g_queryContext.startTime > 2000) {
      g_queryContext.state = QueryState::TIMEOUT;
      return false;
    }
    
    unsigned long now = millis();
    if (now - g_queryContext.lastHelloSend >= 500) {
      if (g_queryContext.udp) {
        uint8_t hello[32] = {0x21, 0x31, 0x00, 0x20};
        memset(hello + 4, 0xFF, 28);
        g_queryContext.udp->beginPacket(g_queryContext.targetIp, kMiioPort);
        g_queryContext.udp->write(hello, sizeof(hello));
        g_queryContext.udp->endPacket();
        g_queryContext.lastHelloSend = now;
      }
    }
    
    if (g_queryContext.udp) {
      int len = g_queryContext.udp->parsePacket();
      if (len == 32) {
        uint8_t buf[32];
        g_queryContext.udp->read(buf, 32);
        if (storeHelloCandidate(g_queryContext.udp->remoteIP(), buf, 32, g_queryContext.candidate)) {
          g_queryContext.state = QueryState::SENDING_QUERY;
          g_queryContext.querySent = false;
          return true;
        }
      } else if (len > 0) {
        discardUdpPacket(g_queryContext.udp);  // Safe discard instead of flush()
      }
    }
    
    return true;
  }
  
  if (g_queryContext.state == QueryState::SENDING_QUERY) {
    QueryInfoResult result = attemptMiioInfoAsync(g_queryContext);
    
    if (result == QueryInfoResult::SUCCESS) {
      g_queryContext.state = QueryState::COMPLETE;
      return false;
    } else if (result == QueryInfoResult::FAILED) {
      g_queryContext.state = QueryState::ERROR;
      return false;
    }
    return true;
  }
  
  return false;
}

QueryState SmartMiFanAsync_getQueryState() {
  return g_queryContext.state;
}

bool SmartMiFanAsync_isQueryComplete() {
  return g_queryContext.state == QueryState::COMPLETE;
}

bool SmartMiFanAsync_isQueryInProgress() {
  return g_queryContext.state != QueryState::IDLE &&
         g_queryContext.state != QueryState::COMPLETE &&
         g_queryContext.state != QueryState::ERROR &&
         g_queryContext.state != QueryState::TIMEOUT;
}

void SmartMiFanAsync_cancelQuery() {
  g_queryContext.state = QueryState::IDLE;
  g_queryContext.reset();
}

const SmartMiFanDiscoveredDevice *SmartMiFanAsync_getDiscoveredFans(size_t &count) {
  count = g_discoveredFanCount;
  return g_discoveredFans;
}

void SmartMiFanAsync_printDiscoveredFans() {
  FAN_LOGI_F("Discovered SmartMi fans:");
  if (g_discoveredFanCount == 0) {
    FAN_LOGI_F("  (none)");
    return;
  }
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    const auto &fan = g_discoveredFans[i];
    FAN_LOGI_F("  Model: %s | IP: %d.%d.%d.%d | DID: %lu | Token: %s | FW: %s | HW: %s",
           fan.model, fan.ip[0], fan.ip[1], fan.ip[2], fan.ip[3], fan.did, fan.token, fan.fw_ver, fan.hw_ver);
  }
  
  #if defined(FAN_DEBUG_GEN)
  FAN_LOGI_F("Fan diagnostics after discovery:");
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    const auto &fan = g_discoveredFans[i];
    FanParticipationState participation = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    const char* participationStr = (participation == FanParticipationState::ACTIVE) ? "ACTIVE" :
                                  (participation == FanParticipationState::INACTIVE) ? "INACTIVE" : "ERROR";
    const char* errorStr = (fan.lastError == MiioErr::OK) ? "OK" :
                           (fan.lastError == MiioErr::TIMEOUT) ? "TIMEOUT" :
                           (fan.lastError == MiioErr::WRONG_SOURCE_IP) ? "WRONG_SOURCE_IP" :
                           (fan.lastError == MiioErr::DECRYPT_FAIL) ? "DECRYPT_FAIL" : "INVALID_RESPONSE";
    FAN_LOGI_F("  Fan[%zu]: enabled=%s, ready=%s, lastError=%s, participation=%s",
           i, fan.userEnabled ? "true" : "false", fan.ready ? "true" : "false", errorStr, participationStr);
  }
  #endif
}

// =========================
// Basic Control APIs
// =========================

bool SmartMiFanAsync_handshakeAll() {
  if (!g_udpContext) return false;
  bool overall = true;
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    const auto &fan = g_discoveredFans[i];
    if (!prepareFanContext(fan)) {
      overall = false;
      continue;
    }
    if (!SmartMiFanAsync.handshake()) {
      overall = false;
    }
  }
  return overall;
}

bool SmartMiFanAsync_setPowerAll(bool on) {
  if (!g_udpContext) return false;
  bool overall = true;
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    const auto &fan = g_discoveredFans[i];
    if (!prepareFanContext(fan)) {
      overall = false;
      continue;
    }
    if (!SmartMiFanAsync.setPower(on)) {
      overall = false;
    }
  }
  return overall;
}

bool SmartMiFanAsync_setSpeedAll(uint8_t percent) {
  if (!g_udpContext) return false;
  bool overall = true;
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    const auto &fan = g_discoveredFans[i];
    if (!prepareFanContext(fan)) {
      overall = false;
      continue;
    }
    if (!SmartMiFanAsync.setSpeed(percent)) {
      overall = false;
    }
  }
  return overall;
}
