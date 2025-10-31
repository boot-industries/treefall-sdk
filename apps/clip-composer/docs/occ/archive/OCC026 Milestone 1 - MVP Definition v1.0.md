# OCC026 Milestone 1 - MVP Definition v1.0

**Document Version:** 1.0
**Date:** October 12, 2025
**Status:** Draft
**Target Completion:** Month 6 (April 2025)
**Supersedes:** None

---

## Executive Summary

**Milestone 1 delivers a functional soundboard application** suitable for testing by early adopters in broadcast and theater environments. This MVP focuses on core audio playback, basic editing, and session management—establishing the foundation for v1.0 feature expansion.

**Key Principle:** Ship the minimum feature set that delivers real value to users while validating core technical assumptions (JUCE performance, Orpheus SDK integration, cross-platform audio drivers).

**Success Definition:** 10 beta users (5 broadcast, 5 theater) successfully use OCC for non-critical live work and provide feedback for v1.0 refinement.

---

## 1. Milestone 1 Feature Scope

### 1.1 Core Features (MUST HAVE)

**Audio Playback Engine**

- ✅ Trigger clips from grid buttons (click or keyboard)
- ✅ Simultaneous playback (up to 16 clips per Clip Group)
- ✅ FIFO choke mode (one clip at a time per group, configurable)
- ✅ Sample-accurate timing (<5ms latency on ASIO)
- ✅ Supported formats: WAV, AIFF, FLAC (primary formats only)

**Clip Grid Interface**

- ✅ 10×12 button grid layout
- ✅ 8 tabs for organizing clips
- ✅ Single tab view (dual-tab view deferred to v1.0)
- ✅ Basic button appearance (color, label, waveform thumbnail)
- ✅ Visual feedback (playing state, button highlighting)

**Waveform Editor (Bottom Panel)**

- ✅ Waveform visualization with zoom/pan
- ✅ Mouse logic: Left-click = IN, Right-click = OUT, Middle-click = playhead jump
- ✅ Basic transport controls (play, pause, stop)
- ✅ Trim point editing (sample-accurate)
- ✅ Fade in/out with linear curve only
- ✅ IN/OUT time entry fields (seconds display)

**Clip Groups & Routing**

- ✅ 4 Clip Groups (A, B, C, D)
- ✅ Default routing:
  - Group A → Outputs 1-2
  - Group B → Outputs 3-4
  - Group C → Outputs 5-6
  - Group D → Outputs 7-8
  - Audition → Outputs 9-10 (fixed)
- ✅ Routing configurable via Routing panel (Outputs tab only)

**Session Management**

- ✅ Save/load sessions (.occ JSON format)
- ✅ Relative file paths (session-portable)
- ✅ Manual auto-save (user-triggered, not automatic)
- ✅ New session creation with templates (Blank only)

**Platform Support**

- ✅ macOS (CoreAudio)
- ✅ Windows (ASIO primary, WASAPI fallback)
- ✅ Linux support deferred to v1.0

**Performance Monitoring**

- ✅ Real-time CPU meter
- ✅ Audio latency display
- ✅ Dropout/underrun detection

### 1.2 Excluded from MVP (Deferred to v1.0+)

**Deferred to v1.0:**

- ❌ Recording directly into buttons
- ❌ iOS companion app
- ❌ Master/slave linking
- ❌ AutoPlay/sequential playback
- ❌ AutoTrim function
- ❌ Cue points (Control+click markers)
- ❌ Rehearsal mode
- ❌ Track preview before loading
- ❌ Advanced fade curves (exponential, logarithmic, s-curve)
- ❌ DSP (time-stretch, pitch-shift, normalization)
- ❌ Remote control (WebSocket, OSC, MIDI)
- ❌ Logging and export
- ❌ Cloud sync
- ❌ Dual-tab view
- ❌ Button stretching (up to 4 cells)
- ❌ MP3, AAC, OGG support (secondary formats)
- ❌ Intelligent file recovery (hash-based search)
- ❌ Routing panel Inputs tab
- ❌ Preferences panel (use system defaults)
- ❌ Clip Labelling panel (edit via right-click menu only)

