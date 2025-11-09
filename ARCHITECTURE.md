<!-- SPDX-License-Identifier: MIT -->

# Architecture Overview

**Document Version:** 2.0
**Last Updated:** October 26, 2025
**Status:** Authoritative

Orpheus is a professional audio SDK built around a deterministic, host-neutral C++20 core library with optional adapter layers for different integration scenarios. The architecture prioritizes offline-first operation, sample-accurate determinism, and broadcast-safe real-time processing.

---

## Table of Contents

- [Design Principles](#design-principles)
- [System Architecture](#system-architecture)
- [Core Library](#core-library-src--include)
- [Driver Architecture](#driver-architecture-packages)
- [Transport Controller](#transport-controller)
- [Contract System](#contract-system)
- [Applications](#applications-apps)
- [Adapters](#adapters-adapters)
- [Testing](#testing-tests--tools)
- [Threading Model](#threading-model)
- [Tooling](#tooling)
- [Future Architecture](#future-architecture)
- [Related Documentation](#related-documentation)

---

## Design Principles

The Orpheus SDK is built on four non-negotiable principles:

1. **Offline-first** – No runtime network calls for core features. All essential functionality works without internet connectivity.

2. **Deterministic** – Same input produces bit-identical output across platforms and runs. Sample-accurate timing, reproducible renders.

3. **Host-neutral** – Core SDK works across REAPER, standalone apps, plugins, embedded systems without host-specific dependencies.

4. **Broadcast-safe** – 24/7 reliability with no allocations in audio threads, no blocking operations in real-time paths.

These principles guide all architectural decisions and feature implementations.

---

## System Architecture

Orpheus is organized into distinct layers, each with clear responsibilities:

```mermaid
graph TD
    A[Applications: OCC, Wave Finder, FX Engine] --> B[Adapters: REAPER, minhost]
    A --> C[Driver Layer: Service/Native/WASM]
    B --> D[Core SDK: Transport, Session, Audio I/O]
    C --> D
    D --> E[Platform: CoreAudio, WASAPI, ALSA, libsndfile]

    style A fill:#4a9eff
    style B fill:#ffa94a
    style C fill:#9eff4a
    style D fill:#ff4a9e
    style E fill:#4aff9e
```

**Layer Hierarchy:**

- **Core SDK** (`src/`, `include/`) – Minimal, deterministic primitives. No UI, no network, no host assumptions.
- **Adapters** (`adapters/`) – Optional, platform-specific integrations. Thin wrappers around core SDK.
- **Driver Layer** (`packages/`) – TypeScript/JavaScript bindings for web and Node.js environments.
- **Applications** (`apps/`) – Complete solutions that compose adapters and drivers (OCC, Wave Finder, etc.).
- **UI Prototypes** (`packages/shmui/`) – Demos only, not production. Local-mocked, no SaaS dependencies.

---

## Core Library (`src/` + `include/`)

The core library provides deterministic audio session and transport management.

### Key Components

#### 1. Session Graph (`orpheus::core::SessionGraph`)

In-memory representation of an audio project:

- **Tracks** – Ordered containers for musical events (Audio or MIDI)
- **Clips** – Named spans with trim points, fade settings, gain, loop state
- **Tempo** – BPM and time signature information
- **Metadata** – Session name, creation date, versioning

**Thread Safety:** Read-only from audio thread, mutable from UI thread with atomic updates.

**File:** `include/orpheus/session_graph.h`, `src/core/session/session_graph.cpp`

#### 2. Transport Controller (`orpheus::ITransportController`)

Real-time audio playback engine:

- **Clip Playback** – Start, stop, pause clips with sample-accurate timing
- **Fade Processing** – Configurable fade IN/OUT with curve types (Linear, EqualPower, Exponential)
- **Loop Mode** – Sample-accurate restart at trim IN point
- **Gain Control** – Per-clip gain adjustment in decibels (-96dB to +12dB)
- **Metadata Persistence** – Trim/fade/gain/loop settings survive session reload

**Audio Thread Contract:**

- No allocations (pre-allocated lock-free structures)
- No locks (atomic operations only)
- No I/O (file reads handled during load, not playback)
- No blocking operations

**File:** `include/orpheus/transport_controller.h`, `src/core/transport/transport_controller.cpp`

#### 3. Audio File Reader (`orpheus::IAudioFileReader`)

Decodes audio files for playback:

- **Supported Formats** – WAV, AIFF, FLAC via libsndfile
- **Memory Management** – Pre-loaded into memory for real-time access
- **Sample Rate** – Currently requires 48kHz (no resampling yet)

**File:** `include/orpheus/audio_file_reader.h`, `src/core/audio_io/audio_file_reader.cpp`

#### 4. Audio Driver Interface (`orpheus::IAudioDriver`)

Platform-agnostic audio I/O abstraction:

- **CoreAudio** (macOS) – Native low-latency driver
- **WASAPI** (Windows) – Planned for v1.0
- **ALSA** (Linux) – Planned for v1.0
- **Dummy Driver** – For testing without hardware

**File:** `include/orpheus/audio_driver.h`, `src/core/audio_io/drivers/`

#### 5. Session JSON (`orpheus::core::session_json`)

Session serialization utilities:

- **Load/Save** – Human-readable JSON format for version control
- **Validation** – Schema-based validation (see Contract System)
- **Filesystem Helpers** – Path resolution, filename generation

**File:** `include/orpheus/session_json.h`, `src/core/session/session_json.cpp`

#### 6. ABI Negotiation (`orpheus::AbiVersion`)

Version compatibility helpers:

- **Version Descriptors** – Major/minor/patch semantic versioning
- **Compatibility Checks** – Graceful degradation on version mismatch
- **Future-Proofing** – Enables plugin-host compatibility across releases

**File:** `include/orpheus/abi_version.h`

### Build Artifacts

- **`orpheus_core`** – Static library with session/transport primitives
- **`orpheus_transport`** – Real-time transport controller module
- **`orpheus_audio_io`** – Audio file reader and platform drivers

---

## Driver Architecture (`packages/`)

The driver layer provides JavaScript/TypeScript bindings for web and Node.js environments. Implemented in ORP068 Phases 1-2.

### Driver Types

#### 1. Service Driver (`@orpheus/engine-service`)

**Architecture:** Node.js HTTP server + WebSocket event streaming

- **Process Model** – Spawns `orpheus_minhost` as child process
- **Communication** – HTTP POST for commands, WebSocket for events
- **Security** – Bearer token authentication, 127.0.0.1 bind
- **Use Cases** – Web apps, Electron apps, remote control scenarios

**Endpoints:**

- `GET /health` – Health check (public)
- `GET /version` – SDK version info (public)
- `GET /contract` – Available commands/events (authenticated)
- `POST /command` – Execute SDK command (authenticated)
- `WS /ws` – WebSocket event stream (authenticated)

**File:** `packages/engine-service/src/server.ts`

#### 2. Native Driver (`@orpheus/engine-native`)

**Architecture:** N-API native addon with direct C++ SDK integration

- **Process Model** – In-process, no IPC overhead
- **Communication** – Direct function calls via N-API bindings
- **Performance** – Lowest latency, highest throughput
- **Use Cases** – Desktop apps (Electron), server-side rendering

**Bindings:**

- `loadSession()` – Load session from JSON file
- `renderClick()` – Render click track to WAV
- `getTempo()`, `setTempo()` – Session tempo access
- `getSessionInfo()` – Query session metadata
- `subscribe()` – Event callback registration

**File:** `packages/engine-native/src/bindings/session_wrapper.cpp`

#### 3. WASM Driver (`@orthpeus/engine-wasm`)

**Architecture:** Emscripten-compiled WASM module in Web Worker

- **Process Model** – Main thread client + worker thread engine
- **Communication** – Structured cloning via `postMessage()`
- **Security** – Subresource Integrity (SRI) verification
- **Use Cases** – Browser-based DAWs, offline-capable web apps

**Status:** Infrastructure complete (ORP068 Phase 2), requires Emscripten SDK for compilation.

**File:** `packages/engine-wasm/src/worker.ts`, `packages/engine-wasm/src/wasm_bindings.cpp`

### Client Broker (`@orpheus/client`)

Unified interface across all driver types:

- **Driver Selection** – Automatic priority-based selection (Native → WASM → Service)
- **Handshake Protocol** – Capability verification, version negotiation
- **Health Monitoring** – Automatic reconnection, heartbeat checks
- **Event Forwarding** – Unified event interface across drivers

**File:** `packages/client/src/client.ts`

---

## Contract System

The **Orpheus Contract** defines the JSON schema for all commands and events between drivers and the SDK.

### Version History

- **v0.1.0-alpha** – Initial minimal contract (LoadSession, RenderClick)
- **v1.0.0-beta** – Expanded contract with real-time features (SaveSession, TriggerClipGridScene, SetTransport, TransportTick, RenderProgress)

### Command Schema Example

```json
{
  "command": "LoadSession",
  "params": {
    "sessionPath": "/path/to/session.json"
  }
}
```

### Event Schema Example

```json
{
  "event": "SessionChanged",
  "data": {
    "trackCount": 4,
    "clipCount": 12,
    "tempo": 120.0
  }
}
```

### Event Frequency Validation

To prevent client overload, the contract enforces maximum event frequencies:

- **TransportTick** – ≤30 Hz (real-time transport position)
- **RenderProgress** – ≤10 Hz (offline render progress)
- **Heartbeat** – 1 Hz (liveness check)

**File:** `packages/contract/src/frequency-validator.ts`

### Documentation

- **Contract Development Guide** – `docs/CONTRACT_DEVELOPMENT.md`
- **Migration Guide** – `packages/contract/MIGRATION.md`
- **Schemas** – `packages/contract/schemas/v1.0.0-beta/`

---

## Transport Controller

The transport controller is the heart of real-time audio processing in Orpheus.

### Architecture

```
UI Thread                   Audio Thread
┌──────────────┐           ┌──────────────┐
│ startClip()  │──Atomic──>│ processAudio()│
│ stopClip()   │           │   - Read trim │
│ updateGain() │           │   - Apply gain│
│ setLoopMode()│           │   - Apply fade│
│              │           │   - Check loop│
└──────────────┘           └──────────────┘
     ▲                             │
     │                             │
     └─────────Callback────────────┘
           (onClipFinished, onClipLooped)
```

### Data Structures

#### ActiveClip (Audio Thread, Lock-Free)

```cpp
struct ActiveClip {
    std::atomic<uint64_t> currentFrame;
    std::atomic<float> gainLinear;
    std::atomic<bool> loopEnabled;
    std::shared_ptr<IAudioFileReader> reader; // Immutable after creation
    // ... fade state, trim points
};
```

#### AudioFileEntry (Persistent Storage)

```cpp
struct AudioFileEntry {
    std::string filePath;
    uint64_t trimInSamples;
    uint64_t trimOutSamples;
    float fadeInSeconds;
    float fadeOutSeconds;
    FadeCurve fadeInCurve;
    FadeCurve fadeOutCurve;
    float gainDb;
    bool loopEnabled;
};
```

### Threading Model

See [Threading Model](#threading-model) section below for full details.

---

## Applications (`apps/`)

The `apps/` directory contains complete applications built on the Orpheus SDK.

### Orpheus Clip Composer (OCC)

**Status:** ✅ v0.2.0-alpha (Sprint 1-2 Complete)

Professional soundboard application for broadcast, theater, and live performance.

**Architecture:**

- **Framework** – JUCE 8.0.4 for cross-platform UI
- **Audio Engine** – Direct integration with `ITransportController`
- **Platform Driver** – CoreAudio (macOS native)
- **Session Format** – JSON with metadata persistence

**Key Features:**

- 48-button clip grid with 8 tabs (384 total clips)
- Loop playback with sample-accurate restart
- Fade IN/OUT processing (3 curve types)
- Audio Settings Dialog (device, sample rate, buffer)
- Edit Dialog with waveform display and preview
- Broadcast-safe audio thread

**Performance:** <15% CPU with 8 clips, ~100MB memory, 10.7ms latency

**Documentation:** `apps/clip-composer/docs/OCC/`, `apps/clip-composer/CHANGELOG.md`

### Orpheus Wave Finder (Planned)

Harmonic calculator and frequency scope for audio analysis.

**Status:** Planned for v1.0 (Months 7-12)

### Orpheus FX Engine (Planned)

LLM-powered effects processing and creative workflows.

**Status:** Planned for v1.0 (Months 10-12)

---

## Adapters (`adapters/`)

Adapters are thin CMake targets that integrate the core SDK into specific host environments.

### Minhost (`orpheus_minhost`)

Command-line interface for SDK operations:

**Features:**

- Load session JSON
- Render click tracks to WAV
- Offline render with deterministic output
- Transport simulation (beat ticks to stdout)

**Example Usage:**

```sh
./build/adapters/minhost/orpheus_minhost \
  --session tools/fixtures/solo_click.json \
  --render click.wav \
  --bars 2 \
  --bpm 100
```

**File:** `adapters/minhost/main.cpp`

### REAPER Adapter (`reaper_orpheus`) - Quarantined

**Status:** Quarantined pending SDK stabilization

The REAPER adapter builds a shared library that exports extension metadata for REAPER DAW integration. Currently isolated from active builds to simplify SDK development.

**File:** `adapters/reaper/` (quarantined)

---

## Testing (`tests/` + `tools/`)

Comprehensive test suite ensures correctness across platforms.

### Test Categories

#### 1. Unit Tests (GoogleTest)

- **ABI Smoke** (`tests/abi_smoke.cpp`) – Version negotiation, compatibility
- **Session Round-Trip** (`tests/session_roundtrip.cpp`) – JSON serialization
- **Transport Tests** (`tests/transport/`) – Clip playback, gain, loop, metadata
- **Routing Tests** (`tests/routing/`) – Gain smoother, routing matrix
- **Audio I/O Tests** (`tests/audio_io/`) – Driver integration, file reading

**Total:** 51+ tests (as of October 2025)

#### 2. Integration Tests

- **Multi-Clip Stress Test** (`tests/transport/multi_clip_stress_test.cpp`) – 16 simultaneous clips
- **Offline Render Test** (`tests/determinism/offline_render_test.cpp`) – Cross-platform determinism

#### 3. Conformance Tools

- **JSON Round-Trip** (`tools/conformance/json_roundtrip.cpp`) – Full-file comparison
- **Session Inspector** (`tools/cli/inspect_session`) – Human-readable session summary

### Test Infrastructure

- **AddressSanitizer** – Detects memory leaks, use-after-free, buffer overflows
- **UBSanitizer** – Detects undefined behavior (signed overflow, null deref, etc.)
- **ThreadSanitizer** – Detects race conditions (manual runs, not CI)
- **ctest** – Test orchestration and reporting

### Running Tests

```sh
# Full test suite
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Specific test
ctest --test-dir build -R clip_gain_test -VV

# With sanitizers (Debug builds only)
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
```

---

## Threading Model

Orpheus follows a strict two-thread model for real-time safety.

### UI Thread

**Responsibilities:**

- User interaction (button clicks, keyboard shortcuts)
- File I/O (load audio files, save sessions)
- Metadata updates (trim points, fade settings, gain, loop)
- Driver communication (send commands, receive events)

**Allowed Operations:**

- Memory allocation (heap, stack)
- Blocking I/O (file reads, network calls)
- Lock acquisition (mutexes, condition variables)

**Forbidden Operations:**

- Direct audio buffer manipulation

### Audio Thread

**Responsibilities:**

- Audio callback processing (`processAudio()`)
- Read clip state (trim, gain, fade, loop)
- Apply audio processing (gain, fade)
- Advance playhead, check loop restart
- Post completion callbacks (via lock-free queue)

**Allowed Operations:**

- Read atomic values (clip state)
- Lock-free queue operations
- CPU-bound calculations (fade curves, gain conversion)

**Forbidden Operations (Broadcast-Safe Contract):**

- ❌ Memory allocation (`new`, `malloc`, `std::vector::push_back`)
- ❌ Locks (`std::mutex::lock()`, `std::condition_variable::wait()`)
- ❌ File I/O (`fread()`, `fwrite()`, `fopen()`)
- ❌ Network I/O (HTTP, WebSocket, TCP/UDP)
- ❌ System calls (blocking operations)

### Communication Patterns

**UI → Audio Thread:**

- Atomic writes to clip state (`std::atomic<float> gainLinear`)
- Lock-free command queue (future enhancement)

**Audio → UI Thread:**

- Lock-free callback queue (`onClipFinished`, `onClipLooped`)
- Callbacks invoked from UI thread timer

### Verification

- **rt.safety.auditor skill** – Static analysis for audio thread violations
- **orpheus-audio-safety-checker agent** – Automated real-time safety verification
- **AddressSanitizer** – Runtime detection of allocations in audio thread

---

## Tooling

### CMake Build System

**Superbuild** with fine-grained control:

```sh
# Minimal build (core + minhost)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# With OCC application
cmake -S . -B build -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON

# Disable real-time audio (offline rendering only)
cmake -S . -B build -DORPHEUS_ENABLE_REALTIME=OFF
```

**Build Types:**

- **Debug** – Sanitizers enabled, assertions enabled, debug symbols
- **Release** – Optimizations enabled, assertions disabled, minimal symbols
- **RelWithDebInfo** – Optimizations + debug symbols (for profiling)

### Code Quality

- **clang-format** – Enforces code style (CI gate)
- **clang-tidy** – Static analysis (recommended, not CI gate)
- **Doxygen** – API documentation generation

### CI/CD Pipeline

**Unified Pipeline** (`.github/workflows/ci-pipeline.yml`):

- Matrix builds: 3 OS × 2 build types (ubuntu/windows/macos, Debug/Release)
- 7 parallel jobs: C++ build/test, lint, native driver, TypeScript, integration, dependency check, performance
- Target duration: <25 minutes

**Specialized Workflows:**

- **Chaos Tests** (`.github/workflows/chaos-tests.yml`) – Nightly failure recovery testing
- **Security Audit** (`.github/workflows/security-audit.yml`) – Weekly vulnerability scans
- **Docs Publish** (`.github/workflows/docs-publish.yml`) – Auto-publish Doxygen on release

### Developer Scripts

- `scripts/bootstrap-dev.sh` – One-command setup
- `scripts/validate-sdk.sh` – Run all tests + linters
- `scripts/check-determinism.sh` – Verify bit-identical output
- `scripts/generate-coverage.sh` – Code coverage reports

---

## Future Architecture

### Routing Matrix (ORP070 Phase 2)

Multi-channel audio routing with flexible matrix architecture:

- **Busses** – Master, Cue, Aux sends/returns
- **Insert FX** – Per-track DSP processing
- **Sends/Returns** – Flexible routing graph
- **Gain Smoothing** – Anti-click parameter automation

**Status:** Planned for OCC v0.3.0-alpha

### Performance Monitoring

Real-time diagnostics for production environments:

- **CPU Usage** – Per-thread profiling
- **Latency Tracking** – Buffer underrun detection
- **Memory Profiling** – Heap allocations, fragmentation

**Status:** Planned for v1.0

---

## Related Documentation

### Getting Started

- [Quick Start Guide](docs/QUICK_START.md) – Build SDK in <10 minutes
- [Transport Integration Guide](docs/integration/TRANSPORT_INTEGRATION.md) – Comprehensive `ITransportController` usage
- [Cross-Platform Development Guide](docs/platform/CROSS_PLATFORM.md) – Platform-specific considerations

### Implementation Plans

- [ORP068 - SDK Integration Plan v2.0](<docs/ORP/ORP068%20Implementation%20Plan%20(v2.0).md>) – Driver architecture (Phases 0-4)
- [ORP077 - SDK Core Quality Sprint](docs/ORP/ORP077.md) – Unit tests, API docs, determinism validation

### Application Documentation

- [OCC Product Vision](apps/clip-composer/docs/OCC/OCC021%20Orpheus%20Clip%20Composer%20-%20Product%20Vision.md) – Market positioning
- [OCC CHANGELOG](apps/clip-composer/CHANGELOG.md) – Full release history

### Developer Tools

- [AGENTS.md](AGENTS.md) – Coding assistant guidelines
- [CLAUDE.md](CLAUDE.md) – Claude Code development guide
- [CONTRIBUTING.md](docs/CONTRIBUTING.md) – Contribution guidelines

---

**Document Status:** Authoritative
**Maintained By:** SDK Core Team
**Next Review:** After ORP077 completion
**Last Updated:** October 26, 2025
