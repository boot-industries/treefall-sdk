# Orpheus Clip Composer - Design Documentation

**Professional Soundboard Application for Broadcast, Theater, and Live Performance**

This directory contains the complete product design, architecture, and planning documents for **Orpheus Clip Composer** (OCC), the flagship application of the Orpheus audio ecosystem.

---

## Quick Start

**New to OCC?** Start here:
1. **[OCC021 - Product Vision](OCC021%20Orpheus%20Clip%20Composer%20-%20Product%20Vision.md)** - Strategic direction, market positioning, user personas
2. **[OCC023 - Component Architecture](OCC023%20Component%20Architecture%20v1.0.md)** - System design and component relationships
3. **[OCC011 - Wireframes v2](OCC011%20Wireframes%20v2.md)** - UI layouts and visual design

**Technical Implementation?** See:
- **[OCC022 - Clip Metadata Schema](OCC022%20Clip%20Metadata%20Schema%20v1.0.md)** - Data model for clips
- **[OCC027 - API Contracts](OCC027%20API%20Contracts%20-%20OCC%20to%20SDK%20Interfaces%20v1.0.md)** - C++ interfaces (OCC ↔ SDK)
- **[OCC009 - Session Metadata Manifest](OCC009%20Session%20Metadata%20Manifest.md)** - Session-level data

**Planning & Roadmap?** See:
- **[OCC026 - Milestone 1 MVP](OCC026%20Milestone%201%20-%20MVP%20Definition%20v1.0.md)** - 6-month MVP plan with deliverables
- **[OCC025 - UI Framework Decision](OCC025%20UI%20Framework%20Decision%20-%20JUCE%20vs%20Electron%20v1.0.md)** - JUCE vs Electron analysis
- **[OCC028 - DSP Library Evaluation](OCC028%20DSP%20Library%20Evaluation%20-%20Time-Stretch%20and%20Pitch-Shift%20v1.0.md)** - Rubber Band vs SoundTouch
- **[OCC029 - SDK Enhancement Recommendations](OCC029%20SDK%20Enhancement%20Recommendations%20for%20Clip%20Composer%20v1.0.md)** - Gap analysis and 5 critical modules

**Platform-Specific Details?** See:
- **[OCC013 - Audio Driver Integration](OCC013%20Advanced%20Audio%20Driver%20Integration%20for%20a%20Professional%20Soundboard%20Application.md)** - CoreAudio, ASIO, WASAPI
- **[OCC015 - Apple Silicon vs Intel](OCC015%20Appendix_%20Best%20Practices%20for%20Apple%20Silicon%20vs_%20Intel%20Chips.md)** - macOS deployment
- **[OCC016 - Windows Deployment](OCC016%20Appendix_%20Best%20Practices%20for%20Deploying%20on%20Windows%20Platforms.md)** - Windows best practices

---

## Document Index

### Authoritative Documents

| Document | Status | Description |
|----------|--------|-------------|
| **[OCC021](OCC021%20Orpheus%20Clip%20Composer%20-%20Product%20Vision.md)** | **AUTHORITATIVE** | Product vision, market analysis, strategic direction |
| **[CLAUDE.md](CLAUDE.md)** | **AUTHORITATIVE** | Design documentation governance and guidelines |

### Core Design Specifications (Current)

