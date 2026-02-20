// =============================================================================
// SmartMiFanAsync - Orchestration Module (Phase 5: File Split)
// =============================================================================
// Contains: Error/Health APIs, Sleep Hooks, Fan Participation, Command Coalescing
// =============================================================================

#include "SmartMiFanInternal.h"

using namespace SmartMiFanInternal;

// =========================
// Command Coalescing State
// =========================
namespace {
  unsigned long g_lastCommandTime = 0;
  constexpr unsigned long kCommandCooldownMs = 100;  // Minimum time between orchestrated commands
}

// =========================
// Error and Health API
// =========================

void SmartMiFanAsync_setErrorCallback(FanErrorCallback cb) {
  g_errorCallback = cb;
}

bool SmartMiFanAsync_isFanReady(uint8_t fanIndex) {
  if (fanIndex >= g_discoveredFanCount) return false;
  return g_discoveredFans[fanIndex].ready;
}

MiioErr SmartMiFanAsync_getFanLastError(uint8_t fanIndex) {
  if (fanIndex >= g_discoveredFanCount) return MiioErr::TIMEOUT;
  return g_discoveredFans[fanIndex].lastError;
}

bool SmartMiFanAsync_healthCheck(uint8_t fanIndex, uint32_t timeoutMs) {
  if (fanIndex >= g_discoveredFanCount) return false;
  if (!g_udpContext) return false;
  
  const SmartMiFanDiscoveredDevice &fan = g_discoveredFans[fanIndex];
  
  if (!prepareFanContext(fan)) return false;
  
  // Try handshake as health check
  bool success = SmartMiFanAsync.handshake(timeoutMs);
  
  if (success) {
    g_discoveredFans[fanIndex].ready = true;
    g_discoveredFans[fanIndex].lastError = MiioErr::OK;
  } else {
    g_discoveredFans[fanIndex].ready = false;
    g_discoveredFans[fanIndex].lastError = MiioErr::TIMEOUT;
  }
  
  return success;
}

bool SmartMiFanAsync_healthCheckAll(uint32_t timeoutMs) {
  if (!g_udpContext) return false;
  
  bool allHealthy = true;
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    if (!SmartMiFanAsync_healthCheck(static_cast<uint8_t>(i), timeoutMs)) {
      allHealthy = false;
    }
  }
  return allHealthy;
}

// =========================
// Sleep/Wake Hooks
// =========================

void SmartMiFanAsync_prepareForSleep(bool closeUdp, bool invalidateHandshake) {
  // Mark all fans as not ready
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    g_discoveredFans[i].ready = false;
  }
  
  // Close UDP if requested
  if (closeUdp && g_udpContext) {
    g_udpContext->stop();
  }
  
  // Invalidate handshake cache if requested
  if (invalidateHandshake) {
    SmartMiFanAsync.invalidateHandshake();
  }
  
  // Reset coalescing timer
  g_lastCommandTime = 0;
}

void SmartMiFanAsync_softWakeUp() {
  // Re-initialize UDP if context exists
  if (g_udpContext) {
    g_udpContext->begin(0);
  }
  
  // Mark fans as needing re-handshake
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    g_discoveredFans[i].ready = false;
    g_discoveredFans[i].cryptoCached = false;
  }
}

// =========================
// Fan Participation State API
// =========================

FanParticipationState SmartMiFanAsync_getFanParticipationState(uint8_t fanIndex) {
  if (fanIndex >= g_discoveredFanCount) return FanParticipationState::ERROR;
  
  const SmartMiFanDiscoveredDevice &fan = g_discoveredFans[fanIndex];
  
  if (!fan.userEnabled) {
    return FanParticipationState::INACTIVE;
  }
  
  if (fan.lastError != MiioErr::OK && !g_softActive[fanIndex]) {
    return FanParticipationState::ERROR;
  }
  
  return FanParticipationState::ACTIVE;
}

void SmartMiFanAsync_setFanEnabled(uint8_t fanIndex, bool enabled) {
  if (fanIndex >= g_discoveredFanCount) return;
  g_discoveredFans[fanIndex].userEnabled = enabled;
}

bool SmartMiFanAsync_isFanEnabled(uint8_t fanIndex) {
  if (fanIndex >= g_discoveredFanCount) return false;
  return g_discoveredFans[fanIndex].userEnabled;
}

