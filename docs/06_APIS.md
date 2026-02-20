# SmartMiFanAsync API Reference

Complete API reference for SmartMiFanAsync library.

---

## Discovery Functions

### `bool SmartMiFanAsync_startDiscovery(WiFiUDP &udp, const char *const tokens[], size_t tokenCount, unsigned long discoveryMs = 3000)`

Start asynchronous discovery of SmartMi fans.

**Parameters**:
- `udp`: WiFiUDP instance for network communication
- `tokens`: Array of token strings (32 hex characters each)
- `tokenCount`: Number of tokens in the array
- `discoveryMs`: Discovery timeout in milliseconds (default: 3000)

**Returns**: `true` if discovery started successfully, `false` if another discovery is already in progress or Fast Connect is enabled

**Example**:
```cpp
const char *TOKENS[] = {"token1", "token2"};
SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, 2, 5000);
```

---

### `bool SmartMiFanAsync_startDiscovery(WiFiUDP &udp, const char *tokenHex, unsigned long discoveryMs = 3000)`

Start asynchronous discovery with a single token.

**Parameters**:
- `udp`: WiFiUDP instance for network communication
- `tokenHex`: Single token string (32 hex characters)
- `discoveryMs`: Discovery timeout in milliseconds (default: 3000)

**Returns**: `true` if discovery started successfully

**Example**:
```cpp
SmartMiFanAsync_startDiscovery(fanUdp, "your-32-char-token", 5000);
```

---

### `bool SmartMiFanAsync_updateDiscovery()`

Update the discovery state machine. Call this in your `loop()` function.

**Returns**: `true` if discovery is still in progress, `false` if complete, failed, or not started

**Example**:
```cpp
void loop() {
  if (SmartMiFanAsync_updateDiscovery()) {
    // Discovery still running
  }
}
```

---

### `DiscoveryState SmartMiFanAsync_getDiscoveryState()`

Get the current discovery state.

**Returns**: Current `DiscoveryState` enum value

**States**:
- `IDLE`: No discovery active
- `SENDING_HELLO`: Sending broadcast hello packets
- `COLLECTING_CANDIDATES`: Collecting device responses
- `QUERYING_DEVICES`: Querying discovered devices for info
- `COMPLETE`: Discovery finished successfully
- `ERROR`: Discovery failed
- `TIMEOUT`: Discovery timed out

**Example**:
```cpp
DiscoveryState state = SmartMiFanAsync_getDiscoveryState();
if (state == DiscoveryState::QUERYING_DEVICES) {
  Serial.println("Querying devices...");
}
```

---

### `bool SmartMiFanAsync_isDiscoveryComplete()`

Check if discovery is complete.

**Returns**: `true` if discovery completed successfully

**Example**:
```cpp
if (SmartMiFanAsync_isDiscoveryComplete()) {
  // Use discovered fans
}
```

---

### `bool SmartMiFanAsync_isDiscoveryInProgress()`

Check if discovery is currently running.

**Returns**: `true` if discovery is active and not finished

**Example**:
```cpp
if (SmartMiFanAsync_isDiscoveryInProgress()) {
  SmartMiFanAsync_updateDiscovery();
}
```

---

### `void SmartMiFanAsync_cancelDiscovery()`

Cancel an ongoing discovery operation.

**Example**:
```cpp
if (shouldCancel) {
  SmartMiFanAsync_cancelDiscovery();
}
```

---

## Query Functions

### `bool SmartMiFanAsync_startQueryDevice(WiFiUDP &udp, const IPAddress &ip, const char *tokenHex)`

Start asynchronous query of a single device by IP address.

**Parameters**:
- `udp`: WiFiUDP instance for network communication
- `ip`: IP address of the device to query
- `tokenHex`: Token string (32 hex characters)

**Returns**: `true` if query started successfully, `false` if another query is in progress

**Example**:
```cpp
IPAddress fanIp;
fanIp.fromString("192.168.1.100");
SmartMiFanAsync_startQueryDevice(fanUdp, fanIp, "your-token");
```

