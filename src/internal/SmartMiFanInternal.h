#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiUdp.h>
#include <string.h>
#include <stdint.h>

#include "SmartMiFanAsync.h"
#include "MiioCodec.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#endif

namespace SmartMiFanInternal {

constexpr uint8_t kMaxFans = SMARTMIFAN_MAX_FANS;
constexpr uint8_t kMaxCandidates = SMARTMIFAN_MAX_CANDIDATES;
constexpr uint16_t kRxBuffer = SMARTMIFAN_RX_BUFFER;
constexpr uint16_t kTxBuffer = 320;
constexpr uint8_t kEventDepth = static_cast<uint8_t>(8 * SMARTMIFAN_MAX_FANS + 16);
constexpr uint16_t kHelloTimeoutMs = 500;
constexpr uint8_t kHelloRetries = 3;
constexpr uint16_t kReplyTimeoutMs = 1500;
constexpr uint16_t kGroupTimeoutMs = 4000;
constexpr uint16_t kHelloIntervalMs = 500;
constexpr uint16_t kFastConnectMs = 3500;
constexpr uint16_t kMiioPort = 54321;
constexpr uint8_t kPowerSiid = 2;
constexpr uint8_t kPowerPiid = 1;

enum class SessionState : uint8_t { EMPTY, UNKNOWN, HANDSHAKING, READY, BUSY, ERROR };
enum class ReqKind : uint8_t { NONE, HELLO, INFO, SET_PROPERTY };
enum class ConnectPhase : uint8_t { IDLE, FAST_CONNECT, DISCOVERY, DONE };

struct DeferredCmd {
  CmdKind kind = CmdKind::NONE;
  uint8_t value = 0;
  bool isGroup = false;
  bool retried = false;
};

struct MiioSession {
  IPAddress ip;
  IPAddress ipBeforeProbe;
  uint8_t token[16]{};
  uint8_t key[16]{};
  uint8_t iv[16]{};
  FanModelType modelType = FanModelType::UNKNOWN;
  uint8_t deviceId[4]{};
  uint32_t deviceTs = 0;
  uint32_t lastHandshakeMs = 0;
  MiioErr errorBeforeProbe = MiioErr::OK;
  ReqKind reqKind = ReqKind::NONE;
  CmdKind reqCmd = CmdKind::NONE;
  uint32_t reqId = 0;
  uint32_t reqSentMs = 0;
  uint16_t reqTimeoutMs = 0;
  uint8_t helloRetries = 0;
  DeferredCmd deferred[3];
  SessionState state = SessionState::EMPTY;
  MiioErr lastError = MiioErr::OK;
  bool userEnabled = false;
  bool softActive = false;
  int8_t probingCandidate = -1;
};

struct FanSlot {
  MiioSession s;
  char model[24]{};
  char fw_ver[16]{};
  char hw_ver[16]{};
  char token[33]{};
};

struct GroupSlot {
  bool active = false;
  CmdKind kind = CmdKind::NONE;
  uint8_t value = 0;
  Scope scope = Scope::ALL;
  uint8_t seq = 0;
  uint32_t startMs = 0;
  FanMask pendingMask = 0;
  FanMask okMask = 0;
  FanMask failMask = 0;
  bool hasNext = false;
  uint8_t nextValue = 0;
  Scope nextScope = Scope::ALL;
  uint8_t nextSeq = 0;
};

struct DiscoveryCandidate {
  IPAddress ip;
  uint8_t deviceId[4]{};
  uint32_t ts = 0;
  FanMask triedMask = 0;
  bool done = false;
};

struct DiscoveryState {
  bool active = false;
  bool collecting = false;
  uint32_t startMs = 0;
  uint32_t durationMs = 0;
  uint32_t lastHelloMs = 0;
  uint32_t deadlineMs = 0;
  FanMask slotMask = 0;
  FanMask startMask = 0;
  FanMask abortMask = 0;
  DiscoveryCandidate candidates[kMaxCandidates];
  uint8_t candidateCount = 0;
};

struct MiioTransport {
  WiFiUDP* udp = nullptr;
  bool bound = false;
  uint8_t rx[kRxBuffer]{};
  uint8_t tx[kTxBuffer]{};
  uint32_t nextMsgId = 1;
};

struct Manager {
  MiioTransport transport;
  FanSlot fans[kMaxFans];
  FanMask usedMask = 0;
  uint8_t nextGroupSeq = 1;
  DiscoveryState discovery;
  ConnectPhase connectPhase = ConnectPhase::IDLE;
  uint32_t connectDeadlineMs = 0;
  uint32_t connectDiscoveryMs = 3000;
  FanMask infoSentMask = 0;
  GroupSlot power;
  GroupSlot speed;
  GroupSlot handshakeGroup;
  uint32_t lastPowerCmdMs = 0;
  uint32_t lastSpeedCmdMs = 0;
  uint32_t handshakeTtlMs = SMARTMIFAN_HANDSHAKE_TTL_S * 1000UL;
  EventInfo eventQueue[kEventDepth];
  uint8_t eventCount = 0;
  uint32_t eventsDropped = 0;
  EventCallback onEvent = nullptr;
  bool inUpdate = false;
#if defined(ARDUINO_ARCH_ESP32)
  portMUX_TYPE infoMux = portMUX_INITIALIZER_UNLOCKED;
  TaskHandle_t updateTask = nullptr;
#endif
};

inline bool elapsedAtLeast(uint32_t now, uint32_t start, uint32_t dur) {
  return static_cast<uint32_t>(now - start) >= dur;
}

inline bool deviceKnown(const uint8_t id[4]) {
  return (id[0] | id[1] | id[2] | id[3]) != 0;
}

inline FanMask bitOf(uint8_t i) {
  return static_cast<FanMask>(1u << i);
}

inline uint8_t idx(CmdKind kind) {
  switch (kind) {
    case CmdKind::POWER: return 0;
    case CmdKind::SPEED: return 1;
    case CmdKind::HANDSHAKE: return 2;
    default: return 0;
  }
}

inline bool tokenEqHex(const char* a, const char* b) {
  if (!a || !b) return false;
  for (int i = 0; i < 32; ++i) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'F') ca = static_cast<char>(ca + 32);
    if (cb >= 'A' && cb <= 'F') cb = static_cast<char>(cb + 32);
    if (ca != cb) return false;
  }
  return true;
}