| Document | Version | Description |
|----------|---------|-------------|
| **[OCC022](OCC022%20Clip%20Metadata%20Schema%20v1.0.md)** | v1.0 (Draft) | Detailed clip metadata schema (JSON) |
| **[OCC023](OCC023%20Component%20Architecture%20v1.0.md)** | v1.0 (Draft) | Component architecture, threading model, data flow |
| **[OCC024](OCC024%20User%20Interaction%20Flows%20v1.0.md)** | v1.0 (Draft) | User workflows for key personas and use cases |
| **[OCC025](OCC025%20UI%20Framework%20Decision%20-%20JUCE%20vs%20Electron%20v1.0.md)** | v1.0 (Draft) | UI framework decision analysis (JUCE vs Electron) |
| **[OCC026](OCC026%20Milestone%201%20-%20MVP%20Definition%20v1.0.md)** | v1.0 (Draft) | MVP feature scope, timeline, acceptance criteria |
| **[OCC027](OCC027%20API%20Contracts%20-%20OCC%20to%20SDK%20Interfaces%20v1.0.md)** | v1.0 (Draft) | C++ interfaces between OCC and Orpheus SDK |
| **[OCC028](OCC028%20DSP%20Library%20Evaluation%20-%20Time-Stretch%20and%20Pitch-Shift%20v1.0.md)** | v1.0 (Draft) | Rubber Band vs SoundTouch vs Sonic comparison |
| **[OCC029](OCC029%20SDK%20Enhancement%20Recommendations%20for%20Clip%20Composer%20v1.0.md)** | v1.0 (Draft) | Gap analysis, 5 critical SDK modules, timeline alignment |
| **[OCC019](OCC019%20Cursor%20Agent%20Prompt%20v10.md)** | v10 | Comprehensive technical specification for AI agents |
| **[OCC011](OCC011%20Wireframes%20v2.md)** | v2 | Latest wireframes (single/dual tab view, bottom panels) |
| **[OCC009](OCC009%20Session%20Metadata%20Manifest.md)** | v1 | Session-level metadata schema |
| **[OCC013](OCC013%20Advanced%20Audio%20Driver%20Integration%20for%20a%20Professional%20Soundboard%20Application.md)** | v1 | Audio driver integration (CoreAudio, ASIO, WASAPI) |

### Platform-Specific Guides

| Document | Description |
|----------|-------------|
| **[OCC015](OCC015%20Appendix_%20Best%20Practices%20for%20Apple%20Silicon%20vs_%20Intel%20Chips.md)** | Apple Silicon vs Intel deployment |
| **[OCC016](OCC016%20Appendix_%20Best%20Practices%20for%20Deploying%20on%20Windows%20Platforms.md)** | Windows deployment best practices |
| **[OCC017](OCC017%20Security%20Best%20Practices%20for%20a%20Professional%20Soundboard%20Application.md)** | Security guidelines |

### Historical Versions (Superseded)

| Document | Status | Notes |
|----------|--------|-------|
| OCC001-OCC008 | Superseded | Early cursor agent prompts and wireframes |
| OCC010 | Superseded | Wireframes v1 (see OCC011 for latest) |
| OCC012, OCC014 | Superseded | Older cursor agent prompts |
| OCC018 | Superseded | Cursor Rules v2 (JSON format) |

---

## Competitive Analysis

### Primary Inspirations (SpotOn & Ovation)

| Document | Product | Focus |
|----------|---------|-------|
| **[SPT023](peers/SPT023%20Comprehensive%20SpotOn%20Report_%20Functional%20Enhancements%20&%20Workflow%20Insights.md)** | Sigma SpotOn | Broadcast workflows, AutoTrim, master/slave linking |
| **[SPT020-022](peers/)** | SpotOn (various) | Feature analysis and edit menu deep-dive |
| **[MOV001](peers/MOV001%20Merging%20Ovation%20Media%20Server.md)** | Merging Ovation | Enterprise show control, interaction rules, MassCore |

### Reference Analysis

| Document | Product | Focus |
|----------|---------|-------|
| **[IZT001](peers/IZT001%20Analysis%20of%20iZotope%20RX%20and%20Spectrographic%20Processing%20Systems.md)** | iZotope RX | Spectral editing, professional audio processing |
| **[APT001-005](peers/)** | Pro Tools / REAPER | DAW capabilities, SDK architecture |
| **[ELV001](peers/ELV001%20ElevenLabs%20Audio%20Intelligence%20Platform.md)** | ElevenLabs | AI audio processing (future inspiration) |

---

## Product Overview

### What is Orpheus Clip Composer?

**OCC is a professional audio playout system** designed for environments where reliability, ultra-low-latency, and sample-accurate timing are non-negotiable:

- **Broadcast studios** - 24/7 reliability with instant clip triggering
- **Theater sound designers** - Precise cue management and show control
- **Live performance operators** - Responsive, tactile control surfaces
- **Installation and museum exhibits** - Long-term stability
- **Radio production** - Complex multi-track coordination

### Key Differentiators

1. **Sovereign Ecosystem** - No cloud dependencies, local-first design
2. **Professional-Grade Architecture** - Built on Orpheus SDK's deterministic audio core
3. **Cross-Platform Native Performance** - Windows, macOS, Linux
4. **Modern Workflow Design** - Intuitive waveform editing, configurable grid, unified bottom panel

### Market Position