---

### `bool SmartMiFanAsync_updateQueryDevice()`

Update the query state machine. Call this in your `loop()` function.

**Returns**: `true` if query is still in progress, `false` if complete or failed

**Example**:
```cpp
void loop() {
  if (SmartMiFanAsync_updateQueryDevice()) {
    // Query still running
  }
}
```

---

### `QueryState SmartMiFanAsync_getQueryState()`

Get the current query state.

**Returns**: Current `QueryState` enum value

**States**:
- `IDLE`: No query active
- `WAITING_HELLO`: Waiting for hello response
- `SENDING_QUERY`: Sending device info query
- `WAITING_RESPONSE`: Waiting for encrypted response
- `COMPLETE`: Query finished successfully
- `ERROR`: Query failed
- `TIMEOUT`: Query timed out

---

### `bool SmartMiFanAsync_isQueryComplete()`

Check if query is complete.

**Returns**: `true` if query completed successfully

---

### `bool SmartMiFanAsync_isQueryInProgress()`

Check if query is currently running.

**Returns**: `true` if query is active and not finished

---

### `void SmartMiFanAsync_cancelQuery()`

Cancel an ongoing query operation.

---

## Fast Connect Functions

### `bool SmartMiFanAsync_setFastConnectConfig(const SmartMiFanFastConnectEntry entries[], size_t count)`

Set the Fast Connect configuration with fan entries (IP and token).

**Parameters**:
- `entries`: Array of `SmartMiFanFastConnectEntry` structures
- `count`: Number of entries (maximum 4)

**Returns**: `true` if at least one valid entry was configured

**Example**:
```cpp
SmartMiFanFastConnectEntry fans[] = {
  {"192.168.1.100", "token123..."},                     // Model queried via queryInfo()
  {"192.168.1.101", "token456...", "zhimi.fan.za5"}     // Model provided (skips queryInfo)
};
SmartMiFanAsync_setFastConnectConfig(fans, 2);
```

**Note**: When model is provided, `queryInfo()` is skipped during validation for faster, more stable connection (~100ms+ faster per fan).

---

### `bool SmartMiFanAsync_registerFastConnectFans(WiFiUDP &udp)`

Register configured Fast Connect fans into the discovered fans array. Fans are registered immediately without validation (lazy validation on first handshake).

**Parameters**:
- `udp`: WiFiUDP instance for network communication

**Returns**: `true` if at least one fan was registered

**Note**: Call `SmartMiFanAsync_resetDiscoveredFans()` before this if you want a clean list.

---

### `bool SmartMiFanAsync_isFastConnectEnabled()`

Check if Fast Connect mode is currently enabled.

**Returns**: `true` if Fast Connect is enabled

---

### `void SmartMiFanAsync_setFastConnectEnabled(bool enabled)`

Enable or disable Fast Connect mode at runtime.

**Parameters**:
- `enabled`: `true` to enable, `false` to disable

**Note**: When Fast Connect is enabled, `SmartMiFanAsync_startDiscovery()` will return `false` (discovery is skipped).

---

### `void SmartMiFanAsync_clearFastConnectConfig()`

Clear the Fast Connect configuration.

---

### `void SmartMiFanAsync_setFastConnectValidationCallback(FastConnectValidationCallback callback)`

Set an optional callback function that will be called once after all Fast Connect fans are validated.

**Parameters**:
- `callback`: Function pointer to callback, or `nullptr` to disable

**Callback Signature**:
```cpp
void callback(const SmartMiFanFastConnectResult results[], size_t count);
```

