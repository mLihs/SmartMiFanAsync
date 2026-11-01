namespace SmartMiFanInternal {

static Manager g;

static void enqueueEvent(const EventInfo& ev);
static void finishCommand(uint8_t i, CmdKind kind, bool ok);
static void clearRequest(MiioSession& s);
static void clearDeferred(MiioSession& s, CmdKind kind);
static void clearAllDeferred(MiioSession& s);
static void setState(uint8_t i, SessionState st, MiioErr err);
static bool sendHello(uint8_t i, CmdKind cmd);
static bool sendInfo(uint8_t i);
static bool sendSetProperty(uint8_t i, CmdKind kind, uint8_t value);
static void probeFailed(uint8_t i);
static void probeSucceeded(uint8_t i);
static CmdAccept startGroupWithSeq(GroupSlot& gs, CmdKind kind, uint8_t value, Scope scope, uint8_t seq);
static FanMask scopeMask(CmdKind kind, Scope scope);
static void abortDiscovery(bool restarted);
static bool transportBegin();
static void transportEnd();

#if defined(ARDUINO_ARCH_ESP32)
static void enterInfo() { portENTER_CRITICAL(&g.infoMux); }
static void exitInfo() { portEXIT_CRITICAL(&g.infoMux); }
#else
static void enterInfo() {}
static void exitInfo() {}
#endif

static void warnWrongTask(const char* api) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FAN_DEBUG_GEN)
  if (g.updateTask && xTaskGetCurrentTaskHandle() != g.updateTask) {
    FAN_LOGW_F("mutating %s from other task", api);
  }
#else
  (void)api;
#endif
}

static bool bound() {
  return g.transport.bound && g.transport.udp;
}

static bool connectingNow() {
  return g.connectPhase == ConnectPhase::FAST_CONNECT ||
         g.connectPhase == ConnectPhase::DISCOVERY;
}

static bool anyCommandPending() {
  if (g.power.active || g.power.hasNext) return true;
  if (g.speed.active || g.speed.hasNext) return true;
  if (g.handshakeGroup.active || g.handshakeGroup.hasNext) return true;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    const MiioSession& s = g.fans[i].s;
    if (s.reqKind != ReqKind::NONE) return true;
    for (uint8_t k = 0; k < 3; ++k) {
      if (s.deferred[k].kind != CmdKind::NONE) return true;
    }
  }
  return false;
}

static bool kindPending(CmdKind kind) {
  const GroupSlot* gs = (kind == CmdKind::POWER) ? &g.power :
                        (kind == CmdKind::SPEED) ? &g.speed :
                        (kind == CmdKind::HANDSHAKE) ? &g.handshakeGroup : nullptr;
  if (gs && (gs->active || gs->hasNext)) return true;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    const MiioSession& s = g.fans[i].s;
    if (s.reqCmd == kind) return true;
    if (kind != CmdKind::NONE && s.deferred[idx(kind)].kind == kind) return true;
  }
  return false;
}

static void discardUdp(WiFiUDP* udp) {
  if (!udp) return;
  uint8_t dump[64];
  while (udp->available() > 0) {
    const int n = udp->available();
    udp->read(dump, n > static_cast<int>(sizeof(dump)) ? sizeof(dump) : n);
  }
}

static bool transportBegin() {
  if (!g.transport.udp) return false;
  if (g.transport.bound) return true;
  if (g.transport.udp->begin(0) == 0) return false;
  g.transport.bound = true;
  return true;
}

static void transportEnd() {
  if (g.transport.udp && g.transport.bound) {
    g.transport.udp->stop();
  }
  g.transport.bound = false;
}

static bool sendRaw(const IPAddress& ip, const uint8_t* data, size_t len) {
  if (!bound()) return false;
  if (!g.transport.udp->beginPacket(ip, kMiioPort)) return false;
  const size_t wrote = g.transport.udp->write(data, len);
  if (!g.transport.udp->endPacket()) return false;
  return wrote == len;
}

static bool sendHelloPacket(const IPAddress& ip) {
  uint8_t hello[32];
  memset(hello, 0xFF, sizeof(hello));
  hello[0] = 0x21;
  hello[1] = 0x31;
  hello[2] = 0x00;
  hello[3] = 0x20;
  return sendRaw(ip, hello, sizeof(hello));
}

static int slotByIp(const IPAddress& ip) {
  if (ip == IPAddress(0, 0, 0, 0)) return -1;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    if (g.fans[i].s.ip == ip) return static_cast<int>(i);
  }
  return -1;
}

static bool ipOwnedByOther(uint8_t self, const IPAddress& ip) {
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (i == self || !(g.usedMask & bitOf(i))) continue;
    if (g.fans[i].s.ip == ip) return true;
  }
  return false;
}

static void setState(uint8_t i, SessionState st, MiioErr err) {
  enterInfo();
  g.fans[i].s.state = st;
  g.fans[i].s.lastError = err;
  if (st == SessionState::READY || st == SessionState::BUSY) {
    g.fans[i].s.lastError = MiioErr::OK;
  }
  exitInfo();
}

static void clearRequest(MiioSession& s) {
  s.reqKind = ReqKind::NONE;
  s.reqCmd = CmdKind::NONE;
  s.reqId = 0;
  s.helloRetries = 0;
}

static void clearDeferred(MiioSession& s, CmdKind kind) {
  if (kind == CmdKind::NONE) return;
  s.deferred[idx(kind)] = DeferredCmd{};
}

static void clearAllDeferred(MiioSession& s) {
  s.deferred[0] = DeferredCmd{};
  s.deferred[1] = DeferredCmd{};
  s.deferred[2] = DeferredCmd{};
}

static GroupSlot& groupFor(CmdKind kind) {
  if (kind == CmdKind::POWER) return g.power;
  if (kind == CmdKind::SPEED) return g.speed;
  return g.handshakeGroup;
}

static bool isDroppable(EventType t) {
  return t == EventType::FAN_INFO || t == EventType::FAN_DEVICE_ERROR;
}

static void enqueueEvent(const EventInfo& ev) {
  if (g.eventCount >= kEventDepth) {
    int dropAt = -1;
    for (uint8_t i = 0; i < g.eventCount; ++i) {
      if (isDroppable(g.eventQueue[i].type)) {
        dropAt = i;
        break;
      }
    }
    if (dropAt < 0) {
      if (isDroppable(ev.type)) {
        g.eventsDropped++;
        return;
      }
      dropAt = 0;
    }
    if (dropAt >= 0 && dropAt + 1 < g.eventCount) {
      memmove(&g.eventQueue[dropAt], &g.eventQueue[dropAt + 1],
              sizeof(EventInfo) * (g.eventCount - dropAt - 1));
    }
    if (g.eventCount > 0) g.eventCount--;
    g.eventsDropped++;
  }
  g.eventQueue[g.eventCount++] = ev;
}

