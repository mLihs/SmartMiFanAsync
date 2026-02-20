#pragma once

#include <ESPAsyncWebServer.h>
#include <string.h>

// Binary WebSocket message types
enum WsMsgType : uint8_t {
  MSG_STATE_CHANGED = 0x01,
  MSG_PROGRESS = 0x02,
  MSG_TELEMETRY = 0x03,
  MSG_ERROR = 0x04,
  MSG_LOG = 0x05
};

// State values for MSG_STATE_CHANGED
enum WsState : uint8_t {
  STATE_IDLE = 0,
  STATE_SCANNING = 1,
  STATE_READY = 2,
  STATE_ERROR = 3
};

// Participation state values for telemetry
enum WsParticipationState : uint8_t {
  PART_ACTIVE = 0,
  PART_INACTIVE = 1,
  PART_ERROR = 2
};

class WebSocketHandler {
public:
  static void init(AsyncWebServer* server);
  static void cleanup();
  
  // Binary event sending methods
  static void sendStateChanged(const char* state);
  static void sendProgress(const char* jobId, const char* status);
  static void sendTelemetry();
  static void sendError(const char* error);
  static void sendLog(const char* level, const char* message);
  
  // Update method (for throttled telemetry) - call from loop() only
  static void updateTelemetry();
  
  // STEP 5: Mark telemetry as dirty (call from async contexts)
  static void markTelemetryDirty();
  
private:
  static AsyncWebSocket* ws;
  static unsigned long lastTelemetryTime;
  static const unsigned long TELEMETRY_INTERVAL_MS; // Throttle to max 10-20 updates/s
  static bool telemetryPending;
  static bool telemetryDirty; // STEP 5: Dirty flag pattern
  
  // Static buffers to avoid stack allocation and heap fragmentation
  static uint8_t stateChangedBuffer[2]; // For MSG_STATE_CHANGED (2 bytes)
  static uint8_t telemetryBuffer[1024];
  static uint8_t progressBuffer[256];
  static uint8_t errorBuffer[256];
  static uint8_t logBuffer[256];
  
  static void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                               AwsEventType type, void *arg, uint8_t *data, size_t len);
  
  // Helper to convert state string to enum
  static uint8_t stateStringToEnum(const char* state);
};