**Example**:
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
```

---

### `bool SmartMiFanAsync_validateFastConnectFans(WiFiUDP &udp)`

Validate all Fast Connect fans by performing handshake and querying device info. If a validation callback is set, it will be called once with all validation results after completion.

**Parameters**:
- `udp`: WiFiUDP instance for network communication

**Returns**: `true` if validation process started/completed successfully

**Behavior**:
- For each configured fan:
  1. Performs handshake to establish encrypted session
  2. If handshake succeeds, queries `miIO.info` to retrieve model, firmware, and hardware version
  3. Stores model info in the discovered fans array (required for correct `setSpeed()` parameters)
- If a callback is set via `SmartMiFanAsync_setFastConnectValidationCallback()`, validates all fans and calls the callback with results
- If no callback is set, falls back to `SmartMiFanAsync_handshakeAll()` behavior
- Fans must be registered first using `SmartMiFanAsync_registerFastConnectFans()`

**Note**: The model info retrieval is essential for `setSpeed()` to work correctly, as different fan models use different MIoT service/property IDs (siid/piid).

---

## Smart Connect Functions

### `bool SmartMiFanAsync_startSmartConnect(WiFiUDP &udp, unsigned long discoveryMs = 3000)`

Start Smart Connect process. Requires Fast Connect configuration to be set first. Tries Fast Connect first, then automatically starts Discovery for any failed fans.

**Parameters**:
- `udp`: WiFiUDP instance for network communication
- `discoveryMs`: Discovery timeout in milliseconds for failed fans (default: 3000)

**Returns**: `true` if Smart Connect started successfully

**Note**: Fast Connect configuration must be set before calling this function.

---

### `bool SmartMiFanAsync_updateSmartConnect()`

Update the Smart Connect state machine. Call this in your `loop()` function.

**Returns**: `true` if Smart Connect is still in progress, `false` if complete, failed, or not started

---

### `SmartConnectState SmartMiFanAsync_getSmartConnectState()`

Get the current Smart Connect state.

**Returns**: Current `SmartConnectState` enum value

**States**:
- `IDLE`: No Smart Connect active
- `VALIDATING_FAST_CONNECT`: Validating Fast Connect fans
- `STARTING_DISCOVERY`: Starting discovery for failed fans
- `DISCOVERING`: Async discovery in progress
- `COMPLETE`: Smart Connect finished successfully
- `ERROR`: Smart Connect failed

---

### `bool SmartMiFanAsync_isSmartConnectComplete()`

Check if Smart Connect is complete.

**Returns**: `true` if Smart Connect completed successfully

---

### `bool SmartMiFanAsync_isSmartConnectInProgress()`

Check if Smart Connect is currently running.

**Returns**: `true` if Smart Connect is active and not finished

---

### `void SmartMiFanAsync_cancelSmartConnect()`

Cancel ongoing Smart Connect process.

**Note**: This will also cancel any ongoing Discovery process if Smart Connect is in the discovery phase.

---

## Helper Functions

### `void SmartMiFanAsync_resetDiscoveredFans()`

Clear the list of discovered fans.

**Example**:
```cpp
SmartMiFanAsync_resetDiscoveredFans();
SmartMiFanAsync_startDiscovery(fanUdp, TOKENS, TOKEN_COUNT, 5000);
```

---

### `const SmartMiFanDiscoveredDevice *SmartMiFanAsync_getDiscoveredFans(size_t &count)`

Get the list of discovered fans.

**Parameters**:
- `count`: Output parameter that receives the number of discovered fans

**Returns**: Pointer to array of `SmartMiFanDiscoveredDevice` structures

**Example**:
```cpp
size_t count = 0;
const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
for (size_t i = 0; i < count; i++) {
  Serial.print("Fan ");
  Serial.print(i);
  Serial.print(": ");
  Serial.println(fans[i].ip);
}
```

---

### `void SmartMiFanAsync_printDiscoveredFans(Stream &out = Serial)`

Print discovered fans to Serial (or other Stream).

**Parameters**:
- `out`: Stream to print to (default: Serial)

**Example**:
```cpp
SmartMiFanAsync_printDiscoveredFans();
```

---

## Control Functions

### `bool SmartMiFanAsync_handshakeAll()`

Perform handshake with all discovered fans.

**Returns**: `true` if all handshakes succeeded, `false` if any failed

**Note**: This function is synchronous and may block.

---

### `bool SmartMiFanAsync_setPowerAll(bool on)`

Set power state for all discovered fans.

**Parameters**:
- `on`: `true` to turn on, `false` to turn off

**Returns**: `true` if all operations succeeded

**Note**: This function is synchronous and may block.

---

### `bool SmartMiFanAsync_setSpeedAll(uint8_t percent)`

Set speed for all discovered fans.

**Parameters**:
- `percent`: Speed percentage (1-100)

**Returns**: `true` if all operations succeeded

**Note**: This function is synchronous and may block.

---

## Fan Participation State API

### `FanParticipationState SmartMiFanAsync_getFanParticipationState(uint8_t fanIndex)`

Get the participation state of a specific fan.

**Parameters**:
- `fanIndex`: Index of the fan (0-based)

**Returns**: `FanParticipationState` enum value (ACTIVE, INACTIVE, or ERROR)

**States**:
- `ACTIVE`: Fan is enabled and ready - participates in control (default)
- `INACTIVE`: Fan is disabled by user/project - excluded from commands
- `ERROR`: Fan has technical issues - not available (automatically derived)

**Example**:
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

### `void SmartMiFanAsync_setFanEnabled(uint8_t fanIndex, bool enabled)`

Enable or disable a fan from receiving commands. Setting a fan to disabled makes it INACTIVE (excluded from orchestrated commands).

**Parameters**:
- `fanIndex`: Index of the fan (0-based)
- `enabled`: `true` to enable (ACTIVE), `false` to disable (INACTIVE)

**Note**: 
- Setting `enabled = false` makes the fan INACTIVE (excluded from orchestrated commands)
- Setting `enabled = true` makes the fan ACTIVE (if technically ready) or ERROR (if not ready)
- ERROR state is automatically derived from technical readiness and cannot be set directly

**Example**:
```cpp
// Disable fan 1 (exclude from control operations)
SmartMiFanAsync_setFanEnabled(1, false);

