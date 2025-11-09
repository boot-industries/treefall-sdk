# Architecture Overview Notes

## Design Philosophy

The Orpheus SDK follows a **strict layered architecture** where each layer has clearly defined responsibilities and dependencies flow in one direction: from applications down to the platform. This design enables:

1. **Host neutrality** - Core SDK works across different environments
2. **Testability** - Each layer can be tested independently
3. **Extensibility** - New adapters and applications without core changes
4. **Maintainability** - Clear boundaries prevent coupling

## Architecture Layers (Bottom to Top)

### Platform Layer

**Responsibility:** OS-level audio I/O and file decoding

This is the lowest layer, providing hardware access and platform-specific functionality. The SDK abstracts these platform differences behind consistent interfaces.

**Current implementations:**

- **CoreAudio** (macOS) - Native low-latency audio driver
- **libsndfile** - Cross-platform WAV/AIFF/FLAC decoding

**Planned implementations:**

- **WASAPI** (Windows) - Standard Windows audio (v1.0)
- **ASIO** (Windows) - Professional low-latency (v1.0)
- **ALSA** (Linux) - Standard Linux audio (v1.0)

**Key characteristic:** Platform-specific code is isolated here and never leaks into higher layers.

### Core SDK Layer (C++20)

**Responsibility:** Deterministic audio processing primitives

This is the heart of the system, implementing all audio logic in a host-neutral, deterministic way. The core SDK has **zero dependencies** on any host environment or network services.

#### Four Core Principles (Non-Negotiable)

1. **Offline-first** - No runtime network calls for core features. Essential functionality works without internet.

2. **Deterministic** - Same input produces bit-identical output across platforms and runs. Sample-accurate timing is guaranteed.

3. **Host-neutral** - Works equally well in REAPER, standalone apps, plugins, embedded systems. No host-specific assumptions.

4. **Broadcast-safe** - 24/7 reliability with no allocations in audio threads, no blocking operations in real-time paths.

#### Core SDK Components

**Session Management:**

- `SessionGraph` - In-memory representation of audio projects (tracks, clips, tempo, metadata)
- `session_json` - Human-readable JSON serialization for version control

**Real-Time Transport:**

- `TransportController` - Sample-accurate clip playback with fade/gain/loop support
- `ActiveClip` - Lock-free audio thread state management
- Audio thread runs without allocations, locks, or I/O

**Audio I/O:**

- `AudioFileReader` - Decode audio files, pre-load into memory for real-time access
- `IAudioDriver` - Platform-agnostic driver interface with multiple backends

**Routing & DSP:**

- `RoutingMatrix` - Professional N×M audio routing with gain smoothing (planned v0.3.0)
- `dsp/` - DSP primitives (oscillators, gain conversion)

**ABI & Contracts:**

- `AbiVersion` - Semantic versioning for cross-version compatibility
- Contract System - JSON schemas for all commands and events

**Threading model:**
The core SDK follows a strict two-thread model:

- **UI Thread** - User interaction, file I/O, metadata updates (allocations allowed)
- **Audio Thread** - Real-time processing only (NO allocations, locks, or I/O)

Communication uses atomic operations and lock-free queues.

### Driver Layer (JavaScript/TypeScript)

**Responsibility:** JavaScript/TypeScript bindings for the C++ SDK

This layer enables web and Node.js applications to use the SDK. Three driver types provide different trade-offs:

**1. Native Driver** (`@orpheus/engine-native`)

- **Architecture:** N-API bindings with direct C++ integration
- **Performance:** Lowest latency, highest throughput (in-process)
- **Use cases:** Desktop apps (Electron), server-side rendering
- **Status:** Production-ready

**2. Service Driver** (`@orpheus/engine-service`)

- **Architecture:** Node.js HTTP server + WebSocket event streaming
- **Performance:** IPC overhead, network latency
- **Use cases:** Web apps, remote control, multi-process architectures
- **Status:** Production-ready
- **Security:** Bearer token authentication, 127.0.0.1 bind only

**3. WASM Driver** (`@orpheus/engine-wasm`)

