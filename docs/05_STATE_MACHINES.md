# SmartMiFanAsync State Machines

## Overview

SmartMiFanAsync uses state machines to manage asynchronous operations. All state machines are **non-blocking** and must be updated in the main loop via `*_update*()` functions.

---

## Discovery State Machine

### State Diagram

```
        ┌─────┐
        │ IDLE│
        └──┬──┘
           │ startDiscovery()
           ▼
┌──────────────────┐
│ SENDING_HELLO    │◄──┐
│ (broadcast)      │   │ (send every 500ms)
└────────┬─────────┘   │
         │             │
         │ (after      │
         │  timeout)   │
         ▼             │
┌──────────────────┐  │
│ QUERYING_DEVICES  │  │
│ (async query)     │──┘
└────────┬──────────┘
         │
         │ (all devices queried)
         ▼
    ┌─────────┐
    │ COMPLETE│
    └─────────┘

    ┌─────────┐
    │ ERROR   │ (on error)
    └─────────┘

    ┌─────────┐
    │ TIMEOUT │ (on timeout)
    └─────────┘
```

### States

| State | Description |
|-------|-------------|
| `IDLE` | No discovery active |
| `SENDING_HELLO` | Sending broadcast hello packets to discover devices |
| `COLLECTING_CANDIDATES` | Collecting device responses (sub-state of SENDING_HELLO) |
| `QUERYING_DEVICES` | Querying discovered devices for info (model, firmware, hardware) |
| `COMPLETE` | Discovery finished successfully |
| `ERROR` | Discovery failed |
| `TIMEOUT` | Discovery timed out |

### State Transitions

**IDLE → SENDING_HELLO**
- Trigger: `SmartMiFanAsync_startDiscovery()` called
- Action: Initialize discovery context, send first hello packet

**SENDING_HELLO → QUERYING_DEVICES**
- Trigger: Collection period elapsed (discoveryMs timeout)
- Action: Start querying candidates with tokens

**QUERYING_DEVICES → COMPLETE**
- Trigger: All candidates queried (or max fans reached)
- Action: Discovery finished, discovered fans available

**Any State → ERROR**
- Trigger: Fatal error during discovery
- Action: Discovery aborted, error state set

**Any State → TIMEOUT**
- Trigger: Discovery timeout exceeded
- Action: Discovery aborted, timeout state set

### State Machine Update

```cpp
void loop() {
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();  // Advance state machine
  }
}
```

**Update Function Behavior:**
- `SENDING_HELLO`: Sends hello every 500ms, collects responses
- `QUERYING_DEVICES`: Processes one candidate+token combination per call
- Returns `true` if still in progress, `false` if complete/error/timeout

### Timeout Handling

**Collection Phase:**
- Timeout: `discoveryMs` (default: 3000ms)
- After timeout, moves to `QUERYING_DEVICES` phase

**Query Phase:**
- Timeout: `discoveryMs * 3` minimum, or `discoveryMs + (candidateCount × tokenCount × 2500ms)`
- Ensures enough time to try all candidate/token combinations
- After timeout, moves to `TIMEOUT` state

---

## Query State Machine

### State Diagram

```
        ┌─────┐
        │ IDLE│
        └──┬──┘
           │ startQueryDevice()
           ▼
┌──────────────────┐
│ WAITING_HELLO    │◄──┐
│ (send hello)     │   │ (resend every 500ms)
└────────┬─────────┘   │
         │             │
         │ (hello      │
         │  received)  │
         ▼             │
┌──────────────────┐  │
│ SENDING_QUERY    │  │
│ (miio.info)      │──┘
└────────┬──────────┘
         │
         │ (query sent)
         ▼
┌──────────────────┐
│ WAITING_RESPONSE │
│ (encrypted)      │
└────────┬──────────┘
         │
         │ (response received)
         ▼
    ┌─────────┐
    │ COMPLETE│
    └─────────┘

    ┌─────────┐
    │ ERROR   │ (on error)
    └─────────┘

    ┌─────────┐
    │ TIMEOUT │ (on timeout)
    └─────────┘
```