**Deferred to v2.0+:**

- ❌ GPI external triggering
- ❌ Interaction rules engine
- ❌ VST3 plugin hosting
- ❌ 3D spatial audio
- ❌ Linux support

---

## 2. MVP User Stories

### 2.1 Primary User Story (Broadcast Engineer)

**As a** broadcast engineer testing a new soundboard system,
**I want to** load audio clips into a grid, organize them across tabs, trigger them with low latency, and save my session,
**So that** I can evaluate if OCC is suitable for replacing our aging SpotOn system.

**Acceptance Criteria:**

1. Can load 50+ clips (WAV files) into grid across 4 tabs
2. Can trigger clips with <5ms latency (ASIO on Windows)
3. FIFO choke prevents overlapping clips in same group
4. Can trim IN/OUT points with sample accuracy
5. Session saves/loads reliably, preserving all clip metadata
6. CPU usage <30% with 16 simultaneous clips playing
7. No audio dropouts during 1-hour continuous operation

### 2.2 Secondary User Story (Theater Sound Designer)

**As a** theater sound designer preparing for a small production,
**I want to** organize sound cues in a visual grid, edit trim points precisely, and route different cue types to separate outputs,
**So that** I can build a show file and test if OCC meets my needs before committing to it.

**Acceptance Criteria:**

1. Can organize 30-40 cues across 3 tabs (Preshow, Act 1, Act 2)
2. Can edit trim points to sample accuracy for precise cue timing
3. Can assign cues to different Clip Groups (music vs. effects)
4. Can route music to outputs 1-2, effects to outputs 3-4
5. Session saves reliably for rehearsal continuity
6. Waveform editor provides clear visual feedback

---

## 3. Technical Requirements

### 3.1 Performance Targets

| Metric                     | Target           | Measurement Method                            |
| -------------------------- | ---------------- | --------------------------------------------- |
| **Audio Latency (ASIO)**   | <5ms             | Round-trip measurement (input → output)       |
| **Audio Latency (WASAPI)** | <10ms            | Round-trip measurement                        |
| **UI Responsiveness**      | <16ms frame time | 60fps sustained during playback               |
| **Clip Trigger Latency**   | <5ms             | Button click to audio output start            |
| **CPU Usage (16 clips)**   | <30%             | macOS Activity Monitor / Windows Task Manager |
| **Memory Footprint**       | <200 MB          | Typical session (100 clips loaded)            |
| **Session Load Time**      | <2s              | 100-clip session on SSD                       |
| **Session Save Time**      | <1s              | Non-blocking save                             |
| **Stability**              | >4 hours         | Continuous operation without crash            |

### 3.2 Audio Driver Requirements

**macOS:**

- CoreAudio integration via JUCE
- Buffer size: 128-512 samples (configurable)
- Sample rates: 44.1kHz, 48kHz, 96kHz
- Aggregated device support
- Device hotplug handling (graceful degradation)

**Windows:**

- ASIO drivers (primary)
  - ASIO SDK integration
  - Buffer size: 64-512 samples
  - Sample rates: 44.1kHz, 48kHz, 96kHz
- WASAPI fallback (exclusive mode)
  - Buffer size: 256-1024 samples
  - Automatic fallback if ASIO unavailable

**Testing Coverage:**

- macOS: Built-in audio, Focusrite Scarlett, RME Babyface
- Windows: Built-in audio (WASAPI), Focusrite ASIO, RME ASIO

### 3.3 Orpheus SDK Dependencies

**Required SDK Components:**

- `SessionGraph` - Manages clips, tempo, transport
- `TransportController` - Playback control
- `RoutingMatrix` - Multi-channel routing
- `AudioFileReader` - WAV/AIFF/FLAC decoding
- `PerformanceMonitor` - CPU/latency metrics

**SDK Assumptions:**

