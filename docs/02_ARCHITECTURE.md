# SmartMiFanAsync Architecture

## System Architecture Overview

SmartMiFanAsync follows a **modular, state machine-based architecture** with clear separation between discovery, query, control, and participation state management. The design emphasizes:
- **Asynchronous (non-blocking) operations** throughout
- **State machine-based workflows** for discovery and query
- **Multiple fan support** with independent per-fan tracking
- **Resource efficiency** (static buffers, minimal heap allocation)
- **Fan participation state management** (ACTIVE, INACTIVE, ERROR)

---

## Architecture Layers

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   User Code  │  │   Callbacks   │  │   Examples   │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Public API Layer                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │  Discovery   │  │    Query     │  │    Control   │        │
│  │   Functions  │  │   Functions  │  │   Functions  │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │ Fast Connect │  │Smart Connect │  │ Participation│        │
│  │   Functions  │  │   Functions  │  │   Functions  │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Core Components                               │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              Discovery Context                           │ │
│  │  • State machine (IDLE → SENDING_HELLO → ... → COMPLETE)│ │
│  │  • Candidate collection                                  │ │
│  │  • Async device query                                    │ │
│  │  • Token matching                                        │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              Query Context                                │ │
│  │  • State machine (IDLE → WAITING_HELLO → ... → COMPLETE) │ │
│  │  • Single device query                                    │ │
│  │  • Async miio.info query                                  │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              Smart Connect Context                        │ │
│  │  • State machine (IDLE → VALIDATING → ... → COMPLETE)    │ │
│  │  • Fast Connect validation                                │ │
│  │  • Discovery fallback                                     │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              Discovered Fans Array                        │ │
│  │  • Max 16 fans                                            │ │
│  │  • Per-fan state (ready, lastError, userEnabled)         │ │
│  │  • Participation state derivation                         │ │
│  └──────────────────────────────────────────────────────────┘ │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │              Client Instance                              │ │
│  │  • Handshake cache                                        │ │
│  │  • Control operations (setPower, setSpeed)                │ │
│  │  • Encryption/decryption                                  │ │
│  └──────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Protocol Layer (miIO)                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   Hello      │  │   Encrypted  │  │   Commands   │        │
│  │   Packets    │  │   Messages    │  │   (miot)     │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Network Layer (UDP)                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐        │
│  │   WiFiUDP    │  │   Port 54321 │  │   Broadcast   │        │
│  └──────────────┘  └──────────────┘  └──────────────┘        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   ESP32 WiFi     │
                    │   (Hardware)     │
                    └─────────────────┘