static EventInfo baseEvent(EventType type, int8_t fan = -1) {
  EventInfo ev{};
  ev.type = type;
  ev.fanIndex = fan;
  ev.error = MiioErr::OK;
  ev.kind = CmdKind::NONE;
  ev.seq = 0;
  ev.okMask = 0;
  ev.failMask = 0;
  ev.ok = false;
  ev.allOk = false;
  ev.anyOk = false;
  ev.restarted = false;
  return ev;
}

static void eventReady(uint8_t i) {
  EventInfo ev = baseEvent(EventType::FAN_READY, static_cast<int8_t>(i));
  ev.ok = true;
  enqueueEvent(ev);
}

static void eventError(uint8_t i, MiioErr err) {
  EventInfo ev = baseEvent(EventType::FAN_ERROR, static_cast<int8_t>(i));
  ev.error = err;
  enqueueEvent(ev);
}

static void eventInfo(uint8_t i) {
  enqueueEvent(baseEvent(EventType::FAN_INFO, static_cast<int8_t>(i)));
}

static void eventDeviceError(uint8_t i) {
  enqueueEvent(baseEvent(EventType::FAN_DEVICE_ERROR, static_cast<int8_t>(i)));
}

static void finishCommand(uint8_t i, CmdKind kind, bool ok) {
  if (kind == CmdKind::NONE) return;
  DeferredCmd& d = g.fans[i].s.deferred[idx(kind)];
  if (d.kind == CmdKind::NONE) return;
  if (d.isGroup) {
    GroupSlot& gs = groupFor(kind);
    gs.pendingMask = static_cast<FanMask>(gs.pendingMask & ~bitOf(i));
    if (ok) gs.okMask = static_cast<FanMask>(gs.okMask | bitOf(i));
    else gs.failMask = static_cast<FanMask>(gs.failMask | bitOf(i));
  } else {
    EventInfo ev = baseEvent(EventType::CMD_DONE, static_cast<int8_t>(i));
    ev.kind = kind;
    ev.ok = ok;
    enqueueEvent(ev);
  }
  clearDeferred(g.fans[i].s, kind);
}

static void copyInfoStrings(uint8_t i, const char* model, const char* fw, const char* hw) {
  enterInfo();
  if (model) {
    strncpy(g.fans[i].model, model, sizeof(g.fans[i].model) - 1);
    g.fans[i].model[sizeof(g.fans[i].model) - 1] = '\0';
    g.fans[i].s.modelType = modelStringToType(g.fans[i].model);
  }
  if (fw) {
    strncpy(g.fans[i].fw_ver, fw, sizeof(g.fans[i].fw_ver) - 1);
    g.fans[i].fw_ver[sizeof(g.fans[i].fw_ver) - 1] = '\0';
  }
  if (hw) {
    strncpy(g.fans[i].hw_ver, hw, sizeof(g.fans[i].hw_ver) - 1);
    g.fans[i].hw_ver[sizeof(g.fans[i].hw_ver) - 1] = '\0';
  }
  exitInfo();
}

static bool sendHello(uint8_t i, CmdKind cmd) {
  MiioSession& s = g.fans[i].s;
  if (!sendHelloPacket(s.ip)) return false;
  s.reqKind = ReqKind::HELLO;
  s.reqCmd = cmd;
  s.reqId = 0;
  s.reqSentMs = millis();
  s.reqTimeoutMs = kHelloTimeoutMs;
  const MiioErr err = s.lastError;
  setState(i, SessionState::HANDSHAKING, err);
  return true;
}

static bool sendEncryptedJson(uint8_t i, const char* json, uint32_t id, ReqKind kind, CmdKind cmd) {
  MiioSession& s = g.fans[i].s;
  uint8_t cipher[256];
  const size_t clen = MiioCodec::encrypt(s.key, s.iv,
                                         reinterpret_cast<const uint8_t*>(json),
                                         strlen(json), cipher, sizeof(cipher));
  if (clen == 0) return false;
  const uint16_t total = static_cast<uint16_t>(32 + clen);
  if (total > kTxBuffer) return false;
  s.deviceTs += 1;
  MiioCodec::buildHeader(g.transport.tx, total, s.deviceId, s.deviceTs);
  MiioCodec::writeChecksum(g.transport.tx, s.token, cipher, clen);
  memcpy(g.transport.tx + 32, cipher, clen);
  if (!sendRaw(s.ip, g.transport.tx, total)) return false;
  s.reqKind = kind;
  s.reqCmd = cmd;
  s.reqId = id;
  s.reqSentMs = millis();
  s.reqTimeoutMs = kReplyTimeoutMs;
  return true;
}

static bool sendInfo(uint8_t i) {
  MiioSession& s = g.fans[i].s;
  const uint32_t id = g.transport.nextMsgId++;
  char json[48];
  snprintf(json, sizeof(json), "{\"id\":%u,\"method\":\"miIO.info\"}", static_cast<unsigned>(id));
  if (!sendEncryptedJson(i, json, id, ReqKind::INFO, CmdKind::NONE)) return false;
  setState(i, SessionState::HANDSHAKING, s.lastError);
  return true;
}

static bool sendSetProperty(uint8_t i, CmdKind kind, uint8_t value) {
  MiioSession& s = g.fans[i].s;
  const uint32_t id = g.transport.nextMsgId++;
  char json[128];
  if (kind == CmdKind::POWER) {
    snprintf(json, sizeof(json),
             "{\"id\":%u,\"method\":\"set_properties\",\"params\":[{\"siid\":%u,\"piid\":%u,\"value\":%s}]}",
             static_cast<unsigned>(id), kPowerSiid, kPowerPiid, value ? "true" : "false");
  } else {
    int siid = 6;
    int piid = 8;
    bool useLevel = false;
    getSpeedParamsByType(s.modelType, siid, piid, useLevel);
    uint8_t pct = value;
    if (pct < 1) pct = 1;
    if (pct > 100) pct = 100;
    if (useLevel) {
      uint8_t level = 1;
      if (pct > 66) level = 3;
      else if (pct > 33) level = 2;
      snprintf(json, sizeof(json),
               "{\"id\":%u,\"method\":\"set_properties\",\"params\":[{\"siid\":%d,\"piid\":%d,\"value\":%u}]}",
               static_cast<unsigned>(id), siid, piid, level);
    } else {
      snprintf(json, sizeof(json),
               "{\"id\":%u,\"method\":\"set_properties\",\"params\":[{\"siid\":%d,\"piid\":%d,\"value\":%u}]}",
               static_cast<unsigned>(id), siid, piid, pct);
    }
  }
  if (!sendEncryptedJson(i, json, id, ReqKind::SET_PROPERTY, kind)) return false;
  setState(i, SessionState::BUSY, MiioErr::OK);
  return true;
}