void SmartMiFanAsync_setFanSoftActive(uint8_t fanIndex, bool enabled) {
  if (fanIndex >= kMaxSmartMiFans) return;
  g_softActive[fanIndex] = enabled;
}

// =========================
// Orchestrated Commands with Coalescing
// =========================

bool SmartMiFanAsync_handshakeAllOrchestrated() {
  if (!g_udpContext) return false;
  
  bool anySuccess = false;
  
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    SmartMiFanDiscoveredDevice &fan = g_discoveredFans[i];
    
    // Skip disabled fans
    if (!fan.userEnabled) continue;
    
    // Skip fans in error state (they need health check first)
    if (fan.lastError != MiioErr::OK && !fan.ready) continue;
    
    if (!prepareFanContext(fan)) continue;
    
    if (SmartMiFanAsync.handshake()) {
      fan.ready = true;
      fan.lastError = MiioErr::OK;
      anySuccess = true;
    } else {
      fan.ready = false;
      fan.lastError = MiioErr::TIMEOUT;
    }
  }
  
  return anySuccess;
}

bool SmartMiFanAsync_setPowerAllOrchestrated(bool on) {
  if (!g_udpContext) return false;
  
  // Command coalescing - skip if called too frequently
  unsigned long now = millis();
  if (g_lastCommandTime > 0 && (now - g_lastCommandTime) < kCommandCooldownMs) {
    return true;  // Silently succeed (command coalesced)
  }
  g_lastCommandTime = now;
  
  bool anySuccess = false;
  
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    SmartMiFanDiscoveredDevice &fan = g_discoveredFans[i];
    
    // Only send to ACTIVE fans
    FanParticipationState participation = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    if (participation != FanParticipationState::ACTIVE) continue;
    
    if (!prepareFanContext(fan)) {
      fan.lastError = MiioErr::TIMEOUT;
      // DBG_FAN_TIMEOUT: log prepare context failure before setPower
      FAN_LOGW_F("[DBG_FAN_TIMEOUT] prepareFanContext failed (setPower): fanIndex=%u ip=%d.%d.%d.%d t=%lums",
                 (unsigned)i, fan.ip[0], fan.ip[1], fan.ip[2], fan.ip[3], (unsigned long)millis());
      continue;
    }
    
    if (SmartMiFanAsync.setPower(on)) {
      fan.ready = true;
      fan.lastError = MiioErr::OK;
      anySuccess = true;
    } else {
      fan.ready = false;
      // lastError is set by miotSetPropertyBool
    }
  }
  
  return anySuccess;
}

bool SmartMiFanAsync_setSpeedAllOrchestrated(uint8_t percent) {
  if (!g_udpContext) return false;
  
  // Command coalescing - skip if called too frequently
  unsigned long now = millis();
  if (g_lastCommandTime > 0 && (now - g_lastCommandTime) < kCommandCooldownMs) {
    return true;  // Silently succeed (command coalesced)
  }
  g_lastCommandTime = now;
  
  bool anySuccess = false;
  
  for (size_t i = 0; i < g_discoveredFanCount; ++i) {
    SmartMiFanDiscoveredDevice &fan = g_discoveredFans[i];
    
    // Only send to ACTIVE fans
    FanParticipationState participation = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    if (participation != FanParticipationState::ACTIVE) continue;
    
    if (!prepareFanContext(fan)) {
      fan.lastError = MiioErr::TIMEOUT;
      // DBG_FAN_TIMEOUT: log prepare context failure before setSpeed
      FAN_LOGW_F("[DBG_FAN_TIMEOUT] prepareFanContext failed (setSpeed): fanIndex=%u ip=%d.%d.%d.%d t=%lums",
                 (unsigned)i, fan.ip[0], fan.ip[1], fan.ip[2], fan.ip[3], (unsigned long)millis());
      continue;
    }
    
    if (SmartMiFanAsync.setSpeed(percent)) {
      fan.ready = true;
      fan.lastError = MiioErr::OK;
      anySuccess = true;
    } else {
      fan.ready = false;
      // lastError is set by miotSetPropertyUint
    }
  }
  
  return anySuccess;
}

// =========================
// Utility Functions
// =========================

size_t SmartMiFanAsync_getDiscoveredFanCount() {
  return g_discoveredFanCount;
}

const SmartMiFanDiscoveredDevice* SmartMiFanAsync_getFan(uint8_t index) {
  if (index >= g_discoveredFanCount) return nullptr;
  return &g_discoveredFans[index];
}