// Re-enable fan 1
SmartMiFanAsync_setFanEnabled(1, true);
```

---

### `bool SmartMiFanAsync_isFanEnabled(uint8_t fanIndex)`

Check if a fan is enabled by user/project.

**Parameters**:
- `fanIndex`: Index of the fan (0-based)

**Returns**: `true` if fan is enabled, `false` if disabled

**Note**: This only checks the user-enabled state. Use `getFanParticipationState()` to get the full participation state (including ERROR).

---

## Orchestrated Control Functions

### `bool SmartMiFanAsync_handshakeAllOrchestrated()`

Perform handshake with all ACTIVE fans only. INACTIVE and ERROR fans are skipped.

**Returns**: `true` if all ACTIVE fan handshakes succeeded, `false` if any failed

**Example**:
```cpp
// Only handshakes fans that are ACTIVE (enabled and ready)
if (SmartMiFanAsync_handshakeAllOrchestrated()) {
  Serial.println("All active fans handshaked successfully");
}
```

---

### `bool SmartMiFanAsync_setPowerAllOrchestrated(bool on)`

Set power state for all ACTIVE fans only. INACTIVE and ERROR fans are skipped. Includes command coalescing (max 1 command per second).

**Parameters**:
- `on`: `true` to turn on, `false` to turn off

**Returns**: `true` if all ACTIVE fan operations succeeded

**Example**:
```cpp
// Only turns on fans that are ACTIVE
// Fans that are INACTIVE or ERROR are automatically skipped
SmartMiFanAsync_setPowerAllOrchestrated(true);
```

---

### `bool SmartMiFanAsync_setSpeedAllOrchestrated(uint8_t percent)`

Set speed for all ACTIVE fans only. INACTIVE and ERROR fans are skipped. Includes command coalescing (max 1 command per second).

**Parameters**:
- `percent`: Speed percentage (1-100)

**Returns**: `true` if all ACTIVE fan operations succeeded

**Example**:
```cpp
// Only sets speed for fans that are ACTIVE
// Fans that are INACTIVE or ERROR are automatically skipped
SmartMiFanAsync_setSpeedAllOrchestrated(50);
```

---

## Error and Health Callback API

### `void SmartMiFanAsync_setErrorCallback(FanErrorCallback cb)`

Register a callback function that will be called when miIO operations encounter errors. The callback is observational only and does NOT affect control flow.

**Parameters**:
- `cb`: Function pointer to callback, or `nullptr` to disable

**Callback Signature**:
```cpp
void callback(const FanErrorInfo& info);
```

**Callback Structure**:
```cpp
struct FanErrorInfo {
  uint8_t fanIndex;              // Index of the fan (0-based)
  IPAddress ip;                  // Fan IP address
  FanOp operation;               // Operation that failed
  MiioErr error;                 // Error type
  uint32_t elapsedMs;            // Time elapsed before error
  bool handshakeInvalidated;     // true if handshake was invalidated
};
```

**Important Notes**:
- Callback must never block, trigger retries, or modify discovery/smart connect state
- Callback is called synchronously during error conditions
- Multiple errors may be reported for the same operation
- `handshakeInvalidated = true` indicates a handshake was invalidated (e.g., during sleep), not an actual error

**Example**:
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

---

### `bool SmartMiFanAsync_isFanReady(uint8_t fanIndex)`

Check if a fan is technically ready (has completed successful handshake).

**Parameters**:
- `fanIndex`: Index of the fan (0-based)

**Returns**: `true` if fan is ready, `false` if not ready or invalid index

**Note**: This checks technical readiness only. Use `getFanParticipationState()` for full participation state.

---

### `MiioErr SmartMiFanAsync_getFanLastError(uint8_t fanIndex)`

Get the last error encountered by a fan.

**Parameters**:
- `fanIndex`: Index of the fan (0-based)

**Returns**: `MiioErr` enum value (OK if no error, or specific error type)

---

### `bool SmartMiFanAsync_healthCheck(uint8_t fanIndex, uint32_t timeoutMs)`

Perform a health check on a specific fan by attempting a handshake.

**Parameters**:
- `fanIndex`: Index of the fan (0-based)
- `timeoutMs`: Handshake timeout in milliseconds

**Returns**: `true` if health check succeeded (handshake successful), `false` if failed

**Behavior**:
- Performs handshake with the specified fan
- Updates fan readiness state on success
- Emits error callback on failure (if callback is registered)
- Does not affect fan participation state

**Example**:
```cpp
// Check health of fan 0
if (SmartMiFanAsync_healthCheck(0, 2000)) {
  Serial.println("Fan 0 is healthy");
} else {
  Serial.println("Fan 0 health check failed");
}
```

---

### `bool SmartMiFanAsync_healthCheckAll(uint32_t timeoutMs)`

Perform health check on all discovered fans.

**Parameters**:
- `timeoutMs`: Handshake timeout in milliseconds for each fan

**Returns**: `true` if all health checks succeeded, `false` if any failed

**Note**: This function is synchronous and may block for `timeoutMs * fanCount` milliseconds.

---

## Transport and Sleep Hooks

### `void SmartMiFanAsync_prepareForSleep(bool closeUdp, bool invalidateHandshake)`

Prepare the library for system sleep. Invalidates handshake caches and optionally closes UDP socket.

**Parameters**:
- `closeUdp`: If `true`, closes the UDP socket. If `false`, keeps socket open.
- `invalidateHandshake`: If `true`, invalidates all fan handshake caches and marks fans as not ready. If `false`, preserves handshake state.

**Behavior**:
- If `invalidateHandshake = true`, all fans are marked as not ready (`ready = false`)
- Error callback is called for each fan that was ready (with `handshakeInvalidated = true`)
- UDP socket is closed if `closeUdp = true`
- Handshake caches are invalidated for next use

**Example**:
```cpp
// Prepare for deep sleep (close UDP, invalidate handshakes)
SmartMiFanAsync_prepareForSleep(true, true);