- Real-time playback mode available (not just offline rendering)
- Streaming playback (no full-file loading)
- Sample-accurate transport control
- Thread-safe API for UI → Audio communication

**If SDK Not Ready:**

- Develop stub interfaces matching OCC027 API contracts
- Build MVP against stubs
- Integrate real SDK when available (parallel development strategy)

---

## 4. Acceptance Criteria

### 4.1 Functional Acceptance

**Must Pass All:**

1. ✅ **Clip Loading:** Load 100+ clips without crash
2. ✅ **Playback:** Trigger and stop clips reliably
3. ✅ **FIFO Choke:** Only one clip plays per group (when enabled)
4. ✅ **Editing:** Trim IN/OUT points, apply fades, changes persist
5. ✅ **Routing:** Clips route to correct outputs per group assignment
6. ✅ **Session:** Save/load preserves all clip metadata and grid layout
7. ✅ **Performance:** Meet all targets in Section 3.1
8. ✅ **Stability:** Run 4+ hours without crash or audio dropout

### 4.2 Usability Acceptance

**Must Pass 80%:**

1. ✅ New user can load first clip in <5 minutes (without manual)
2. ✅ User can organize 20 clips into grid in <15 minutes
3. ✅ User can edit trim points intuitively using mouse
4. ✅ User understands Clip Groups and routing without explanation
5. ✅ Session save/load is obvious and reliable

**Testing Method:** 5 users (mix of broadcast/theater) complete tasks, observe time and friction points.

### 4.3 Platform Acceptance

**Must Pass Per Platform:**

**macOS:**

- ✅ Runs on macOS 12+ (Monterey, Ventura, Sonoma)
- ✅ Universal binary (Intel + Apple Silicon)
- ✅ CoreAudio latency <8ms with built-in audio
- ✅ No kernel panics or system freezes

**Windows:**

- ✅ Runs on Windows 10 22H2, Windows 11
- ✅ ASIO latency <5ms with Focusrite Scarlett
- ✅ WASAPI fallback works on built-in audio
- ✅ No BSOD or system crashes

---

## 5. Development Phases (6 Months)

### Month 1: Foundation & Audio Engine

**Weeks 1-2: Project Setup**

- CMake project structure
- JUCE integration and licensing
- CI/CD pipeline (GitHub Actions)
- Basic window and empty grid rendering

**Weeks 3-4: Audio Engine Core**

- Orpheus SDK integration (or stub interfaces)
- CoreAudio/ASIO driver selection
- Audio thread setup (lock-free communication)
- Single-clip playback working

**Deliverables:**

- Runnable application (macOS + Windows)
- Can play one WAV file through selected audio output
- Performance baseline established

### Month 2: Clip Grid & Basic Playback

**Weeks 5-6: Grid UI Implementation**

- 10×12 button grid component (JUCE)
- 8 tab navigation
- Button states (empty, loaded, playing, stopped)
- Drag-and-drop file loading

**Weeks 7-8: Multi-Clip Playback**

- Clip trigger logic (ClipTriggerEngine from OCC023)
- FIFO choke mode
- Clip Groups (A/B/C/D) and routing
- Up to 16 simultaneous clips

**Deliverables:**

- Functional grid with loadable clips
- Multi-clip playback working
- Visual feedback for playing clips

### Month 3: Waveform Editor & Editing

**Weeks 9-10: Waveform Rendering**

- Waveform visualization (JUCE or custom OpenGL)
- Zoom/pan controls
- Trim IN/OUT markers (visual)

**Weeks 11-12: Editing Logic**

- Mouse click handling (L=IN, R=OUT, M=playhead)
- Trim point updates
- Basic fade in/out (linear)
- Transport controls (play, pause, stop)

**Deliverables:**

- Waveform editor panel functional
- Sample-accurate trim editing working
- Fades applied and audible

### Month 4: Session Management & Routing

**Weeks 13-14: Session Save/Load**

- JSON serialization (clip metadata per OCC022)
- Session file format (.occ)
- Relative file path handling
- New session creation