static bool ttlExpired(const MiioSession& s, uint32_t now) {
  if (g.handshakeTtlMs == 0) return true;
  if (s.lastHandshakeMs == 0) return true;
  return elapsedAtLeast(now, s.lastHandshakeMs, g.handshakeTtlMs);
}

static void handleHelloReply(uint8_t i, const uint8_t* rx) {
  MiioSession& s = g.fans[i].s;
  const CmdKind cmd = s.reqCmd;
  if (deviceKnown(s.deviceId) && memcmp(rx + 8, s.deviceId, 4) != 0) {
    setState(i, SessionState::ERROR, MiioErr::WRONG_DEVICE);
    clearRequest(s);
    finishCommand(i, cmd, false);
    eventError(i, MiioErr::WRONG_DEVICE);
    return;
  }
  memcpy(s.deviceId, rx + 8, 4);
  s.deviceTs = MiioCodec::fromBe32(rx + 12);
  s.lastHandshakeMs = millis();
  setState(i, SessionState::READY, MiioErr::OK);
  clearRequest(s);
  if (cmd == CmdKind::HANDSHAKE) finishCommand(i, CmdKind::HANDSHAKE, true);
  eventReady(i);
}

static void addCandidate(const IPAddress& ip, const uint8_t* rx) {
  for (uint8_t c = 0; c < g.discovery.candidateCount; ++c) {
    if (g.discovery.candidates[c].ip == ip) {
      memcpy(g.discovery.candidates[c].deviceId, rx + 8, 4);
      g.discovery.candidates[c].ts = MiioCodec::fromBe32(rx + 12);
      return;
    }
  }
  if (g.discovery.candidateCount >= kMaxCandidates) return;
  DiscoveryCandidate& c = g.discovery.candidates[g.discovery.candidateCount++];
  c = DiscoveryCandidate{};
  c.ip = ip;
  memcpy(c.deviceId, rx + 8, 4);
  c.ts = MiioCodec::fromBe32(rx + 12);
  const uint32_t total = (g.discovery.durationMs * 3U > g.discovery.durationMs + g.discovery.candidateCount * 1500U)
                             ? g.discovery.durationMs * 3U
                             : g.discovery.durationMs + g.discovery.candidateCount * 1500U;
  g.discovery.deadlineMs = g.discovery.startMs + total;
}

static void handleEncrypted(uint8_t i, uint8_t* rx, size_t len) {
  MiioSession& s = g.fans[i].s;
  uint32_t ts = 0;
  if (!MiioCodec::parseHeader(rx, len, ts)) return;
  size_t plainLen = 0;
  if (!MiioCodec::decryptInPlace(s.key, s.iv, rx + 32, len - 32, plainLen)) {
    if (s.probingCandidate >= 0) {
      probeFailed(i);
    } else {
      const CmdKind cmd = s.reqCmd;
      setState(i, SessionState::ERROR, MiioErr::DECRYPT_FAIL);
      clearRequest(s);
      finishCommand(i, cmd, false);
      eventError(i, MiioErr::DECRYPT_FAIL);
    }
    return;
  }
  char* plain = reinterpret_cast<char*>(rx + 32);
  if (plainLen == 0 || plain[0] != '{') {
    if (s.probingCandidate >= 0) {
      probeFailed(i);
    } else {
      const CmdKind cmd = s.reqCmd;
      setState(i, SessionState::ERROR, MiioErr::DECRYPT_FAIL);
      clearRequest(s);
      finishCommand(i, cmd, false);
      eventError(i, MiioErr::DECRYPT_FAIL);
    }
    return;
  }
  if (plainLen < kRxBuffer - 32) plain[plainLen] = '\0';
  else rx[kRxBuffer - 1] = 0;
  if (strstr(plain, "\"id\"") == nullptr) {
    if (s.probingCandidate >= 0) probeFailed(i);
    else {
      const CmdKind cmd = s.reqCmd;
      setState(i, SessionState::ERROR, MiioErr::INVALID_RESPONSE);
      clearRequest(s);
      finishCommand(i, cmd, false);
      eventError(i, MiioErr::INVALID_RESPONSE);
    }
    return;
  }
  if (MiioCodec::jsonExtractUint(plain, "id") != s.reqId) return;
  s.deviceTs = ts;
  if (MiioCodec::jsonHasError(plain)) {
    eventDeviceError(i);
#if SMARTMIFAN_STRICT_RESPONSES
    const CmdKind cmd = s.reqCmd;
    setState(i, SessionState::ERROR, MiioErr::INVALID_RESPONSE);
    clearRequest(s);
    finishCommand(i, cmd, false);
    return;
#endif
  }
  if (s.reqKind == ReqKind::INFO) {
    char model[24]{};
    char fw[16]{};
    char hw[16]{};
    MiioCodec::jsonExtractString(plain, "model", model, sizeof(model));
    MiioCodec::jsonExtractString(plain, "fw_ver", fw, sizeof(fw));
    MiioCodec::jsonExtractString(plain, "hw_ver", hw, sizeof(hw));
    if (s.probingCandidate >= 0) {
      if (isSupportedModel(model)) {
        copyInfoStrings(i, model, fw, hw);
        s.lastHandshakeMs = millis();
        probeSucceeded(i);
      } else {
        g.discovery.candidates[s.probingCandidate].done = true;
        probeFailed(i);
      }
      return;
    }
    copyInfoStrings(i, model, fw, hw);
    if (g.connectPhase == ConnectPhase::FAST_CONNECT && !isSupportedModel(model)) {
      setState(i, SessionState::ERROR, MiioErr::INVALID_RESPONSE);
      clearRequest(s);
      return;
    }
    eventInfo(i);
  } else if (s.reqKind == ReqKind::SET_PROPERTY) {
    const CmdKind cmd = s.reqCmd;
    finishCommand(i, cmd, true);
  }
  setState(i, SessionState::READY, MiioErr::OK);
  clearRequest(s);
}

