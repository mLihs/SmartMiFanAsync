// =============================================================================
// SmartMiFanAsync - Connect Module (Phase 5: File Split)
// =============================================================================
// Contains: Fast Connect and Smart Connect APIs
// =============================================================================

#include "SmartMiFanInternal.h"

using namespace SmartMiFanInternal;

// =========================
// Fast Connect API
// =========================

bool SmartMiFanAsync_setFastConnectConfig(const SmartMiFanFastConnectEntry entries[], size_t count) {
  if (entries == nullptr || count == 0 || count > kMaxFastConnectFans) return false;
  
  g_fastConnectConfigCount = 0;
  memset(g_fastConnectConfig, 0, sizeof(g_fastConnectConfig));
  
  for (size_t i = 0; i < count; ++i) {
    const SmartMiFanFastConnectEntry &entry = entries[i];
    
    if (entry.ipStr == nullptr) continue;
    IPAddress ip;
    if (!ip.fromString(entry.ipStr)) continue;
    
    if (entry.tokenHex == nullptr) continue;
    size_t tokenLen = strlen(entry.tokenHex);
    if (tokenLen != 32) continue;
    
    bool validToken = true;
    for (size_t j = 0; j < tokenLen; ++j) {
      char c = entry.tokenHex[j];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        validToken = false;
        break;
      }
    }
    if (!validToken) continue;
    
    g_fastConnectConfig[g_fastConnectConfigCount].ip = ip;
    safeCopyStr(g_fastConnectConfig[g_fastConnectConfigCount].token, 
                sizeof(g_fastConnectConfig[g_fastConnectConfigCount].token), entry.tokenHex);
    
    if (entry.model != nullptr && entry.model[0] != '\0') {
      safeCopyStr(g_fastConnectConfig[g_fastConnectConfigCount].model, 
                  sizeof(g_fastConnectConfig[g_fastConnectConfigCount].model), entry.model);
    } else {
      g_fastConnectConfig[g_fastConnectConfigCount].model[0] = '\0';
    }
    g_fastConnectConfig[g_fastConnectConfigCount].enabled = true;
    g_fastConnectConfigCount++;
  }
  
  // Automatically enable Fast Connect when configuration is set
  if (g_fastConnectConfigCount > 0) {
    g_useFastConnect = true;
  }
  
  return g_fastConnectConfigCount > 0;
}

void SmartMiFanAsync_clearFastConnectConfig() {
  g_fastConnectConfigCount = 0;
  memset(g_fastConnectConfig, 0, sizeof(g_fastConnectConfig));
}

bool SmartMiFanAsync_isFastConnectEnabled() {
  return g_useFastConnect;
}

void SmartMiFanAsync_setFastConnectEnabled(bool enabled) {
  g_useFastConnect = enabled;
}

bool SmartMiFanAsync_registerFastConnectFans(WiFiUDP &udp) {
  if (g_fastConnectConfigCount == 0) return false;
  
  g_udpContext = &udp;
  
  for (size_t i = 0; i < g_fastConnectConfigCount; ++i) {
    const FastConnectConfigEntry &entry = g_fastConnectConfig[i];
    if (!entry.enabled) continue;
    
    SmartMiFanDiscoveredDevice fan{};
    fan.ip = entry.ip;
    fan.did = 0;
    safeCopyStr(fan.token, sizeof(fan.token), entry.token);
    
    if (entry.model[0] != '\0') {
      safeCopyStr(fan.model, sizeof(fan.model), entry.model);
    } else {
      fan.model[0] = '\0';
    }
    fan.fw_ver[0] = '\0';
    fan.hw_ver[0] = '\0';
    fan.ready = false;
    fan.lastError = MiioErr::OK;
    fan.userEnabled = true;
    fan.cryptoCached = false;
    
    appendDiscoveredFan(fan);
  }
  
  return g_discoveredFanCount > 0;
}

void SmartMiFanAsync_setFastConnectValidationCallback(FastConnectValidationCallback callback) {
  g_fastConnectCallback = callback;
}

