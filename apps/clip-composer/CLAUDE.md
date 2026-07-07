# Orpheus Clip Composer - Application Development Guide

**Workspace:** This repo inherits general conventions from `~/chrislyons/dev/CLAUDE.md`
**Best Practices:** See `~/dev/docs/CLAUDE_CODE_BEST_PRACTICES.md` for comprehensive Claude Code workflow guidance

**Location:** `/apps/clip-composer/`
**Status:** v0.2.0 Sprint Complete (OCC093)
**Framework:** JUCE 8.0.4
**SDK:** Orpheus SDK M2 (real-time infrastructure)

## Configuration Hierarchy

This application follows a three-tier configuration hierarchy:

1. **This file (CLAUDE.md)** — Application-specific rules and conventions
2. **Parent SDK config** (`/Users/chrislyons/dev/orpheus-sdk/CLAUDE.md`) — SDK-level patterns
3. **Workspace config** (`~/chrislyons/dev/CLAUDE.md`) — Cross-repo patterns
4. **Global config** (`~/.claude/CLAUDE.md`) — Universal rules

**Conflict Resolution:** App > SDK > Workspace > Global > Code behavior

---

## Purpose

This guide helps developers build the **Orpheus Clip Composer** application—a professional soundboard for broadcast, theater, and live performance. This is an **application** that uses the Orpheus SDK as its audio engine foundation.

**What this guide covers:**

- Core development principles and workflows
- Essential architecture and threading model
- Quick reference for common tasks
- Links to detailed implementation documentation

**What this guide does NOT cover:**

- Orpheus SDK core development (see `/CLAUDE.md` at repository root)
- Design specifications (see `docs/occ/` for design documents)
- SDK module implementation (see `/src/core/` and SDK documentation)

---

## Quick Start

### Prerequisites

1. **Orpheus SDK built and tested:**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ctest --test-dir build  # Verify all tests pass
   ```

2. **JUCE Framework installed:**
   - Download from https://juce.com/
   - Recommended: JUCE 7.x (latest stable)
   - License: JUCE Indie or higher (budget ~€800/year for commercial use)

3. **Development environment:**
   - macOS: Xcode 14+ (Apple Clang)
   - Windows: Visual Studio 2019+ (MSVC)
   - Linux: GCC 11+ or Clang 13+

### Building Clip Composer

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
# TODO: CMake integration will be added once project structure is finalized
```

---

## Architecture Overview

Clip Composer follows a **5-layer architecture** (see `docs/occ/OCC023` for full details):

```
┌─────────────────────────────────────────────────────────┐
│ Layer 1: UI Components (JUCE)                          │
│  - ClipGrid (10×12 buttons × 8 tabs = 960 clips)       │
│  - WaveformDisplay, TransportControls, RoutingPanel    │
│  - RemoteControl (iOS companion app via OSC)           │
└─────────────────────────────────────────────────────────┘
                        ↓ (Message Thread)
┌─────────────────────────────────────────────────────────┐
│ Layer 2: Application Logic                             │
│  - SessionManager (load/save JSON sessions)            │
│  - ClipManager (track metadata, button assignments)    │
│  - RoutingManager (4 Clip Groups → Master)             │
│  - PreferencesManager (user settings)                  │
└─────────────────────────────────────────────────────────┘
                        ↓ (Lock-Free Commands)
┌─────────────────────────────────────────────────────────┐
│ Layer 3: Orpheus SDK Integration                       │
│  - ITransportController (clip playback)                │
│  - IAudioFileReader (WAV/AIFF/FLAC decoding)           │
│  - IRoutingMatrix (multi-channel routing)              │
│  - IPerformanceMonitor (CPU/latency diagnostics)       │
└─────────────────────────────────────────────────────────┘
                        ↓ (Audio Thread)
┌─────────────────────────────────────────────────────────┐
│ Layer 4: Real-Time Audio Processing                    │
│  - IAudioDriver (CoreAudio/ASIO/WASAPI)                │
│  - Mixing, gain smoothing, fade-out                    │
│  - Sample-accurate timing (±1 sample)                  │
└─────────────────────────────────────────────────────────┘
                        ↓ (Hardware)
┌─────────────────────────────────────────────────────────┐
│ Layer 5: Platform Audio I/O                            │
│  - Audio interfaces (2-32 channels)                    │
│  - ASIO/CoreAudio/WASAPI drivers                       │
└─────────────────────────────────────────────────────────┘
```

