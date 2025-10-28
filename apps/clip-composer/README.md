# Orpheus Clip Composer

**Professional soundboard for broadcast, theater, and live performance**

---

## Quick Start

### Prerequisites

1. **Orpheus SDK built:**

   ```bash
   cd /Users/chrislyons/dev/orpheus-sdk
   cmake -S . -B build && cmake --build build
   ctest --test-dir build  # Verify all tests pass
   ```

2. **JUCE Framework:** Download from https://juce.com/ (v7.x recommended)

3. **System Requirements:**
   - macOS 11+ (Big Sur) or Windows 10+ (x64)
   - Intel i5 8th gen or Apple M1/M2
   - Audio interface with ASIO (Windows) or CoreAudio (macOS) support
   - 8GB RAM minimum, 16GB recommended

### Building from Source

**macOS (Debug build):**

```bash
cd /Users/chrislyons/dev/orpheus-sdk
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON
cmake --build build
open build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app
```

**Creating DMG Package:**

```bash
# Build the application
cmake --build build

# Create DMG (example for arm64)
mkdir -p /tmp/occ-staging
cp -R build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Debug/OrpheusClipComposer.app /tmp/occ-staging/
hdiutil create -volname "Orpheus Clip Composer" -srcfolder /tmp/occ-staging -ov -format UDZO OrpheusClipComposer-v0.1.0-arm64.dmg
```

**Download Pre-Built:**