bool SmartMiFanAsync_validateFastConnectFans(WiFiUDP &udp) {
  if (g_discoveredFanCount == 0) return false;
  
  SmartMiFanFastConnectResult results[kMaxFastConnectFans];
  size_t resultCount = 0;
  bool overallSuccess = true;
  
  for (size_t i = 0; i < g_discoveredFanCount && resultCount < kMaxFastConnectFans; ++i) {
    SmartMiFanDiscoveredDevice &fan = g_discoveredFans[i];
    
    SmartMiFanFastConnectResult result{};
    result.ip = fan.ip;
    safeCopyStr(result.token, sizeof(result.token), fan.token);
    result.success = false;
    
    // Try handshake
    SmartMiFanAsync.attachUdp(udp);
    if (!SmartMiFanAsync.setTokenFromHex(fan.token)) {
      results[resultCount++] = result;
      overallSuccess = false;
      continue;
    }
    SmartMiFanAsync.setFanAddress(fan.ip);
    
    udp.stop();
    udp.begin(0);
    
    if (!SmartMiFanAsync.handshake()) {
      fan.ready = false;
      fan.lastError = MiioErr::TIMEOUT;
      results[resultCount++] = result;
      overallSuccess = false;
      continue;
    }
    
    fan.ready = true;
    fan.lastError = MiioErr::OK;
    
    // Check if model already provided - skip queryInfo if so
    if (fan.model[0] != '\0') {
      result.success = true;
      
      // Cache crypto data if not already cached
      if (!fan.cryptoCached) {
        cacheFanCrypto(fan);
      }
    } else {
      // Query device info to get model
      // Non-blocking wait after handshake (yield instead of delay)
      unsigned long waitStart = millis();
      while (millis() - waitStart < 100) {
        yield();
      }
      
      char model[24] = {0};
      char fw[16] = {0};
      char hw[16] = {0};
      uint32_t did = 0;
      
      if (SmartMiFanAsync.queryInfo(model, sizeof(model), fw, sizeof(fw), hw, sizeof(hw), &did)) {
        safeCopyStr(fan.model, sizeof(fan.model), model);
        safeCopyStr(fan.fw_ver, sizeof(fan.fw_ver), fw);
        safeCopyStr(fan.hw_ver, sizeof(fan.hw_ver), hw);
        if (did != 0) fan.did = did;
        
        result.success = true;
        
        // Re-cache crypto data with updated model
        fan.cryptoCached = false;
        cacheFanCrypto(fan);
      } else {
        result.success = false;
        overallSuccess = false;
      }
    }
    
    results[resultCount++] = result;
  }
  
  // Invoke callback if set
  if (g_fastConnectCallback && resultCount > 0) {
    g_fastConnectCallback(results, resultCount);
  }
  
  return overallSuccess;
}

// =========================
// Smart Connect API
// =========================

// Internal callback for Smart Connect to collect failed fans and remove them from discovered list
static void SmartConnectCollectFailedFans(const SmartMiFanFastConnectResult results[], size_t count) {
  g_smartConnectContext.failedTokenCount = 0;
  
  for (size_t i = 0; i < count && i < kMaxFastConnectFans; ++i) {
    if (!results[i].success) {
      // Find matching token for this IP
      for (size_t j = 0; j < g_fastConnectConfigCount; ++j) {
        if (g_fastConnectConfig[j].ip == results[i].ip) {
          if (g_smartConnectContext.failedTokenCount < kMaxFastConnectFans) {
            g_smartConnectContext.failedTokens[g_smartConnectContext.failedTokenCount++] = 
              g_fastConnectConfig[j].token;
          }
          break;
        }
      }
      
      // Remove failed fan from discovered list
      for (size_t k = 0; k < g_discoveredFanCount; ++k) {
        if (g_discoveredFans[k].ip == results[i].ip) {
          // Shift remaining fans down
          for (size_t m = k; m < g_discoveredFanCount - 1; ++m) {
            g_discoveredFans[m] = g_discoveredFans[m + 1];
          }
          g_discoveredFanCount--;
          break;
        }
      }
    }
    
    #if defined(FAN_DEBUG_GEN)
    char ipStr[16];
    ipToStr(results[i].ip, ipStr, sizeof(ipStr));
    FAN_LOGI_F("  [%s] IP=%s", results[i].success ? "PASS" : "FAIL", ipStr);
    #endif
  }
  
  // Call original callback if set
  if (g_originalFastConnectCallback) {
    g_originalFastConnectCallback(results, count);
  }
}

