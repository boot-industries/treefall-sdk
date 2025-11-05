# @orpheus/engine-native

Direct N-API bindings for Orpheus SDK - Native Node.js ↔ C++ integration.

## Overview

The Native Driver provides zero-copy, low-latency access to Orpheus SDK features through N-API bindings. This is the highest-performance driver option, suitable for:

- Electron applications requiring direct SDK access
- Node.js services with performance-critical workloads
- Development and testing environments

## Architecture

```
Node.js Application
       ↓
  N-API Bindings (this package)
       ↓
  Orpheus C++ SDK
```

## Status

**Phase 1 (P1.DRIV.005) - Package Structure** ✅

- [x] Package configuration (package.json, tsconfig.json)
- [x] CMake build system integration
- [x] N-API binding stubs (Session wrapper)
- [x] TypeScript definitions

**Phase 1 (P1.DRIV.006) - Command Execution** (Pending)

- [ ] Session loading from JSON
- [ ] Click rendering
- [ ] Command execution pipeline

**Phase 1 (P1.DRIV.007) - Event Callbacks** (Pending)

- [ ] Event emission system
- [ ] N-API callbacks
- [ ] Event type definitions

## Installation

```bash
# Install dependencies
pnpm install

# Build native addon
pnpm run build:native

# Build TypeScript
pnpm run build:ts

# Or build both
pnpm run build
```

## Prerequisites

- **Orpheus SDK**: Must be built first (`cmake --build build-release`)
- **Node.js**: 20.x or later
- **CMake**: 3.22 or later
- **C++ Compiler**: Clang, GCC, or MSVC with C++20 support

## Usage

```typescript
import { Session } from '@orpheus/engine-native';

const session = new Session();

// Load session (stub for P1.DRIV.005)
const result = await session.loadSession({
  sessionPath: './session.json',
});

console.log(result);
// { success: true, message: "Session loading not yet implemented..." }
```

## Development

### Build Commands

```bash
# Clean build artifacts
pnpm run clean

# Rebuild from scratch
pnpm run rebuild

# Build native addon only
pnpm run build:native

# Build TypeScript only
pnpm run build:ts
```

### Debugging

The native addon links against Orpheus SDK libraries. Ensure the correct library paths are set:

**macOS:**
```bash
export DYLD_LIBRARY_PATH=/path/to/orpheus-sdk/build-release/src:$DYLD_LIBRARY_PATH
```

**Linux:**
```bash
export LD_LIBRARY_PATH=/path/to/orpheus-sdk/build-release/src:$LD_LIBRARY_PATH
```

## Implementation Plan

### P1.DRIV.005 (Current) - Create Native Driver Package

**Objective:** Set up package structure and build system

**Completed:**
- ✅ package.json with N-API dependencies
- ✅ tsconfig.json for TypeScript compilation
- ✅ CMakeLists.txt for native addon compilation
- ✅ Binding stub files (binding.cpp, session_wrapper.cpp)
- ✅ TypeScript definitions and exports

### P1.DRIV.006 (Next) - Implement Native Driver Command Execution

**Objective:** Add command execution (LoadSession, RenderClick)

**Tasks:**
1. Integrate session_json::LoadSessionFromFile
2. Implement SessionGraph initialization
3. Add track/clip management
4. Implement click rendering
5. Add error handling

### P1.DRIV.007 (Future) - Implement Native Driver Event Callbacks

**Objective:** Add event emission system

**Tasks:**
1. Create event callback infrastructure
2. Wire up SessionChanged, Heartbeat events
3. Add N-API ThreadSafeFunction for async events
4. Document event callback patterns

## License

MIT

## Related Packages

- **@orpheus/engine-service**: HTTP/WebSocket service driver
- **@orpheus/client**: High-level client with driver broker
- **@orpheus/contract**: Shared command/event schemas
