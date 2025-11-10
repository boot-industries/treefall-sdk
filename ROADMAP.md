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

## Milestone M2 – Real-Time Infrastructure (COMPLETE - November 2025)

**Status:** ✅ Complete - All core modules implemented and tested

**Purpose:** Enable real-time audio playback for Orpheus Clip Composer MVP and
future applications requiring sample-accurate, low-latency performance.

**Core Modules:**

- ✅ **Transport Controller** - Complete
  - Sample-accurate clip playback with start/stop control
  - Lock-free audio thread with command queue
  - Transport callbacks (clip started/stopped/looped/restarted/seeked)
  - Multi-clip playback (16+ simultaneous clips tested)
  - Gain control (-96 to +12 dB)
  - Loop mode with seamless boundary enforcement
  - Persistent metadata storage
  - Seek API for sample-accurate position control

- ✅ **Audio File Reader** - Complete
  - Decode WAV/AIFF/FLAC using libsndfile
  - Pre-loaded memory for real-time access
  - File integrity verification
  - 48kHz sample rate support

- ✅ **Platform Audio Drivers** - Partial
  - CoreAudio (macOS) - Complete and tested
  - Dummy driver for testing - Complete
  - WASAPI (Windows) - Planned
  - ASIO (Windows) - Planned
  - ALSA (Linux) - Planned

- ⏳ **Routing Matrix** - Planned for OCC v0.3.0
  - Professional N×M routing
  - Real-time gain/pan adjustment with click-free smoothing
  - Multiple solo modes, mute controls
  - Real-time metering
  - Snapshot/preset system

- ⏳ **Performance Monitor** - Planned for v1.0
  - Real-time diagnostics: CPU usage, buffer underruns, latency
  - Thread-safe queries, memory tracking
  - Per-clip CPU breakdown

**Testing & Quality:**

- ✅ Unit tests for sample-accurate timing (32/32 tests passing)
- ✅ Integration tests (16 simultaneous clips, 74.9% callback accuracy)
- ✅ Stress tests (60-second multi-clip test, no memory leaks)
- ✅ Cross-platform validation (macOS complete, Windows/Linux pending)
- ✅ AddressSanitizer clean (no memory leaks detected)

**Success Criteria:**

- ✅ Single-clip playback working
- ✅ 16 simultaneous clips, CPU <30%
- ✅ OCC MVP integration complete (v0.2.0-alpha released)

**Documentation:**

- ✅ Doxygen-ready API headers (all public interfaces documented)
- ✅ Migration guide (v0.x → v1.0)
- ✅ README with quick start guide
- ✅ CHANGELOG with detailed release notes
- ⏳ Platform-specific guides (CoreAudio complete, ASIO/WASAPI pending)

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

## Milestone M4 – Recording & DSP (Planned: Q1-Q2 2026)

**Purpose:** Support advanced features for Orpheus Clip Composer v1.0 and enable
creative workflows for FX Engine.

**Core Modules:**

- ⏳ **Input Audio Graphs** - Planned
  - Capture input from audio drivers (recording)
  - Buffer management for real-time recording
  - Disk streaming (WAV/AIFF file writer)

- ⏳ **DSP Processing** - Planned
  - Pluggable DSP processor interface
  - Rubber Band integration (time-stretch, pitch-shift)
  - Real-time parameter updates
  - Quality vs performance profiles

- ⏳ **Remote Control Protocols** - Planned
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

## Milestone M6 – Advanced Features (Planned: Q3-Q4 2026)

**Purpose:** Enterprise capabilities and advanced workflows for Orpheus Clip
Composer v2.0 and professional installations.

**Core Modules:**

- ⏳ **Advanced Transport** - Planned
  - Master/slave clip linking
  - AutoPlay/jukebox mode with crossfades
  - Rehearsal mode (silent playback with visual feedback)

- ⏳ **Spatial Audio** (Basic VBAP) - Planned
  - 3D positioning for clips
  - Multi-channel speaker configurations
  - Distance-based attenuation

- ⏳ **Interaction Rules Engine** - Planned
  - Conditional triggering (Ovation-inspired)
  - State machines for show control
  - External hardware integration (GPI)

- ⏳ **VST3 Plugin Hosting** - Planned
  - Plugin scanning and loading
  - Parameter automation
  - MIDI routing to plugins

---

## Application Roadmap

**Orpheus Clip Composer** (Flagship soundboard application)

- ✅ MVP (v0.2.0-alpha): Released November 2025 - Core playback features
- ⏳ v1.0: Q1-Q2 2026 (recording, iOS app, DSP, remote control)
- ⏳ v2.0: Q3-Q4 2026 (AutoPlay, GPI, interaction rules, spatial audio)

**Orpheus Wave Finder** (Harmonic calculator and frequency scope)

- ⏳ v1.0: Q2 2026 (FFT analysis, harmonic detection)

**Orpheus FX Engine** (LLM-powered effects processing)

- ⏳ v1.0: Q2 2026 (DSP integration, LLM hooks)

**Related Documentation:**

- `apps/clip-composer/docs/OCC/` - Orpheus Clip Composer design documentation
- `apps/clip-composer/docs/OCC/OCC021` - Product vision (authoritative)
- `apps/clip-composer/docs/OCC/OCC026` - MVP milestone definition
- `apps/clip-composer/docs/OCC/OCC029` - SDK enhancement recommendations
- `apps/clip-composer/docs/OCC/PROGRESS.md` - Design phase progress report