- **Architecture:** Emscripten-compiled WASM in Web Worker
- **Performance:** Near-native, no syscalls
- **Use cases:** Browser-based DAWs, offline-capable web apps
- **Status:** Infrastructure complete, awaiting Emscripten SDK integration

**Client Broker** (`@orpheus/client`)
Provides a unified interface that automatically selects the best available driver with fallback priority: Native → WASM → Service

**Contract System:**
All drivers use the same JSON schema-based command/event system defined in `@orpheus/contract`. This ensures consistent behavior across drivers and enables automated validation.

### Adapters Layer

**Responsibility:** Thin integration layers (≤300 LOC when possible)

Adapters bridge the core SDK to specific host environments. They should contain **minimal logic** - just enough to translate between the host's API and the SDK's API.

**Current adapters:**

**Minhost** - Command-line interface

- Load sessions, render click tracks, offline rendering
- Transport simulation with beat ticks to stdout
- Primary tool for testing and automation

**REAPER Extension** - DAW integration (quarantined)

- Currently isolated from active builds
- Will be reactivated after SDK stabilization
- Provides track/session integration with REAPER

**Design guideline:**
If an adapter grows beyond ~300 LOC or contains complex logic, consider whether that logic belongs in the core SDK instead. Adapters should be "dumb" translation layers.

### Applications Layer

**Responsibility:** Complete end-user applications

Applications compose adapters and/or integrate the SDK directly to create full-featured programs with their own UI and workflows.

**Current applications:**

**Orpheus Clip Composer** (flagship, v0.2.0-alpha)

- Professional soundboard for broadcast, theater, live performance
- JUCE 8.0.4 framework for cross-platform UI
- Direct integration with `ITransportController`
- 48-button clip grid × 8 tabs = 384 clips
- Loop playback, fade processing, waveform display
- Target market: €500-1,500 price point
- See `apps/clip-composer/docs/OCC/` for comprehensive docs

**Future applications:**

- **Wave Finder** - Harmonic calculator and frequency scope (v1.0)
- **FX Engine** - LLM-powered effects processing (v1.0)

### External Services (Optional)

**Responsibility:** Optional network-based features

External services are **never required** for core functionality. They provide optional enhancements for specific use cases.

**Planned features:**

- **Remote Control** (WebSocket/OSC) - iOS companion app, lighting desk integration
- **Cloud Services** - License validation, optional cloud storage

**Critical constraint:**
The SDK must remain fully functional offline. Network services only enhance existing capabilities or add optional features.

## Key Architectural Patterns

### Dependency Inversion

Higher layers depend on interfaces defined in lower layers, not concrete implementations. This enables:

- Mock drivers for testing
- Platform abstraction (CoreAudio/WASAPI/ALSA behind `IAudioDriver`)
- Multiple adapter types without core changes

### Lock-Free Communication

Audio thread communication uses atomic operations and lock-free queues to avoid priority inversion and ensure real-time safety:

```
UI Thread                   Audio Thread
┌──────────────┐           ┌──────────────┐
│ startClip()  │──Atomic──>│ processAudio()│
│ updateGain() │           │ Read state    │
│              │<─Queue────│ Post events   │
└──────────────┘           └──────────────┘
```

### Contract-Based Communication

All driver communication uses JSON schemas with:

- **Command validation** - Reject invalid commands before SDK execution
- **Event validation** - Ensure events match schema before client delivery
- **Frequency limits** - Prevent client overload (TransportTick ≤30Hz, RenderProgress ≤10Hz)
- **Version negotiation** - Graceful degradation on version mismatch

### Deterministic Design

Sample counts use 64-bit integers (never floating-point seconds) to ensure:

- Bit-identical output across platforms
- No accumulated timing errors over long sessions
- Reproducible renders for automated testing

## Design Principles in Practice

### Example: Adding a New Feature

**Question:** Should this feature be in the core SDK?

**Decision tree:**

1. **Will this work offline?** → If no, wrong layer for core
2. **Is this deterministic?** → If no, not in render path
3. **Is this host-neutral?** → If no, belongs in adapter
4. **For all applications?** → If no, app-specific feature

### Example: Audio Thread Safety

**Forbidden in audio thread:**