static void pollTransport() {
  if (!bound()) return;
  for (uint8_t n = 0; n < 16; ++n) {
    const int len = g.transport.udp->parsePacket();
    if (len <= 0) break;
    if (len > static_cast<int>(kRxBuffer)) {
      discardUdp(g.transport.udp);
      continue;
    }
    const int got = g.transport.udp->read(g.transport.rx, len);
    discardUdp(g.transport.udp);
    if (got != len) continue;
    const IPAddress sender = g.transport.udp->remoteIP();
    const int slot = slotByIp(sender);

    if (len == 32) {
      if (!MiioCodec::isHelloReply(g.transport.rx, static_cast<size_t>(len))) continue;
      if (slot >= 0 && g.fans[slot].s.reqKind == ReqKind::HELLO) {
        handleHelloReply(static_cast<uint8_t>(slot), g.transport.rx);
        continue;
      }
      if (g.discovery.collecting) {
        const bool accept = (slot < 0) ||
                            ((g.fans[slot].s.state == SessionState::UNKNOWN ||
                              g.fans[slot].s.state == SessionState::ERROR) &&
                             g.fans[slot].s.probingCandidate < 0);
        if (accept) addCandidate(sender, g.transport.rx);
      }
      continue;
    }

    if (len < 32 || slot < 0) continue;
    const ReqKind rk = g.fans[slot].s.reqKind;
    if (rk != ReqKind::INFO && rk != ReqKind::SET_PROPERTY) continue;
    handleEncrypted(static_cast<uint8_t>(slot), g.transport.rx, static_cast<size_t>(len));
  }
}

static void handleTimeouts(uint32_t now) {
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    MiioSession& s = g.fans[i].s;
    if (s.reqKind == ReqKind::NONE) continue;
    if (!elapsedAtLeast(now, s.reqSentMs, s.reqTimeoutMs)) continue;

    if (s.reqKind == ReqKind::HELLO && s.helloRetries < kHelloRetries) {
      if (sendHelloPacket(s.ip)) {
        s.helloRetries++;
        s.reqSentMs = now;
      }
      continue;
    }
    if (s.reqKind == ReqKind::INFO && s.probingCandidate >= 0) {
      probeFailed(i);
      continue;
    }
    if ((s.reqKind == ReqKind::SET_PROPERTY ||
         (s.reqKind == ReqKind::HELLO && s.reqCmd != CmdKind::NONE)) &&
        s.reqCmd != CmdKind::NONE && s.lastHandshakeMs != 0 &&
        !s.deferred[idx(s.reqCmd)].retried) {
      s.deferred[idx(s.reqCmd)].retried = true;
      s.reqKind = ReqKind::NONE;
      s.helloRetries = 0;
      s.reqId = 0;
      sendHello(i, s.reqCmd);
      continue;
    }
    const CmdKind cmd = s.reqCmd;
    setState(i, SessionState::ERROR, MiioErr::TIMEOUT);
    clearRequest(s);
    finishCommand(i, cmd, false);
    eventError(i, MiioErr::TIMEOUT);
  }
}

static void advanceDeferred() {
  const uint32_t now = millis();
  uint8_t sent = 0;
  for (uint8_t i = 0; i < kMaxFans && sent < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    MiioSession& s = g.fans[i].s;
    if (s.probingCandidate >= 0 || s.reqKind != ReqKind::NONE) continue;
    DeferredCmd* pick = nullptr;
    for (uint8_t k = 0; k < 3; ++k) {
      if (s.deferred[k].kind != CmdKind::NONE) {
        pick = &s.deferred[k];
        break;
      }
    }
    if (!pick) continue;
    if (pick->kind == CmdKind::HANDSHAKE) {
      if (sendHello(i, CmdKind::HANDSHAKE)) sent++;
      continue;
    }
    if (s.state == SessionState::UNKNOWN || s.state == SessionState::ERROR ||
        (s.state == SessionState::READY && ttlExpired(s, now))) {
      if (sendHello(i, pick->kind)) sent++;
      continue;
    }
    if (s.state == SessionState::READY) {
      if (sendSetProperty(i, pick->kind, pick->value)) sent++;
    }
  }
}

static uint8_t allocSeq() {
  uint8_t s = g.nextGroupSeq;
  if (s == 0) s = 1;
  g.nextGroupSeq = static_cast<uint8_t>(s + 1);
  if (g.nextGroupSeq == 0) g.nextGroupSeq = 1;
  return s;
}

static FanMask scopeMask(CmdKind kind, Scope scope) {
  FanMask mask = 0;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    if (g.fans[i].s.probingCandidate >= 0) continue;
    if (scope == Scope::ALL) {
      mask = static_cast<FanMask>(mask | bitOf(i));
      continue;
    }
    const MiioSession& s = g.fans[i].s;
    const bool ready = deriveReady(s);
    bool inc = false;
    if (kind == CmdKind::HANDSHAKE) {
      inc = s.userEnabled && !(s.lastError != MiioErr::OK && !ready);
    } else {
      inc = s.userEnabled && (s.lastError == MiioErr::OK || s.softActive);
    }
    if (inc) mask = static_cast<FanMask>(mask | bitOf(i));
  }
  return mask;
}

static CmdAccept startGroupWithSeq(GroupSlot& gs, CmdKind kind, uint8_t value, Scope scope, uint8_t seq) {
  const uint32_t now = millis();
  if (kind == CmdKind::POWER) g.lastPowerCmdMs = now;
  if (kind == CmdKind::SPEED) g.lastSpeedCmdMs = now;
  gs.active = true;
  gs.kind = kind;
  gs.value = value;
  gs.scope = scope;
  gs.seq = seq;
  gs.startMs = now;
  gs.okMask = 0;
  gs.failMask = 0;
  gs.pendingMask = 0;
  gs.hasNext = false;
  const FanMask mask = scopeMask(kind, scope);
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(mask & bitOf(i))) continue;
    MiioSession& s = g.fans[i].s;
    if (s.reqCmd == kind) {
      gs.failMask = static_cast<FanMask>(gs.failMask | bitOf(i));
      continue;
    }
    if (s.deferred[idx(kind)].kind == kind && !s.deferred[idx(kind)].isGroup) {
      finishCommand(i, kind, false);
    }
    s.deferred[idx(kind)] = DeferredCmd{kind, value, true, false};
    gs.pendingMask = static_cast<FanMask>(gs.pendingMask | bitOf(i));
  }
  CmdAccept acc;
  acc.result = CmdResult::STARTED;
  acc.seq = seq;
  return acc;
}

static CmdAccept startGroup(GroupSlot& gs, CmdKind kind, uint8_t value, Scope scope) {
  return startGroupWithSeq(gs, kind, value, scope, allocSeq());
}

static void emitGroupDone(GroupSlot& gs, bool allOk, bool anyOk) {
  EventInfo ev = baseEvent(EventType::GROUP_DONE);
  ev.kind = gs.kind;
  ev.seq = gs.seq;
  ev.okMask = gs.okMask;
  ev.failMask = gs.failMask;
  ev.allOk = allOk;
  ev.anyOk = anyOk;
  enqueueEvent(ev);
}

