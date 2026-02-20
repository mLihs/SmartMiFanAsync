# SmartMiFanAsync Open Topics

This document tracks TODOs, known issues, planned features, and open questions for the SmartMiFanAsync library.

---

## Known Issues

### ⚠️ Speed Control Parameters for Some Models

Some fan models may need verification for speed control parameters:

- **dmaker.fan.p5** - Mi Smart Standing Fan 1X
- **dmaker.fan.p8** - Mi Smart Standing Fan 1C CN
- **dmaker.fan.p30** - Xiaomi Smart Standing Fan 2
- **dmaker.fan.p220** - Mijia DC Inverter Circulating Floor Fan

**Status**: These models use default/fallback values that may need adjustment based on actual device testing.

**Workaround**: Test with actual devices and adjust `getSpeedParams()` function if needed.

---

### ⚠️ Control Operations Are Synchronous

Control operations (`setPower`, `setSpeed`) are synchronous and may block for ~100-1500ms per fan.

**Status**: This is by design for simplicity, but could be made asynchronous in the future.

**Workaround**: Use orchestrated functions with rate limiting to prevent overwhelming the network.

---

### ⚠️ Maximum Fan Limits

- Maximum 16 discovered fans (hardcoded constant `kMaxSmartMiFans`)
- Maximum 4 Fast Connect fans (hardcoded constant `kMaxFastConnectFans`)

**Status**: These are hardcoded limits. Could be made configurable in the future.

**Workaround**: If you need more fans, you can modify the constants in the source code.

---

## Planned Features

### 📋 Asynchronous Control Operations

Make control operations (`setPower`, `setSpeed`) asynchronous to match the discovery/query pattern.

**Priority**: Medium

**Benefits**:
- Non-blocking control operations
- Better integration with web servers and other async tasks
- More consistent API design

**Challenges**:
- Requires state machine for each control operation
- More complex error handling
- Breaking change to existing API

---

### 📋 Fan Status Reading

Add functions to read fan status (current power state, speed, mode, etc.).

**Priority**: Medium

**Benefits**:
- Query current fan state
- Verify commands were applied
- Monitor fan status

**Challenges**:
- Requires parsing device-specific responses
- Different models may have different status formats
- Need to handle encrypted responses

---

### 📋 More Fan Models

Add support for additional fan models as they become available.

**Priority**: Low

**Current Models**: 17 supported models

**Process**:
1. Test with actual device
2. Determine speed control parameters (siid/piid)
3. Add to `kSupportedModels` array
4. Add to `getSpeedParams()` function
5. Test and verify

---

### 📋 Configurable Limits

Make maximum fan limits configurable (currently hardcoded).

**Priority**: Low

**Benefits**:
- More flexible for different use cases
- Can reduce memory usage for small deployments
- Can increase limits for large deployments

**Challenges**:
- Requires compile-time configuration or dynamic allocation
- May break existing code that assumes fixed limits

---

### 📋 Better Error Recovery

Improve error recovery and retry logic.

**Priority**: Low

**Current Behavior**:
- Errors are reported via callback
- No automatic retry
- Handshake cache is invalidated on error

**Potential Improvements**:
- Automatic retry with exponential backoff
- Configurable retry count
- Separate retry logic for different error types

---

### 📋 Device Property Reading

Add functions to read device properties (not just set them).

**Priority**: Low

**Benefits**:
- Query current fan settings
- Verify device state
- Monitor device health

**Challenges**:
- Requires parsing device-specific responses
- Different models may have different property formats
- Need to handle encrypted responses

---

## Open Questions

### ❓ Speed Control for Unknown Models

**Question**: How should the library handle speed control for unknown/unsupported models?

**Current Behavior**: Uses default fallback values (siid=6, piid=8)

**Options**:
1. Return error and require model-specific configuration
2. Use default values and log warning
3. Auto-detect speed control parameters from device info

**Status**: Currently uses option 2 (default values with warning in documentation)

---

### ❓ Token Management

**Question**: Should the library provide token management or discovery?

**Current Behavior**: Tokens must be provided by user (obtained separately)

**Options**:
1. Keep as-is (tokens provided by user)
2. Add token discovery/management features
3. Support token storage/retrieval from NVS

**Status**: Currently option 1 (tokens provided by user)

