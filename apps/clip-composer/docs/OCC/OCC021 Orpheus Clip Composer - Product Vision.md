# OCC021 Orpheus Clip Composer - Product Vision

**Document Version:** 1.0
**Date:** October 10, 2025
**Status:** Authoritative
**Supersedes:** OCC019 (retains as comprehensive prompt for AI agents)

***

## Executive Summary

Orpheus Clip Composer (OCC) is a professional soundboard application for broadcast, theater, and live performance. It is the flagship application of the Orpheus audio ecosystem—a sovereign, host-neutral audio tooling platform built on C++20 foundations. OCC drives the evolution of Orpheus SDK while demonstrating its capabilities in real-world, mission-critical scenarios.

**OCC is not a thin adapter.** It is a full-featured professional application that uses Orpheus SDK as its audio engine foundation while extending it with real-time playback, user interface, recording, and remote control capabilities.

***

## 1. Product Identity

### What Orpheus Clip Composer Is

**OCC is a professional audio playout system** designed for environments where reliability, ultra-low-latency, and sample-accurate timing are non-negotiable:

- **Broadcast studios** requiring 24/7 reliability with instant clip triggering
- **Theater sound designers** needing precise cue management and show control
- **Live performance operators** demanding responsive, tactile control surfaces
- **Installation and museum exhibits** requiring long-term stability
- **Radio production** with complex multi-track coordination

### What Sets OCC Apart

1. **Sovereign Ecosystem**
    - No cloud dependencies or third-party SaaS requirements
    - Self-contained audio processing without external agents
    - Open architecture built on MIT-licensed foundations
    - Local-first design with optional network features
2. **Professional-Grade Architecture**
    - Built on Orpheus SDK's deterministic audio core
    - Ultra-low-latency real-time playback (<5ms ASIO, <10ms WASAPI)
    - Sample-accurate timing and transport control
    - Crash-proof, production-tested reliability
3. **Cross-Platform Native Performance**
    - Windows (ASIO/WASAPI), macOS (CoreAudio), Linux (ALSA/JACK)
    - Native audio driver integration, no abstraction penalties
    - Platform-optimized threading and SIMD acceleration
    - Consistent behavior across operating systems
4. **Modern Workflow Design**
    - Intuitive waveform editing with established mouse logic
    - Configurable clip grid (10×12, 8 tabs, dual-view)
    - Unified bottom panel (Labelling, Editor, Routing, Preferences)
    - Real-time performance monitoring

***

## 2. Market Position

### Primary Market: Broadcast Audio Playout

**Target:** Radio stations, production houses, live broadcast environments

**Competes with:** Sigma SpotOn (€3,000-5,000), BSI WaveCart, Dalet

**Differentiation:**

- Modern UI/UX vs. dated SpotOn interface
- Cross-platform (Windows/macOS/Linux) vs. Windows-only
- Open architecture vs. proprietary lock-in
- Advanced waveform editor with AutoTrim, rehearsal mode
- iOS companion app for remote triggering
- Competitive pricing through sovereign development model

**SpotOn Insights to Adopt:**

- Robust clip triggering with instant response
- Track preview before assignment
- AutoPlay/jukebox mode for continuous playout
- Master/slave button linking for synchronized cues
- Fine-trim and AutoTrim functionality
- Rehearsal mode for practicing segments

### Secondary Market: Theater Sound Design

**Target:** Sound designers, theater technicians, touring productions

**Competes with:** QLab (macOS, $0-$999), SFX (€399-€799)

**Differentiation:**

