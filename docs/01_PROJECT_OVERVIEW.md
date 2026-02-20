# SmartMiFanAsync Project Overview

## What is SmartMiFanAsync?

SmartMiFanAsync is an **asynchronous (non-blocking) Arduino library** for discovering and controlling Xiaomi SmartMi fan devices over the miIO protocol on ESP32. Unlike blocking libraries, SmartMiFanAsync allows your Arduino sketch to continue executing other tasks while discovery and query operations run in the background.

### Key Features

- ✅ **Asynchronous Discovery**: Non-blocking fan discovery that doesn't freeze your main loop
- ✅ **Asynchronous Query**: Query individual devices without blocking
- ✅ **State Machine Based**: Clear state tracking for discovery and query operations
- ✅ **Multiple Fan Support**: Discover and control multiple fans simultaneously (up to 16)
- ✅ **Progress Tracking**: Check discovery/query status at any time
- ✅ **Cancellable Operations**: Cancel ongoing discoveries or queries
- ✅ **Fan Participation Control**: Include/exclude individual fans from control operations (3-state system: ACTIVE, INACTIVE, ERROR)
- ✅ **Orchestrated Commands**: Control functions that respect participation states and include rate limiting
- ✅ **Backward Compatible Control**: Synchronous control functions for power and speed
- ✅ **Fast Connect Mode**: Direct IP connection (skips discovery) for known fans
- ✅ **Smart Connect Mode**: Combines Fast Connect + Discovery for optimal connection
- ✅ **Error Monitoring**: Observational error callbacks for diagnostics
- ✅ **Health Checks**: API for verifying fan connectivity
- ✅ **Sleep/Wake Hooks**: Integration with system sleep/wake cycles
- ✅ **ESP32 Optimized**: Built for ESP32 with WiFi support

---

## Supported Fan Models

### Zhimi Fans
- **zhimi.fan.za5** - Smartmi Standing Fan 3
- **zhimi.fan.v2** - Smartmi DC Pedestal Fan
- **zhimi.fan.v3** - Smartmi DC Pedestal Fan
- **zhimi.fan.za4** - Smartmi Standing Fan 2S
- **zhimi.fan.za3** - Smartmi Standing Fan 2

### Xiaomi Fans
- **xiaomi.fan.p76** - Xiaomi Smart Fan

### Dmaker Fans
- **dmaker.fan.1c** - Mi Smart Standing Fan 1C
- **dmaker.fan.p5** - Mi Smart Standing Fan 1X ⚠️ *Speed control parameters may need verification*
- **dmaker.fan.p8** - Mi Smart Standing Fan 1C CN ⚠️ *Speed control parameters may need verification*
- **dmaker.fan.p9** - Mi Smart Tower Fan
- **dmaker.fan.p10** - Mi Smart Standing Fan 2
- **dmaker.fan.p11** - Mi Smart Standing Fan Pro
- **dmaker.fan.p15** - Mi Smart Standing Fan Pro EU
- **dmaker.fan.p18** - Mi Smart Fan 2
- **dmaker.fan.p30** - Xiaomi Smart Standing Fan 2 ⚠️ *Speed control parameters may need verification*
- **dmaker.fan.p33** - Xiaomi Smart Standing Fan 2 Pro
- **dmaker.fan.p220** - Mijia DC Inverter Circulating Floor Fan ⚠️ *Speed control parameters may need verification*

---

## How It Works

### miIO Protocol

The library uses the **miIO protocol** over UDP (port 54321) to communicate with SmartMi devices:

1. **Hello Packet**: Broadcast UDP packet to discover devices on the network
2. **Device Response**: Devices respond with device ID and timestamp
3. **Encrypted Communication**: All commands and responses are encrypted using AES-128-CBC
4. **Key Derivation**: Encryption keys are derived from device tokens using MD5 hashing

### Discovery Process

1. **Send Hello**: Broadcast hello packets to discover devices
2. **Collect Responses**: Collect device responses (IP, device ID, timestamp)
3. **Query Devices**: For each candidate, query device info (model, firmware, hardware version)
4. **Filter by Model**: Only supported fan models are added to discovered list
5. **Store Results**: Store discovered fans in internal array (max 16 fans)

