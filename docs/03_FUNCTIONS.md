# SmartMiFanAsync Functions and Workflows

## Overview

This document describes the core functions and workflows of SmartMiFanAsync, including discovery, query, control, participation states, and error handling.

---

## Discovery Workflow

### Async Discovery Process

The discovery process is **asynchronous and non-blocking**, allowing your main loop to continue executing other tasks.

**Steps:**
1. **Start Discovery**: Call `SmartMiFanAsync_startDiscovery()` with tokens and timeout
2. **Update State Machine**: Call `SmartMiFanAsync_updateDiscovery()` in `loop()`
3. **Check Status**: Use `SmartMiFanAsync_isDiscoveryInProgress()` or `SmartMiFanAsync_isDiscoveryComplete()`
4. **Get Results**: Use `SmartMiFanAsync_getDiscoveredFans()` to access discovered fans

**Example:**
```cpp
void setup() {
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
}

void loop() {
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  } else if (SmartMiFanAsync_isDiscoveryComplete()) {
    size_t count = 0;
    const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
    // Use discovered fans...
  }
}
```

### Discovery Phases

**Phase 1: Sending Hello**
- Broadcasts hello packets to UDP port 54321
- Sends hello every 500ms
- Collects device responses (IP, device ID, timestamp)
- Duration: Configurable (default: 3000ms)

**Phase 2: Querying Devices**
- For each candidate device, queries device info using `miIO.info`
- Matches candidates with provided tokens
- Filters by supported fan models
- Adds discovered fans to internal array
- Duration: Dynamic (candidateCount × tokenCount × 2.5 seconds)

### Discovery States

- `IDLE`: No discovery active
- `SENDING_HELLO`: Sending broadcast hello packets
- `QUERYING_DEVICES`: Querying discovered devices for info
- `COMPLETE`: Discovery finished successfully
- `ERROR`: Discovery failed
- `TIMEOUT`: Discovery timed out

See: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Discovery State Machine"

---

## Query Workflow

### Async Query Process

The query process allows you to query a single device by IP address **asynchronously**.

**Steps:**
1. **Start Query**: Call `SmartMiFanAsync_startQueryDevice()` with IP and token
2. **Update State Machine**: Call `SmartMiFanAsync_updateQueryDevice()` in `loop()`
3. **Check Status**: Use `SmartMiFanAsync_isQueryInProgress()` or `SmartMiFanAsync_isQueryComplete()`
4. **Get Results**: Device is added to discovered fans array on success

**Example:**
```cpp
void setup() {
  IPAddress ip;
  ip.fromString("192.168.1.100");
  SmartMiFanAsync_startQueryDevice(fanUdp, ip, TOKEN);
}

void loop() {
  if (SmartMiFanAsync_isQueryInProgress()) {
    SmartMiFanAsync_updateQueryDevice();
  } else if (SmartMiFanAsync_isQueryComplete()) {
    // Query finished, device added to discovered fans
  }
}
```

### Query Phases

**Phase 1: Waiting Hello**
- Sends hello packet to target IP
- Resends hello every 500ms
- Waits for hello response (device ID, timestamp)
- Timeout: 2 seconds

**Phase 2: Sending Query**
- Sends encrypted `miIO.info` query
- Uses device ID and timestamp from hello response
- Encrypts payload with token-derived key/IV

**Phase 3: Waiting Response**
- Waits for encrypted response
- Decrypts response
- Parses JSON (model, firmware, hardware version)
- Filters by supported fan models
- Adds device to discovered fans array
- Timeout: 2 seconds

### Query States

- `IDLE`: No query active
- `WAITING_HELLO`: Waiting for hello response
- `SENDING_QUERY`: Sending device info query
- `WAITING_RESPONSE`: Waiting for encrypted response
- `COMPLETE`: Query finished successfully
- `ERROR`: Query failed
- `TIMEOUT`: Query timed out

See: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Query State Machine"

---

## Fast Connect Workflow

### Fast Connect Process

Fast Connect mode allows you to register fans directly by IP address and token, **skipping the discovery process**.

