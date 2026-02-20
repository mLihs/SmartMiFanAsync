# SmartMiFanAsync Examples Documentation

This document describes all example sketches included with SmartMiFanAsync.

---

## Example Overview

The library includes 11 example sketches demonstrating different use cases:

1. **BasicAsyncDiscovery** - Minimal async discovery example
2. **AsyncQueryDevice** - Query single device by IP
3. **MultipleFansAsync** - Discover and control multiple fans
4. **FanParticipationToggle** - Fan participation state control
5. **FullOrchestrationFlow** - Complete orchestration flow
6. **ErrorAndHealthCallbackExample** - Error callbacks and health checks
7. **SystemStateOrchestration** - System state management
8. **MultipleFansFastConnect** - Fast Connect mode
9. **MultipleFansSmartConnect** - Smart Connect mode
10. **WebServerControl** - Web server integration
11. **MultipleFansWebServer** - Full web server with multiple fans

---

## 1. BasicAsyncDiscovery

**Location**: `examples/BasicAsyncDiscovery/BasicAsyncDiscovery.ino`

**Purpose**: Minimal example demonstrating async discovery of SmartMi fans.

**Key Features**:
- Simple async discovery
- Progress reporting
- Non-blocking operation
- Status checking

**Code Structure**:
```cpp
const char *TOKENS[] = {
  "your-32-char-token"
};
const size_t TOKEN_COUNT = 1;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Wait for WiFi...
  
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
}

void loop() {
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
    
    // Print progress
    DiscoveryState state = SmartMiFanAsync_getDiscoveryState();
    switch (state) {
      case DiscoveryState::SENDING_HELLO:
        Serial.println("Sending hello...");
        break;
      case DiscoveryState::QUERYING_DEVICES:
        Serial.println("Querying devices...");
        break;
    }
  } else if (SmartMiFanAsync_isDiscoveryComplete()) {
    SmartMiFanAsync_printDiscoveredFans();
  }
}
```

**Usage**:
1. Update `TOKENS` array with your fan tokens
2. Update WiFi credentials
3. Upload sketch
4. Monitor Serial output for discovery progress and results

---

## 2. AsyncQueryDevice

**Location**: `examples/AsyncQueryDevice/AsyncQueryDevice.ino`

**Purpose**: Demonstrates querying a single device by IP address.

**Key Features**:
- Query specific device by IP
- Error handling
- Device information display

**Code Structure**:
```cpp
void setup() {
  IPAddress ip;
  ip.fromString("192.168.1.100");
  SmartMiFanAsync_startQueryDevice(fanUdp, ip, TOKEN);
}

void loop() {
  if (SmartMiFanAsync_isQueryInProgress()) {
    SmartMiFanAsync_updateQueryDevice();
    
    QueryState state = SmartMiFanAsync_getQueryState();
    switch (state) {
      case QueryState::WAITING_HELLO:
        Serial.println("Waiting for hello...");
        break;
      case QueryState::SENDING_QUERY:
        Serial.println("Sending query...");
        break;
    }
  } else if (SmartMiFanAsync_isQueryComplete()) {
    Serial.println("Query complete!");
    SmartMiFanAsync_printDiscoveredFans();
  }
}
```

**Usage**:
1. Update IP address and token
2. Upload sketch
3. Monitor Serial output for query progress and results

---

## 3. MultipleFansAsync

**Location**: `examples/MultipleFansAsync/MultipleFansAsync.ino`

**Purpose**: Demonstrates discovering and controlling multiple fans.

**Key Features**:
- Multiple fan discovery
- Handshake with all fans
- Control all fans simultaneously
- Integration with other tasks

**Code Structure**:
```cpp
const char *TOKENS[] = {
  "token1...",
  "token2...",
  "token3..."
};
const size_t TOKEN_COUNT = 3;

void setup() {
  // ... WiFi setup ...
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
}

void loop() {
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  } else if (SmartMiFanAsync_isDiscoveryComplete()) {
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    
    if (count > 0) {
      // Handshake all fans
      if (SmartMiFanAsync_handshakeAll()) {
        // Control all fans
        SmartMiFanAsync_setPowerAll(true);
        SmartMiFanAsync_setSpeedAll(45);
      }
    }
  }
  
  // Other tasks can run here...
}
```

**Usage**:
1. Update `TOKENS` array with your fan tokens
2. Upload sketch
3. Monitor Serial output for discovery and control results

---

## 4. FanParticipationToggle