inline bool deriveReady(const MiioSession& s) {
  if (s.state == SessionState::READY || s.state == SessionState::BUSY) return true;
  return s.state == SessionState::HANDSHAKING && s.lastError == MiioErr::OK && deviceKnown(s.deviceId);
}

inline FanParticipationState deriveParticipation(const MiioSession& s) {
  if (s.state == SessionState::EMPTY) return FanParticipationState::INACTIVE;
  const bool ready = deriveReady(s);
  if (s.userEnabled && (s.lastError == MiioErr::OK || s.softActive)) {
    return FanParticipationState::ACTIVE;
  }
  if (s.lastError != MiioErr::OK && !ready) return FanParticipationState::ERROR;
  return FanParticipationState::INACTIVE;
}

inline FanModelType modelStringToType(const char* model) {
  if (!model || !model[0]) return FanModelType::UNKNOWN;
  const size_t len = strlen(model);
  if (len < 3) return FanModelType::UNKNOWN;
  const char* s = model + len - 3;
  const uint32_t suffix = (static_cast<uint32_t>(s[0]) << 16) |
                          (static_cast<uint32_t>(s[1]) << 8) |
                          static_cast<uint32_t>(s[2]);
  switch (suffix) {
    case ('z' << 16) | ('a' << 8) | '5': return FanModelType::ZHIMI_FAN_ZA5;
    case ('z' << 16) | ('a' << 8) | '4':
    case ('z' << 16) | ('a' << 8) | '3': return FanModelType::ZHIMI_FAN_ZA4;
    case ('.' << 16) | ('v' << 8) | '3':
    case ('.' << 16) | ('v' << 8) | '2': return FanModelType::ZHIMI_FAN_V3;
    case ('.' << 16) | ('1' << 8) | 'c': return FanModelType::DMAKER_FAN_1C;
    case ('.' << 16) | ('p' << 8) | '5': return FanModelType::DMAKER_FAN_P5;
    case ('.' << 16) | ('p' << 8) | '8':
    case ('.' << 16) | ('p' << 8) | '9': return FanModelType::DMAKER_FAN_P9;
    case ('p' << 16) | ('1' << 8) | '0':
    case ('p' << 16) | ('1' << 8) | '8': return FanModelType::DMAKER_FAN_P10;
    case ('p' << 16) | ('1' << 8) | '1':
    case ('p' << 16) | ('1' << 8) | '5':
    case ('p' << 16) | ('3' << 8) | '0':
    case ('p' << 16) | ('3' << 8) | '3': return FanModelType::DMAKER_FAN_P11;
    case ('p' << 16) | ('7' << 8) | '6': return FanModelType::XIAOMI_FAN_P76;
    case ('2' << 16) | ('2' << 8) | '0': return FanModelType::DMAKER_FAN_P11;
    default: return FanModelType::UNKNOWN;
  }
}

inline bool isSupportedModel(const char* model) {
  return modelStringToType(model) != FanModelType::UNKNOWN;
}

inline void getSpeedParamsByType(FanModelType type, int& siid, int& piid, bool& useFanLevel) {
  useFanLevel = false;
  switch (type) {
    case FanModelType::DMAKER_FAN_1C:
      siid = 2;
      piid = 2;
      useFanLevel = true;
      return;
    case FanModelType::DMAKER_FAN_P9:
      siid = 2;
      piid = 11;
      return;
    case FanModelType::DMAKER_FAN_P10:
      siid = 2;
      piid = 10;
      return;
    case FanModelType::DMAKER_FAN_P11:
    case FanModelType::DMAKER_FAN_P5:
      siid = 2;
      piid = 6;
      return;
    case FanModelType::XIAOMI_FAN_P76:
      siid = 2;
      piid = 5;
      return;
    case FanModelType::ZHIMI_FAN_ZA5:
    case FanModelType::ZHIMI_FAN_ZA4:
    case FanModelType::ZHIMI_FAN_V3:
    case FanModelType::UNKNOWN:
    default:
      siid = 6;
      piid = 8;
      return;
  }
}

}  // namespace SmartMiFanInternal