**Key Principle:** Layers 1-2 are OCC-specific, Layers 3-5 are Orpheus SDK (shared infrastructure).

---

## Transport Model (CRITICAL — OCC151)

**One clip identity = one voice.** The Edit dialog is a "zoomed in" *view* of the
grid clip's transport, not a separate player. These invariants are load-bearing;
do not reintroduce a parallel handle for a clip.

- **One transport per clip; the dialog shares the grid handle.** `PreviewPlayer`
  drives the grid `ClipHandle` (via `buttonIndex`) directly. It never allocates a
  dedicated cue-buss handle for the same file. Play/stop/position are
  single-sourced, so grid and dialog can never desync and the same file never
  sums to 2× at the master. (The cue-buss pool is retained only for a future,
  genuinely-different auxiliary source — PFL / Cue Buss dispatch.)
- **Voice policy is `MonoWithFadeOverlap`, cap 2.** Every grid clip is registered
  with `setClipVoiceMode(handle, MonoWithFadeOverlap)` and the transport runs
  `setMaxVoicesPerClip(2)`. Firing a live voice restarts it in place; firing
  while only a fade-out tail exists starts a fresh voice alongside the tail
  (voices == 2 only during the fade-overlap window). The SDK enforces this;
  `AudioEngine::startClip` is a thin passthrough — do not re-add an OCC-side
  restart-if-playing dedup branch.
- **Every re-fire goes through the OCC wrapper.** Never call the raw SDK
  `startClip` directly (e.g. from the device-change restart loop) — route through
  `AudioEngine::startClip` / `startCueBuss` so the voice model is honored
  universally.
- **Transport callbacks drain on the message thread only.** The SDK callback
  ring is SPSC; the audio thread is the sole producer. `processAudio` must NOT
  call `processCallbacks()`. `AudioEngine::drainTransportCallbacks()` is called
  from `MainComponent`'s UI timer and asserts (Debug) it is off the audio thread.
- **Device swaps quiesce the audio thread first.** `setAudioDevice` stops the
  running driver (which drains in-flight callbacks) *before* moving the
  transport/driver pointers, then publishes the new pair before starting it.
- **Mismatched-rate files are resampled by the SDK.** `registerClipAudio` wraps
  the reader in the SDK's polyphase `ResamplingAudioFileReader`. OCC presents the
  UI metadata in the engine-rate timeline so trims/fades stay sample-accurate. No
  "sample rate mismatch" warnings — files play at correct pitch.
- **"Stop Others" choke is playgroup-scoped.** Scoped to the firing clip's
  `clipGroup` (0-3), not global and not the visible tab. Decision lives in the
  pure `occ::shouldChokeStop` policy (`Source/Core/ChokePolicy.h`). Group A never
  stops group B.

**SDK dependency:** ORP127 (SDK 0.3.0 — voice API, choke primitive, SR
conversion). Currently tracked on branch `feat/orp127-transport-voice-integrity`;
pin to the `v0.3.0` tag once the SDK agent cuts it (OCC151 T12).

---

## Threading Model (CRITICAL)

Clip Composer uses **3 threads** to maintain real-time performance:

### 1. Message Thread (UI Thread)

- **Owner:** JUCE MessageManager
- **Responsibilities:**
  - Handle UI events (button clicks, keyboard, mouse)
  - Update visual components (waveforms, meters, transport position)
  - Process SDK callbacks (`ITransportCallback::onClipStarted()`, etc.)
  - Save/load sessions (I/O operations allowed)
- **Rules:**
  - NO audio processing
  - NO blocking the audio thread
  - Use lock-free commands to communicate with audio thread

### 2. Audio Thread (Real-Time Thread)

- **Owner:** IAudioDriver (CoreAudio, ASIO, WASAPI)
- **Responsibilities:**
  - Process audio in `processAudio()` callback (~10ms @ 512 samples)
  - Read audio files via `IAudioFileReader`
  - Mix clips via `IRoutingMatrix`
  - Update transport position atomically
  - Post callbacks to message thread (lock-free queue)