- **Windows-native** (QLab is macOS-exclusive)
- Advanced audio mixing capabilities (vs. QLab's 128-channel limit)
- Recording directly into buttons (no external editor required)
- Real-time DSP (time-stretching, pitch-shifting)
- Professional driver integration (ASIO for Windows)
- Cross-platform session portability

### Tertiary Market: Installation & Museum

**Target:** Permanent installations, museums, theme parks, immersive experiences

**Competes with:** Merging Ovation (€8,000-€25,000+), d3/disguise (video-centric)

**Differentiation:**

- **Accessible pricing** vs. Ovation's enterprise cost
- Audio-first architecture (vs. video servers with basic audio)
- Simpler interaction model (grid-based vs. complex rules engine)
- Local processing (no MassCore hardware requirements)
- Modular extensibility for custom installations

**Ovation Insights to Consider (Long-Term):**

- Optional GPI triggering for legacy hardware integration
- Multi-instance synchronization for large venues
- Interaction rules for dynamic show logic (v2.0+)
- Spatial audio rendering (future expansion)

**Realistic Scoping:**

- OCC will **not** compete with Ovation's enterprise scale (384 channels, MassCore)
- Focus on **accessible professional quality** (16-32 channels, standard hardware)
- Offer **growth path** through modular architecture

***

## 3. Relationship to Orpheus SDK

### Co-Evolutionary Development Model

OCC and Orpheus SDK inform each other's evolution:

```javascript
┌─────────────────────────────────────────────────────────────┐
│                     ORPHEUS ECOSYSTEM                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────────────────────────────────────────────┐   │
│  │         ORPHEUS SDK (Core Library)                   │   │
│  │  • SessionGraph (tracks, clips, tempo)               │   │
│  │  • TransportController (sync, timecode)              │   │
│  │  • RoutingMatrix (multi-channel)                     │   │
│  │  • AudioFileReader (codec abstraction)               │   │
│  │  • PerformanceMonitor (diagnostics)                  │   │
│  │  • Interfaces: AudioProcessor, RemoteControl         │   │
│  └─────────────────────────────────────────────────────┘   │
│                           ▲                                   │
│                           │ uses                              │
│  ┌────────────────────────┴────────────────────────────┐   │
│  │         ORPHEUS ADAPTERS (Optional Modules)          │   │
│  │  • realtime_engine/ (CoreAudio, ASIO, WASAPI)       │   │
│  │  • clip_grid/ (ClipGridScheduler, FIFO choke)       │   │
│  │  • codecs/ (FLAC, MP3, OGG - optional LGPL)         │   │
│  │  • dsp/ (Rubber Band, VST3 hosting)                 │   │
│  │  • protocols/ (OSC, MIDI, WebSockets)               │   │
│  └──────────────────────────────────────────────────────┘   │
│                           ▲                                   │
│                           │ integrates                        │
│  ┌────────────────────────┴────────────────────────────┐   │
│  │   ORPHEUS CLIP COMPOSER (Flagship Application)       │   │
│  │  • JUCE or Electron UI framework                     │   │
│  │  • Clip triggering engine                            │   │
│  │  • Waveform editor with mouse logic                  │   │
│  │  • Recording system (InputAudioGraph)                │   │
│  │  • iOS companion app (remote control)                │   │
│  │  • Session management with app-specific metadata     │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
│  Future Applications:                                         │
│  • Orpheus DAW (multitrack editor)                           │
│  • Orpheus Stream (broadcast automation)                     │
│  • Orpheus Mixer (live console)                              │
│  • Orpheus Spatial (3D audio renderer)                       │
│                                                               │
└─────────────────────────────────────────────────────────────┘

```

### What OCC Takes from Orpheus SDK

- **SessionGraph** for managing clips, tempo, transport state
- **Deterministic rendering** for bounce/export operations
- **ABI negotiation** for plugin compatibility
- **Transport synchronization** with LTC/MTC/MIDI Clock
- **Routing matrix** for flexible input/output configuration
- **Performance monitoring** for real-time diagnostics

### What OCC Drives in Orpheus SDK Evolution

OCC's requirements push Orpheus SDK to add:

1. **Real-time playback mode** (not just offline rendering)
2. **Input audio graphs** (for recording, not just playback)
3. **Clip grid scheduler** (soundboard-specific logic)
4. **Plugin architecture** (DSP, codecs, protocols)
5. **Network protocol abstractions** (remote control)
6. **Session metadata extensions** (app-specific data)

### Design Principle: Modular Sovereignty

**Orpheus SDK remains host-neutral and minimal:**

- Core library provides foundational audio graph capabilities
- Optional adapters add real-time, DSP, networking
- Applications like OCC compose adapters as needed

**OCC remains self-contained:**

- Can be built and distributed independently
- Uses Orpheus SDK as a library dependency
- Extends SDK without modifying core
- Contributes improvements back to SDK

**Build flags enable flexibility:**

```javascript
# Minimal build (DAW plugin adapter)
ORPHEUS_BUILD_REALTIME_ENGINE=OFF
ORPHEUS_BUILD_CLIP_GRID=OFF

# Full build (OCC application)
ORPHEUS_BUILD_REALTIME_ENGINE=ON
ORPHEUS_BUILD_CLIP_GRID=ON
OCC_ENABLE_REMOTE_CONTROL=ON
OCC_ENABLE_RECORDING=ON

```

***

## 4. Core Principles

### Reliability Above All

**24/7 operational capability:**

- Crash-proof architecture with graceful error recovery
- Automatic session auto-save (incremental, non-blocking)
- Intelligent file recovery for missing media
- Real-time performance monitoring with threshold alerts
- Driver fallback mechanisms (ASIO → WASAPI on Windows)

**Zero-compromise audio quality:**

- Sample-accurate timing throughout signal chain
- Native 64-bit floating-point processing
- Support for any sample rate (44.1kHz to 192kHz+)
- No resampling unless explicitly configured
- Bit-transparent recording and playback

### Performance-First Architecture

**Ultra-low-latency design:**

- Native audio driver integration (CoreAudio, ASIO)
- Lock-free audio thread (no mutexes in hot path)
- SIMD-optimized processing where applicable
- Multi-threaded clip loading (background I/O)
- Ring buffer architecture for sample-accurate triggering

**Resource efficiency:**

- Memory-mapped file I/O for large audio files
- Efficient clip streaming (no full-file loading required)
- CPU-optimized DSP chains
- GPU acceleration for waveform rendering (optional)

### User Experience Excellence

**Intuitive by design:**

- Established mouse logic (L=IN, R=OUT, M=playhead)
- Keyboard shortcuts for all critical operations
- Visual feedback for all state changes
- Contextual help and tooltips
- Consistent behavior across platforms

**Configurable without complexity:**

- Sensible defaults for immediate use
- Progressive disclosure (simple → advanced)
- Templates for common workflows
- Preset management for routing/DSP

**Accessibility:**

- High-contrast mode for low-light environments
- Keyboard-only navigation
- Screen reader compatibility (JUCE Accessibility)
- Colorblind-friendly palettes

***

## 5. Technical Architecture

### Audio Engine Foundation

**Core audio path:**

```javascript
Hardware Input (optional)
    ↓
InputAudioGraph (recording)
    ↓
ClipGridScheduler (trigger logic)
    ↓
Clip Playback (multi-threaded streaming)
    ↓
DSP Chain (time-stretch, pitch-shift, EQ, dynamics)
    ↓
RoutingMatrix (group A/B/C/D → outputs)
    ↓
Mix Bus (per-group summing)
    ↓
Master Section (limiting, metering)
    ↓
Hardware Output (ASIO/CoreAudio/WASAPI)

```

**Performance targets:**

- Latency: <5ms (ASIO), <10ms (WASAPI), <8ms (CoreAudio)
- Polyphony: 16+ simultaneous clips per group
- Channel count: 16 outputs (4 groups × stereo + 2 audition + 6 spare)
- Input count: 6 channels (2 stereo record + 1 mono LTC + 1 spare)
- CPU usage: <30% on modern quad-core systems

### UI Framework Decision (To Be Finalized)

**Option A: JUCE (C++ Native)**

- Pros: Native performance, cross-platform consistency, mature
- Cons: C++ complexity, slower iteration, larger binary
- Best for: Maximum performance, professional feel

**Option B: Electron + Shmui (Web-Based)**

- Pros: Rapid development, leverage existing Shmui work, hot-reload
- Cons: Higher memory usage, potential latency concerns
- Best for: Faster MVP, modern UI/UX iteration

**Decision criteria:**

- Performance requirements (real-time audio critical)
- Development timeline (MVP in 6 months?)
- Team expertise (C++ vs. TypeScript/React)
- Long-term maintenance burden

**Recommendation (tentative):** JUCE for desktop app, React Native for iOS companion

### Data Architecture

**Session structure:**

```javascript
Session File (.occ format, JSON-based)
├── metadata (name, created, modified)
├── routing (group outputs, inputs)
├── transport (tempo, time signature, sync source)
├── clips[] (array of clip definitions)
│   ├── identity (uuid, name, group)
│   ├── audio (file_path, format, sample_rate)
│   ├── edit_points (trim_in, trim_out, cue_points[])
│   ├── dsp (fades, time_stretch, pitch_shift)
│   ├── ui (grid_position, span, color, label)
│   └── playback (loop, stop_others, master_of[])
└── ui_state (active_tab, bottom_panel, dual_view)

```

**File format principles:**

- Human-readable JSON (not binary)
- Relative file paths (portable sessions)
- Extensible metadata (app-specific fields)
- Backward-compatible versioning
- Optional sidecar files for large metadata

***

## 6. Target Users & Use Cases

### User Persona 1: Broadcast Engineer

**Profile:**

- Works in radio or TV production
- Needs instant clip triggering during live shows
- Requires 24/7 reliability
- Familiar with SpotOn or similar systems

**Key workflows:**

- Load morning show clips into grid tabs
- Trigger sound effects, beds, stingers on-air
- Record caller segments directly into buttons
- Export show logs for archival

**Must-have features:**

- Ultra-low-latency triggering
- Track preview before loading
- AutoPlay for continuous playout
- Detailed logging with timestamps

### User Persona 2: Theater Sound Designer

**Profile:**

- Designs sound for theatrical productions
- Currently uses QLab on macOS
- Needs precise cue timing and complex sequences
- Works on Windows laptops for touring

**Key workflows:**

- Build cue lists from rehearsal recordings
- Edit IN/OUT points with sample accuracy
- Create fade sequences between scenes
- Export stems for archival

**Must-have features:**

- Waveform editor with fine-trim
- Master/slave linking for coordinated cues
- MIDI/OSC triggering from lighting console
- Rehearsal mode for practicing segments

### User Persona 3: Installation Technician

**Profile:**

- Installs audio systems in museums, exhibits
- Needs long-term stability (months/years)
- May use external triggers (GPI, sensors)
- Limited on-site maintenance access

**Key workflows:**

- Set up multi-zone audio playout
- Configure external triggers (motion sensors)
- Test system reliability over weeks
- Remote monitoring via network

**Must-have features:**

- 24/7 operational stability
- Auto-restart on power loss
- Remote diagnostics via iOS app
- Optional GPI triggering (v2.0)

### User Persona 4: Live Performance Musician

**Profile:**

- Uses backing tracks and loops live
- Needs tactile, responsive control
- Integrates with MIDI controllers
- Requires multi-channel routing (IEMs, FOH)

**Key workflows:**

- Trigger backing tracks in sync with performance
- Time-stretch clips to match live tempo
- Route click track to IEMs only
- Record rehearsals for review

**Must-have features:**

- MIDI controller mapping
- Real-time time-stretching
- Flexible routing (4 groups + audition)
- iOS companion for remote control

***

## 7. Success Metrics

### Technical Metrics

**Reliability:**

- Mean time between crashes: >100 hours continuous operation
- Session load success rate: >99.9%
- File recovery success rate: >95%

**Performance:**

- Audio latency: <5ms (ASIO), <10ms (WASAPI)
- UI responsiveness: <16ms frame time (60fps)
- Clip trigger latency: <5ms from MIDI input to audio output
- CPU usage: <30% with 16 simultaneous clips

**Quality:**

- Audio artifacts: None detectable in A/B testing
- Sample-accurate timing: <1ms deviation over 1 hour
- Bit-transparent recording: Verified via null test

### User Experience Metrics

**Ease of use:**

- New user onboarding: <15 minutes to first clip trigger
- Waveform edit operation: <30 seconds average
- Session save/load time: <2 seconds for 100-clip session

**Feature adoption:**

- Recording to buttons: >60% of users
- Remote control (iOS): >40% of users
- AutoTrim feature: >50% of users
- Master/slave linking: >25% of users

### Market Adoption Metrics

**Target (Year 1):**

- 500 active users
- 50 broadcast installations
- 100 theater productions
- 20 permanent installations

**Target (Year 3):**

- 2,000 active users
- 200 broadcast installations
- 500 theater productions
- 100 permanent installations
- 5 enterprise installations (multi-room, multi-instance)

***

## 8. Development Roadmap

### MVP (Months 1-6)

**Goal:** Functional soundboard for basic broadcast/theater use

**Core features:**

- Clip triggering (10×12 grid, 8 tabs)
- 4 clip groups with routing (A/B/C/D)
- Waveform editor with mouse logic (L/R/M click)
- Basic fade in/out
- Session save/load
- CoreAudio (macOS) / ASIO (Windows) integration
- Performance monitoring

**Deliverables:**

- Functional desktop application
- Basic documentation
- Installation packages (DMG, MSI)

### v1.0 (Months 7-12)

**Goal:** Professional feature parity with entry-level competitors

**Added features:**

- Recording directly into buttons
- iOS companion app (remote control)
- Advanced waveform editor (AutoTrim, cue points, rehearsal mode)
- Master/slave button linking
- Track preview before loading
- MIDI/OSC control mapping
- Detailed logging with CSV/JSON export

**Deliverables:**

- Production-ready release
- Comprehensive documentation
- Tutorial videos
- iOS app on TestFlight/App Store

### v2.0 (Months 13-18)

**Goal:** Advanced features for demanding professional use

**Added features:**

- AutoPlay/jukebox mode
- GPI external triggering (optional)
- Interaction rules engine (Ovation-inspired, simplified)
- 3D spatial audio (basic, stereo-to-surround)
- Advanced routing matrix (dynamic re-patching)
- Plugin architecture (VST3 hosting)

**Deliverables:**

- Advanced professional release
- Training materials for enterprise users
- Integration guides (lighting consoles, automation)

### Future / Enterprise (18+ Months)

**Potential features:**

- Multi-instance synchronization (large venues)
- RAVENNA/AES67 networking
- Redundancy/failover modes
- Hardware controller integration (dedicated surfaces)
- Web-based remote control (browser access)
- AI-powered auto-tagging and organization

***

## 9. Competitive Advantages

### vs. Sigma SpotOn

| **Aspect** | **SpotOn** | **OCC** |
| --- | --- | --- |
| Platform | Windows only | Windows, macOS, Linux |
| UI/UX | Dated (late 1990s design) | Modern, configurable |
| Audio drivers | DirectSound, ASIO | ASIO (primary), WASAPI, CoreAudio |
| Extensibility | Closed, proprietary | Open architecture, plugin support |
| Remote control | Limited, RS-232 focus | iOS app, WebSockets, OSC, MIDI |
| Price | €3,000-5,000 | Target: €500-1,500 (open pricing model) |
| Development | Mature but stagnant | Active, community-driven |

**Why users would switch:**

- Cross-platform flexibility (macOS users can't use SpotOn)
- Modern interface with touchscreen support
- iOS companion app (SpotOn has none)
- Open architecture for custom integration
- Lower cost for small studios

### vs. QLab

| **Aspect** | **QLab** | **OCC** |
| --- | --- | --- |
| Platform | macOS only | Windows, macOS, Linux |
| Channel limit | 128 (Audio license) | Unlimited (hardware-dependent) |
| Audio drivers | CoreAudio only | ASIO, WASAPI, CoreAudio |
| Recording | External (requires separate app) | Built-in, direct to buttons |
| Windows users | Cannot use | Native Windows support |
| Price | $0-$999 | Target: €500-1,500 |
| DSP | Basic | Advanced (time-stretch, pitch-shift) |

**Why users would switch:**

- **Windows support** (huge unmet market)
- Advanced audio processing capabilities
- Recording without external tools
- Professional driver support (ASIO)
- No channel limitations

### vs. Merging Ovation

| **Aspect** | **Ovation** | **OCC** |
| --- | --- | --- |
| Price | €8,000-25,000+ | €500-1,500 |
| Target | Enterprise installations | Small studios to mid-size venues |
| Complexity | High (interaction rules, MassCore) | Moderate (intuitive grid-based) |
| Hardware | MassCore required for full features | Standard computers |
| Channel count | 384 (MassCore) | 16-32 (expandable) |
| Setup time | Days/weeks | Hours |

**Why users would choose OCC:**

- **Accessible pricing** (10-20x cheaper)
- Simpler learning curve
- No specialized hardware required
- Faster deployment
- Suitable for 90% of use cases (non-enterprise)

**When Ovation is better:**

- Massive installations (384 channels, multi-room sync)
- Complex interaction rules (theme parks)
- Mission-critical redundancy (Dubai Expo-level)
- Enterprise budget available

***

## 10. Philosophical Commitments

### Sovereign Ecosystem

**What this means:**

- No mandatory cloud services or online activation
- Local-first architecture (network features optional)
- No telemetry or analytics without explicit user consent
- Open data formats (human-readable JSON sessions)
- Plugin architecture for extensibility

**Why this matters:**

- Broadcast environments often air-gapped for security
- Permanent installations need long-term stability
- Users own their data and workflows
- No vendor lock-in or subscription dependencies

### Professional-Grade, Accessible Pricing

**Commitment:**

- Target €500-1,500 pricing (vs. €3,000+ competitors)
- Open-source foundation (Orpheus SDK MIT licensed)
- Optional paid features (enterprise sync, advanced DSP)
- No per-channel or per-output licensing

**Business model (tentative):**

- Core OCC: One-time purchase or modest subscription
- iOS app: Included or separate low-cost purchase
- Enterprise features: Licensed separately
- Support contracts: Optional for installations

### Community-Driven Development

**Commitment:**

- Public roadmap with user input
- Open issue tracker for bug reports and feature requests
- Documentation contributions welcome
- Third-party plugin ecosystem encouraged

**Governance:**

- Orpheus SDK: Open-source, community-governed
- OCC: Proprietary UI/app, but extensible
- Clear separation between core and application

***

## 11. Open Questions for Resolution

### Technical Decisions

1. **UI Framework: JUCE vs. Electron?**
    - Impact: Development speed vs. performance
    - Timeline: Decide by Month 1
2. **DSP Library: Rubber Band vs. SoundTouch vs. Sonic?**
    - Impact: Quality vs. licensing (GPL vs. LGPL vs. Apache)
    - Timeline: Decide by Month 2
3. **Session Format: JSON vs. Custom Binary?**
    - Impact: Human-readable vs. compact/fast
    - Recommendation: JSON for MVP, binary optional later
4. **Plugin Architecture: VST3 only or also AU/LV2?**
    - Impact: Cross-platform support vs. development effort
    - Timeline: v2.0 decision, not MVP

### Product Decisions

5. **Pricing Model: One-time vs. Subscription?**
    - Impact: Revenue stability vs. user preference
    - Research: Survey potential users
6. **Open-Source Strategy: Fully open vs. Dual-license?**
    - Impact: Community adoption vs. monetization
    - Consider: Core SDK open (MIT), OCC app proprietary
7. **Enterprise Features: Separate SKU or Modular Add-ons?**
    - Impact: Product complexity vs. flexibility
    - Recommendation: Modular (buy sync, GPI, etc. separately)

### Strategic Decisions

8. **Launch Platform: macOS, Windows, or Both?**
    - Impact: Development focus, market entry speed
    - Recommendation: Windows first (underserved vs. QLab dominance)
9. **Partnership Strategy: Integration with Lighting/Video?**
    - Impact: Market reach, technical complexity
    - Timeline: Post-v1.0 consideration
10. **Certification/Standards: EBU R128, Dolby Atmos?**
    - Impact: Professional credibility, development cost
    - Timeline: v2.0+ for advanced standards

***

## 12. Conclusion

Orpheus Clip Composer represents a strategic opportunity to establish a sovereign, professional-grade audio tooling ecosystem. By building OCC as the flagship application, we simultaneously:

1. **Deliver immediate value** to broadcast, theater, and performance users
2. **Drive Orpheus SDK evolution** through real-world requirements
3. **Establish architectural patterns** for future ecosystem applications
4. **Prove market viability** of sovereign audio tools

**Success requires:**

- Clear technical architecture (OCC as application, not thin adapter)
- Ruthless feature prioritization (MVP → v1.0 → v2.0)
- Co-evolutionary SDK development (real-time engine, codecs, protocols)
- Professional reliability (24/7 operational stability)
- User-focused design (intuitive, accessible, powerful)

**Next steps:**

- Formalize data models (OCC022: Clip Metadata, OCC023: Session Metadata)
- Draft Orpheus SDK evolution plan (identify required capabilities)
- Make UI framework decision (JUCE vs. Electron)
- Create architectural specification (component diagram, data flow)
- Define Milestone 1 deliverables (minimal functional prototype)

***

**Document Control:**

- **Author:** Product Design Team
- **Review Cycle:** Quarterly (or upon major architectural changes)
- **Related Documents:**
    - OCC019: Comprehensive AI Agent Prompt (operational guidance)
    - OCC013-017: Audio Drivers, Security, Deployment (technical details)
    - SPT020-023: SpotOn Analysis (competitive insights)
    - MOV001: Ovation Analysis (enterprise features reference)
    - AGENTS.md: Orpheus SDK Coding Guidelines (architectural boundaries)

***

*This vision document establishes the foundation for Orpheus Clip Composer. All subsequent technical decisions, feature prioritizations, and architectural choices should align with the principles and goals outlined herein.*