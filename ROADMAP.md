<!-- SPDX-License-Identifier: MIT -->

# Roadmap

The Orpheus roadmap captures the near-term milestones for evolving the SDK and
its adapters. Updated October 2025 to include real-time infrastructure for
Orpheus Clip Composer and future applications.

## Milestone M1 – Foundations (COMPLETE)

- ✅ Quarantine legacy, non-Orpheus code and establish a clean repository.
- ✅ Create core ABI/session primitives with a modern CMake superbuild.
- ✅ Provide Hello World adapters (REAPER panel, minhost click renderer).
- ✅ Enable smoke testing, sanitizers, and cross-platform CI.

## Milestone M2 – Real-Time Infrastructure (Months 1-6, 2025)

**Purpose:** Enable real-time audio playback for Orpheus Clip Composer MVP and
future applications requiring sample-accurate, low-latency performance.

**Core Modules:**

- **Transport Controller** (Months 1-2)
  - Sample-accurate clip playback with start/stop control
  - Lock-free audio thread with command queue
  - Transport callbacks (clip started/stopped/looped)
  - Multi-clip playback (16+ simultaneous clips)

- **Audio File Reader** (Months 1-2)
  - Decode WAV/AIFF/FLAC using libsndfile
  - Stream audio data into ring buffers (non-blocking)
  - Sample rate conversion, file integrity verification
  - Background loading for low-latency triggering

- **Platform Audio Drivers** (Months 1-2)
  - CoreAudio (macOS), WASAPI (Windows), ASIO (Windows professional)
  - Low-latency I/O (<5ms ASIO, <10ms WASAPI/CoreAudio)
  - Device enumeration, configuration, latency reporting
  - Network audio devices (Dante Virtual Soundcard, AES67 interfaces) supported via manufacturer-provided drivers

- **Routing Matrix** (Months 3-4)
  - Professional N×M routing (64 channels → 16 groups → 32 outputs)
  - Real-time gain/pan adjustment with click-free smoothing
  - Multiple solo modes (SIP, AFL, PFL), mute controls, per-clip routing
  - Real-time metering (Peak, RMS, TruePeak, LUFS)
  - Snapshot/preset system for instant recall

- **Performance Monitor** (Months 4-5)
  - Real-time diagnostics: CPU usage, buffer underruns, latency
  - Thread-safe queries, memory tracking
  - Per-clip CPU breakdown (optional profiling)

**Testing & Quality:**

- Unit tests for sample-accurate timing (±1 sample tolerance)
- Integration tests (16 simultaneous clips, latency <10ms)
- Stress tests (24-hour stability, rapid trigger 100/sec)
- Cross-platform validation (macOS + Windows)

**Success Criteria:**

- ✅ Single-clip playback working (Month 2)
- ✅ 16 simultaneous clips, CPU <30% (Month 4)
- ✅ OCC MVP integration complete (Month 6)

**Documentation:**

- Doxygen API docs for all public interfaces
- Integration guide for application developers
- Platform-specific guides (CoreAudio, ASIO/WASAPI)

**Implementation Plans:**

- **ORP068** - SDK Integration Plan v2.0 (driver architecture, contracts, client integration)
- **ORP069** - OCC-Aligned SDK Enhancements (platform drivers, routing, performance monitoring)
- These plans run in parallel with coordinated timelines and shared validation checkpoints

**References:**

- See `docs/ORP/ORP068.md` for integration infrastructure
- See `docs/ORP/ORP069.md` for OCC-aligned enhancements
- See `docs/ORP/ORP070.md` for OCC MVP sprint plan (routing matrix, real audio)
- See `apps/clip-composer/docs/OCC/OCC029.md` for detailed specifications
- See `apps/clip-composer/docs/OCC/OCC027.md` for interface definitions
- See `apps/clip-composer/docs/OCC/OCC030.md` for current implementation status

## Milestone M3 – Feature Expansion

- Extend the core library with richer session models (tempo, markers, media).
- Flesh out the REAPER adapter with interactive UI surfaces and message routing.
- Add audio rendering scenarios to the minhost (streamed audio, bus routing).
- Introduce integration tests that exercise host ↔ adapter handshakes.

## Milestone M4 – Recording & DSP (Months 7-12, 2025-2026)

**Purpose:** Support advanced features for Orpheus Clip Composer v1.0 and enable
creative workflows for FX Engine.

**Core Modules:**

- **Input Audio Graphs** (Months 7-9)
  - Capture input from audio drivers (recording)
  - Buffer management for real-time recording
  - Disk streaming (WAV/AIFF file writer)

- **DSP Processing** (Months 10-12)
  - Pluggable DSP processor interface
  - Rubber Band integration (time-stretch, pitch-shift)
  - Real-time parameter updates
  - Quality vs performance profiles

- **Remote Control Protocols** (Months 10-12)
  - WebSocket server (iOS companion app)
  - OSC integration (Open Sound Control)
  - MIDI control mapping
  - Command registration API

**Applications Enabled:**

- OCC v1.0: Recording directly into buttons, DSP processing
- FX Engine: LLM-powered effects, real-time parameter automation
- Wave Finder: Real-time frequency analysis

## Milestone M5 – Production Readiness

- Harden ABI compatibility guarantees and publish migration guidelines.
- Ship official SDK packaging (binary + headers) for supported platforms.
- Expand CI with packaging, code coverage, and static analysis gates.
- Document extension points and authoring guides for partner teams.

## Milestone M6 – Advanced Features (Months 13-18, 2026)

**Purpose:** Enterprise capabilities and advanced workflows for Orpheus Clip
Composer v2.0 and professional installations.

**Core Modules:**

- **Advanced Transport**
  - Master/slave clip linking
  - AutoPlay/jukebox mode with crossfades
  - Rehearsal mode (silent playback with visual feedback)

- **Spatial Audio** (Basic VBAP)
  - 3D positioning for clips
  - Multi-channel speaker configurations
  - Distance-based attenuation

- **Interaction Rules Engine**
  - Conditional triggering (Ovation-inspired)
  - State machines for show control
  - External hardware integration (GPI)

- **VST3 Plugin Hosting**
  - Plugin scanning and loading
  - Parameter automation
  - MIDI routing to plugins

---

## Application Roadmap

**Orpheus Clip Composer** (Flagship soundboard application)

- MVP: Months 1-6 (10-user beta, core features)
- v1.0: Months 7-12 (recording, iOS app, DSP, remote control)
- v2.0: Months 13-18 (AutoPlay, GPI, interaction rules, spatial audio)

**Orpheus Wave Finder** (Harmonic calculator and frequency scope)

- v1.0: Months 7-12 (FFT analysis, harmonic detection)

**Orpheus FX Engine** (LLM-powered effects processing)

- v1.0: Months 10-12 (DSP integration, LLM hooks)

**Related Documentation:**

- `apps/clip-composer/docs/OCC/` - Orpheus Clip Composer design documentation
- `apps/clip-composer/docs/OCC/OCC021` - Product vision (authoritative)
- `apps/clip-composer/docs/OCC/OCC026` - MVP milestone definition
- `apps/clip-composer/docs/OCC/OCC029` - SDK enhancement recommendations
- `apps/clip-composer/docs/OCC/PROGRESS.md` - Design phase progress report