// Or prepare for light sleep (keep UDP open, but invalidate handshakes)
SmartMiFanAsync_prepareForSleep(false, true);
```

---

### `void SmartMiFanAsync_softWakeUp()`

Wake up the library after sleep. Ensures UDP socket is open and ready for use.

**Behavior**:
- Ensures UDP socket is open (`begin(0)` is safe to call multiple times)
- Does not restore handshake state (handshakes must be re-established)
- Should be called before attempting any fan operations after sleep

**Example**:
```cpp
// After waking from sleep
SmartMiFanAsync_softWakeUp();

// Re-establish handshakes
SmartMiFanAsync_handshakeAll();
```

---

## Client Class

### `SmartMiFanAsyncClient`

Class for controlling individual fan devices. Control operations (setPower, setSpeed) are synchronous.

**Global Instance**: `SmartMiFanAsync` (extern)

**Methods**:
```cpp
// Initialize connection
bool begin(WiFiUDP &udp, const IPAddress &fanAddress, const uint8_t token[16]);
bool begin(WiFiUDP &udp, const char *fanIp, const uint8_t token[16]);

// Handshake
bool handshake(uint32_t timeoutMs = 2000);

// Query device info (requires successful handshake first)
bool queryInfo(char *outModel, size_t modelSize,
               char *outFwVer, size_t fwSize,
               char *outHwVer, size_t hwSize,
               uint32_t *outDid,
               uint32_t timeoutMs = 2000);

