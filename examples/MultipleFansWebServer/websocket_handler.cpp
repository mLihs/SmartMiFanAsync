#include "websocket_handler.h"
#include <SmartMiFanAsync.h>
#include <DebugLog.h>

AsyncWebSocket* WebSocketHandler::ws = nullptr;
unsigned long WebSocketHandler::lastTelemetryTime = 0;
const unsigned long WebSocketHandler::TELEMETRY_INTERVAL_MS = 100; // 10 updates/s max
bool WebSocketHandler::telemetryPending = false;
bool WebSocketHandler::telemetryDirty = false; // STEP 5: Dirty flag pattern

// Static buffers to avoid stack allocation and heap fragmentation
uint8_t WebSocketHandler::stateChangedBuffer[2];
uint8_t WebSocketHandler::telemetryBuffer[1024];
uint8_t WebSocketHandler::progressBuffer[256];
uint8_t WebSocketHandler::errorBuffer[256];
uint8_t WebSocketHandler::logBuffer[256];

void WebSocketHandler::init(AsyncWebServer* server) {
  ws = new AsyncWebSocket("/ws");
  ws->onEvent(onWebSocketEvent);
  server->addHandler(ws);
}

void WebSocketHandler::cleanup() {
  if (ws) {
    ws->cleanupClients();
  }
}

void WebSocketHandler::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                                        AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    IPAddress ip = client->remoteIP();
    LOGI_F("WebSocket client #%u connected from %d.%d.%d.%d", 
           client->id(), ip[0], ip[1], ip[2], ip[3]);
    
    // Send initial state (IDLE) - using static buffers, safe for async operations
    sendStateChanged("IDLE");
    
    // STEP 5: Mark telemetry dirty instead of sending directly from async callback
    markTelemetryDirty();
  } else if (type == WS_EVT_DISCONNECT) {
    LOGI_F("WebSocket client #%u disconnected", client->id());
  } else if (type == WS_EVT_DATA) {
    // Handle incoming WebSocket data if needed
    // For now, we only send data, not receive commands via WebSocket
  }
}

uint8_t WebSocketHandler::stateStringToEnum(const char* state) {
  if (strcmp(state, "IDLE") == 0) return STATE_IDLE;
  if (strcmp(state, "SCANNING") == 0) return STATE_SCANNING;
  if (strcmp(state, "READY") == 0) return STATE_READY;
  if (strcmp(state, "ERROR") == 0) return STATE_ERROR;
  return STATE_IDLE; // Default
}

void WebSocketHandler::sendStateChanged(const char* state) {
  if (!ws || ws->count() == 0) {
    return; // No clients connected
  }
  
  // Use static buffer to avoid stack allocation - critical for async operations
  stateChangedBuffer[0] = MSG_STATE_CHANGED;
  stateChangedBuffer[1] = stateStringToEnum(state);
  
  ws->binaryAll(stateChangedBuffer, 2);
}

void WebSocketHandler::sendProgress(const char* jobId, const char* status) {
  if (!ws || ws->count() == 0) {
    return; // No clients connected
  }
  
  uint8_t jobIdLen = 0;
  if (jobId != nullptr) {
    // Safely get length
    for (uint8_t i = 0; i < 63 && jobId[i] != '\0'; i++) {
      jobIdLen++;
    }
  }
  
  uint8_t statusLen = 0;
  if (status != nullptr) {
    // Safely get length
    for (uint8_t i = 0; i < 127 && status[i] != '\0'; i++) {
      statusLen++;
    }
  }
  
  uint8_t* msg = progressBuffer; // Use static buffer
  size_t pos = 0;
  const size_t maxSize = sizeof(progressBuffer);
  
  if (pos + 1 > maxSize) return;
  msg[pos++] = MSG_PROGRESS;
  
  if (pos + 1 + jobIdLen > maxSize) {
    jobIdLen = maxSize - pos - 1; // Truncate if needed
  }
  msg[pos++] = jobIdLen;
  if (jobIdLen > 0 && jobId != nullptr) {
    memcpy(msg + pos, jobId, jobIdLen);
    pos += jobIdLen;
  }
  
  if (pos + 1 + statusLen > maxSize) {
    statusLen = maxSize - pos - 1; // Truncate if needed
  }
  msg[pos++] = statusLen;
  if (statusLen > 0 && status != nullptr) {
    memcpy(msg + pos, status, statusLen);
    pos += statusLen;
  }
  
  ws->binaryAll(msg, pos);
}

