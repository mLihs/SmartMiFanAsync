// =============================================================================
// SmartMiFanAsync Library - Unity Build (Phase 5: File Split)
// =============================================================================
// This is a Unity Build file that includes all implementation modules.
// Advantages:
//   - Arduino IDE compatible (single translation unit)
//   - Fast compilation
//   - Logical separation for maintainability
//   - No linking issues with Arduino build system
// =============================================================================

// Include the public header first
#include "SmartMiFanAsync.h"

// Include all implementation modules (.inl files are not compiled separately by Arduino)
// Order matters: Core must be first (defines globals and utilities)
#include "internal/SmartMiFanCore.inl"
#include "internal/SmartMiFanClient.inl"
#include "internal/SmartMiFanDiscovery.inl"
#include "internal/SmartMiFanConnect.inl"
#include "internal/SmartMiFanOrchestration.inl"