**Location**: `examples/FanParticipationToggle/FanParticipationToggle.ino`

**Purpose**: Demonstrates fan participation state control - enabling and disabling individual fans from receiving commands.

**Key Features**:
- Get fan participation states (ACTIVE, INACTIVE, ERROR)
- Enable/disable individual fans
- Orchestrated control functions that respect participation states
- State monitoring and reporting

**Code Structure**:
```cpp
void loop() {
  // ... discovery update ...
  
  if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Disable fan 1 (exclude from control)
    SmartMiFanAsync_setFanEnabled(1, false);
    
    // Check participation states
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    for (size_t i = 0; i < count; i++) {
      FanParticipationState state = SmartMiFanAsync_getFanParticipationState(i);
      Serial.print("Fan ");
      Serial.print(i);
      Serial.print(": ");
      switch (state) {
        case FanParticipationState::ACTIVE:
          Serial.println("ACTIVE - will receive commands");
          break;
        case FanParticipationState::INACTIVE:
          Serial.println("INACTIVE - excluded from commands");
          break;
        case FanParticipationState::ERROR:
          Serial.println("ERROR - technical issues");
          break;
      }
    }
    
    // Only ACTIVE fans will receive these commands
    SmartMiFanAsync_handshakeAllOrchestrated();
    SmartMiFanAsync_setPowerAllOrchestrated(true);
    SmartMiFanAsync_setSpeedAllOrchestrated(45);
  }
}
```

**Usage**:
1. Update tokens and WiFi credentials
2. Upload sketch
3. Monitor Serial output for participation states and control results

---

## 5. FullOrchestrationFlow

**Location**: `examples/FullOrchestrationFlow/FullOrchestrationFlow.ino`

**Purpose**: Demonstrates complete orchestration flow with fan participation states, error handling, and system state management.

**Key Features**:
- Fan participation state management
- Orchestrated handshake and control operations
- Error handling and health checks
- Integration with system state management

**Code Structure**:
```cpp
void loop() {
  // ... discovery update ...
  
  if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Set up fan participation states
    SmartMiFanAsync_setFanEnabled(0, true);   // Fan 0: ACTIVE
    SmartMiFanAsync_setFanEnabled(1, false);  // Fan 1: INACTIVE
    
    // Orchestrated handshake (only ACTIVE fans)
    if (SmartMiFanAsync_handshakeAllOrchestrated()) {
      // Orchestrated control (only ACTIVE fans, rate limited)
      SmartMiFanAsync_setPowerAllOrchestrated(true);
      SmartMiFanAsync_setSpeedAllOrchestrated(50);
    }
    
    // Health check all fans
    SmartMiFanAsync_healthCheckAll(2000);
  }
}
```

**Usage**:
1. Update tokens and WiFi credentials
2. Upload sketch
3. Monitor Serial output for orchestration flow

---

## 6. ErrorAndHealthCallbackExample

**Location**: `examples/ErrorAndHealthCallbackExample/ErrorAndHealthCallbackExample.ino`

**Purpose**: Demonstrates error callbacks, health checks, and transport/sleep hooks.

**Key Features**:
- Error callback registration and handling
- Per-fan readiness state tracking
- Health check API usage
- Transport/sleep hooks (`prepareForSleep`, `softWakeUp`)
- Error classification and reporting

**Code Structure**:
```cpp
void onFanError(const FanErrorInfo& info) {
  Serial.print("Fan ");
  Serial.print(info.fanIndex);
  Serial.print(" error: ");
  
  switch (info.error) {
    case MiioErr::TIMEOUT:
      Serial.println("Timeout");
      break;
    case MiioErr::WRONG_SOURCE_IP:
      Serial.println("Wrong source IP");
      break;
    case MiioErr::DECRYPT_FAIL:
      Serial.println("Decrypt failed");
      break;
    case MiioErr::INVALID_RESPONSE:
      Serial.println("Invalid response");
      break;
  }
}

void setup() {
  // ... WiFi setup ...
  
  // Register error callback
  SmartMiFanAsync_setErrorCallback(onFanError);
  
  // ... discovery ...
}

void loop() {
  // ... discovery update ...
  
  // Health check example
  if (shouldCheckHealth) {
    bool healthy = SmartMiFanAsync_healthCheck(0, 2000);
    Serial.print("Fan 0 health: ");
    Serial.println(healthy ? "OK" : "FAILED");
  }
  
  // Sleep/wake example
  if (shouldSleep) {
    SmartMiFanAsync_prepareForSleep(true, true);
    // Enter sleep...
  }
  
  if (shouldWake) {
    SmartMiFanAsync_softWakeUp();
    SmartMiFanAsync_handshakeAll();
  }
}
```