static void advanceOneGroup(GroupSlot& gs, uint32_t now, uint32_t cooldown) {
  if (!gs.active) {
    if (gs.hasNext && (cooldown == 0 || elapsedAtLeast(now, (gs.kind == CmdKind::POWER) ? g.lastPowerCmdMs : g.lastSpeedCmdMs, cooldown))) {
      startGroupWithSeq(gs, gs.kind, gs.nextValue, gs.nextScope, gs.nextSeq);
    }
    return;
  }
  if (gs.pendingMask != 0 && !elapsedAtLeast(now, gs.startMs, kGroupTimeoutMs)) return;
  if (gs.pendingMask != 0) {
    for (uint8_t i = 0; i < kMaxFans; ++i) {
      if (!(gs.pendingMask & bitOf(i))) continue;
      gs.failMask = static_cast<FanMask>(gs.failMask | bitOf(i));
      MiioSession& s = g.fans[i].s;
      if (s.reqCmd == gs.kind) {
        clearRequest(s);
        if (s.state == SessionState::BUSY || s.state == SessionState::HANDSHAKING) {
          const SessionState next = (deviceKnown(s.deviceId) && s.lastError == MiioErr::OK)
                                        ? SessionState::READY
                                        : SessionState::UNKNOWN;
          setState(i, next, s.lastError);
        }
      }
      clearDeferred(s, gs.kind);
    }
    gs.pendingMask = 0;
  }
  gs.active = false;
  emitGroupDone(gs, gs.failMask == 0, gs.okMask != 0);
}

static void advanceGroups() {
  const uint32_t now = millis();
  advanceOneGroup(g.power, now, SMARTMIFAN_POWER_COOLDOWN_MS);
  advanceOneGroup(g.speed, now, SMARTMIFAN_SPEED_COOLDOWN_MS);
  advanceOneGroup(g.handshakeGroup, now, 0);
}

static int candidateInUse(uint8_t c) {
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (g.fans[i].s.probingCandidate == static_cast<int8_t>(c)) return static_cast<int>(i);
  }
  return -1;
}

static void probeFailed(uint8_t i) {
  MiioSession& s = g.fans[i].s;
  s.ip = s.ipBeforeProbe;
  s.probingCandidate = -1;
  clearRequest(s);
  const MiioErr err = (s.errorBeforeProbe == MiioErr::OK) ? MiioErr::TIMEOUT : s.errorBeforeProbe;
  setState(i, SessionState::ERROR, err);
}

static void probeSucceeded(uint8_t i) {
  MiioSession& s = g.fans[i].s;
  if (s.probingCandidate >= 0) {
    g.discovery.candidates[s.probingCandidate].done = true;
  }
  s.probingCandidate = -1;
  setState(i, SessionState::READY, MiioErr::OK);
  clearRequest(s);
  eventReady(i);
  eventInfo(i);
  g.discovery.slotMask = static_cast<FanMask>(g.discovery.slotMask & ~bitOf(i));
}

static void abortDiscovery(bool restarted) {
  const FanMask abortMask = g.discovery.slotMask;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (g.fans[i].s.probingCandidate >= 0) probeFailed(i);
  }
  g.discovery.active = false;
  g.discovery.collecting = false;
  EventInfo ev = baseEvent(EventType::DISCOVERY_DONE);
  ev.okMask = 0;
  ev.failMask = abortMask;
  ev.restarted = restarted;
  ev.allOk = false;
  ev.anyOk = false;
  enqueueEvent(ev);
}

static void finishDiscovery() {
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (g.fans[i].s.probingCandidate >= 0) probeFailed(i);
  }
  const FanMask failMask = g.discovery.slotMask;
  const FanMask okMask = static_cast<FanMask>(g.discovery.startMask & ~failMask);
  g.discovery.active = false;
  g.discovery.collecting = false;
  EventInfo ev = baseEvent(EventType::DISCOVERY_DONE);
  ev.okMask = okMask;
  ev.failMask = failMask;
  ev.allOk = failMask == 0;
  ev.anyOk = okMask != 0;
  ev.restarted = false;
  enqueueEvent(ev);
}

static bool startDiscover(FanMask slotMask, uint32_t durationMs) {
  if (g.connectPhase == ConnectPhase::FAST_CONNECT) return false;
  slotMask = static_cast<FanMask>(slotMask & g.usedMask);
  if (slotMask == 0) return false;
  if (g.discovery.active) abortDiscovery(true);
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(slotMask & bitOf(i))) continue;
    MiioSession& s = g.fans[i].s;
    if (s.reqCmd != CmdKind::NONE) finishCommand(i, s.reqCmd, false);
    for (uint8_t k = 0; k < 3; ++k) {
      if (s.deferred[k].kind != CmdKind::NONE) finishCommand(i, s.deferred[k].kind, false);
    }
    clearRequest(s);
    s.errorBeforeProbe = s.lastError;
    const SessionState next = (s.state == SessionState::ERROR) ? SessionState::ERROR : SessionState::UNKNOWN;
    setState(i, next, s.lastError);
  }
  g.discovery.active = true;
  g.discovery.collecting = true;
  g.discovery.slotMask = slotMask;
  g.discovery.startMask = slotMask;
  g.discovery.startMs = millis();
  g.discovery.durationMs = durationMs ? durationMs : 3000;
  const uint32_t total = (g.discovery.durationMs * 3U > g.discovery.durationMs)
                             ? g.discovery.durationMs * 3U
                             : g.discovery.durationMs;
  g.discovery.deadlineMs = g.discovery.startMs + total;
  sendHelloPacket(IPAddress(255, 255, 255, 255));
  g.discovery.lastHelloMs = g.discovery.startMs;
  return true;
}