### Control Operations

1. **Handshake**: Establish encrypted session with device (cached for reuse)
2. **Send Command**: Send encrypted command (set power, set speed)
3. **Receive Response**: Wait for encrypted response
4. **Update State**: Update fan readiness state based on success/failure

---

## Installation

### Arduino IDE

1. Download or clone this library
2. Copy the `SmartMiFanAsync` folder to your Arduino `libraries` directory:
   - **Windows**: `Documents\Arduino\libraries\`
   - **macOS**: `~/Documents/Arduino/libraries/`
   - **Linux**: `~/Arduino/libraries/`
3. Restart the Arduino IDE
4. Include the library in your sketch: `#include <SmartMiFanAsync.h>`

### PlatformIO

Add to your `platformio.ini`:
```ini
lib_deps = 
    path = ../libraries/SmartMiFanAsync
```

Or if published to a registry:
```ini
lib_deps = 
    SmartMiFanAsync
```

### Dependencies

The library requires:
- **ESP32 board support package** (Arduino ESP32 core)
- **WiFi library** (included with ESP32)
- **mbedTLS** (included with ESP32, used for AES-128-CBC encryption and MD5 hashing)

No additional libraries need to be installed manually.

---

## Quick Start

### Basic Async Discovery

```cpp
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "your-ssid";
const char *WIFI_PASS = "your-password";
const char *TOKENS[] = {"your-32-char-token"};
const size_t TOKEN_COUNT = 1;

WiFiUDP fanUdp;

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Wait for WiFi connection...
  
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
}

void loop() {
  // Update discovery (non-blocking)
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  } else if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Discovery finished
    SmartMiFanAsync_printDiscoveredFans();
    
    // Control fans
    SmartMiFanAsync_handshakeAll();
    SmartMiFanAsync_setPowerAll(true);
    SmartMiFanAsync_setSpeedAll(45);
  }
  
  // Your other code continues here...
}
```

### Fast Connect Mode (Direct IP Connection)

```cpp
// Enable Fast Connect mode
#define SMART_MI_FAN_FAST_CONNECT_ENABLED 1
#include <SmartMiFanAsync.h>

// Define fan configuration (optional model for faster connection)
SmartMiFanFastConnectEntry fans[] = {
  {"192.168.1.100", "your-32-char-token-hex"},                    // Model queried
  {"192.168.1.101", "another-token-hex", "zhimi.fan.za5"}         // Model provided (faster!)
};

void setup() {
  // ... WiFi setup ...
  
  SmartMiFanAsync_setFastConnectConfig(fans, 2);
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_registerFastConnectFans(fanUdp);
  
  // Validate (lazy validation on first use)
  SmartMiFanAsync_handshakeAll();
  SmartMiFanAsync_setPowerAll(true);
}
```

### Smart Connect Mode (Fast Connect + Discovery)

```cpp
// Configure Fast Connect (optional model skips queryInfo for faster connection)
SmartMiFanFastConnectEntry fans[] = {
  {"192.168.1.100", "token1...", "zhimi.fan.za5"},   // Model known (faster)
  {"192.168.1.101", "token2..."}                      // May fail, will use Discovery
};
SmartMiFanAsync_setFastConnectConfig(fans, 2);

// Start Smart Connect
SmartMiFanAsync_startSmartConnect(fanUdp, 5000);

void loop() {
  if (SmartMiFanAsync_isSmartConnectInProgress()) {
    SmartMiFanAsync_updateSmartConnect();
  } else if (SmartMiFanAsync_isSmartConnectComplete()) {
    // All fans connected (via Fast Connect or Discovery)
    SmartMiFanAsync_printDiscoveredFans();
  }
}
```

---

## Usage Patterns

### Pattern 1: Simple Discovery

```cpp
void setup() {
  // ... WiFi setup ...
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
    switch (state) {
      case DiscoveryState::SENDING_HELLO:
        Serial.println("Sending hello...");
        break;
      case DiscoveryState::QUERYING_DEVICES:
        Serial.println("Querying devices...");
        break;
      // ...
    }
  }
}
```