**Usage**:
1. Update tokens and WiFi credentials
2. Upload sketch
3. Monitor Serial output for errors and health status

---

## 7. SystemStateOrchestration

**Location**: `examples/SystemStateOrchestration/SystemStateOrchestration.ino`

**Purpose**: Demonstrates project-level system state management (ACTIVE/IDLE/SLEEP) with library hooks.

**Key Features**:
- System state transitions (ACTIVE, IDLE, SLEEP)
- Integration with `prepareForSleep()` and `softWakeUp()` hooks
- Project-level state machine (library does not manage system state)
- Activity-based state transitions

**Code Structure**:
```cpp
SystemState currentState = SystemState::IDLE;

void loop() {
  // ... discovery update ...
  
  // Project decides to enter sleep
  if (shouldSleep()) {
    currentState = SystemState::SLEEP;
    SmartMiFanAsync_prepareForSleep(true, true);
    // Enter sleep...
  }
  
  // Project decides to wake up
  if (shouldWake()) {
    currentState = SystemState::ACTIVE;
    SmartMiFanAsync_softWakeUp();
    SmartMiFanAsync_handshakeAll();
  }
  
  // State-based fan control
  if (currentState == SystemState::ACTIVE) {
    // Control fans...
  }
}
```

**Usage**:
1. Update tokens and WiFi credentials
2. Implement your state transition logic
3. Upload sketch
4. Monitor Serial output for state transitions

---

## 8. MultipleFansFastConnect

**Location**: `examples/MultipleFansFastConnect/MultipleFansFastConnect.ino`

**Purpose**: Demonstrates Fast Connect mode for direct IP connection.

**Key Features**:
- Fast Connect configuration
- Direct IP connection (skips discovery)
- Lazy validation
- Validation callback

**Code Structure**:
```cpp
#define SMART_MI_FAN_FAST_CONNECT_ENABLED 1
#include <SmartMiFanAsync.h>

SmartMiFanFastConnectEntry fans[] = {
  {"192.168.1.100", "token1..."},                    // Model queried
  {"192.168.1.101", "token2...", "zhimi.fan.za5"}    // Model provided (faster!)
};

void setup() {
  // ... WiFi setup ...
  
  SmartMiFanAsync_setFastConnectConfig(fans, 2);
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_registerFastConnectFans(fanUdp);
  
  // Validation happens lazily on first use
  SmartMiFanAsync_handshakeAll();
  SmartMiFanAsync_setPowerAll(true);
}
```

**Usage**:
1. Enable Fast Connect mode (compile-time or runtime)
2. Update IP addresses and tokens
3. Upload sketch
4. Monitor Serial output for connection status

---

## 9. MultipleFansSmartConnect

**Location**: `examples/MultipleFansSmartConnect/MultipleFansSmartConnect.ino`

**Purpose**: Demonstrates Smart Connect mode (Fast Connect + Discovery).

**Key Features**:
- Fast Connect configuration
- Automatic Discovery fallback
- Combined connection strategy

**Code Structure**:
```cpp
SmartMiFanFastConnectEntry fans[] = {
  {"192.168.1.100", "token1...", "zhimi.fan.za5"},   // Model known (faster)
  {"192.168.1.101", "token2..."}                      // May fail, will use Discovery
};

void setup() {
  // ... WiFi setup ...
  
  SmartMiFanAsync_setFastConnectConfig(fans, 2);
  SmartMiFanAsync_startSmartConnect(fanUdp, 5000);
}

void loop() {
  if (SmartMiFanAsync_isSmartConnectInProgress()) {
    SmartMiFanAsync_updateSmartConnect();
  } else if (SmartMiFanAsync_isSmartConnectComplete()) {
    // All fans connected (via Fast Connect or Discovery)
    SmartMiFanAsync_printDiscoveredFans();
  }
}
```

**Usage**:
1. Update IP addresses and tokens
2. Upload sketch
3. Monitor Serial output for Smart Connect progress

---

## 10. WebServerControl

**Location**: `examples/WebServerControl/WebServerControl.ino`

**Purpose**: Demonstrates web server integration with async discovery.