static void advanceDiscovery() {
  if (!g.discovery.active) return;
  const uint32_t now = millis();
  g.discovery.collecting = !elapsedAtLeast(now, g.discovery.startMs, g.discovery.durationMs);
  if (g.discovery.collecting) {
    if (elapsedAtLeast(now, g.discovery.lastHelloMs, kHelloIntervalMs)) {
      sendHelloPacket(IPAddress(255, 255, 255, 255));
      g.discovery.lastHelloMs = now;
    }
  }
  if (!g.discovery.collecting) {
    for (uint8_t i = 0; i < kMaxFans; ++i) {
      if (!(g.discovery.slotMask & bitOf(i))) continue;
      MiioSession& s = g.fans[i].s;
      if (s.probingCandidate >= 0 || s.reqKind != ReqKind::NONE) continue;
      int pick = -1;
      for (uint8_t c = 0; c < g.discovery.candidateCount; ++c) {
        DiscoveryCandidate& cand = g.discovery.candidates[c];
        if (cand.done || (cand.triedMask & bitOf(i))) continue;
        if (candidateInUse(c) >= 0) continue;
        if (ipOwnedByOther(i, cand.ip)) continue;
        pick = static_cast<int>(c);
        break;
      }
      if (pick < 0) continue;
      DiscoveryCandidate& cand = g.discovery.candidates[pick];
      s.ipBeforeProbe = s.ip;
      s.errorBeforeProbe = s.lastError;
      s.ip = cand.ip;
      s.probingCandidate = static_cast<int8_t>(pick);
      cand.triedMask = static_cast<FanMask>(cand.triedMask | bitOf(i));
      memcpy(s.deviceId, cand.deviceId, 4);
      s.deviceTs = cand.ts + 1;
      sendInfo(i);
    }
  }

  bool allTried = g.discovery.candidateCount > 0;
  for (uint8_t c = 0; c < g.discovery.candidateCount; ++c) {
    const DiscoveryCandidate& cand = g.discovery.candidates[c];
    if (!cand.done && (cand.triedMask & g.discovery.slotMask) != g.discovery.slotMask) {
      allTried = false;
      break;
    }
  }
  if (g.discovery.slotMask == 0 ||
      (!g.discovery.collecting && allTried && g.discovery.candidateCount > 0) ||
      elapsedAtLeast(now, g.discovery.startMs, g.discovery.deadlineMs - g.discovery.startMs)) {
    // wait until no slot still probing unless deadline
    bool probing = false;
    for (uint8_t i = 0; i < kMaxFans; ++i) {
      if (g.fans[i].s.probingCandidate >= 0) probing = true;
    }
    if (!probing || elapsedAtLeast(now, g.discovery.startMs, g.discovery.deadlineMs - g.discovery.startMs)) {
      finishDiscovery();
    }
  }
}

static void eventConnectDone() {
  EventInfo ev = baseEvent(EventType::CONNECT_DONE);
  ev.ok = true;
  enqueueEvent(ev);
}

static void advanceConnect() {
  if (g.connectPhase == ConnectPhase::FAST_CONNECT) {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < kMaxFans; ++i) {
      if (!(g.usedMask & bitOf(i))) continue;
      MiioSession& s = g.fans[i].s;
      if (s.state == SessionState::READY && s.modelType == FanModelType::UNKNOWN &&
          s.reqKind == ReqKind::NONE && !(g.infoSentMask & bitOf(i))) {
        if (sendInfo(i)) g.infoSentMask = static_cast<FanMask>(g.infoSentMask | bitOf(i));
      }
    }
    bool done = elapsedAtLeast(now, g.connectDeadlineMs - kFastConnectMs, kFastConnectMs);
    if (!done) {
      done = true;
      for (uint8_t i = 0; i < kMaxFans; ++i) {
        if (!(g.usedMask & bitOf(i))) continue;
        const MiioSession& s = g.fans[i].s;
        const bool ok = (s.state == SessionState::READY && s.modelType != FanModelType::UNKNOWN);
        const bool err = (s.state == SessionState::ERROR);
        if (!ok && !err) {
          done = false;
          break;
        }
      }
    }
    if (done) {
      FanMask mask = 0;
      for (uint8_t i = 0; i < kMaxFans; ++i) {
        if (!(g.usedMask & bitOf(i))) continue;
        const MiioSession& s = g.fans[i].s;
        if (s.state == SessionState::ERROR &&
            (s.lastError == MiioErr::TIMEOUT || s.lastError == MiioErr::WRONG_DEVICE)) {
          mask = static_cast<FanMask>(mask | bitOf(i));
        }
      }
      if (mask == 0) {
        g.connectPhase = ConnectPhase::DONE;
        eventConnectDone();
      } else {
        g.connectPhase = ConnectPhase::DISCOVERY;
        startDiscover(mask, g.connectDiscoveryMs);
      }
    }
  } else if (g.connectPhase == ConnectPhase::DISCOVERY) {
    if (!g.discovery.active) {
      g.connectPhase = ConnectPhase::DONE;
      eventConnectDone();
    }
  }
}

static void dispatchEvents() {
  const uint8_t n = g.eventCount;
  g.eventCount = 0;
  if (!g.onEvent) return;
  for (uint8_t i = 0; i < n; ++i) {
    g.onEvent(g.eventQueue[i]);
  }
}

static void resetSlotIdentity(uint8_t i) {
  FanSlot& f = g.fans[i];
  f = FanSlot{};
  f.s.probingCandidate = -1;
}

static CmdResult queueSingle(uint8_t i, CmdKind kind, uint8_t value) {
  warnWrongTask("set");
  if (i >= kMaxFans || !(g.usedMask & bitOf(i)) || !bound()) return CmdResult::REJECTED;
  if (connectingNow() || g.discovery.active) return CmdResult::REJECTED;
  MiioSession& s = g.fans[i].s;
  if (s.probingCandidate >= 0) return CmdResult::REJECTED;
  if (s.reqCmd == kind) return CmdResult::REJECTED;
  if (s.deferred[idx(kind)].kind != CmdKind::NONE && s.deferred[idx(kind)].isGroup) {
    GroupSlot& gs = groupFor(kind);
    gs.pendingMask = static_cast<FanMask>(gs.pendingMask & ~bitOf(i));
    gs.failMask = static_cast<FanMask>(gs.failMask | bitOf(i));
  } else if (s.deferred[idx(kind)].kind != CmdKind::NONE && !s.deferred[idx(kind)].isGroup) {
    finishCommand(i, kind, false);
  }
  s.deferred[idx(kind)] = DeferredCmd{kind, value, false, false};
  return CmdResult::QUEUED;
}

static CmdAccept queueGroup(GroupSlot& gs, CmdKind kind, uint8_t value, Scope scope, uint32_t cooldown, uint32_t lastMs) {
  warnWrongTask("setAll");
  CmdAccept acc;
  acc.result = CmdResult::REJECTED;
  acc.seq = 0;
  if (!bound()) return acc;
  if (connectingNow() || g.discovery.active) return acc;
  if (gs.hasNext) {
    gs.nextValue = value;
    gs.nextScope = scope;
    acc.result = CmdResult::QUEUED;
    acc.seq = gs.nextSeq;
    return acc;
  }
  if (gs.active) {
    gs.value = value;
    gs.scope = scope;
    const FanMask mask = scopeMask(kind, scope);
    const FanMask already = static_cast<FanMask>(gs.okMask | gs.failMask | gs.pendingMask);
    for (uint8_t i = 0; i < kMaxFans; ++i) {
      if (!(mask & bitOf(i))) continue;
      if (already & bitOf(i)) {
        if (g.fans[i].s.deferred[idx(kind)].kind == kind) {
          g.fans[i].s.deferred[idx(kind)].value = value;
        }
        continue;
      }
      MiioSession& s = g.fans[i].s;
      if (s.reqCmd == kind) {
        gs.failMask = static_cast<FanMask>(gs.failMask | bitOf(i));
        continue;
      }
      if (s.deferred[idx(kind)].kind == kind && !s.deferred[idx(kind)].isGroup) {
        finishCommand(i, kind, false);
      }
      s.deferred[idx(kind)] = DeferredCmd{kind, value, true, false};
      gs.pendingMask = static_cast<FanMask>(gs.pendingMask | bitOf(i));
    }
    acc.result = CmdResult::QUEUED;
    acc.seq = gs.seq;
    return acc;
  }
  if (scope == Scope::PARTICIPATING && cooldown != 0 &&
      !elapsedAtLeast(millis(), lastMs, cooldown) && lastMs != 0) {
    gs.nextSeq = allocSeq();
    gs.nextValue = value;
    gs.nextScope = scope;
    gs.hasNext = true;
    acc.result = CmdResult::QUEUED;
    acc.seq = gs.nextSeq;
    return acc;
  }
  return startGroup(gs, kind, value, scope);
}