**Note**: Token discovery would require additional protocol support and may not be feasible for all devices.

---

### ❓ Command Coalescing Strategy

**Question**: Should command coalescing be configurable or more sophisticated?

**Current Behavior**: Fixed rate limit (max 1 command per second) for orchestrated functions

**Options**:
1. Keep as-is (fixed rate limit)
2. Make rate limit configurable
3. Add more sophisticated coalescing (e.g., batch commands, priority queue)

**Status**: Currently option 1 (fixed rate limit)

---

### ❓ Error Callback Behavior

**Question**: Should error callbacks be able to affect control flow?

**Current Behavior**: Error callbacks are observational only (do not affect control flow)

**Options**:
1. Keep as-is (observational only)
2. Allow callbacks to trigger retries
3. Allow callbacks to modify fan participation state

**Status**: Currently option 1 (observational only)

**Rationale**: Keeps error handling simple and predictable. User code can implement custom retry logic if needed.

---

### ❓ System State Management

**Question**: Should the library manage system state (ACTIVE/IDLE/SLEEP) or leave it to project code?

**Current Behavior**: Library provides hooks but does not manage system state

**Options**:
1. Keep as-is (project manages system state)
2. Library manages system state internally
3. Hybrid approach (library tracks state, project controls transitions)

**Status**: Currently option 1 (project manages system state)

**Rationale**: System state is project-specific and depends on other components (BLE sensors, web server, etc.). Library should not make assumptions about when to enter/exit sleep.

---

## Testing & Verification

### 📋 Test Coverage

**Current Status**: Examples exist but no formal test suite

**Needed**:
- Unit tests for core functions
- Integration tests for discovery/query workflows
- Hardware tests with actual devices
- Stress tests (multiple fans, network issues, etc.)

---

### 📋 Device Compatibility Matrix

**Current Status**: Partial (17 models listed, some need verification)

**Needed**:
- Complete compatibility matrix with actual device testing
- Document speed control parameters for each model
- Document any model-specific quirks or limitations

---

## Documentation

### 📋 API Documentation

**Status**: ✅ Complete (see [06_APIS.md](./06_APIS.md))

---

### 📋 Architecture Documentation

**Status**: ✅ Complete (see [02_ARCHITECTURE.md](./02_ARCHITECTURE.md))

---

### 📋 Examples Documentation

**Status**: ✅ Complete (see [09_EXAMPLES.md](./09_EXAMPLES.md))

---

### 📋 Troubleshooting Guide

**Status**: ⚠️ Partial (basic troubleshooting in README.md)

**Needed**:
- Common issues and solutions
- Debug logging guide
- Network troubleshooting
- Token extraction guide

---

## Performance

### 📋 Memory Usage

**Current Status**: 
- Discovery context: ~1KB
- Query context: ~1KB
- Discovered fans array: ~1KB (16 fans max)
- Total: ~3KB RAM

**Optimization Opportunities**:
- Reduce buffer sizes if not needed
- Use smaller data types where possible
- Consider dynamic allocation for large arrays (if acceptable)

---

### 📋 Network Performance

**Current Status**:
- Discovery: ~3-5 seconds for typical network
- Query: ~1-2 seconds per device
- Control operations: <100ms per device (after handshake)

**Optimization Opportunities**:
- Parallel query processing (currently sequential)
- Faster timeout detection
- Better network error recovery

---

## Security

### 📋 Token Security

**Current Status**: Tokens are stored in plain text (as hex strings)

**Considerations**:
- Tokens are device-specific and not sensitive (device authentication only)
- No encryption of stored tokens (would require additional key management)
- Tokens are not transmitted over network (only used for local encryption)

**Future Considerations**:
- Optional token encryption at rest
- Token storage in secure NVS partition

---

## Related Documentation

- [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md) - Project overview
- [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) - System architecture
- [06_APIS.md](./06_APIS.md) - API reference
- [09_EXAMPLES.md](./09_EXAMPLES.md) - Example sketches

---

## Contributing

If you want to contribute to addressing these open topics:

1. **Known Issues**: Test with actual devices and report findings
2. **Planned Features**: Discuss implementation approach before starting
3. **Open Questions**: Provide feedback based on your use case
4. **Testing**: Contribute test cases and device compatibility data

---

**Last Updated**: 2025-01-11