**Primary Market:** Broadcast audio playout (competes with Sigma SpotOn €3k-5k)
**Secondary Market:** Theater sound design (competes with QLab $0-$999, SFX €399-€799)
**Tertiary Market:** Installation & museum (competes with Merging Ovation €8k-25k+)

**Target Pricing:** €500-€1,500 (accessible professional quality)

---

## Technical Architecture

### Relationship to Orpheus SDK

**OCC is an APPLICATION, not a thin adapter:**
- Uses Orpheus SDK as audio engine foundation
- Extends SDK with real-time playback, UI, recording, remote control
- Drives SDK evolution through real-world requirements

**What OCC Uses from SDK:**
- `SessionGraph` - Tracks, clips, tempo, transport state
- `TransportController` - Sample-accurate playback control
- `RoutingMatrix` - Multi-channel routing
- `AudioFileReader` - Multi-format decoding
- `PerformanceMonitor` - Real-time diagnostics

**What OCC Implements:**
- Clip grid UI and triggering logic
- Waveform editor with mouse/touchscreen controls
- Direct-to-button recording
- Session management (JSON save/load)
- Remote control (iOS app, WebSocket, OSC, MIDI)
- Logging and cloud sync

### Data Model

**Session File:** JSON format (.occ extension)
- Human-readable for portability
- Relative file paths for session migration
- Extensible metadata structure

**Clip Metadata:** Comprehensive schema (OCC022)
- Identity (UUID, name, timestamps)
- Audio file reference (path, format, sample rate, hash)
- Edit points (trim IN/OUT, cue points, fades)
- DSP (time-stretch, pitch-shift, normalization)
- Playback (clip group, routing, loop, FIFO choke)
- Linking (master/slave, AutoPlay)
- UI (grid position, color, label)
- Recording metadata (timestamp, LTC, operator)

---

## Development Roadmap

### MVP (Months 1-6)
- Core audio engine (Orpheus SDK integration)
- Clip triggering (10×12 grid, 8 tabs)
- Basic waveform editor
- 4 Clip Groups with routing
- Session save/load
- CoreAudio/ASIO integration

### v1.0 (Months 7-12)
- Recording directly into buttons
- iOS companion app (remote control)
- Advanced editor (AutoTrim, cue points, rehearsal mode)
- Master/slave linking
- Track preview before loading
- MIDI/OSC control
- Logging with CSV/JSON export

### v2.0 (Months 13-18)
- AutoPlay/jukebox mode
- GPI external triggering (optional)
- Interaction rules engine (Ovation-inspired)
- 3D spatial audio (basic)
- VST3 plugin hosting

---

## Design Principles

### Reliability Above All
- 24/7 operational capability
- Crash-proof architecture with graceful error recovery
- Automatic session auto-save (incremental, non-blocking)
- Intelligent file recovery for missing media

### Performance-First Architecture
- Ultra-low-latency (<5ms ASIO, <10ms WASAPI)
- Lock-free audio thread (no mutexes in hot path)
- Sample-accurate timing throughout signal chain
- Multi-threaded clip loading

### User Experience Excellence
- Intuitive by design (established mouse logic)
- Keyboard shortcuts for all critical operations
- Visual feedback for all state changes
- Progressive disclosure (simple → advanced)

### Sovereign Ecosystem
- No cloud dependencies or SaaS requirements
- Local-first design with optional network features
- Open architecture (MIT-licensed SDK)
- Human-readable data formats

---

## Key Questions & Decisions

### Resolved
- ✅ **Product Vision** - Defined in OCC021
- ✅ **Data Model** - Clip schema (OCC022), Session schema (OCC009)
- ✅ **Component Architecture** - Defined in OCC023
- ✅ **User Flows** - Documented in OCC024

### Pending Decision (Critical)
- ❓ **UI Framework** - JUCE vs Electron? (Analysis in OCC025, decision by Nov 1, 2025)

### Open Questions (From OCC021)

**Technical:**
- DSP Library (Rubber Band vs SoundTouch vs Sonic)
- Session Format (JSON confirmed for MVP, binary optional later)
- Plugin Architecture (VST3 only or also AU/LV2) - v2.0 decision

**Product:**
- Pricing Model (One-time vs Subscription)
- Open-Source Strategy (Fully open vs Dual-license)
- Enterprise Features (Separate SKU vs Modular)