static void fillPublicInfo(uint8_t i, SmartMiFanInfo& out) {
  const FanSlot& f = g.fans[i];
  out.ip = f.s.ip;
  out.did = MiioCodec::didFromDeviceId(f.s.deviceId);
  memcpy(out.model, f.model, sizeof(out.model));
  memcpy(out.fw_ver, f.fw_ver, sizeof(out.fw_ver));
  memcpy(out.hw_ver, f.hw_ver, sizeof(out.hw_ver));
  memcpy(out.token, f.token, sizeof(out.token));
  out.ready = deriveReady(f.s);
  out.userEnabled = f.s.userEnabled;
  out.softActive = f.s.softActive;
  out.used = f.s.state != SessionState::EMPTY;
  out.lastError = f.s.lastError;
  out.participation = deriveParticipation(f.s);
}

}  // namespace SmartMiFanInternal

namespace SmartMiFan {

using namespace SmartMiFanInternal;

bool begin(WiFiUDP& udp) {
  g.transport.udp = &udp;
  g.handshakeTtlMs = SMARTMIFAN_HANDSHAKE_TTL_S * 1000UL;
  g.transport.nextMsgId = 1;
  g.nextGroupSeq = 1;
  return transportBegin();
}

void end() {
  warnWrongTask("end");
  cancel();
  clear();
  transportEnd();
  g.transport.udp = nullptr;
}

void update() {
  if (g.inUpdate) {
    FAN_LOGE_F("reentrant update()");
    return;
  }
  g.inUpdate = true;
#if defined(ARDUINO_ARCH_ESP32)
  if (!g.updateTask) g.updateTask = xTaskGetCurrentTaskHandle();
#endif
  pollTransport();
  handleTimeouts(millis());
  advanceDeferred();
  advanceGroups();
  advanceDiscovery();
  advanceConnect();
  dispatchEvents();
  g.inUpdate = false;
}

void setHandshakeTtl(uint32_t seconds) {
  warnWrongTask("setHandshakeTtl");
  g.handshakeTtlMs = seconds * 1000UL;
}

uint32_t handshakeTtl() {
  return g.handshakeTtlMs / 1000UL;
}

void clear() {
  warnWrongTask("clear");
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (g.usedMask & bitOf(i)) remove(i);
  }
}

bool add(uint8_t index, const char* ipStr, const char* tokenHex, const char* model) {
  warnWrongTask("add");
  if (index >= kMaxFans || !tokenHex) return false;
  uint8_t token[16];
  if (!MiioCodec::hexToBytes16(tokenHex, token)) return false;
  IPAddress ip;
  if (ipStr && ipStr[0]) {
    if (!ip.fromString(ipStr)) return false;
  } else {
    ip = IPAddress(0, 0, 0, 0);
  }
  FanSlot& f = g.fans[index];
  if (f.s.state != SessionState::EMPTY && tokenEqHex(f.token, tokenHex)) {
    if (ip != IPAddress(0, 0, 0, 0) && ipOwnedByOther(index, ip)) return false;
    if (f.s.ip != ip) {
      if (f.s.reqCmd != CmdKind::NONE) finishCommand(index, f.s.reqCmd, false);
      for (uint8_t k = 0; k < 3; ++k) {
        if (f.s.deferred[k].kind != CmdKind::NONE) finishCommand(index, f.s.deferred[k].kind, false);
      }
      clearRequest(f.s);
      f.s.ip = ip;
      setState(index, SessionState::UNKNOWN, MiioErr::OK);
    }
    if (model && model[0]) copyInfoStrings(index, model, nullptr, nullptr);
    return true;
  }
  if (ip != IPAddress(0, 0, 0, 0) && ipOwnedByOther(index, ip)) return false;
  if (f.s.state != SessionState::EMPTY) remove(index);
  f = FanSlot{};
  f.s.ip = ip;
  memcpy(f.s.token, token, 16);
  MiioCodec::computeKeyIv(token, f.s.key, f.s.iv);
  memset(f.s.deviceId, 0, 4);
  f.s.probingCandidate = -1;
  strncpy(f.token, tokenHex, 32);
  f.token[32] = '\0';
  for (char* p = f.token; *p; ++p) {
    if (*p >= 'A' && *p <= 'F') *p = static_cast<char>(*p + 32);
  }
  if (model && model[0]) copyInfoStrings(index, model, nullptr, nullptr);
  f.s.userEnabled = true;
  setState(index, SessionState::UNKNOWN, MiioErr::OK);
  g.usedMask = static_cast<FanMask>(g.usedMask | bitOf(index));
  return true;
}

bool remove(uint8_t index) {
  warnWrongTask("remove");
  if (index >= kMaxFans || !(g.usedMask & bitOf(index))) return false;
  MiioSession& s = g.fans[index].s;
  if (s.reqCmd != CmdKind::NONE) finishCommand(index, s.reqCmd, false);
  for (uint8_t k = 0; k < 3; ++k) {
    if (s.deferred[k].kind != CmdKind::NONE) finishCommand(index, s.deferred[k].kind, false);
  }
  clearRequest(s);
  g.discovery.slotMask = static_cast<FanMask>(g.discovery.slotMask & ~bitOf(index));
  if (s.probingCandidate >= 0) probeFailed(index);
  setState(index, SessionState::EMPTY, MiioErr::OK);
  memset(s.deviceId, 0, 4);
  s.softActive = false;
  s.userEnabled = false;
  g.usedMask = static_cast<FanMask>(g.usedMask & ~bitOf(index));
  resetSlotIdentity(index);
  return true;
}

FanMask used() {
  return g.usedMask;
}