- [v0.1.0-alpha (arm64, macOS 12+)](https://github.com/chrislyons/orpheus-sdk/releases/tag/v0.1.0-alpha) - 36MB DMG

---

## What is Clip Composer?

Orpheus Clip Composer is a **professional soundboard application** designed for:

- **Broadcast radio/TV** - Playout automation, jingles, sound effects
- **Theater sound design** - Cue playback, multi-scene control
- **Live performance** - Concert soundscapes, DJ sets, installations

**Key Features (MVP - Month 6):**

- 960 clip buttons (10×12 grid, 8 tabs)
- 16 simultaneous clips playing
- 4 Clip Groups with routing to Master Output
- <5ms latency with ASIO driver
- Sample-accurate timing (±1 sample @ 48kHz)
- Waveform editor (trim IN/OUT, cue points)
- Remote control via iOS companion app (OSC)
- Session save/load (JSON format)

**Compare to:**

- SpotOn (€1,200) - Broadcast playout, Windows-only
- QLab (€700) - Theater cues, macOS-only
- Ovation (€500) - Live performance, basic routing

**Clip Composer advantage:** Cross-platform, open SDK, sovereign (no cloud dependencies)

---

## Project Status

**Current Release:** v0.1.0-alpha (October 22, 2025)

**Download:** [OrpheusClipComposer-v0.1.0-arm64.dmg](https://github.com/chrislyons/orpheus-sdk/releases/tag/v0.1.0-alpha) (36MB)
**Platform:** macOS 12+ (Apple Silicon only)
**Build Type:** Debug (Release optimization pending)

**What's Working:**

- ✅ 48-button clip grid (6×8) with 8 tabs = 384 total clips
- ✅ Real-time audio playback via CoreAudio
- ✅ Drag & drop audio file loading (WAV, AIFF, FLAC)
- ✅ Keyboard shortcuts for all 48 buttons (QWERTY layout)
- ✅ Session save/load (JSON format with full metadata)
- ✅ Transport controls (Stop All, Panic)
- ✅ Clip editing: Name, color, clip group assignment
- ✅ Waveform visualization with real-time trim markers
- ✅ Fade In/Out times (0.0-3.0s) with curve selection
- ✅ UI polish: Inter typeface, 20% larger fonts, 3-line text wrapping
- ✅ Drag-to-reorder clips (240ms hold time)
- ✅ Stop Others On Play mode (per-clip solo)

**Known Limitations:**

- ⚠️ Debug build only (Release has linker issues to fix)
- ⚠️ Apple Silicon only (no Intel or universal binary yet)
- ⚠️ Sample rate locked to 48kHz (auto-conversion coming)
- ⚠️ Trim handles not interactive (slider-based only)
- ⚠️ Fade curves stored but not applied during playback

**Timeline:**

- ✅ **Phase 0:** Design & Documentation (Complete - Oct 12, 2025)
- ✅ **Phase 1:** Project Setup & SDK Integration (Complete - Oct 13, 2025)
- ✅ **Phase 2:** UI Enhancements & Edit Dialog Phase 1 (Complete - Oct 13-14, 2025)
- ✅ **Phase 3:** Edit Dialog Phases 2 & 3 + Waveform Rendering (Complete - Oct 22, 2025)
- ✅ **Phase 4:** Build & Release (Complete - Oct 22, 2025)
- ⏳ **v0.2.0:** Optimization, fade curve integration, interactive trim handles (In Progress)
- 🎯 **v1.0:** Recording, time-stretching, VST/AU hosting (6 months from Oct 2025)

**See also:** `.claude/implementation_progress.md` for detailed task tracking

---

## Architecture

Clip Composer is a **JUCE application** that uses the **Orpheus SDK** as its audio engine foundation.

**5-Layer Architecture:**

1. **UI Components** (JUCE) - ClipGrid, WaveformDisplay, TransportControls
2. **Application Logic** - SessionManager, ClipManager, RoutingManager
3. **SDK Integration** - ITransportController, IAudioFileReader, IRoutingMatrix
4. **Real-Time Audio** - IAudioDriver, mixing, gain smoothing
5. **Platform I/O** - CoreAudio (macOS), ASIO/WASAPI (Windows)

**Threading Model:**

- **Message Thread:** UI events, session I/O, SDK callbacks
- **Audio Thread:** Real-time processing (lock-free, no allocations)
- **File I/O Thread:** Waveform pre-rendering, directory scanning

**See also:** `docs/OCC/OCC023` for complete architecture documentation

---

## Design Documentation

All design specifications are in `docs/OCC/`:

**Essential Reading:**

- **OCC021** - Product Vision (authoritative) - Market positioning, competitive analysis
- **OCC026** - MVP Definition - 6-month plan, deliverables, acceptance criteria
- **OCC027** - API Contracts - C++ interfaces between OCC and SDK
- **OCC030** - SDK Status Report - Current SDK readiness, timeline

**Full Documentation:** See `docs/OCC/README.md` for complete index (11 documents)

---

## Development Guide

**For developers building Clip Composer:**

- Read **CLAUDE.md** in this directory - Comprehensive development guide
- Read **`docs/OCC/`** design documents - Product specifications
- Follow **threading model** - UI thread vs audio thread separation
- Use **SDK integration patterns** - Lock-free commands, callbacks
- Test with **dummy audio driver** first, then platform drivers

**For SDK developers:**

- Read **`/CLAUDE.md`** at repository root - SDK development principles
- Implement modules from **`apps/clip-composer/docs/OCC/OCC029`** - SDK enhancement recommendations
- Coordinate via **Slack `#orpheus-occ-integration`** - Weekly syncs

---

## Performance Targets (MVP)

From `docs/OCC/OCC026`:

| Metric                | Target                | Hardware                   |
| --------------------- | --------------------- | -------------------------- |
| Latency               | <5ms                  | ASIO driver @ 48kHz        |
| CPU Usage             | <30%                  | Intel i5 8th gen, 16 clips |
| Clip Capacity         | 960                   | 10×12 grid × 8 tabs        |
| Simultaneous Playback | 16 clips              | 4 Clip Groups → Master     |
| File Formats          | WAV, AIFF, FLAC       | Via libsndfile             |
| Sample Rates          | 44.1kHz, 48kHz, 96kHz |                            |
| MTBF                  | >100 hours            | Continuous operation       |

---

## Testing Strategy

### Unit Tests (GoogleTest)

- Session loading/saving
- Clip metadata parsing
- Routing configuration logic

### Integration Tests (OCC + SDK)

- Clip playback via button triggers
- Multi-clip routing
- Transport callbacks
- Performance under load (16 clips)

### Manual Testing

- 960-clip load test (stress test)
- 24-hour stability test (no crashes)
- Cross-platform validation (macOS + Windows)
- Beta testing (10 users, Month 6)

---

## Known Limitations

### MVP Scope (v1.0 features deferred)

- ❌ No recording (playback only)
- ❌ No VST3/AU plugin hosting
- ❌ No time-stretching (Rubber Band integration in v1.0)
- ❌ No aux sends (4 groups → master only)
- ❌ No network streaming (local files only)

### SDK Dependencies (in progress)

- ⏳ Platform audio drivers (Month 2)
- ⏳ Routing matrix (Month 3-4)
- ⏳ Performance monitor (Month 4-5)

**All limitations documented** and tracked in design documents.

---

## License

Clip Composer is part of the Orpheus ecosystem and inherits the **MIT License** from the Orpheus SDK.

**Dependencies:**

- **JUCE Framework** - GPL or paid license (recommend JUCE Indie ~€800/year)
- **libsndfile** - LGPL 2.1 (dynamic linking, compliant)
- **Rubber Band** - AGPL or paid license (v1.0 feature, deferred)

---

## Communication

**Slack:** `#orpheus-occ-integration`
**GitHub Issues:** Tag with `occ-blocker` for urgent needs
**Weekly Sync:** Fridays, 30 minutes (OCC team + SDK team)

**Questions?**

- Read **CLAUDE.md** (development guide)
- Check **`docs/OCC/`** (design documentation)
- Ask on Slack channel

---

## Success Criteria (MVP)

From `docs/OCC/OCC026`:

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

**Status:** v0.1.0-alpha Released (October 22, 2025)
**Last Updated:** October 22, 2025
**Next Milestone:** v0.2.0 planning - Release build optimization, fade curve integration