```

---

## Module Organization

### Public API (`SmartMiFanAsync.h`)

**Discovery Functions**
- `SmartMiFanAsync_startDiscovery()` - Start async discovery
- `SmartMiFanAsync_updateDiscovery()` - Update discovery state machine
- `SmartMiFanAsync_getDiscoveryState()` - Get current state
- `SmartMiFanAsync_isDiscoveryComplete()` - Check if complete
- `SmartMiFanAsync_isDiscoveryInProgress()` - Check if in progress
- `SmartMiFanAsync_cancelDiscovery()` - Cancel discovery

**Query Functions**
- `SmartMiFanAsync_startQueryDevice()` - Start async query
- `SmartMiFanAsync_updateQueryDevice()` - Update query state machine
- `SmartMiFanAsync_getQueryState()` - Get current state
- `SmartMiFanAsync_isQueryComplete()` - Check if complete
- `SmartMiFanAsync_isQueryInProgress()` - Check if in progress
- `SmartMiFanAsync_cancelQuery()` - Cancel query

**Fast Connect Functions**
- `SmartMiFanAsync_setFastConnectConfig()` - Set Fast Connect config
- `SmartMiFanAsync_registerFastConnectFans()` - Register fans
- `SmartMiFanAsync_validateFastConnectFans()` - Validate fans
- `SmartMiFanAsync_setFastConnectValidationCallback()` - Set callback

**Smart Connect Functions**
- `SmartMiFanAsync_startSmartConnect()` - Start Smart Connect
- `SmartMiFanAsync_updateSmartConnect()` - Update Smart Connect
- `SmartMiFanAsync_getSmartConnectState()` - Get current state
- `SmartMiFanAsync_isSmartConnectComplete()` - Check if complete
- `SmartMiFanAsync_isSmartConnectInProgress()` - Check if in progress
- `SmartMiFanAsync_cancelSmartConnect()` - Cancel Smart Connect

**Control Functions**
- `SmartMiFanAsync_handshakeAll()` - Handshake all fans (synchronous)
- `SmartMiFanAsync_setPowerAll()` - Set power all fans (synchronous)
- `SmartMiFanAsync_setSpeedAll()` - Set speed all fans (synchronous)
- `SmartMiFanAsync_handshakeAllOrchestrated()` - Handshake ACTIVE fans only
- `SmartMiFanAsync_setPowerAllOrchestrated()` - Set power ACTIVE fans only (rate limited)
- `SmartMiFanAsync_setSpeedAllOrchestrated()` - Set speed ACTIVE fans only (rate limited)

**Fan Participation State Functions**
- `SmartMiFanAsync_getFanParticipationState()` - Get participation state
- `SmartMiFanAsync_setFanEnabled()` - Enable/disable fan
- `SmartMiFanAsync_isFanEnabled()` - Check if enabled

**Error and Health Functions**
- `SmartMiFanAsync_setErrorCallback()` - Set error callback
- `SmartMiFanAsync_isFanReady()` - Check if fan is ready
- `SmartMiFanAsync_getFanLastError()` - Get last error
- `SmartMiFanAsync_healthCheck()` - Health check single fan
- `SmartMiFanAsync_healthCheckAll()` - Health check all fans

**Sleep/Wake Functions**
- `SmartMiFanAsync_prepareForSleep()` - Prepare for sleep
- `SmartMiFanAsync_softWakeUp()` - Wake up after sleep

**Helper Functions**
- `SmartMiFanAsync_resetDiscoveredFans()` - Clear discovered fans
- `SmartMiFanAsync_getDiscoveredFans()` - Get discovered fans array
- `SmartMiFanAsync_printDiscoveredFans()` - Print discovered fans

### Core Implementation (`SmartMiFanAsync.cpp`)

**Discovery Context**
- `DiscoveryContext` structure - Tracks discovery state machine
- `attemptMiioInfoAsync()` - Async device info query
- `storeHelloCandidate()` - Store hello response candidate
- `candidateExists()` - Check if candidate already exists
- `fanAlreadyStored()` - Check if fan already in discovered list

**Query Context**
- `QueryContext` structure - Tracks query state machine
- `attemptMiioInfoAsync()` - Async device info query (overload for QueryContext)

**Smart Connect Context**
- `SmartConnectContext` structure - Tracks Smart Connect state machine
- `SmartConnectCollectFailedFans()` - Internal callback for failed Fast Connect fans

**Fast Connect Configuration**
- `FastConnectConfigEntry` structure - Stores Fast Connect config
- `g_fastConnectConfig[]` - Array of Fast Connect entries (max 4)

**Discovered Fans Array**
- `SmartMiFanDiscoveredDevice` structure - Stores discovered fan info
- `g_discoveredFans[]` - Array of discovered fans (max 16)
- `appendDiscoveredFan()` - Add fan to discovered list

**Client Instance**
- `SmartMiFanAsyncClient` class - Handles individual fan control
- `handshake()` - Establish encrypted session (cached)
- `setPower()` - Set fan power (synchronous)
- `setSpeed()` - Set fan speed (synchronous)
- `miotSetPropertyUint()` - Send miot property (uint)
- `miotSetPropertyBool()` - Send miot property (bool)
- `encryptPayload()` - Encrypt command payload
- `deriveKeyIv()` - Derive encryption key/IV from token

**Protocol Functions**
- `computeKeyIv()` - Compute encryption key/IV from token
- `hexToBytes16Helper()` - Convert hex string to bytes
- `jsonExtractString()` - Extract string from JSON
- `jsonExtractUint()` - Extract uint from JSON
- `pkcs7Unpad()` - Remove PKCS7 padding
- `isSupportedModel()` - Check if model is supported
- `getSpeedParams()` - Get speed control parameters for model

---

## Protocol Details

### miIO Protocol

The library uses the **miIO protocol** over UDP (port 54321):

**Hello Packet (Discovery)**
```
[0x21, 0x31, 0x00, 0x20] + [0xFF × 28]
```
- Magic: `0x2131`
- Length: `0x0020` (32 bytes)
- Payload: All `0xFF` (unencrypted)

**Hello Response**
```
[Magic: 2 bytes] [Length: 2 bytes] [Unknown: 4 bytes] 
[Device ID: 4 bytes] [Timestamp: 4 bytes] [Checksum: 16 bytes]
```
- Device ID: Unique device identifier
- Timestamp: Device timestamp (used for encryption)
- Checksum: MD5 checksum

**Encrypted Message**
```
[Header: 32 bytes] + [Encrypted Payload: variable]
```
- Header: Magic, Length, Unknown, Device ID, Timestamp, Checksum
- Payload: AES-128-CBC encrypted JSON
- Key/IV: Derived from token using MD5

**Encryption**
- Algorithm: AES-128-CBC
- Key: MD5(token)
- IV: MD5(key + token)
- Padding: PKCS7

**Commands**
- `miIO.info` - Get device information
- `set_properties` - Set device properties (miot protocol)
  - Power: `siid=2, piid=1` (bool)
  - Speed: `siid=2, piid=6/8/10/11` (uint, model-dependent)

---

## State Machine Architecture

### Discovery State Machine

**States:**
- `IDLE` - No discovery active
- `SENDING_HELLO` - Sending broadcast hello packets
- `COLLECTING_CANDIDATES` - Collecting device responses (sub-state of SENDING_HELLO)
- `QUERYING_DEVICES` - Querying discovered devices for info
- `COMPLETE` - Discovery finished successfully
- `ERROR` - Discovery failed
- `TIMEOUT` - Discovery timed out

**Flow:**
1. `IDLE` → `SENDING_HELLO`: Start discovery
2. `SENDING_HELLO` → `QUERYING_DEVICES`: After collection period
3. `QUERYING_DEVICES` → `COMPLETE`: All devices queried
4. Any state → `ERROR`/`TIMEOUT`: On error/timeout

See: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Discovery State Machine"

### Query State Machine

**States:**
- `IDLE` - No query active
- `WAITING_HELLO` - Waiting for hello response
- `SENDING_QUERY` - Sending device info query
- `WAITING_RESPONSE` - Waiting for encrypted response
- `COMPLETE` - Query finished successfully
- `ERROR` - Query failed
- `TIMEOUT` - Query timed out

**Flow:**
1. `IDLE` → `WAITING_HELLO`: Start query
2. `WAITING_HELLO` → `SENDING_QUERY`: Hello received
3. `SENDING_QUERY` → `WAITING_RESPONSE`: Query sent
4. `WAITING_RESPONSE` → `COMPLETE`: Response received
5. Any state → `ERROR`/`TIMEOUT`: On error/timeout

See: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Query State Machine"

### Smart Connect State Machine

**States:**
- `IDLE` - No Smart Connect active
- `VALIDATING_FAST_CONNECT` - Validating Fast Connect fans
- `STARTING_DISCOVERY` - Starting discovery for failed fans
- `DISCOVERING` - Async discovery in progress
- `COMPLETE` - Smart Connect finished successfully
- `ERROR` - Smart Connect failed

**Flow:**
1. `IDLE` → `VALIDATING_FAST_CONNECT`: Start Smart Connect
2. `VALIDATING_FAST_CONNECT` → `STARTING_DISCOVERY`: If failed fans exist
3. `STARTING_DISCOVERY` → `DISCOVERING`: Discovery started
4. `DISCOVERING` → `COMPLETE`: Discovery finished
5. `VALIDATING_FAST_CONNECT` → `COMPLETE`: If all fans connected
6. Any state → `ERROR`: On error

See: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Smart Connect State Machine"

---

## Fan Participation State System

### Three-State System

Each fan can be in one of three participation states:

1. **ACTIVE**: Fan is enabled and ready - participates in control (default)
2. **INACTIVE**: Fan is disabled by user/project - excluded from commands
3. **ERROR**: Fan has technical issues - not available (automatically derived)

### State Derivation

```cpp
if (fan.lastError != MiioErr::OK) {
  return FanParticipationState::ERROR;  // Technical issues
}
if (!fan.userEnabled) {
  return FanParticipationState::INACTIVE;  // User disabled
}
return FanParticipationState::ACTIVE;  // Default
```

**Key Points:**
- `ready == false` does NOT mean ERROR (it means 'not handshaked yet')
- ERROR state is derived ONLY from `lastError != OK`
- INACTIVE state is set explicitly by user via `setFanEnabled(false)`
- ACTIVE state is default (if enabled and no errors)

### Orchestrated Functions

Orchestrated functions (`*AllOrchestrated()`) respect participation states:
- Only ACTIVE fans receive commands
- INACTIVE and ERROR fans are skipped
- Commands are sent in deterministic order (Fan 0 → 1 → 2 → ...)
- Command coalescing: max 1 command per second

See: [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Fan Participation States"

---

## Memory Management

### Static Buffers
- **Discovered fans array**: Fixed-size array (max 16 fans)
- **Fast Connect config**: Fixed-size array (max 4 fans)
- **Discovery context**: Single instance (not reentrant)
- **Query context**: Single instance (not reentrant)
- **Smart Connect context**: Single instance (not reentrant)
- **Client instance**: Single global instance (reused for all fans)

### Heap Allocation
- **UDP socket**: Managed by WiFiUDP (ESP32 internal)
- **JSON parsing**: Stack-based (no heap allocation)
- **Encryption buffers**: Stack-based (temporary)

### String Management
- **Device tokens**: Stored as hex strings (33 bytes: 32 chars + null)
- **Model names**: Fixed-size char arrays (24 bytes)
- **Firmware/hardware versions**: Fixed-size char arrays (16 bytes)
- **IP addresses**: IPAddress objects (4 bytes)

---

## Thread Safety

### Single-Threaded Main Loop
All code runs in the main `loop()` function (single thread). UDP callbacks run in WiFi context but should not directly modify shared state.

### State Machine Updates
- **From main loop**: Call `*_update*()` functions to advance state machines
- **From UDP callbacks**: Read packets, but let main loop process them
- **State transitions**: Atomic (single assignment)

### Error Callbacks
- **Observational only**: Callbacks do NOT affect control flow
- **Synchronous**: Called during error conditions
- **Non-blocking**: Callbacks must not block

---

## Error Handling

### Discovery Errors
- **Timeout**: Discovery times out after specified duration
- **No candidates**: No devices respond to hello packets
- **Query failures**: Device query fails (wrong token, network issues)
- **Max fans reached**: Discovered fans array is full (16 fans)

### Query Errors
- **Timeout**: Query times out (2 seconds per device)
- **Wrong source IP**: UDP response from unexpected device (filtered automatically)
- **Decrypt failure**: AES decrypt failed (wrong token or stale handshake)
- **Invalid response**: Decrypted but malformed payload

### Control Errors
- **Handshake failure**: Cannot establish encrypted session
- **Command timeout**: No response to command (1.5 seconds)
- **Wrong source IP**: Response from unexpected device (filtered automatically)
- **Decrypt failure**: Cannot decrypt response (stale handshake)

### Error Callback
All errors are reported via error callback (if registered):
- **Observational**: Does not affect control flow
- **Structured**: Includes fan index, IP, operation, error type, elapsed time
- **Handshake invalidation**: Special flag for sleep/wake scenarios

---

## Extension Points

### Adding a New Fan Model

1. **Add to supported models array**:
   ```cpp
   const char *kSupportedModels[] = {
       // ... existing models ...
       "new.model.name"
   };
   ```

2. **Add speed control parameters** (if needed):
   ```cpp
   if (strcmp(model, "new.model.name") == 0) {
       siid = 2;
       piid = 6;  // Adjust based on model
       return true;
   }
   ```

3. **Test with actual device** to verify parameters

### Adding a New Control Command

1. **Add miot property function**:
   ```cpp
   bool miotSetPropertyString(const char *name, int siid, int piid, const char *value);
   ```

2. **Add public API function**:
   ```cpp
   bool SmartMiFanAsync_setModeAll(uint8_t mode);
   ```

3. **Add orchestrated version** (if needed):
   ```cpp
   bool SmartMiFanAsync_setModeAllOrchestrated(uint8_t mode);
   ```

---

## Related Documentation

- [03_FUNCTIONS.md](./03_FUNCTIONS.md) - Core functions and workflows
- [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) - State machines
- [06_APIS.md](./06_APIS.md) - API reference
- [09_EXAMPLES.md](./09_EXAMPLES.md) - Example sketches