**Weeks 15-16: Routing Panel**

- Routing matrix UI (Outputs tab)
- Dynamic routing changes (real-time)
- Routing presets (save/load)

**Deliverables:**

- Sessions save/load reliably
- Routing configurable via UI
- Session portability tested (move folder, still works)

### Month 5: Polish & Cross-Platform

**Weeks 17-18: Windows ASIO/WASAPI**

- ASIO driver integration
- WASAPI fallback logic
- Windows-specific UI adjustments
- Installer (MSI or InnoSetup)

**Weeks 19-20: macOS Finalization**

- Universal binary (Intel + Apple Silicon)
- Code signing and notarization
- DMG installer
- Aggregated device testing

**Deliverables:**

- Windows build stable and performant
- macOS build signed and distributable
- Installers for both platforms

### Month 6: Beta Testing & Bug Fixing

**Weeks 21-22: Internal Testing**

- Stress testing (100+ clips, 4+ hours)
- Edge case testing (missing files, corrupt sessions, device unplugging)
- Performance profiling and optimization

**Weeks 23-24: Beta Release**

- 10 beta users recruited (5 broadcast, 5 theater)
- Documentation (quick start guide, keyboard shortcuts)
- Feedback collection (surveys, interviews)
- Critical bug fixes

**Deliverables:**

- MVP shipped to 10 beta users
- Beta feedback report
- Prioritized v1.0 feature list

---

## 6. Resource Requirements

### 6.1 Team Composition

**Minimum Viable Team:**

- 1x C++ Developer (JUCE + audio expertise) - Full-time
- 0.5x SDK Developer (Orpheus SDK integration) - Part-time
- 0.5x QA/Testing (cross-platform validation) - Part-time

**Ideal Team:**

- 2x C++ Developers (UI + Audio split) - Full-time
- 1x SDK Developer (dedicated Orpheus SDK) - Full-time
- 1x UX Designer (high-fidelity mockups, usability testing) - Part-time
- 1x QA Engineer (automated + manual testing) - Full-time

**Skills Required:**

- C++20 proficiency
- JUCE framework experience
- Audio programming (real-time constraints, lock-free design)
- Cross-platform development (macOS + Windows)
- CMake build systems

### 6.2 Infrastructure

**Development:**

- GitHub repository (private or public)
- CI/CD (GitHub Actions)
- Automated builds (macOS + Windows)
- Unit test framework (GoogleTest)

**Testing:**

- Test devices: macOS (Intel + Apple Silicon), Windows 10/11
- Audio interfaces: Focusrite Scarlett, RME Babyface, built-in audio
- Beta testing platform: TestFlight (future iOS) or email distribution

**Tools:**

- JUCE Projucer (project management)
- Figma/Sketch (UI design)
- Xcode (macOS builds)
- Visual Studio 2022 (Windows builds)
- CMake 3.20+

### 6.3 Licensing & Costs

**JUCE License:**

- **Indie License:** $40/month (~$240 for 6 months)
- **Alternative:** GPL v3 (free, requires open-sourcing)

**Code Signing:**

- **Apple Developer:** $99/year (macOS notarization)
- **Windows Authenticode:** $200-500/year (optional but recommended)

**Audio Libraries:**

- libsndfile (LGPL) - Free
- FLAC library (BSD) - Free

**Total Estimated Cost:** $500-1,000 for 6-month MVP (assuming indie JUCE license and Apple Developer account)

---

## 7. Risks & Mitigations

### 7.1 High-Risk Items

| Risk                           | Impact                             | Probability | Mitigation                                                  |
| ------------------------------ | ---------------------------------- | ----------- | ----------------------------------------------------------- |
| **Orpheus SDK not ready**      | High (blocks development)          | Medium      | Develop stub interfaces (OCC027), parallel development      |
| **JUCE learning curve**        | Medium (slower velocity)           | Medium      | Allocate 2 weeks for JUCE training, leverage forum/examples |
| **ASIO driver issues**         | High (Windows unusable)            | Low         | Test with multiple interfaces, WASAPI fallback              |
| **Performance targets missed** | High (unusable for broadcast)      | Low         | Profile early (Month 2), optimize incrementally             |
| **Cross-platform bugs**        | Medium (platform-specific crashes) | Medium      | CI/CD catches early, weekly testing on both platforms       |