bool SmartMiFanAsync_startSmartConnect(WiFiUDP &udp, unsigned long discoveryMs) {
  if (g_smartConnectContext.state != SmartConnectState::IDLE) return false;
  
  g_smartConnectContext.reset();
  g_smartConnectContext.udp = &udp;
  g_smartConnectContext.discoveryMs = discoveryMs;
  
  // Phase 1: Fast Connect registration
  if (g_fastConnectConfigCount > 0 && g_useFastConnect) {
    // Register Fast Connect fans
    SmartMiFanAsync_registerFastConnectFans(udp);
    
    // Set up internal callback to collect failed fans
    g_originalFastConnectCallback = g_fastConnectCallback;
    g_fastConnectCallback = SmartConnectCollectFailedFans;
    
    g_smartConnectContext.state = SmartConnectState::VALIDATING_FAST_CONNECT;
  } else {
    // No Fast Connect config - Smart Connect requires tokens from Fast Connect
    // Without tokens, there's nothing to discover, so complete immediately
    // Note: Callers should add fans with IP 0.0.0.0 to Fast Connect config;
    // validation will fail and Discovery will start with those tokens
    g_smartConnectContext.state = SmartConnectState::COMPLETE;
  }
  
  return true;
}

bool SmartMiFanAsync_updateSmartConnect() {
  if (g_smartConnectContext.state == SmartConnectState::IDLE ||
      g_smartConnectContext.state == SmartConnectState::COMPLETE) {
    return false;
  }
  
  switch (g_smartConnectContext.state) {
    case SmartConnectState::VALIDATING_FAST_CONNECT:
      if (!g_smartConnectContext.fastConnectValidated) {
        // Run validation (synchronous)
        SmartMiFanAsync_validateFastConnectFans(*g_smartConnectContext.udp);
        g_smartConnectContext.fastConnectValidated = true;
        
        // Restore original callback
        g_fastConnectCallback = g_originalFastConnectCallback;
        
        // Move to discovery if we have failed tokens
        if (g_smartConnectContext.failedTokenCount > 0) {
          g_smartConnectContext.state = SmartConnectState::STARTING_DISCOVERY;
        } else {
          g_smartConnectContext.state = SmartConnectState::COMPLETE;
          return false;
        }
      }
      return true;
      
    case SmartConnectState::STARTING_DISCOVERY:
      // Start discovery with failed tokens
      if (g_smartConnectContext.failedTokenCount > 0) {
        SmartMiFanAsync_startDiscovery(*g_smartConnectContext.udp, 
          g_smartConnectContext.failedTokens, 
          g_smartConnectContext.failedTokenCount,
          g_smartConnectContext.discoveryMs);
        g_smartConnectContext.state = SmartConnectState::DISCOVERING;
      } else {
        g_smartConnectContext.state = SmartConnectState::COMPLETE;
        return false;
      }
      return true;
      
    case SmartConnectState::DISCOVERING:
      if (SmartMiFanAsync_updateDiscovery()) {
        return true;  // Still in progress
      }
      // Discovery complete
      g_smartConnectContext.state = SmartConnectState::COMPLETE;
      return false;
      
    default:
      return false;
  }
}

bool SmartMiFanAsync_isSmartConnectComplete() {
  return g_smartConnectContext.state == SmartConnectState::COMPLETE;
}

bool SmartMiFanAsync_isSmartConnectInProgress() {
  return g_smartConnectContext.state != SmartConnectState::IDLE &&
         g_smartConnectContext.state != SmartConnectState::COMPLETE;
}

SmartConnectState SmartMiFanAsync_getSmartConnectState() {
  return g_smartConnectContext.state;
}

void SmartMiFanAsync_cancelSmartConnect() {
  // Cancel any running discovery
  SmartMiFanAsync_cancelDiscovery();
  
  // Restore callback if needed
  if (g_originalFastConnectCallback) {
    g_fastConnectCallback = g_originalFastConnectCallback;
    g_originalFastConnectCallback = nullptr;
  }
  
  g_smartConnectContext.state = SmartConnectState::IDLE;
  g_smartConnectContext.reset();
}