- ❌ Memory allocation (`new`, `malloc`, `std::vector::push_back`)
- ❌ Locks (`std::mutex::lock()`, `std::condition_variable::wait()`)
- ❌ File I/O (`fread()`, `fwrite()`, `fopen()`)
- ❌ Network I/O (HTTP, WebSocket, TCP/UDP)
- ❌ System calls (blocking operations)

**Allowed in audio thread:**

- ✅ Read atomic values (clip state)
- ✅ Lock-free queue operations
- ✅ CPU-bound calculations (fade curves, gain conversion)
- ✅ Pre-allocated buffer access

**Verification:**

- `rt.safety.auditor` skill - Static analysis
- AddressSanitizer - Runtime detection
- `orpheus-audio-safety-checker` agent - Automated validation

## Build System Architecture

**CMake Superbuild:**
Fine-grained control over what gets built:

```bash
# Minimal build (core + minhost only)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# With Clip Composer application
cmake -S . -B build -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON

# Disable real-time audio (offline only)
cmake -S . -B build -DORPHEUS_ENABLE_REALTIME=OFF
```

**Build types:**

- **Debug** - Sanitizers, assertions, debug symbols
- **Release** - Optimizations, no assertions, minimal symbols
- **RelWithDebInfo** - Optimizations + debug symbols (for profiling)

## Testing Architecture

**Three test levels:**

1. **Unit Tests** (GoogleTest) - Individual components in isolation
   - Transport tests (clip playback, gain, loop, fade)
   - Session round-trip (JSON serialization)
   - Routing matrix, audio I/O, ABI negotiation

2. **Integration Tests** - Multiple components working together
   - Multi-clip stress test (16 simultaneous clips)
   - Offline render test (cross-platform determinism)
   - Driver handshake tests

3. **Conformance Tools** - Validate contract compliance
   - JSON round-trip tool
   - Session inspector
   - Event frequency validation

**Test infrastructure:**

- AddressSanitizer - Memory errors, use-after-free
- UBSanitizer - Undefined behavior (signed overflow, null deref)
- ThreadSanitizer - Race conditions (manual runs)

## CI/CD Architecture

**Main pipeline** (`.github/workflows/ci-pipeline.yml`):

- Matrix builds: 3 OS × 2 build types = 6 combinations
- 7 parallel jobs: C++ build/test, lint, native driver, TypeScript, integration, deps, perf
- Target duration: <25 minutes
- Sanitizers on Debug builds

**Specialized workflows:**

- **Chaos tests** (nightly) - Failure recovery, resource exhaustion
- **Security audit** (weekly) - Dependency vulnerabilities, code scanning
- **Docs publish** (on release) - Auto-generate and publish Doxygen

## Future Architecture Evolution

### Routing Matrix (v0.3.0-alpha)

Multi-channel routing with:

- Busses (Master, Cue, Aux sends/returns)
- Insert FX (per-track DSP)
- Flexible routing graph
- Anti-click gain smoothing

### Performance Monitoring (v1.0)

Real-time diagnostics:

- Per-thread CPU profiling
- Buffer underrun detection
- Memory usage tracking

### Plugin Hosting (v2.0)

VST3 plugin support:

- Plugin scanning and loading
- Parameter automation
- MIDI routing to plugins

## Related Documentation

**Core references:**

- `ARCHITECTURE.md` - Full architecture document (authoritative)
- `ROADMAP.md` - Development timeline and milestones
- `docs/ORP/ORP068 Implementation Plan (v2.0).md` - Driver architecture
- `docs/ORP/ORP069.md` - OCC-aligned SDK enhancements

**Application documentation:**

- `apps/clip-composer/docs/OCC/OCC021` - Product vision
- `apps/clip-composer/docs/OCC/OCC026` - MVP plan
- `apps/clip-composer/CHANGELOG.md` - Release history

**Developer guides:**

- `docs/integration/TRANSPORT_INTEGRATION.md` - Using the transport controller
- `docs/platform/CROSS_PLATFORM.md` - Platform considerations
- `CLAUDE.md` - Development conventions

## Related Diagrams

- See `component-map.mermaid.md` for detailed component relationships
- See `data-flow.mermaid.md` for thread communication patterns
- See `entry-points.mermaid.md` for all SDK access methods