- **Rules:**
  - ⛔ NO allocations (no `new`, `std::vector::push_back()`, etc.)
  - ⛔ NO locks (no `std::mutex`, no waiting)
  - ⛔ NO I/O (no file reads, no network calls)
  - ✅ Use lock-free structures (atomic operations only)

### 3. File I/O Thread (Background Thread)

- **Owner:** JUCE ThreadPool or custom worker
- **Responsibilities:**
  - Pre-load audio files for waveform display
  - Scan directories for new clips
  - Write recorded audio to disk
  - Calculate waveform data for UI
- **Rules:**
  - NO interaction with audio thread
  - Communicate with UI via callbacks on message thread

**Thread Safety Verification:**

- SDK provides lock-free primitives (see `ITransportController` implementation)
- Use `juce::MessageManager::callAsync()` for UI updates from background threads
- Never call JUCE UI components from audio thread

**For detailed code examples, see:** `docs/occ/OCC096` (SDK Integration Patterns)

---

## Core Principles

1. **Reliability Above All** - 24/7 operational capability, crash-proof
2. **Performance-First** - Ultra-low latency (<5ms), sample-accurate timing
3. **Real-Time Safety** - No allocations, no locks, no I/O on audio thread
4. **Determinism** - Same input → same output, always (bit-identical)
5. **Host-Neutral SDK** - Core SDK works across REAPER, standalone, plugins

---

## File Organization

```
/apps/clip-composer/
├── CLAUDE.md              # This file (development guide)
├── CMakeLists.txt         # JUCE + Orpheus SDK integration
├── README.md              # Getting started for contributors
├── Source/                # JUCE application code
│   ├── Main.cpp           # Entry point
│   ├── MainComponent.h/cpp # Top-level application component
│   ├── ClipGrid/          # Clip grid UI (960 buttons)
│   ├── AudioEngine/       # SDK integration layer
│   ├── Session/           # Session management
│   └── UI/                # Additional UI components
├── Resources/             # Icons, assets, session templates
├── docs/occ/              # OCC-specific documentation
│   ├── OCC096.md          # SDK Integration Patterns
│   ├── OCC097.md          # Session Format
│   ├── OCC098.md          # UI Components
│   ├── OCC099.md          # Testing Strategy
│   ├── OCC100.md          # Performance Requirements
│   └── OCC101.md          # Troubleshooting Guide
├── .claude/               # OCC-specific progress tracking
└── tests/                 # OCC-specific tests
    ├── unit/
    └── integration/
```

---

## Detailed Implementation Documentation

### SDK Integration

**See:** `docs/occ/OCC096.md` - SDK Integration Patterns

Complete code examples for:

- Starting/stopping clips
- Loading audio files
- Routing configuration
- Performance monitoring
- Anti-patterns (what NOT to do)

### Session Management

**See:** `docs/occ/OCC097.md` - Session Format

Complete reference for:

- JSON schema (metadata, clips, routing, preferences)
- Loading/saving sessions
- Version migration
- Error handling

### UI Components

**See:** `docs/occ/OCC098.md` - UI Components

JUCE component implementations:

- ClipGrid (960 buttons)
- WaveformDisplay
- TransportControls
- RoutingPanel
- PerformanceMonitor

### Testing

**See:** `docs/occ/OCC099.md` - Testing Strategy

Complete testing guide:

- Unit tests (GoogleTest)
- Integration tests (OCC + SDK)
- Manual testing checklist
- Performance benchmarks
- CI/CD integration

### Performance

**See:** `docs/occ/OCC100.md` - Performance Requirements

Performance targets and optimization:

- Latency: <5ms round-trip
- CPU: <30% with 16 simultaneous clips
- Memory: Stable after 1 hour operation
- Optimization guidelines
- Profiling tools

### Troubleshooting

**See:** `docs/occ/OCC101.md` - Troubleshooting Guide

Common issues and solutions:

- Build issues (SDK headers, linker errors, JUCE modules)
- Runtime issues (audio dropouts, high CPU, clips not starting)
- Performance issues (slow session loading, memory leaks)
- Cross-platform issues (macOS, Windows, Linux)

---

## Quick Reference: Common Tasks

### Load and play a single clip:

```cpp
auto reader = orpheus::createAudioFileReader();
reader->open("/path/to/clip.wav");

auto transport = orpheus::createTransportController(nullptr, 48000);
transport->startClip(clipHandle);

auto driver = orpheus::createDummyAudioDriver();
driver->start(audioCallback);
```

### Stop all clips (panic button):

```cpp
transportController->stopAllClips();
```

### Update waveform display:

```cpp
waveformDisplay->setAudioFile(reader.get(), metadata);
waveformDisplay->repaint();
```

### Save current session:

```cpp
sessionManager->saveSession(juce::File("~/Documents/OCC/my_session.json"));
```

**For more examples, see:** `docs/occ/OCC096.md`

---

## Design Documentation Reference

All design specifications live in `docs/occ/`:

**Product & Vision:**

- **OCC021** - Product Vision (authoritative) - Market positioning, competitive analysis
- **OCC026** - MVP Definition - 6-month plan, acceptance criteria

**Technical Specifications:**

- **OCC027** - API Contracts - C++ interfaces between OCC and SDK
- **OCC023** - Component Architecture - 5-layer architecture, threading model
- **OCC022** - Clip Metadata Schema - Complete JSON schema
- **OCC024** - User Interaction Flows - 8 complete workflows

**Technology Decisions:**

- **OCC025** - UI Framework Decision - JUCE vs Electron (JUCE recommended)
- **OCC028** - DSP Library Evaluation - Rubber Band vs SoundTouch (Rubber Band recommended)
- **OCC029** - SDK Enhancement Recommendations - Gap analysis, 5 critical modules
- **OCC030** - SDK Status Report - Current SDK status, implementation timeline

**Implementation Reference:**

- **OCC096** - SDK Integration Patterns - Code examples for OCC + SDK integration
- **OCC097** - Session Format - JSON schema and loading/saving code
- **OCC098** - UI Components - JUCE component implementations
- **OCC099** - Testing Strategy - Unit/integration tests, manual testing
- **OCC100** - Performance Requirements - Targets, optimization, profiling
- **OCC101** - Troubleshooting Guide - Common issues and solutions

**Always reference design docs** before implementing features. If design is incomplete, update design docs first, then code.

---

## Development Workflow

### Phase 1: SDK Integration (Months 1-2)

**Goal:** Get basic clip playback working with SDK

**Deliverable:** Single-clip playback demo

### Phase 2: Core UI (Months 2-3)

**Goal:** Build 960-button clip grid and basic transport controls

**Deliverable:** Multi-clip triggering with visual feedback

### Phase 3: Routing & Mixing (Months 3-4)

**Goal:** Implement 4 Clip Groups with routing controls

**Deliverable:** Full routing matrix with group controls

### Phase 4: Advanced Features (Months 4-6)

**Goal:** Waveform editor, remote control, diagnostics

**Deliverable:** MVP ready for beta testing

### Phase 5: Beta & Polish (Month 6)

**Goal:** Beta testing, bug fixes, cross-platform validation

**Deliverable:** OCC MVP v1.0 release

**See:** `docs/occ/OCC026.md` for complete 6-month plan

---

## Success Metrics

From `docs/occ/OCC030` Section 10.3:

- **Month 2:** OCC playing real audio (CoreAudio/WASAPI)
- **Month 4:** OCC 16-clip demo with routing
- **Month 6:** OCC MVP beta (10 users)

**Definition of Done for MVP:**

- [ ] 960 clips loaded and displayable
- [ ] 16 simultaneous clips playing with routing
- [ ] <5ms latency with ASIO driver
- [ ] <30% CPU with 16 clips (Intel i5 8th gen)
- [ ] Session save/load with JSON
- [ ] Waveform editor (trim IN/OUT)
- [ ] Remote control via OSC (iOS app)
- [ ] 10 beta users successfully running OCC for 1+ hour sessions
- [ ] Zero crashes in 24-hour stability test

---

## Additional Resources

**Orpheus SDK Documentation:**

- `/CLAUDE.md` - SDK development guide (core principles)
- `/README.md` - Repository overview, build instructions
- `/ARCHITECTURE.md` - SDK design rationale
- `/ROADMAP.md` - Milestones, timeline