### Pattern 3: Discovery with Web Server

```cpp
#include <WebServer.h>

WebServer server(80);

void setup() {
  // ... WiFi setup ...
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
  server.begin();
}

void loop() {
  // Update discovery (non-blocking)
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  }
  
  // Handle web requests (non-blocking)
  server.handleClient();
  
  // Other tasks...
}
```

### Pattern 4: Fan Participation Control

```cpp
void setup() {
  // ... discovery ...
}

void loop() {
  // ... discovery update ...
  
  if (SmartMiFanAsync_isDiscoveryComplete()) {
    // Disable fan 1 (exclude from control)
    SmartMiFanAsync_setFanEnabled(1, false);
    
    // Only ACTIVE fans will receive these commands
    SmartMiFanAsync_handshakeAllOrchestrated();
    SmartMiFanAsync_setPowerAllOrchestrated(true);
    SmartMiFanAsync_setSpeedAllOrchestrated(45);
  }
}
```

---

## Common Use Cases

### 1. Home Automation
Control multiple SmartMi fans from a central ESP32 device. Use orchestrated functions to respect fan participation states and include rate limiting.

### 2. Web Server Integration
Discover and control fans while maintaining a responsive web server. Asynchronous operations allow the server to handle requests without blocking.

### 3. Sensor-Based Control
Control fans based on sensor readings (temperature, humidity, etc.). Use fan participation states to exclude fans that shouldn't be controlled.

### 4. Sleep/Wake Integration
Integrate with system sleep/wake cycles. Use `prepareForSleep()` and `softWakeUp()` hooks to manage fan connections during sleep.

### 5. Multi-Fan Orchestration
Control multiple fans with different participation states. Use orchestrated functions to send commands only to ACTIVE fans.

---

## Limitations

1. **Maximum 16 discovered fans** (hardcoded constant `kMaxSmartMiFans`)
2. **Maximum 4 Fast Connect fans** (hardcoded constant `kMaxFastConnectFans`)
3. **Control operations (setPower, setSpeed) are synchronous** (may block for ~100-1500ms per fan)
4. **Requires ESP32** (not compatible with other Arduino boards)
5. **Requires WiFi connection**
6. **Tokens must be obtained separately** (not provided by library)
7. **UDP port 54321 must be accessible** (not blocked by firewall)

---

## Related Documentation

- [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) - System architecture and design
- [03_FUNCTIONS.md](./03_FUNCTIONS.md) - Core functions and workflows
- [06_APIS.md](./06_APIS.md) - Complete API reference
- [09_EXAMPLES.md](./09_EXAMPLES.md) - Example sketches

---

## Getting Help

### Debug Logging

Enable debug logging to diagnose issues:

```cpp
#define FAN_DEBUG_GEN
#define FAN_DEBUG_HOT
#define FAN_DEBUG_NET
#include <SmartMiFanAsync.h>
```

**Log Categories:**
- **GEN**: General operations (discovery start/stop, errors)
- **HOT**: Hot path operations (frequent calls in main loop)
- **NET**: Network operations (UDP send/receive)

### Error Callback

Register an error callback to monitor all errors:

```cpp
void onError(const FanErrorInfo& info) {
  Serial.printf("Fan %u: %s error during %s\n", 
                info.fanIndex, 
                errorToString(info.error),
                operationToString(info.operation));
}

SmartMiFanAsync_setErrorCallback(onError);
```

### Health Checks

Use health checks to verify fan connectivity:

```cpp
// Check single fan
bool healthy = SmartMiFanAsync_healthCheck(0, 2000);

// Check all fans
bool allHealthy = SmartMiFanAsync_healthCheckAll(2000);
```

---

## Next Steps

1. Read [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) to understand the system design
2. Review [03_FUNCTIONS.md](./03_FUNCTIONS.md) to understand core workflows
3. Explore [09_EXAMPLES.md](./09_EXAMPLES.md) for usage examples
4. Check [06_APIS.md](./06_APIS.md) for detailed API reference

