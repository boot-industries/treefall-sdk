<!-- SPDX-License-Identifier: MIT -->

# Roadmap

The Orpheus roadmap captures the near-term milestones for evolving the SDK and
its adapters. Updated 2026-07-09: M4/M6 items are now sequenced by the ORP132
hardening program (`docs/orp/ORP132`–`ORP136`), and Clip Composer references
point to its external repository (extracted 2026-07-09, see `docs/orp/ORP131`).

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

- See `docs/orp/` for the ORP068/ORP069/ORP070 integration and enhancement plans
- OCC specifications (OCC027/OCC029/OCC030) live in the external Clip Composer
  repo (`chrislyons/clip-composer`, `docs/occ/`)

## Milestone M3 – Feature Expansion

- Extend the core library with richer session models (tempo, markers, media).
- Flesh out the REAPER adapter with interactive UI surfaces and message routing.
- Add audio rendering scenarios to the minhost (streamed audio, bus routing).
- Introduce integration tests that exercise host ↔ adapter handshakes.

## Milestone M4 – Recording, Writer & Analysis Primitives

**Now owned by the ORP132 hardening program.** The recording/writer/analysis
items below are scoped and sequenced in
`docs/orp/ORP134 NEXT Sprint - Streaming Reader and Platform Primitives.md`:

- **`IAudioFileWriter`** (WAV/AIFF/FLAC disk writer, FTR007) → ORP134 G5
- **Input capture / recorder plumbing** (lock-free input ring,
  `IAudioInputStream` contract; CoreAudio input capture landed as ORP130) →
  ORP134 G7
- **Analysis primitives** (FFT/STFT facade over the existing
  LoudnessMeter/waveform/true-peak code) → ORP134 G6

Remaining M4 items not yet scheduled:

- ⏳ **DSP Processing** — pluggable processor interface, Rubber Band
  integration (time-stretch/pitch-shift) → prerequisite work is the ORP134 G3
  graph seam; processor nodes are ORP135 B4
- ⏳ **Remote Control Protocols** — WebSocket/OSC/MIDI mapping → ORP135 B5
  (requires the ORP133 G3 single-producer contract + an MPSC dispatcher)

**Applications Enabled:**

- Clip Composer: recording directly into buttons, DSP processing
- FourTrack: SDK-backed capture → disk path (drops its local WavWriter)
- FreqFinder: shared analysis primitives

## Milestone M5 – Production Readiness

- Harden ABI compatibility guarantees and publish migration guidelines.
- Ship official SDK packaging (binary + headers) for supported platforms.
- Expand CI with packaging, code coverage, and static analysis gates.
- Document extension points and authoring guides for partner teams.

## Milestone M6 – Advanced Features (Speculative 2026+)

**Re-expressed as ORP135 platform bets.** Each item below is a *candidate*
tracked in `docs/orp/ORP135 LATER Sprint - Platform Leadership Bets.md`; a bet
starts only when its ORP133/ORP134 dependency is stable and a concrete
downstream consumer is asking:

- **Sample-accurate automation** → ORP135 B1 (needs ORP134 identity/time)
- **Spatial / multichannel graph readiness** (VBAP, speaker maps, ADM
  alignment) → ORP135 B3 (needs ORP134 G3 graph seam)
- **Plugin processor API, then optional hosting** (VST3/AU/LV2 hosting stays
  an adapter concern, never core) → ORP135 B4 (needs B1 + graph seam)
- **MIDI/OSC/control-surface mapping + MPSC control dispatcher** → ORP135 B5
- **Interaction rules / show-control state machines** → app-side until the
  automation and identity primitives exist (then reassess as an ORP135 bet)

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

- `docs/orp/ORP132` – SDK Hardening & Platform Roadmap master index
  (ORP133 NOW / ORP134 NEXT / ORP135 LATER / ORP136 verification)
- Clip Composer design documentation (OCC021/OCC026/OCC029, progress reports)
  lives in the external Clip Composer repo (`chrislyons/clip-composer`,
  `docs/occ/`)