### 7.2 Mitigation Strategies

**Orpheus SDK Risk:**

- Week 1: Define API contracts (OCC027)
- Week 2: Build stub SDK interfaces
- Develop OCC against stubs (Months 1-3)
- Integrate real SDK when available (Month 4+)

**Performance Risk:**

- Week 3: Establish performance baselines
- Monthly performance reviews (automated benchmarks)
- Optimize audio thread first (critical path)

**Cross-Platform Risk:**

- Weekly builds on both macOS and Windows
- Automated unit tests on CI/CD
- Manual testing checklist (shared between platforms)

---

## 8. Success Metrics

### 8.1 Technical Success

**At MVP Completion (Month 6):**

- ✅ All performance targets met (Section 3.1)
- ✅ Zero critical bugs (crashes, data loss)
- ✅ <5 high-priority bugs (audio dropouts, UI freezes)
- ✅ 90% unit test coverage for core components
- ✅ Passes 4-hour stress test on both platforms

### 8.2 User Success

**Beta User Feedback (Month 6):**

- ✅ 80% would recommend OCC to colleagues
- ✅ 70% would use OCC for non-critical live work
- ✅ 50% would consider switching from current tool (SpotOn, QLab)
- ✅ Average SUS (System Usability Scale) score: >70

### 8.3 Product Success

**Market Validation:**

- ✅ 10 beta users complete 2-week trial
- ✅ 5+ beta users request continued access for v1.0
- ✅ Feedback identifies top 3 v1.0 features (e.g., recording, iOS app, AutoTrim)
- ✅ No fundamental architecture changes required

**Strategic Success:**

- ✅ Proves OCC/SDK integration model viable
- ✅ Validates JUCE framework choice
- ✅ Establishes development velocity baseline (features per month)
- ✅ Identifies Orpheus SDK gaps for prioritization

---

## 9. Definition of Done

**MVP is "done" when:**

1. ✅ **All MUST HAVE features implemented and tested** (Section 1.1)
2. ✅ **All acceptance criteria passed** (Section 4)
3. ✅ **Installers built for macOS and Windows**
4. ✅ **Quick start documentation written**
5. ✅ **10 beta users have access and are providing feedback**
6. ✅ **No critical bugs blocking beta testing**
7. ✅ **v1.0 roadmap prioritized based on beta feedback**

**Not required for "done":**

- ❌ Perfect UI polish (acceptable if functional)
- ❌ Complete feature parity with SpotOn/QLab (MVP is intentionally limited)
- ❌ Public launch (MVP is beta-only)
- ❌ Revenue generation (focus is validation, not monetization)

---

## 10. Post-MVP: Transition to v1.0

### 10.1 Beta Feedback Review (Month 7)

**Process:**

1. Collect feedback from 10 beta users (surveys + interviews)
2. Categorize feedback: Must-fix bugs, Nice-to-have features, Long-term ideas
3. Prioritize v1.0 feature list based on:
   - User demand (how many users requested it?)
   - Alignment with product vision (OCC021)
   - Implementation effort (easy wins first)

### 10.2 v1.0 Candidate Features

**Likely Priorities (Based on OCC021):**

1. Recording directly into buttons (core workflow)
2. iOS companion app (differentiation from competitors)
3. AutoTrim function (SpotOn-inspired time-saver)
4. Master/slave linking (theater use case)
5. Advanced fade curves (professional polish)
6. Track preview before loading (reduces mistakes)
7. MP3/AAC support (user convenience)

**v1.0 Timeline:** Months 7-12 (6 additional months)

---

## 11. Appendices

### Appendix A: Feature Comparison (MVP vs. v1.0 vs. v2.0)