void WebSocketHandler::sendTelemetry() {
  if (!ws || ws->count() == 0) {
    return; // No clients connected
  }
  
  size_t fanCount = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(fanCount);
  
  if (fanCount > 16) fanCount = 16; // Limit to 16 fans
  
  // Calculate max size: header(2) + per fan: index(1) + ip(4) + did(4) + modelLen(1) + model(24) + flags(2) + state(1) = 37 bytes per fan
  // With 16 fans max: 2 + (16 * 37) = 594 bytes, use 1024 for safety
  uint8_t* msg = telemetryBuffer; // Use static buffer to avoid stack allocation
  size_t pos = 0;
  const size_t maxSize = sizeof(telemetryBuffer);
  
  if (pos + 2 > maxSize) return;
  msg[pos++] = MSG_TELEMETRY;
  msg[pos++] = (uint8_t)fanCount;
  
  for (size_t i = 0; i < fanCount; i++) {
    // Check if we have enough space for one fan entry (37 bytes max)
    if (pos + 37 > maxSize) {
      break; // Stop if buffer would overflow
    }
    
    // Index
    msg[pos++] = (uint8_t)i;
    
    // IP (4 bytes)
    msg[pos++] = fans[i].ip[0];
    msg[pos++] = fans[i].ip[1];
    msg[pos++] = fans[i].ip[2];
    msg[pos++] = fans[i].ip[3];
    
    // DID (4 bytes, little-endian)
    uint32_t did = fans[i].did;
    msg[pos++] = did & 0xFF;
    msg[pos++] = (did >> 8) & 0xFF;
    msg[pos++] = (did >> 16) & 0xFF;
    msg[pos++] = (did >> 24) & 0xFF;
    
    // Model (string with length prefix, max 24 chars)
    // Safely get model length - ensure null-terminated
    uint8_t modelLen = 0;
    if (fans[i].model != nullptr) {
      // Find length safely, max 24
      const char* model = fans[i].model;
      for (uint8_t j = 0; j < 24 && model[j] != '\0'; j++) {
        modelLen++;
      }
    }
    
    if (pos + 1 + modelLen + 3 > maxSize) {
      // Not enough space for model + remaining fields
      modelLen = 0; // Skip model if no space
    }
    
    msg[pos++] = modelLen;
    if (modelLen > 0) {
      memcpy(msg + pos, fans[i].model, modelLen);
      pos += modelLen;
    }
    
    // Flags
    msg[pos++] = fans[i].ready ? 1 : 0;
    msg[pos++] = SmartMiFanAsync_isFanEnabled(static_cast<uint8_t>(i)) ? 1 : 0;
    
    // Participation state
    FanParticipationState partState = SmartMiFanAsync_getFanParticipationState(static_cast<uint8_t>(i));
    uint8_t stateVal = PART_ERROR;
    if (partState == FanParticipationState::ACTIVE) {
      stateVal = PART_ACTIVE;
    } else if (partState == FanParticipationState::INACTIVE) {
      stateVal = PART_INACTIVE;
    }
    msg[pos++] = stateVal;
  }
  
  ws->binaryAll(msg, pos);
}

// STEP 5: Telemetry serialization - only call from loop()
void WebSocketHandler::updateTelemetry() {
  unsigned long now = millis();
  
  // STEP 5: Check dirty flag and throttle
  if (telemetryDirty && (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS)) {
    lastTelemetryTime = now;
    sendTelemetry();
    telemetryDirty = false;
    telemetryPending = false;
  } else if (telemetryDirty) {
    telemetryPending = true;
  }
}

// STEP 5: Mark telemetry dirty (safe to call from async contexts)
void WebSocketHandler::markTelemetryDirty() {
  telemetryDirty = true;
}

void WebSocketHandler::sendError(const char* error) {
  if (!ws || ws->count() == 0) {
    return; // No clients connected
  }
  
  uint8_t len = 0;
  if (error != nullptr) {
    // Safely get length, max 253 (256 - 2 header bytes - 1 safety)
    for (uint8_t i = 0; i < 253 && error[i] != '\0'; i++) {
      len++;
    }
  }
  
  uint8_t* msg = errorBuffer; // Use static buffer
  msg[0] = MSG_ERROR;
  msg[1] = len;
  if (len > 0 && error != nullptr) {
    memcpy(msg + 2, error, len);
  }
  
  ws->binaryAll(msg, len + 2);
}

void WebSocketHandler::sendLog(const char* level, const char* message) {
  if (!ws || ws->count() == 0) {
    return; // No clients connected
  }
  
  uint8_t levelLen = 0;
  if (level != nullptr) {
    // Safely get length, max 15
    for (uint8_t i = 0; i < 15 && level[i] != '\0'; i++) {
      levelLen++;
    }
  }
  
  uint8_t msgLen = 0;
  if (message != nullptr) {
    // Safely get length, max 240 (256 - 3 header bytes - 1 safety)
    for (uint8_t i = 0; i < 240 && message[i] != '\0'; i++) {
      msgLen++;
    }
  }
  
  uint8_t* msg = logBuffer; // Use static buffer
  size_t pos = 0;
  const size_t maxSize = sizeof(logBuffer);
  
  if (pos + 1 > maxSize) return;
  msg[pos++] = MSG_LOG;
  
  if (pos + 1 + levelLen > maxSize) {
    levelLen = maxSize - pos - 1; // Truncate if needed
  }
  msg[pos++] = levelLen;
  if (levelLen > 0 && level != nullptr) {
    memcpy(msg + pos, level, levelLen);
    pos += levelLen;
  }
  
  if (pos + 1 + msgLen > maxSize) {
    msgLen = maxSize - pos - 1; // Truncate if needed
  }
  msg[pos++] = msgLen;
  if (msgLen > 0 && message != nullptr) {
    memcpy(msg + pos, message, msgLen);
    pos += msgLen;
  }
  
  ws->binaryAll(msg, pos);
}

