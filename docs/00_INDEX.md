# SmartMiFanAsync Documentation Index

**Version**: 1.8.2  
**Platform**: ESP32 (Arduino Core)  
**Author**: Martin Lihs  
**Last Updated**: 2026-02-20

---

## Overview

This documentation provides comprehensive information about the SmartMiFanAsync ESP32 Arduino library, covering architecture, implementation details, APIs, state machines, and development guidelines. The documentation is structured for both human developers and AI agents.

---

## Documentation Structure

### 📋 Quick Start
- **[01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md)** - What is SmartMiFanAsync? Purpose, features, installation, and quick start guide

### 🏗️ Architecture & Design
- **[02_ARCHITECTURE.md](./02_ARCHITECTURE.md)** - System architecture, module boundaries, design principles, protocol details

### 🔧 Implementation Details
- **[03_FUNCTIONS.md](./03_FUNCTIONS.md)** - Core functions and workflows (discovery, query, control, participation states)
- **[04_DEPENDENCIES.md](./04_DEPENDENCIES.md)** - Externe Abhängigkeiten und verwendete Libraries
- **[05_STATE_MACHINES.md](./05_STATE_MACHINES.md)** - All state machines (discovery, query, smart connect)
- **[06_APIS.md](./06_APIS.md)** - Complete API reference with function signatures and examples

### 👨‍💻 Development
- **[09_EXAMPLES.md](./09_EXAMPLES.md)** - Example sketches documentation and usage patterns

### 📝 Status & Future
- **[08_OPEN_TOPICS.md](./08_OPEN_TOPICS.md)** - TODOs, known issues, planned features, open questions

---

## Quick Navigation by Topic

### For New Developers
1. Start with [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md) to understand what SmartMiFanAsync does
2. Read [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) for system design
3. Review [03_FUNCTIONS.md](./03_FUNCTIONS.md) to understand core workflows
4. Check [09_EXAMPLES.md](./09_EXAMPLES.md) for usage examples
5. Explore [06_APIS.md](./06_APIS.md) for detailed API reference

### For AI Agents
- **Architecture queries**: [02_ARCHITECTURE.md](./02_ARCHITECTURE.md)
- **State machine queries**: [05_STATE_MACHINES.md](./05_STATE_MACHINES.md), [03_FUNCTIONS.md](./03_FUNCTIONS.md)
- **API queries**: [06_APIS.md](./06_APIS.md)
- **Implementation status**: [08_OPEN_TOPICS.md](./08_OPEN_TOPICS.md)
- **Example usage**: [09_EXAMPLES.md](./09_EXAMPLES.md)

### For Contributors
- **Adding features**: [08_OPEN_TOPICS.md](./08_OPEN_TOPICS.md) → "Planned Features"
- **Understanding existing code**: [02_ARCHITECTURE.md](./02_ARCHITECTURE.md)
- **API changes**: [06_APIS.md](./06_APIS.md)
- **Current work**: [08_OPEN_TOPICS.md](./08_OPEN_TOPICS.md)

---

## Key Concepts

### Asynchronous (Non-Blocking) Operations
All discovery and query operations are asynchronous and non-blocking. The library uses state machines that must be updated in the main loop via `*_update*()` functions.

See: [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Discovery Workflow", [05_STATE_MACHINES.md](./05_STATE_MACHINES.md) → "Discovery State Machine"

### miIO Protocol
The library uses the miIO protocol over UDP (port 54321) to communicate with SmartMi devices. Communication is encrypted using AES-128-CBC with keys derived from device tokens.

See: [02_ARCHITECTURE.md](./02_ARCHITECTURE.md) → "Protocol Details", [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md) → "How It Works"

### Fan Participation States
Three-state system (ACTIVE, INACTIVE, ERROR) that determines whether a fan receives control commands. Orchestrated functions only send commands to ACTIVE fans.

See: [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Fan Participation States", [06_APIS.md](./06_APIS.md) → "Fan Participation State API"

### Fast Connect Mode
Direct IP connection mode that skips discovery. Useful when you already know fan IP addresses and tokens. Fans are registered immediately with lazy validation.

See: [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Fast Connect Workflow", [06_APIS.md](./06_APIS.md) → "Fast Connect Functions"

### Smart Connect Mode
Combines Fast Connect and Async Discovery. Tries Fast Connect first, then automatically falls back to Discovery for any failed fans.

See: [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Smart Connect Workflow", [06_APIS.md](./06_APIS.md) → "Smart Connect Functions"

### Error and Health Monitoring
Observational error callbacks and health check API for monitoring fan connectivity and diagnosing issues.

See: [06_APIS.md](./06_APIS.md) → "Error and Health Callback API", [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Error Handling"

### Sleep/Wake Integration
Hooks for integrating with system sleep/wake cycles. Allows preparing the library for sleep and waking it up when needed.

See: [06_APIS.md](./06_APIS.md) → "Transport and Sleep Hooks", [03_FUNCTIONS.md](./03_FUNCTIONS.md) → "Sleep/Wake Integration"

---

## Related Documentation

### In Repository
- `README.md` - Quick start and installation (legacy, see [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md))
- `library.properties` - Arduino library metadata
- `src/SmartMiFanAsync.h` - Header file with inline documentation
- `src/SmartMiFanAsync.cpp` - Implementation file
- `examples/` - Example sketches (see [09_EXAMPLES.md](./09_EXAMPLES.md))

### Dependencies
Siehe **[04_DEPENDENCIES.md](./04_DEPENDENCIES.md)** für vollständige Details.

- **ESP32 Arduino Core** - Required (WiFi, UDP support)
- **mbedTLS** - Included with ESP32 (AES-128-CBC encryption, MD5 hashing)
- **WiFiUDP** - Included with ESP32

---

## Documentation Conventions

### Code Examples
- C++ code uses `cpp` language identifier
- Configuration examples show actual values
- Function signatures include parameter types and return values

### Status Indicators
- ✅ **Implemented** - Feature is complete and functional
- 🚧 **In Progress** - Feature is partially implemented
- 📋 **Planned** - Feature is planned but not started
- ⚠️ **Stub/Placeholder** - Interface exists but implementation is missing

### File References
- Relative paths from repository root: `src/SmartMiFanAsync.h`
- Absolute paths for clarity: `/src/SmartMiFanAsync.h` (when needed)

### Fan Model Notation
- **zhimi.fan.za5** - Smartmi Standing Fan 3
- **dmaker.fan.p11** - Mi Smart Standing Fan Pro
- See [01_PROJECT_OVERVIEW.md](./01_PROJECT_OVERVIEW.md) for full list

---

## Feedback & Updates

This documentation is maintained alongside the codebase. If you find errors or gaps, please update the relevant documentation file.

**Last Documentation Review**: 2025-01-11