// Control
bool setPower(bool on);
bool setSpeed(uint8_t percent);

// Configuration
void setGlobalSpeed(uint8_t percent);
uint8_t getGlobalSpeed() const;
bool setTokenFromHex(const char *tokenHex);
void setToken(const uint8_t token[16]);
const uint8_t *getToken() const;
void setFanAddress(const IPAddress &fanAddress);
IPAddress getFanAddress() const;
void setModel(const char *model);
const char *getModel() const;
bool isReady() const;
void attachUdp(WiFiUDP &udp);
```

**queryInfo() Method**:

Query device information using the established session. Must be called after a successful `handshake()`.

```cpp
bool queryInfo(char *outModel, size_t modelSize,
               char *outFwVer, size_t fwSize,
               char *outHwVer, size_t hwSize,
               uint32_t *outDid,
               uint32_t timeoutMs = 2000);
```

**Parameters**:
- `outModel`: Buffer to receive model string (e.g., "dmaker.fan.p18")
- `modelSize`: Size of model buffer
- `outFwVer`: Buffer to receive firmware version (can be nullptr)
- `fwSize`: Size of firmware version buffer
- `outHwVer`: Buffer to receive hardware version (can be nullptr)
- `hwSize`: Size of hardware version buffer
- `outDid`: Pointer to receive device ID (can be nullptr)
- `timeoutMs`: Timeout in milliseconds (default: 2000)

**Returns**: `true` if query succeeded, `false` on timeout or error

**Example**:
```cpp
if (SmartMiFanAsync.handshake()) {
  char model[24], fw[16], hw[16];
  uint32_t did;
  if (SmartMiFanAsync.queryInfo(model, sizeof(model), fw, sizeof(fw), hw, sizeof(hw), &did)) {
    Serial.printf("Model: %s, FW: %s, HW: %s, DID: %u\n", model, fw, hw, did);
  }
}
```

**Note**: The model is also stored internally and can be retrieved with `getModel()`. This is used by `setSpeed()` to determine the correct MIoT parameters.

**Example**:
```cpp
SmartMiFanAsync.setTokenFromHex("your-token");
if (SmartMiFanAsync.begin(fanUdp, "192.168.1.100", SmartMiFanAsync.getToken())) {
  SmartMiFanAsync.setPower(true);
  SmartMiFanAsync.setSpeed(50);
}
```

---

## Data Structures

### `SmartMiFanDiscoveredDevice`

Structure containing information about a discovered fan.

```cpp
struct SmartMiFanDiscoveredDevice {
  IPAddress ip;        // Device IP address
  uint32_t did;        // Device ID
  char model[24];      // Model name
  char token[33];     // Token (hex string)
  char fw_ver[16];    // Firmware version
  char hw_ver[16];    // Hardware version
  bool ready;          // Technical readiness (true after successful handshake)
  MiioErr lastError;   // Last error encountered (MiioErr::OK if no error)
  bool userEnabled;    // User/project intent: true = enabled, false = disabled (default: true)
};
```

---

### `SmartMiFanFastConnectEntry`

Structure for Fast Connect configuration entry.

```cpp
struct SmartMiFanFastConnectEntry {
  const char* ipStr;      // IP address as string (e.g., "192.168.1.100")
  const char* tokenHex;   // 32-character hex token string
  const char* model;      // Optional: Fan model (e.g., "zhimi.fan.za5") - skips queryInfo if set
};
```

**Usage**:
```cpp
SmartMiFanFastConnectEntry fans[] = {
  {"192.168.1.100", "token..."},                    // Model queried during validation
  {"192.168.1.101", "token...", "zhimi.fan.za5"}    // Model provided (faster, no queryInfo)
};
```

**Note**: When model is provided, `queryInfo()` is skipped during validation for faster, more stable connection. When model is not provided, device info is automatically queried during `validateFastConnectFans()`.

---

### `SmartMiFanFastConnectResult`

Structure for Fast Connect validation result.

```cpp
struct SmartMiFanFastConnectResult {
  IPAddress ip;      // Fan IP address
  char token[33];    // Hex token string
  bool success;      // true = handshake succeeded, false = failed
};
```

---

## Enumerations

### `DiscoveryState`

Enumeration of discovery states.

```cpp
enum class DiscoveryState {
  IDLE,
  SENDING_HELLO,
  COLLECTING_CANDIDATES,
  QUERYING_DEVICES,
  COMPLETE,
  ERROR,
  TIMEOUT
};
```

---

### `QueryState`

Enumeration of query states.

```cpp
enum class QueryState {
  IDLE,
  WAITING_HELLO,
  SENDING_QUERY,
  WAITING_RESPONSE,
  COMPLETE,
  ERROR,
  TIMEOUT
};
```

---

### `SmartConnectState`

Enumeration of Smart Connect states.

```cpp
enum class SmartConnectState {
  IDLE,
  VALIDATING_FAST_CONNECT,
  STARTING_DISCOVERY,
  DISCOVERING,
  COMPLETE,
  ERROR
};
```

---

### `FanParticipationState`

Enumeration of fan participation states.

```cpp
enum class FanParticipationState {
  ACTIVE,     // Fan is enabled and ready - participates in control (default)
  INACTIVE,   // Fan is disabled by user/project - excluded from commands
  ERROR       // Fan has technical issues - not available (automatically derived)
};
```

---

### `MiioErr`

Enumeration of miIO protocol error types.

```cpp
enum class MiioErr {
  OK,              // No error
  TIMEOUT,         // No response from device
  WRONG_SOURCE_IP, // UDP response from unexpected IP address
  DECRYPT_FAIL,    // AES decrypt failed (likely wrong token or stale handshake)
  INVALID_RESPONSE // Decrypted but malformed or unexpected payload
};
```

---

### `FanOp`

Enumeration of fan operations.

```cpp
enum class FanOp {
  Handshake,
  SendCommand,
  ReceiveResponse,
  HealthCheck
};
```

---

### `SystemState`

Enumeration of system states (project-level, library does not manage).

```cpp
enum class SystemState {
  ACTIVE,   // BLE sensors connected OR Web/UI interaction OR first outgoing fan command
  IDLE,     // no BLE sensors, no UI interaction, system remains awake
  SLEEP     // prolonged inactivity, miio transport is inactive
};
```

---

## Related Documentation

- [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) - System architecture
- [03_FUNCTIONS.md](./03_FUNCTIONS.md) - Core functions and workflows
- [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) - State machines
- [09_EXAMPLES.md](./09_EXAMPLES.md) - Example sketches