**Steps:**
1. **Enable Fast Connect**: Set compile-time define or call `SmartMiFanAsync_setFastConnectEnabled(true)`
2. **Set Configuration**: Call `SmartMiFanAsync_setFastConnectConfig()` with fan entries
3. **Register Fans**: Call `SmartMiFanAsync_registerFastConnectFans()` to add fans to discovered list
4. **Validate (Optional)**: Call `SmartMiFanAsync_validateFastConnectFans()` to validate all fans
5. **Use Fans**: Use existing control functions (validation happens lazily on first use)

**Example:**
```cpp
void setup() {
  SmartMiFanFastConnectEntry fans[] = {
    {"192.168.1.100", "token1..."},                    // Model queried
    {"192.168.1.101", "token2...", "zhimi.fan.za5"}    // Model provided (faster!)
  };
  
  SmartMiFanAsync_setFastConnectConfig(fans, 2);
  SmartMiFanAsync_resetDiscoveredFans();
  SmartMiFanAsync_registerFastConnectFans(fanUdp);
  
  // Validation happens lazily on first use
  SmartMiFanAsync_handshakeAll();
}
```

### Eager Validation with Device Info Query

Fast Connect now uses **eager validation** with automatic device info retrieval:
- Fans are registered immediately (added to discovered fans array)
- When `SmartMiFanAsync_validateFastConnectFans()` is called:
  1. Performs handshake with each fan
  2. If handshake succeeds, queries `miIO.info` to retrieve model, firmware, and hardware version
  3. Stores model info in the discovered fans array (required for correct `setSpeed()` parameters)
- Invalid IPs/tokens will result in handshake failures, but won't crash the system
- Model info is essential for `setSpeed()` to use the correct MIoT parameters (siid/piid)

### Validation Callback

You can set a validation callback to receive validation results:

```cpp
void onValidationComplete(const SmartMiFanFastConnectResult results[], size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (results[i].success) {
      Serial.print("Fan ");
      Serial.print(results[i].ip);
      Serial.println(" validated successfully");
    }
  }
}

SmartMiFanAsync_setFastConnectValidationCallback(onValidationComplete);
SmartMiFanAsync_validateFastConnectFans(fanUdp);
```

---

## Smart Connect Workflow

### Smart Connect Process

Smart Connect mode combines Fast Connect and Async Discovery for **optimal fan connection**.

**Steps:**
1. **Configure Fast Connect**: Set Fast Connect configuration (required)
2. **Start Smart Connect**: Call `SmartMiFanAsync_startSmartConnect()` with discovery timeout
3. **Update State Machine**: Call `SmartMiFanAsync_updateSmartConnect()` in `loop()`
4. **Check Status**: Use `SmartMiFanAsync_isSmartConnectInProgress()` or `SmartMiFanAsync_isSmartConnectComplete()`
5. **Use Fans**: All successfully connected fans are available for control