| Feature                  | MVP | v1.0 | v2.0 |
| ------------------------ | --- | ---- | ---- |
| Clip triggering          | ✅  | ✅   | ✅   |
| Waveform editor (basic)  | ✅  | ✅   | ✅   |
| Session save/load        | ✅  | ✅   | ✅   |
| Clip Groups & routing    | ✅  | ✅   | ✅   |
| FIFO choke               | ✅  | ✅   | ✅   |
| Recording to buttons     | ❌  | ✅   | ✅   |
| iOS companion app        | ❌  | ✅   | ✅   |
| AutoTrim                 | ❌  | ✅   | ✅   |
| Master/slave linking     | ❌  | ✅   | ✅   |
| Advanced fades           | ❌  | ✅   | ✅   |
| Track preview            | ❌  | ✅   | ✅   |
| MIDI/OSC control         | ❌  | ✅   | ✅   |
| Logging & export         | ❌  | ✅   | ✅   |
| DSP (time-stretch, etc.) | ❌  | ❌   | ✅   |
| GPI triggering           | ❌  | ❌   | ✅   |
| VST3 hosting             | ❌  | ❌   | ✅   |
| Interaction rules        | ❌  | ❌   | ✅   |

### Appendix B: Testing Checklist (MVP)

**Functional Testing:**

- [ ] Load WAV, AIFF, FLAC files
- [ ] Trigger clips via mouse click
- [ ] Trigger clips via keyboard shortcut
- [ ] FIFO choke: only one clip plays per group
- [ ] Simultaneous clips: up to 16 across groups
- [ ] Edit trim IN/OUT points
- [ ] Apply fade in/out
- [ ] Save session, close, reopen → all clips and edits preserved
- [ ] Change routing, verify audio routes correctly
- [ ] Performance monitor shows accurate CPU/latency

**Platform Testing:**

- [ ] macOS: Intel Mac (High Sierra or later)
- [ ] macOS: Apple Silicon Mac (M1/M2)
- [ ] Windows: Windows 10 (ASIO + WASAPI)
- [ ] Windows: Windows 11 (ASIO + WASAPI)

**Stress Testing:**

- [ ] Load 100 clips without crash
- [ ] 16 simultaneous clips playing (CPU <30%)
- [ ] 4-hour continuous operation (no crashes or dropouts)
- [ ] Device hotplug (unplug audio interface during playback)

**Edge Case Testing:**

- [ ] Missing audio files (graceful error message)
- [ ] Corrupt session file (error handling, no crash)
- [ ] Unsupported audio format (clear error message)
- [ ] Sample rate mismatch (automatic conversion or warning)

### Appendix C: MVP Quick Start Documentation Outline

**Chapter 1: Installation**

- System requirements
- Download and install (DMG/MSI)
- First launch and audio driver selection

**Chapter 2: Your First Session**

- Create new session
- Load first clip
- Trigger clip and hear audio
- Save session

**Chapter 3: Organizing Clips**

- Using tabs
- Clip Groups (A/B/C/D)
- Routing to outputs

**Chapter 4: Editing Clips**

- Open waveform editor
- Set trim IN/OUT points
- Add fades
- Preview edits

**Chapter 5: Keyboard Shortcuts**

- Essential shortcuts table
- Customization (deferred to v1.0)

**Chapter 6: Troubleshooting**

- No audio output? (check routing, driver selection)
- Clips won't load? (check file format)
- Crashes? (report bugs, attach logs)

---

## 12. Related Documents

- **OCC021** - Product Vision (strategic direction)
- **OCC023** - Component Architecture (system design)
- **OCC024** - User Interaction Flows (detailed workflows)
- **OCC025** - UI Framework Decision (JUCE rationale)
- **OCC027** - API Contracts (upcoming: OCC ↔ SDK interfaces)

---

**Document Status:** Ready for stakeholder approval and sprint planning.
**Next Steps:** Create OCC027 (API Contracts) and begin Month 1 development.