bool info(uint8_t index, SmartMiFanInfo& out) {
  if (index >= kMaxFans) return false;
  enterInfo();
  if (g.fans[index].s.state == SessionState::EMPTY) {
    exitInfo();
    return false;
  }
  fillPublicInfo(index, out);
  exitInfo();
  return true;
}

int indexByToken(const char* tokenHex) {
  if (!tokenHex) return -1;
  enterInfo();
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    if (tokenEqHex(g.fans[i].token, tokenHex)) {
      exitInfo();
      return static_cast<int>(i);
    }
  }
  exitInfo();
  return -1;
}

bool connect(uint32_t discoveryMs) {
  warnWrongTask("connect");
  if (connectingNow() || g.discovery.active) return false;
  if (anyCommandPending()) return false;
  if (!bound()) return false;
  g.connectPhase = ConnectPhase::FAST_CONNECT;
  g.connectDeadlineMs = millis() + kFastConnectMs;
  g.connectDiscoveryMs = discoveryMs ? discoveryMs : 3000;
  g.infoSentMask = 0;
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    MiioSession& s = g.fans[i].s;
    if (s.ip == IPAddress(0, 0, 0, 0)) {
      setState(i, SessionState::ERROR, MiioErr::TIMEOUT);
    } else {
      setState(i, SessionState::UNKNOWN, MiioErr::OK);
      sendHello(i, CmdKind::NONE);
    }
  }
  return true;
}

bool discover(FanMask slots, uint32_t durationMs) {
  warnWrongTask("discover");
  return startDiscover(slots, durationMs);
}

void cancel() {
  warnWrongTask("cancel");
  if (g.discovery.active) abortDiscovery(false);
  g.connectPhase = ConnectPhase::IDLE;
  GroupSlot* groups[3] = {&g.power, &g.speed, &g.handshakeGroup};
  for (uint8_t n = 0; n < 3; ++n) {
    GroupSlot& gs = *groups[n];
    if (gs.active) {
      for (uint8_t i = 0; i < kMaxFans; ++i) {
        if (gs.pendingMask & bitOf(i)) clearDeferred(g.fans[i].s, gs.kind);
      }
      EventInfo ev = baseEvent(EventType::GROUP_DONE);
      ev.kind = gs.kind;
      ev.seq = gs.seq;
      ev.okMask = gs.okMask;
      ev.failMask = static_cast<FanMask>(gs.failMask | gs.pendingMask);
      ev.allOk = false;
      ev.anyOk = gs.okMask != 0;
      enqueueEvent(ev);
    }
    if (gs.hasNext) {
      EventInfo ev = baseEvent(EventType::GROUP_DONE);
      ev.kind = gs.kind;
      ev.seq = gs.nextSeq;
      ev.okMask = 0;
      ev.failMask = scopeMask(gs.kind, gs.nextScope);
      ev.allOk = false;
      ev.anyOk = false;
      enqueueEvent(ev);
    }
    gs.active = false;
    gs.hasNext = false;
    gs.pendingMask = 0;
  }
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    MiioSession& s = g.fans[i].s;
    if (s.reqCmd != CmdKind::NONE) finishCommand(i, s.reqCmd, false);
    for (uint8_t k = 0; k < 3; ++k) {
      if (s.deferred[k].kind != CmdKind::NONE) finishCommand(i, s.deferred[k].kind, false);
    }
    clearRequest(s);
    if (s.state == SessionState::HANDSHAKING || s.state == SessionState::BUSY) {
      setState(i, SessionState::UNKNOWN, MiioErr::OK);
    }
  }
  enqueueEvent(baseEvent(EventType::CANCELLED));
}

bool connecting() {
  return connectingNow();
}

bool discovering() {
  return g.discovery.active;
}

void setEnabled(uint8_t index, bool enabled) {
  warnWrongTask("setEnabled");
  if (index >= kMaxFans || !(g.usedMask & bitOf(index))) return;
  g.fans[index].s.userEnabled = enabled;
}

void setSoftActive(uint8_t index, bool soft) {
  warnWrongTask("setSoftActive");
  if (index >= kMaxFans || !(g.usedMask & bitOf(index))) return;
  g.fans[index].s.softActive = soft;
}

FanParticipationState participation(uint8_t index) {
  if (index >= kMaxFans || !(g.usedMask & bitOf(index))) {
    return FanParticipationState::INACTIVE;
  }
  enterInfo();
  const FanParticipationState p = deriveParticipation(g.fans[index].s);
  exitInfo();
  return p;
}

FanMask participating() {
  FanMask mask = 0;
  enterInfo();
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    if (deriveParticipation(g.fans[i].s) == FanParticipationState::ACTIVE) {
      mask = static_cast<FanMask>(mask | bitOf(i));
    }
  }
  exitInfo();
  return mask;
}

CmdResult setPower(uint8_t index, bool on) {
  return queueSingle(index, CmdKind::POWER, on ? 1 : 0);
}

CmdResult setSpeed(uint8_t index, uint8_t pct) {
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  return queueSingle(index, CmdKind::SPEED, pct);
}

CmdResult handshake(uint8_t index) {
  return queueSingle(index, CmdKind::HANDSHAKE, 0);
}

CmdAccept setPowerAll(bool on, Scope scope) {
  return queueGroup(g.power, CmdKind::POWER, on ? 1 : 0, scope,
                    SMARTMIFAN_POWER_COOLDOWN_MS, g.lastPowerCmdMs);
}

CmdAccept setSpeedAll(uint8_t pct, Scope scope) {
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  return queueGroup(g.speed, CmdKind::SPEED, pct, scope,
                    SMARTMIFAN_SPEED_COOLDOWN_MS, g.lastSpeedCmdMs);
}

CmdAccept handshakeAll(Scope scope) {
  return queueGroup(g.handshakeGroup, CmdKind::HANDSHAKE, 0, scope, 0, 0);
}

bool commandPending() {
  return anyCommandPending();
}

bool commandPending(CmdKind kind) {
  return kindPending(kind);
}

void prepareForSleep(bool closeSocket) {
  warnWrongTask("prepareForSleep");
  cancel();
  for (uint8_t i = 0; i < kMaxFans; ++i) {
    if (!(g.usedMask & bitOf(i))) continue;
    setState(i, SessionState::UNKNOWN, MiioErr::OK);
    clearRequest(g.fans[i].s);
    clearAllDeferred(g.fans[i].s);
  }
  if (closeSocket) transportEnd();
}

void wake() {
  warnWrongTask("wake");
  if (!g.transport.bound) transportBegin();
}

void setEventCallback(EventCallback cb) {
  warnWrongTask("setEventCallback");
  g.onEvent = cb;
}

}  // namespace SmartMiFan