**Example:**
```cpp
void setup() {
  SmartMiFanFastConnectEntry fans[] = {
    {"192.168.1.100", "token1...", "zhimi.fan.za5"},   // Model known (faster)
    {"192.168.1.101", "token2..."}                      // May fail, will use Discovery
  };
  
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

### Smart Connect Phases

**Phase 1: Validating Fast Connect**
- Validates all Fast Connect configured fans
- Performs handshake with each fan
- Collects failed fans (wrong IP, wrong token, network issues)

**Phase 2: Starting Discovery**
- If failed fans exist, starts Async Discovery for those fans
- Uses tokens from failed Fast Connect fans
- Removes failed Fast Connect entries from discovered list

**Phase 3: Discovering**
- Runs Async Discovery for failed fans
- Updates discovery state machine
- Adds successfully discovered fans to discovered list

**Phase 4: Complete**
- All fans connected (via Fast Connect or Discovery)
- All fans available for control

### Smart Connect States

- `IDLE`: No Smart Connect active
- `VALIDATING_FAST_CONNECT`: Validating Fast Connect fans
- `STARTING_DISCOVERY`: Starting discovery for failed fans
- `DISCOVERING`: Async discovery in progress
- `COMPLETE`: Smart Connect finished successfully
- `ERROR`: Smart Connect failed

See: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Smart Connect State Machine"

---

## Control Workflow

### Handshake Process

Before sending commands, the library must establish an encrypted session with the device:

**Steps:**
1. **Send Hello**: Send hello packet to device
2. **Receive Response**: Receive device ID and timestamp
3. **Cache Session**: Cache device ID and timestamp for reuse
4. **Derive Keys**: Derive encryption key/IV from token

**Handshake Cache:**
- Handshake is cached per fan (stored in `SmartMiFanAsyncClient`)
- Cache is invalidated on error or when fan address changes
- Cache is invalidated on sleep (if `invalidateHandshake = true`)

### Control Operations

**Set Power:**
```cpp
bool SmartMiFanAsync_setPowerAll(bool on);
```
- Sends `set_properties` command with `siid=2, piid=1` (bool)
- Synchronous (blocks for ~100-1500ms per fan)
- Returns `true` if all operations succeeded

**Set Speed:**
```cpp
bool SmartMiFanAsync_setSpeedAll(uint8_t percent);
```
- Sends `set_properties` command with model-specific `siid/piid`
- Speed range: 1-100 (percent)
- Special handling for `dmaker.fan.1c` (uses fan_level 1-3 instead)
- Synchronous (blocks for ~100-1500ms per fan)
- Returns `true` if all operations succeeded

### Orchestrated Control

Orchestrated functions respect fan participation states and include rate limiting:

**Handshake All Orchestrated:**
```cpp
bool SmartMiFanAsync_handshakeAllOrchestrated();
```
- Only handshakes ACTIVE fans
- INACTIVE and ERROR fans are skipped
- Returns `true` if all ACTIVE fan handshakes succeeded

**Set Power All Orchestrated:**
```cpp
bool SmartMiFanAsync_setPowerAllOrchestrated(bool on);
```
- Only sends commands to ACTIVE fans
- Command coalescing: max 1 command per second
- Returns `true` if all ACTIVE fan operations succeeded

**Set Speed All Orchestrated:**
```cpp
bool SmartMiFanAsync_setSpeedAllOrchestrated(uint8_t percent);
```
- Only sends commands to ACTIVE fans
- Command coalescing: max 1 command per second
- Returns `true` if all ACTIVE fan operations succeeded

---

## Fan Participation States

### Three-State System

Each fan can be in one of three participation states:

1. **ACTIVE**: Fan is enabled and ready - participates in control (default)
2. **INACTIVE**: Fan is disabled by user/project - excluded from commands
3. **ERROR**: Fan has technical issues - not available (automatically derived)

### State Derivation

```cpp
FanParticipationState SmartMiFanAsync_getFanParticipationState(uint8_t fanIndex) {
  const auto &fan = g_discoveredFans[fanIndex];
  
  // ERROR state is derived ONLY from lastError != OK
  if (fan.lastError != MiioErr::OK) {
    return FanParticipationState::ERROR;
  }
  
  // INACTIVE state is set explicitly by user
  if (!fan.userEnabled) {
    return FanParticipationState::INACTIVE;
  }
  
  // Default: ACTIVE (even if ready==false, as long as lastError==OK)
  return FanParticipationState::ACTIVE;
}
```

**Key Points:**
- `ready == false` does NOT mean ERROR (it means 'not handshaked yet')
- ERROR state is derived ONLY from `lastError != OK`
- INACTIVE state is set explicitly by user via `setFanEnabled(false)`
- ACTIVE state is default (if enabled and no errors)

### Managing Participation States

**Enable/Disable Fan:**
```cpp
// Disable fan 1 (exclude from control)
SmartMiFanAsync_setFanEnabled(1, false);

