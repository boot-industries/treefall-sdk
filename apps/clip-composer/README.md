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

### Building (Coming Soon)

```bash
cd /Users/chrislyons/dev/orpheus-sdk/apps/clip-composer
# CMake build will be added once initial implementation is complete
```

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

**Current Phase:** Month 1 - SDK Integration (October 2025)

**Timeline:**
- ✅ **Design Phase Complete** (September-October 2025) - 11 design docs, ~5,300 lines
- 🔄 **Month 1-2:** SDK integration (basic clip playback)
- ⏳ **Month 3-4:** Routing matrix implementation
- ⏳ **Month 5-6:** Advanced features, beta testing
- 🎯 **Month 6:** MVP release

**SDK Status:** 90% ready (3/5 critical modules complete)
- ✅ ITransportController (real-time clip playback)
- ✅ IAudioFileReader (WAV/AIFF/FLAC decoding)
- ✅ IAudioDriver (dummy driver complete, platform drivers in progress)
- ⏳ IRoutingMatrix (4 Clip Groups → Master, Month 3-4)
- ⏳ IPerformanceMonitor (CPU/latency diagnostics, Month 4-5)

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

| Metric | Target | Hardware |
|--------|--------|----------|
| Latency | <5ms | ASIO driver @ 48kHz |
| CPU Usage | <30% | Intel i5 8th gen, 16 clips |
| Clip Capacity | 960 | 10×12 grid × 8 tabs |
| Simultaneous Playback | 16 clips | 4 Clip Groups → Master |
| File Formats | WAV, AIFF, FLAC | Via libsndfile |
| Sample Rates | 44.1kHz, 48kHz, 96kHz | |
| MTBF | >100 hours | Continuous operation |

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

**Status:** Active Development (Month 1 - SDK Integration)
**Last Updated:** October 12, 2025
**Next Milestone:** Basic clip playback working (Month 2)
