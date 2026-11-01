#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>

#define SMART_MI_FAN_ASYNC_VERSION "2.0.0"
#define SMART_MI_FAN_ASYNC_VERSION_MAJOR 2
#define SMART_MI_FAN_ASYNC_VERSION_MINOR 0
#define SMART_MI_FAN_ASYNC_VERSION_PATCH 0

#ifndef SMARTMIFAN_MAX_FANS
#define SMARTMIFAN_MAX_FANS 4
#endif
static_assert(SMARTMIFAN_MAX_FANS >= 1 && SMARTMIFAN_MAX_FANS <= 8,
              "slot masks are uint8_t");

#ifndef SMARTMIFAN_MAX_CANDIDATES
#define SMARTMIFAN_MAX_CANDIDATES 16
#endif
#ifndef SMARTMIFAN_RX_BUFFER
#define SMARTMIFAN_RX_BUFFER 768
#endif
#ifndef SMARTMIFAN_HANDSHAKE_TTL_S
#define SMARTMIFAN_HANDSHAKE_TTL_S 60
#endif
#ifndef SMARTMIFAN_POWER_COOLDOWN_MS
#define SMARTMIFAN_POWER_COOLDOWN_MS 100
#endif
#ifndef SMARTMIFAN_SPEED_COOLDOWN_MS
#define SMARTMIFAN_SPEED_COOLDOWN_MS 1000
#endif
#ifndef SMARTMIFAN_STRICT_RESPONSES
#define SMARTMIFAN_STRICT_RESPONSES 0
#endif

#if defined(__has_include)
#if __has_include("DebugConfig.h")
#include "DebugConfig.h"
#elif __has_include(<DebugConfig.h>)
#include <DebugConfig.h>
#endif
#endif

#if defined(FAN_DEBUG_GEN_DISABLE)
#undef FAN_DEBUG_GEN
#elif !defined(FAN_DEBUG_GEN)
#define FAN_DEBUG_GEN
#endif

#if defined(FAN_DEBUG_GEN)
#define FAN_LOGI_F(...) do { Serial.printf("[I] " __VA_ARGS__); Serial.println(); } while (0)
#define FAN_LOGW_F(...) do { Serial.printf("[W] " __VA_ARGS__); Serial.println(); } while (0)
#define FAN_LOGE_F(...) do { Serial.printf("[E] " __VA_ARGS__); Serial.println(); } while (0)
#else
#define FAN_LOGI_F(...) do {} while (0)
#define FAN_LOGW_F(...) do {} while (0)
#define FAN_LOGE_F(...) do {} while (0)
#endif

#if defined(FAN_DEBUG_HOT)
#define FAN_LOGHOT_F(...) do { Serial.printf("[H] " __VA_ARGS__); Serial.println(); } while (0)
#else
#define FAN_LOGHOT_F(...) do {} while (0)
#endif

#if defined(FAN_DEBUG_NET)
#define FAN_LOGNET_F(...) do { Serial.printf("[N] " __VA_ARGS__); Serial.println(); } while (0)
#else
#define FAN_LOGNET_F(...) do {} while (0)
#endif

typedef uint8_t FanMask;
#define SMARTMIFAN_ALL_MASK \
  ((FanMask)((1u << SMARTMIFAN_MAX_FANS) - 1u))

enum class FanParticipationState : uint8_t {
  ACTIVE,
  INACTIVE,
  ERROR
};

enum class FanModelType : uint8_t {
  UNKNOWN = 0,
  ZHIMI_FAN_ZA5,
  ZHIMI_FAN_ZA4,
  ZHIMI_FAN_V3,
  DMAKER_FAN_1C,
  DMAKER_FAN_P5,
  DMAKER_FAN_P9,
  DMAKER_FAN_P10,
  DMAKER_FAN_P11,
  XIAOMI_FAN_P76
};

enum class Scope : uint8_t {
  ALL,
  PARTICIPATING
};

enum class CmdResult : uint8_t {
  STARTED,
  QUEUED,
  REJECTED
};

enum class CmdKind : uint8_t {
  NONE,
  POWER,
  SPEED,
  HANDSHAKE
};

enum class MiioErr : uint8_t {
  OK,
  TIMEOUT,
  WRONG_SOURCE_IP,
  WRONG_DEVICE,
  DECRYPT_FAIL,
  INVALID_RESPONSE
};

struct CmdAccept {
  CmdResult result;
  uint8_t seq;
};

struct SmartMiFanInfo {
  IPAddress ip;
  uint32_t did;
  char model[24];
  char fw_ver[16];
  char hw_ver[16];
  char token[33];
  bool ready;
  bool userEnabled;
  bool softActive;
  bool used;
  MiioErr lastError;
  FanParticipationState participation;
};

enum class EventType : uint8_t {
  FAN_READY,
  FAN_ERROR,
  FAN_DEVICE_ERROR,
  FAN_INFO,
  CMD_DONE,
  GROUP_DONE,
  CONNECT_DONE,
  DISCOVERY_DONE,
  CANCELLED
};

struct EventInfo {
  EventType type;
  int8_t fanIndex;
  MiioErr error;
  CmdKind kind;
  uint8_t seq;
  FanMask okMask;
  FanMask failMask;
  bool ok;
  bool allOk;
  bool anyOk;
  bool restarted;
};

typedef void (*EventCallback)(const EventInfo&);

namespace SmartMiFan {

bool begin(WiFiUDP& udp);
void end();
void update();
void setHandshakeTtl(uint32_t seconds);
uint32_t handshakeTtl();

void clear();
bool add(uint8_t index, const char* ipStr, const char* tokenHex,
         const char* model = nullptr);
bool remove(uint8_t index);
FanMask used();
bool info(uint8_t index, SmartMiFanInfo& out);
int indexByToken(const char* tokenHex);

bool connect(uint32_t discoveryMs = 3000);
bool discover(FanMask slots, uint32_t durationMs = 3000);
void cancel();
bool connecting();
bool discovering();

void setEnabled(uint8_t index, bool enabled);
void setSoftActive(uint8_t index, bool soft);
FanParticipationState participation(uint8_t index);
FanMask participating();

CmdResult setPower(uint8_t index, bool on);
CmdResult setSpeed(uint8_t index, uint8_t pct);
CmdResult handshake(uint8_t index);
CmdAccept setPowerAll(bool on, Scope scope = Scope::PARTICIPATING);
CmdAccept setSpeedAll(uint8_t pct, Scope scope = Scope::PARTICIPATING);
CmdAccept handshakeAll(Scope scope = Scope::ALL);
bool commandPending();
bool commandPending(CmdKind kind);

void prepareForSleep(bool closeSocket);
void wake();

void setEventCallback(EventCallback cb);

}  // namespace SmartMiFan