**Key Features**:
- Web server integration
- Non-blocking discovery
- HTTP endpoints for fan control
- Async operation compatibility

**Code Structure**:
```cpp
#include <WebServer.h>

WebServer server(80);

void setup() {
  // ... WiFi setup ...
  
  // Start discovery
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
  
  // Setup web server
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.begin();
}

void loop() {
  // Update discovery (non-blocking)
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  }
  
  // Handle web requests (non-blocking)
  server.handleClient();
}
```

**Usage**:
1. Update tokens and WiFi credentials
2. Upload sketch
3. Access web interface at ESP32 IP address
4. Use HTTP endpoints to control fans

---

## 11. MultipleFansWebServer

**Location**: `examples/MultipleFansWebServer/MultipleFansWebServer.ino`

**Purpose**: Full web server example with multiple fans, WebSocket support, and comprehensive API.

**Key Features**:
- Full web server with HTML/CSS/JS
- WebSocket support for real-time updates
- RESTful API endpoints
- Multiple fan management
- State machine integration

**Code Structure**:
```cpp
// Multiple files:
// - MultipleFansWebServer.ino (main sketch)
// - webserver.h/cpp (web server implementation)
// - websocket_handler.h/cpp (WebSocket handling)
// - api_handlers.h/cpp (API endpoint handlers)
// - state_machine.h/cpp (state machine)
// - config.h/cpp (configuration)

void setup() {
  // ... WiFi setup ...
  
  // Initialize web server
  initWebServer();
  
  // Start discovery
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
}

void loop() {
  // Update discovery
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  }
  
  // Update state machine
  updateStateMachine();
  
  // Handle web server
  server.handleClient();
  
  // Handle WebSocket
  handleWebSocket();
}
```

**Usage**:
1. Update tokens and WiFi credentials
2. Upload sketch
3. Access web interface at ESP32 IP address
4. Use web interface to discover and control fans

---

## Common Patterns

### Pattern 1: Simple Discovery

```cpp
void setup() {
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
}

void loop() {
  if (SmartMiFanAsync_updateDiscovery()) {
    // Still discovering
  } else if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Use discovered fans
  }
}
```

### Pattern 2: Discovery with Progress

```cpp
void loop() {
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
    
    DiscoveryState state = SmartMiFanAsync_getDiscoveryState();
    // Report progress...
  }
}
```

### Pattern 3: Discovery with Web Server

```cpp
void loop() {
  // Update discovery (non-blocking)
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  }
  
  // Handle web requests (non-blocking)
  server.handleClient();
}
```

### Pattern 4: Fan Participation Control

```cpp
void loop() {
  // ... discovery update ...
  
  if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Disable fan 1
    SmartMiFanAsync_setFanEnabled(1, false);
    
    // Only ACTIVE fans receive commands
    SmartMiFanAsync_handshakeAllOrchestrated();
    SmartMiFanAsync_setPowerAllOrchestrated(true);
  }
}
```

---

## Troubleshooting Examples

### No Fans Discovered

**Symptoms**: Discovery completes but no fans found

**Possible Causes**:
1. Fans not powered on or not connected to WiFi
2. Incorrect tokens (must be exactly 32 hex characters)
3. Fans on different network
4. Discovery timeout too short
5. Firewall blocking UDP port 54321

**Solutions**:
- Verify fans are powered and connected
- Double-check token format (32 hex characters, no spaces)
- Ensure ESP32 and fans are on same network
- Increase discovery timeout (try 10000ms)
- Check network firewall settings

### Discovery Times Out

**Symptoms**: Discovery state becomes `TIMEOUT`

**Possible Causes**:
1. Network issues
2. Fans not responding
3. Timeout too short

**Solutions**:
- Check WiFi connection quality
- Verify fans are powered on
- Increase timeout value
- Try querying devices individually by IP

### Handshake Fails

**Symptoms**: Handshake returns `false` or error callback reports errors

**Possible Causes**:
1. Token mismatch
2. Device not ready
3. Network issues

**Solutions**:
- Verify token is correct for the device
- Wait a moment and retry
- Check network connection
- Use health check to diagnose

---

## Related Documentation

- [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md) - Project overview
- [03_FUNCTIONS.md](./03_FUNCTIONS.md) - Core functions and workflows
- [06_APIS.md](./06_APIS.md) - API reference
- [08_OPEN_TOPICS.md](./08_OPEN_TOPICS.md) - Known issues and planned features

---

**Last Updated**: 2025-01-11