### States

| State | Description |
|-------|-------------|
| `IDLE` | No query active |
| `WAITING_HELLO` | Waiting for hello response from device |
| `SENDING_QUERY` | Sending encrypted `miIO.info` query |
| `WAITING_RESPONSE` | Waiting for encrypted response |
| `COMPLETE` | Query finished successfully |
| `ERROR` | Query failed |
| `TIMEOUT` | Query timed out |

### State Transitions

**IDLE → WAITING_HELLO**
- Trigger: `SmartMiFanAsync_startQueryDevice()` called
- Action: Send hello packet to target IP

**WAITING_HELLO → SENDING_QUERY**
- Trigger: Hello response received (device ID, timestamp)
- Action: Prepare encrypted query, send to device

**SENDING_QUERY → WAITING_RESPONSE**
- Trigger: Query sent
- Action: Wait for encrypted response

**WAITING_RESPONSE → COMPLETE**
- Trigger: Encrypted response received and decrypted successfully
- Action: Parse JSON, add device to discovered fans array

**Any State → ERROR**
- Trigger: Fatal error during query (decrypt failure, invalid response)
- Action: Query aborted, error state set

**Any State → TIMEOUT**
- Trigger: Query timeout exceeded (2 seconds per phase)
- Action: Query aborted, timeout state set

### State Machine Update

```cpp
void loop() {
  if (SmartMiFanAsync_isQueryInProgress()) {
    SmartMiFanAsync_updateQueryDevice();  // Advance state machine
  }
}
```

**Update Function Behavior:**
- `WAITING_HELLO`: Resends hello every 500ms, checks for response
- `SENDING_QUERY`: Sends query, transitions to `WAITING_RESPONSE`
- `WAITING_RESPONSE`: Checks for response, decrypts and parses
- Returns `true` if still in progress, `false` if complete/error/timeout

### Timeout Handling

**Hello Phase:**
- Timeout: 2 seconds
- Resends hello every 500ms
- After timeout, moves to `TIMEOUT` state

**Query Phase:**
- Timeout: 2 seconds
- After timeout, moves to `TIMEOUT` state

---

## Smart Connect State Machine

### State Diagram

```
        ┌─────┐
        │ IDLE│
        └──┬──┘
           │ startSmartConnect()
           ▼
┌──────────────────────┐
│ VALIDATING_FAST_CONNECT│
│ (handshake all)      │
└──────────┬───────────┘
           │
           │ (validation complete)
           ▼
    ┌──────────────┐
    │ All succeeded?│
    └──┬────────┬──┘
       │        │
    Yes│        │No
       │        │
       ▼        ▼
┌──────────┐ ┌──────────────────┐
│ COMPLETE │ │ STARTING_DISCOVERY│
└──────────┘ └────────┬───────────┘
                     │
                     │ (discovery started)
                     ▼
              ┌──────────────┐
              │ DISCOVERING  │
              │ (async)      │
              └──────┬───────┘
                     │
                     │ (discovery complete)
                     ▼
                ┌─────────┐
                │ COMPLETE│
                └─────────┘

                ┌─────────┐
                │ ERROR   │ (on error)
                └─────────┘
```

### States

| State | Description |
|-------|-------------|
| `IDLE` | No Smart Connect active |
| `VALIDATING_FAST_CONNECT` | Validating Fast Connect configured fans |
| `STARTING_DISCOVERY` | Starting discovery for failed fans |
| `DISCOVERING` | Async discovery in progress |
| `COMPLETE` | Smart Connect finished successfully |
| `ERROR` | Smart Connect failed |

### State Transitions

**IDLE → VALIDATING_FAST_CONNECT**
- Trigger: `SmartMiFanAsync_startSmartConnect()` called
- Action: Register Fast Connect fans, validate all fans

**VALIDATING_FAST_CONNECT → COMPLETE**
- Trigger: All Fast Connect fans validated successfully
- Action: Smart Connect complete, all fans available