**OCC Documentation:**

- `docs/occ/INDEX.md` - OCC documentation catalog (OCC001-OCC101)
- `PROGRESS.md` - Implementation progress tracking (app root)

**JUCE Resources:**

- https://juce.com/learn/documentation - Official JUCE docs
- https://github.com/juce-framework/JUCE/tree/master/examples - Example projects
- https://forum.juce.com/ - Community forum

**External Libraries:**

- libsndfile: https://libsndfile.github.io/libsndfile/
- Rubber Band: https://breakfastquay.com/rubberband/ (v1.0 integration)

---

**Remember:** Clip Composer is a professional tool for broadcast, theater, and live performance. Design for 24/7 reliability, ultra-low latency, and zero crashes. When in doubt, favor simplicity, determinism, and user autonomy over short-term convenience.

---

## Git Workflow Best Practices

### Commit Guidelines

**Read before committing:**

1. Run `git status` to see changes
2. Run `git diff` to review specific changes
3. Create descriptive commit messages

**Commit message format:**

```bash
git commit -m "type: brief summary

- Detailed change 1
- Detailed change 2
- Test results or validation

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

**Types:** `feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `ui`

**Branch naming:**

- `feature/occ-` — New OCC features
- `fix/occ-` — Bug fixes
- `refactor/occ-` — Code refactoring
- `ui/occ-` — UI improvements

### Error Handling

**Common JUCE build errors:**

1. **Missing JUCE headers:** Verify JUCE path in CMakeLists.txt
2. **Linker errors with SDK:** Check Orpheus SDK is built first
3. **Audio driver initialization failures:** Check permissions, device availability

**File edit failures:**

- Always use Read tool before Edit tool
- Ensure exact whitespace match in old_string
- Make smaller, targeted edits if full replacement fails
- Re-read file if edit fails, then retry

**When stuck:**

1. Use `/clear` to reset context
2. Check `docs/occ/OCC101.md` (Troubleshooting Guide)
3. Verify SDK is built and tests pass
4. Check threading model (see Threading Model section)

### Multi-File Editing Strategy

**For coordinated changes across OCC components:**

1. **Discovery Phase:**
   - Identify which layers are affected (UI → Logic → SDK Integration)
   - Read files in dependency order
   - Check threading boundaries (Message vs Audio thread)

2. **Plan Phase:**
   - Order changes by layer (SDK Integration → Logic → UI)
   - Verify real-time safety (no allocations on audio thread)

3. **Execute Phase:**
   - Make changes respecting threading model
   - Test after each layer completion
   - Run with sanitizers to catch threading issues

### Custom Slash Commands

Custom commands in `.claude/commands/` for OCC workflows:

**Example: `/rebuild-occ`**

```markdown
<!-- .claude/commands/rebuild-occ.md -->

Rebuild and launch Clip Composer:

1. Verify SDK is built
2. Clean OCC build artifacts
3. Rebuild OCC application
4. Launch: ./scripts/relaunch-occ.sh
5. Report any build errors or warnings
```

**Example: `/check-threading`**

```markdown
<!-- .claude/commands/check-threading.md -->

Review code for thread safety issues:

1. Check for allocations on audio thread
2. Verify lock-free communication
3. Check for UI calls from background threads
4. Suggest fixes for violations
```

**Example: `/occ-docs`**

```markdown
<!-- .claude/commands/occ-docs.md -->

Reference OCC documentation:

1. List relevant OCC### documents for current task
2. Provide quick summaries
3. Link to full docs in docs/occ/
```

---

**Last Updated:** 2026-07-07 (OCC151 — Transport Unification & Gain Integrity)
**Status:** v0.2.1 active; OCC151 transport unification landed on `feat/occ-transport-unification`
**Release:** v0.1.0-alpha (October 22, 2025)
**Next Milestone:** pin SDK to `v0.3.0` tag (OCC151 T12), then merge to main
**SDK:** ORP127 / SDK 0.3.0 (voice API, choke primitive, SR conversion)

- Use @"docs/occ/OCC110 SDK Integration Guide - Transport State and Loop Features.md" as a reference document for Clip Composer's Orpheus SDK integrations.
- Use ./scripts/relaunch-occ.sh to relaunch Clip Composer.