**Strategic:**
- Launch Platform (macOS, Windows, or Both) - Rec: Windows first
- Partnership Strategy (Integration with lighting/video)
- Certification/Standards (EBU R128, Dolby Atmos) - v2.0+

---

## Success Metrics

### Technical
- Audio latency: <5ms (ASIO), <10ms (WASAPI)
- Mean time between crashes: >100 hours continuous operation
- UI responsiveness: <16ms frame time (60fps)
- CPU usage: <30% with 16 simultaneous clips

### User Experience
- New user onboarding: <15 minutes to first clip trigger
- Session save/load time: <2 seconds for 100-clip session
- Feature adoption: Recording to buttons >60%, Remote control >40%

### Market Adoption (Year 1)
- 500 active users
- 50 broadcast installations
- 100 theater productions
- 20 permanent installations

---

## Working with This Documentation

### For AI Assistants
See **[CLAUDE.md](CLAUDE.md)** for governance guidelines, document templates, and design iteration principles.

### For Developers
1. Start with **OCC021** (Product Vision) to understand strategic direction
2. Review **OCC023** (Component Architecture) for system design
3. Reference **OCC022** (Clip Metadata Schema) for data structures
4. Consult **OCC024** (User Interaction Flows) for UX requirements
5. Check **OCC025** (UI Framework Decision) for platform choices

### For Product Managers
- **OCC021** - Market positioning, competitive analysis, user personas
- **Competitive Analysis** (peers/) - SpotOn, Ovation, QLab, Pro Tools
- **Development Roadmap** (OCC021 Section 8) - MVP, v1.0, v2.0 timelines

### For Designers
- **OCC011** - Wireframes (grid layout, bottom panels)
- **OCC024** - User interaction flows (detailed workflows)
- **SPT023** - SpotOn feature analysis (interaction patterns)

---

## Related Documentation

### Orpheus SDK
- **[/docs/ARCHITECTURE.md](/docs/ARCHITECTURE.md)** - SDK architectural principles
- **[/docs/ROADMAP.md](/docs/ROADMAP.md)** - SDK development timeline
- **[/docs/AGENTS.md](/docs/AGENTS.md)** - SDK coding guidelines for AI assistants
- **[/docs/ORP/](/docs/ORP/)** - SDK implementation plans (ORP065-068)

### Other Orpheus Applications
- Orpheus Wave Finder - Harmonic calculator and frequency scope
- Orpheus FX Engine - LLM-powered effects processing
- Orpheus DAW - Multitrack editor (future)
- Orpheus Stream - Broadcast automation (future)

---

## Document Status

**Last Updated:** October 12, 2025
**Authoritative Version:** OCC021 (Product Vision)
**Current Phase:** Design iteration (MVP planning)

**Recent Additions (Oct 12, 2025):**
- ✨ OCC022 - Clip Metadata Schema v1.0 (Draft)
- ✨ OCC023 - Component Architecture v1.0 (Draft)
- ✨ OCC024 - User Interaction Flows v1.0 (Draft)
- ✨ OCC025 - UI Framework Decision v1.0 (Draft)
- ✨ OCC026 - Milestone 1 MVP Definition v1.0 (Draft)
- ✨ OCC027 - API Contracts (OCC ↔ SDK) v1.0 (Draft)
- ✨ OCC028 - DSP Library Evaluation v1.0 (Draft)
- ✨ OCC029 - SDK Enhancement Recommendations v1.0 (Draft)
- ✨ CLAUDE.md - Design documentation governance
- ✨ README.md - Complete documentation index

**Next Steps:**
1. Review and approve design documents (OCC022-029)
2. Finalize UI framework decision (JUCE recommended, decide by Nov 1, 2025)
3. Finalize DSP library decision (Rubber Band recommended for v1.0)
4. SDK team: Implement 5 critical modules from OCC029 (Months 1-6)
5. OCC team: Begin Month 1 development (foundation & audio engine) using OCC027 stubs

---

## Contact & Contribution

**Project Lead:** Chris Lyons
**Repository:** https://github.com/orpheus-audio/orpheus-sdk (future)
**Documentation Issues:** Open GitHub issue with `[OCC]` prefix

**Contributing:**
- All design documents must align with OCC021 (Product Vision)
- Follow governance guidelines in CLAUDE.md
- Use document templates for consistency
- Mark superseded documents explicitly

---

**Remember:** OCC is infrastructure for professionals who depend on reliability, performance, and quality. Design with 10+ years of stability in mind.