**VALIDATING_FAST_CONNECT → STARTING_DISCOVERY**
- Trigger: Some Fast Connect fans failed validation
- Action: Collect failed tokens, remove failed entries, start discovery

**STARTING_DISCOVERY → DISCOVERING**
- Trigger: Discovery started successfully
- Action: Discovery state machine active

**DISCOVERING → COMPLETE**
- Trigger: Discovery finished successfully
- Action: Smart Connect complete, all fans available

**Any State → ERROR**
- Trigger: Fatal error during Smart Connect
- Action: Smart Connect aborted, error state set

### State Machine Update

```cpp
void loop() {
  if (SmartMiFanAsync_isSmartConnectInProgress()) {
    SmartMiFanAsync_updateSmartConnect();  // Advance state machine
  }
}
```

**Update Function Behavior:**
- `VALIDATING_FAST_CONNECT`: Validation is synchronous, transitions quickly
- `STARTING_DISCOVERY`: Starts discovery, transitions to `DISCOVERING`
- `DISCOVERING`: Updates discovery state machine, waits for completion
- Returns `true` if still in progress, `false` if complete/error

### Fast Connect Validation

**Process:**
1. Register all Fast Connect fans (add to discovered fans array)
2. Validate each fan (perform handshake)
3. Collect failed fans (wrong IP, wrong token, network issues)
4. Call validation callback with results (if set)
5. If failed fans exist, start discovery for those fans

**Failed Fan Handling:**
- Remove failed Fast Connect entries from discovered list
- Start Async Discovery with tokens from failed fans
- Discovery will find correct IP addresses for those tokens

---

## State Machine Best Practices

### 1. Always Update State Machines

State machines must be updated in the main loop:

```cpp
void loop() {
  // Update discovery
  if (SmartMiFanAsync_isDiscoveryInProgress()) {
    SmartMiFanAsync_updateDiscovery();
  }
  
  // Update query
  if (SmartMiFanAsync_isQueryInProgress()) {
    SmartMiFanAsync_updateQueryDevice();
  }
  
  // Update Smart Connect
  if (SmartMiFanAsync_isSmartConnectInProgress()) {
    SmartMiFanAsync_updateSmartConnect();
  }
}
```

### 2. Check Completion Before Using Results

Always check if an operation is complete before using results:

```cpp
if (SmartMiFanAsync_isDiscoveryComplete()) {
  // Safe to use discovered fans
  size_t count = 0;
  const SmartMiFanDiscoveredDevice *fans = SmartMiFanAsync_getDiscoveredFans(count);
}
```

### 3. Handle Errors and Timeouts

Check for errors and timeouts:

```cpp
DiscoveryState state = SmartMiFanAsync_getDiscoveryState();
if (state == DiscoveryState::ERROR) {
  Serial.println("Discovery failed");
} else if (state == DiscoveryState::TIMEOUT) {
  Serial.println("Discovery timed out");
}
```

### 4. Don't Start Multiple Operations

Only one operation of each type can run at a time:

```cpp
// ❌ Wrong: Starting discovery while another is in progress
if (SmartMiFanAsync_startDiscovery(...)) {
  // OK
} else {
  // Already in progress
}

// ✅ Correct: Wait for completion before starting new operation
if (!SmartMiFanAsync_isDiscoveryInProgress()) {
  SmartMiFanAsync_startDiscovery(...);
}
```

### 5. Cancel Operations When Needed

Cancel operations if needed:

```cpp
// Cancel discovery
if (shouldCancel) {
  SmartMiFanAsync_cancelDiscovery();
}

// Cancel query
if (shouldCancel) {
  SmartMiFanAsync_cancelQuery();
}

// Cancel Smart Connect
if (shouldCancel) {
  SmartMiFanAsync_cancelSmartConnect();
}
```

---

## Related Documentation

- [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) - System architecture
- [03_FUNCTIONS.md](./03_FUNCTIONS.md) - Core functions and workflows
- [06_APIS.md](./06_APIS.md) - API reference
- [09_EXAMPLES.md](./09_EXAMPLES.md) - Example sketches