// Re-enable fan 1
SmartMiFanAsync_setFanEnabled(1, true);
```

**Check Participation State:**
```cpp
FanParticipationState state = SmartMiFanAsync_getFanParticipationState(0);
switch (state) {
  case FanParticipationState::ACTIVE:
    Serial.println("Fan 0 is active and will receive commands");
    break;
  case FanParticipationState::INACTIVE:
    Serial.println("Fan 0 is disabled by user");
    break;
  case FanParticipationState::ERROR:
    Serial.println("Fan 0 has technical issues");
    break;
}
```

---

## Error Handling

### Error Types

**MiioErr Enum:**
- `OK`: No error
- `TIMEOUT`: No response from device
- `WRONG_SOURCE_IP`: UDP response from unexpected IP address
- `DECRYPT_FAIL`: AES decrypt failed (likely wrong token or stale handshake)
- `INVALID_RESPONSE`: Decrypted but malformed or unexpected payload

### Error Callback

Register an error callback to monitor all errors:

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
      Serial.println("Decrypt failed (wrong token?)");
      break;
    case MiioErr::INVALID_RESPONSE:
      Serial.println("Invalid response");
      break;
    default:
      Serial.println("Unknown error");
  }
}

SmartMiFanAsync_setErrorCallback(onFanError);
```

**Important Notes:**
- Callback is **observational only** - does NOT affect control flow
- Callback is called synchronously during error conditions
- Multiple errors may be reported for the same operation
- `handshakeInvalidated = true` indicates a handshake was invalidated (e.g., during sleep), not an actual error

### Health Checks

Use health checks to verify fan connectivity:

```cpp
// Check single fan
bool healthy = SmartMiFanAsync_healthCheck(0, 2000);

// Check all fans
bool allHealthy = SmartMiFanAsync_healthCheckAll(2000);
```

**Health Check Behavior:**
- Performs handshake with the specified fan
- Updates fan readiness state on success
- Emits error callback on failure (if callback is registered)
- Does not affect fan participation state

---

## Sleep/Wake Integration

### Prepare for Sleep

Before entering sleep, prepare the library:

```cpp
// Prepare for deep sleep (close UDP, invalidate handshakes)
SmartMiFanAsync_prepareForSleep(true, true);

// Or prepare for light sleep (keep UDP open, but invalidate handshakes)
SmartMiFanAsync_prepareForSleep(false, true);
```

**Parameters:**
- `closeUdp`: If `true`, closes the UDP socket. If `false`, keeps socket open.
- `invalidateHandshake`: If `true`, invalidates all fan handshake caches and marks fans as not ready. If `false`, preserves handshake state.

**Behavior:**
- If `invalidateHandshake = true`, all fans are marked as not ready (`ready = false`)
- Error callback is called for each fan that was ready (with `handshakeInvalidated = true`)
- UDP socket is closed if `closeUdp = true`
- Handshake caches are invalidated for next use

### Wake Up

After waking from sleep, wake up the library:

```cpp
// After waking from sleep
SmartMiFanAsync_softWakeUp();

// Re-establish handshakes
SmartMiFanAsync_handshakeAll();
```

**Behavior:**
- Ensures UDP socket is open (`begin(0)` is safe to call multiple times)
- Does not restore handshake state (handshakes must be re-established)
- Should be called before attempting any fan operations after sleep

### System State Integration

The library provides a `SystemState` enum for project-level system state management:

```cpp
enum class SystemState {
  ACTIVE,   // BLE sensors connected OR Web/UI interaction OR first outgoing fan command
  IDLE,     // no BLE sensors, no UI interaction, system remains awake
  SLEEP     // prolonged inactivity, miio transport is inactive
};
```

**Usage Pattern:**
```cpp
SystemState currentState = SystemState::IDLE;

// Project decides to enter sleep
if (shouldSleep()) {
  currentState = SystemState::SLEEP;
  SmartMiFanAsync_prepareForSleep(true, true);
}

// Project decides to wake up
if (shouldWake()) {
  currentState = SystemState::ACTIVE;
  SmartMiFanAsync_softWakeUp();
  SmartMiFanAsync_handshakeAll();
}
```

**Note:** The library does not track or manage system state. This is purely a project-level concern.

---

## Related Documentation

- [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) - System architecture
- [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) - State machines
- [06_APIS.md](./06_APIS.md) - API reference
- [09_EXAMPLES.md](./09_EXAMPLES.md) - Example sketches

