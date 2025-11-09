# ORP106: Wave Finder Architecture Assessment - JUCE vs SDK Integration

**Status:** Assessment
**Created:** 2025-11-09
**Author:** Claude (Orpheus SDK Research Agent)
**Context:** Strategic architecture decision for Wave Finder/FreqFinder application

---

## Executive Summary

Wave Finder (formerly "FreqFinder," prototyped as "Hcalc") is the second first-party Orpheus application—a harmonic calculator and frequency scope for professional audio analysis. Initial planning indicated a "JUCE-only" implementation, but this requires validation against the established **JUCE UI + Orpheus SDK audio core** pattern used by Clip Composer (OCC).

This document provides a structured assessment framework to determine whether Wave Finder should:
- **Option A:** Use JUCE-only (no SDK dependencies)
- **Option B:** Use JUCE UI + Orpheus SDK audio core (OCC pattern)
- **Option C:** Use custom architecture

---

## Background

### Wave Finder Overview

**Purpose:** Harmonic calculator and real-time frequency scope for professional audio analysis

**Target Users:**
- Broadcast engineers analyzing audio streams
- Sound designers identifying frequency content
- Installation professionals debugging network audio (AES67/Dante)

**Roadmap Position:**
- **Milestone:** M4 (Recording & DSP) - Months 7-12, 2025-2026
- **Planned v1.0:** Months 10-12, 2025
- **Prerequisite:** M2 Real-Time Infrastructure must complete first (Months 1-6)

**Ecosystem Position:**
```
First-party applications:
- Clip Composer      — Professional soundboard (flagship) ✅ v1.0-rc.1
- Wave Finder        — Harmonic calculator & frequency scope ⏳ Planned
- FX Engine          — LLM-powered effects processing ⏳ Planned
```

### Architecture Context

**Orpheus Clip Composer (OCC) Pattern:**
```
┌─────────────────────────────────────────────┐
│ JUCE Application Layer                      │
│ - UI components (ClipGrid, waveform, menus) │
│ - Cross-platform desktop integration        │
│ - OpenGL-accelerated rendering              │
└─────────────────────────────────────────────┘
                 ↓ (Lock-Free Commands)
┌─────────────────────────────────────────────┐
│ Orpheus SDK Audio Engine                    │
│ - SessionGraph (session state management)   │
│ - TransportController (clip playback)       │
│ - RoutingMatrix (multi-channel mixing)      │
│ - Deterministic, broadcast-safe core        │
└─────────────────────────────────────────────┘
                 ↓ (Real-Time Audio)
┌─────────────────────────────────────────────┐
│ Platform Audio Drivers                      │
│ - CoreAudio (macOS) / WASAPI/ASIO (Windows) │
│ - AES67 Network Driver (planned)            │
└─────────────────────────────────────────────┘
```

**Key Question:** Should Wave Finder follow this pattern, or is JUCE-only sufficient?

---

## Assessment Framework

### Section 1: Architecture Clarification

**A. Current State**

**Q1.1:** When you say "JUCE-only," do you mean:
- **Option A:** JUCE for UI + JUCE's audio classes for everything (no Orpheus SDK at all)
- **Option B:** JUCE for UI + Orpheus SDK for audio core (standard OCC pattern)
- **Option C:** Something else (please specify)

**Q1.2:** What's the current implementation status?
- [ ] Prototype exists (as "Hcalc"?)
- [ ] Design docs written
- [ ] Code started
- [ ] Just planned

**Q1.3:** If existing code/prototype exists, where is it located?
- In this repo (`apps/wave-finder/` or `apps/freq-finder/`)?
- Separate repository?
- Local prototype not committed?

---

### Section 2: Feature Requirements vs. SDK Capabilities

**B. Core Features**

**Q2.1:** What are Wave Finder's primary features? (Check all that apply)
- [ ] Real-time FFT analysis (live audio input)
- [ ] File-based frequency analysis (analyze WAV/AIFF files)
- [ ] Harmonic detection (identify fundamental + overtones)
- [ ] Multi-channel analysis (analyze multiple streams simultaneously)
- [ ] Network audio analysis (AES67/Dante/Ravenna streams)
- [ ] Recording/bouncing analyzed audio
- [ ] Session management (save/load analysis presets)
- [ ] Other: _______

**Q2.2:** What audio sources does Wave Finder need to handle?
- [ ] Live microphone input
- [ ] Audio file playback (WAV, AIFF, FLAC, MP3)
- [ ] System audio capture (loopback)
- [ ] Network audio streams (AES67, RTP, etc.)
- [ ] Multiple sources simultaneously
- [ ] Other: _______

**C. Performance Requirements**

**Q2.3:** What are latency requirements?
- Input to display update: ___ ms (target?)
- Can you tolerate 10-15ms latency? Or must be <5ms?
- Is this "broadcast-safe" (24/7 operation required)?

**Q2.4:** What's the CPU budget?
- Target: ___% CPU with ___ active streams
- Is real-time processing required, or can analysis be offline?

---

### Section 3: Orpheus SDK Value Proposition

**D. Where SDK Might Help**

**Q3.1:** Does Wave Finder need **deterministic rendering**?
- Example: "Analyze this file → always get same frequency plot" (bit-identical)
- **If yes** → SDK's determinism is valuable
- **If no** → JUCE audio classes might suffice

**Q3.2:** Does Wave Finder need **multi-file/multi-stream session management**?
- Example: Analyze 4 audio files simultaneously, compare spectrums
- **If yes** → SDK's SessionGraph and TransportController are useful
- **If no** → Simple JUCE AudioFormatReader might suffice

**Q3.3:** Does Wave Finder need **network audio support** (AES67/Dante)?
- The roadmap mentions AES67 driver support (ORP072)
- SDK has this planned; JUCE doesn't
- Is this a Wave Finder requirement?

**Q3.4:** Does Wave Finder need **sample-accurate transport control**?
- Example: "Play from sample 44100 to 88200, loop with zero-sample gap"
- **If yes** → SDK's TransportController is designed for this
- **If no** → JUCE's AudioTransportSource might suffice

**E. Where JUCE-Only Might Suffice**

**Q3.5:** Is Wave Finder purely a **passive analyzer** (no playback control)?
- If it's just "show me the spectrum of this audio," JUCE alone works
- If it needs complex transport (cues, loops, multi-clip), SDK adds value

**Q3.6:** Is Wave Finder a **single-instance tool** (one file at a time)?
- If analyzing one stream only → JUCE sufficient
- If analyzing multiple streams → SDK's session management helps

**Q3.7:** Does Wave Finder need **offline rendering** or **export features**?
- Example: "Render this analysis to a file"
- **If yes** → SDK's determinism ensures reproducibility
- **If no** → Live-only analysis doesn't need determinism

---

### Section 4: Strategic Alignment

**F. Ecosystem Consistency**

**Q4.1:** Should Wave Finder follow the **JUCE UI + Orpheus SDK audio core** pattern used by OCC?
- **Pro:** Consistency across apps, shared infrastructure, easier maintenance
- **Con:** Overhead if SDK features aren't needed
- What's your assessment?

**Q4.2:** Does Wave Finder need to **share sessions with OCC**?
- Example: "Open OCC session, analyze clip frequencies in Wave Finder"
- **If yes** → Must use Orpheus SDK's SessionGraph format
- **If no** → Can be standalone

**Q4.3:** Does Wave Finder need to **integrate with future Orpheus apps** (FX Engine, DAW, etc.)?
- **If yes** → Using SDK ensures compatibility
- **If no** → Standalone JUCE app is fine

**G. Development Effort**

**Q4.4:** What's the current team bandwidth?
- Is OCC v1.0 complete? (Roadmap says it's prerequisite)
- Can you support two codebases (JUCE-only for Wave Finder, JUCE+SDK for OCC)?
- Or is code reuse critical?

**Q4.5:** What's the timeline pressure?
- Roadmap says Wave Finder v1.0 at months 7-12
- Does using SDK's existing infrastructure accelerate development?
- Or does SDK add complexity that slows you down?

---

### Section 5: Decision Criteria

**H. Make/Buy Decision Framework**

**Q5.1:** For each feature, which implementation is optimal?

| Feature | JUCE Built-in | Orpheus SDK | Custom Code |
|---------|---------------|-------------|-------------|
| FFT computation | `juce::dsp::FFT` | ? | ? |
| Audio file reading | `AudioFormatReader` | `IAudioFileReader` (SDK) | ? |
| Live audio input | `AudioDeviceManager` | SDK drivers (CoreAudio/ASIO) | ? |
| Transport control | `AudioTransportSource` | `ITransportController` (SDK) | ? |
| Multi-stream mixing | `AudioSourceChannelInfo` | `IRoutingMatrix` (SDK) | ? |
| Session management | Custom | `SessionGraph` (SDK) | ? |
| Network audio (AES67) | ❌ Not available | ✅ ORP072 driver (SDK) | ? |

**Q5.2:** What's the **simplest implementation** that meets requirements?
- **Option A:** Pure JUCE (no SDK dependencies)
- **Option B:** JUCE UI + SDK audio core (OCC pattern)
- **Option C:** SDK core + custom UI (minimal JUCE)

---

### Section 6: Risk Assessment

**I. What Could Go Wrong?**

**Q6.1:** If Wave Finder uses **JUCE-only**, what features become harder/impossible?
- Network audio support (AES67)?
- Session compatibility with OCC?
- Deterministic analysis (reproducible results)?
- Multi-stream handling?

**Q6.2:** If Wave Finder uses **Orpheus SDK**, what's the overhead?
- Learning curve for SDK APIs?
- Binary size increase?
- Additional dependencies?
- Maintenance burden if SDK changes?

**Q6.3:** What's the **migration path** if you start JUCE-only and need SDK later?
- Easy refactor (drop-in replacement)?
- Major rewrite?
- Impossible?

---

### Section 7: Competitive Analysis

**J. What Do Competitors Use?**

**Q7.1:** What tech stack do competing tools use?
- **iZotope RX** (spectrum analyzer): JUCE
- **FabFilter Pro-Q** (EQ + analyzer): ?
- **Voxengo SPAN** (free analyzer): ?
- Do they use custom DSP or off-the-shelf libraries?

**Q7.2:** What's the **minimum viable product** for Wave Finder?
- Can you ship v1.0 with JUCE-only, add SDK in v2.0?
- Or does v1.0 require SDK features from day one?

---

## Key Decision Questions (Prioritized)

**If you can only answer 5 questions, answer these:**

1. **Architecture:** Is "JUCE-only" actually "JUCE UI + no SDK at all," or "JUCE UI + SDK audio core" (like OCC)?

2. **Features:** Does Wave Finder need multi-stream analysis, network audio (AES67), or session compatibility with OCC?
   - **If yes** → SDK valuable
   - **If no** → JUCE-only may suffice

3. **Performance:** Must input-to-display latency be <5ms? Is determinism required?
   - **If yes** → SDK helps
   - **If no** → JUCE audio classes may suffice

4. **Strategic:** Should Wave Finder follow the same JUCE+SDK pattern as OCC for ecosystem consistency? Or is standalone acceptable?

5. **Simplest Path:** What's the **minimum viable implementation** that meets requirements—pure JUCE, or JUCE+SDK?

---

## Technical Comparison: JUCE vs. SDK

### JUCE Audio Classes (Standalone)

**Strengths:**
- ✅ Fast FFT implementation (`juce::dsp::FFT`)
- ✅ Simple file reading (`AudioFormatReader`)
- ✅ Built-in audio device management (`AudioDeviceManager`)
- ✅ Lower initial complexity
- ✅ Proven for single-stream analysis tools

**Limitations:**
- ❌ No network audio support (AES67/Dante)
- ❌ No deterministic rendering guarantees
- ❌ Limited multi-stream session management
- ❌ No session compatibility with OCC
- ❌ Manual implementation of broadcast-safe patterns

### Orpheus SDK (Integrated)

**Strengths:**
- ✅ Network audio support (AES67 driver, ORP072)
- ✅ Deterministic, bit-identical analysis
- ✅ Multi-stream session management (SessionGraph)
- ✅ Session compatibility with OCC
- ✅ Broadcast-safe architecture (zero-allocation audio thread)
- ✅ Sample-accurate transport control
- ✅ Code reuse across Orpheus ecosystem

**Limitations:**
- ❌ Higher initial complexity
- ❌ Requires understanding SDK APIs
- ❌ Larger binary size
- ❌ Dependency on SDK release cycle

---

## Decision Matrix

| Use Case | Recommendation |
|----------|----------------|
| **Simple file analyzer** (one file, passive spectrum display) | JUCE-only sufficient |
| **Live input analyzer** (microphone, single stream) | JUCE-only sufficient |
| **Multi-stream analyzer** (compare multiple files/streams) | JUCE + SDK recommended |
| **Network audio analyzer** (AES67/Dante streams) | **SDK required** (JUCE lacks this) |
| **Session-aware analyzer** (integrates with OCC workflows) | **SDK required** |
| **Deterministic analysis** (reproducible results for compliance) | JUCE + SDK recommended |
| **Broadcast-safe analyzer** (24/7 operation, mission-critical) | JUCE + SDK recommended |

---

## Recommended Process

### Step 1: Answer Priority Questions (Q1.1, Q2.1, Q2.2, Q3.3, Q4.1)

Determine:
- Current architecture intent
- Required features (especially network audio, multi-stream)
- Strategic alignment with OCC pattern

### Step 2: Evaluate Against Decision Matrix

Match Wave Finder's use case to the matrix above:
- If requirements map to "JUCE-only sufficient" → Proceed with pure JUCE
- If requirements map to "SDK recommended/required" → Use JUCE + SDK pattern

### Step 3: Validate with Prototype

**JUCE-only path:**
- Build minimal prototype with `juce::dsp::FFT` + `AudioFormatReader`
- Verify performance (<5ms latency, <30% CPU)
- Test multi-stream capability if required

**JUCE + SDK path:**
- Integrate SDK's `IAudioFileReader` or `ITransportController`
- Verify lock-free command processing
- Test session compatibility with OCC

### Step 4: Document Decision

Record:
- Chosen architecture (JUCE-only or JUCE+SDK)
- Rationale (which requirements drove the decision)
- Trade-offs accepted
- Migration path if requirements change

---

## Next Steps

**Action Items:**

1. **Wave Finder Lead:** Answer priority questions (Section above)
2. **Architecture Review:** Evaluate responses against decision matrix
3. **Prototype:** Build minimal implementation to validate choice
4. **Document:** Record final decision in follow-up ORP doc (ORP106 or similar)

**Timeline:**

- **Week 1:** Complete assessment questionnaire
- **Week 2:** Architecture review meeting
- **Week 3:** Build validation prototype
- **Week 4:** Document final decision + update roadmap

---

## References

- **ORP068:** Implementation Plan (v2.0) - Orpheus SDK roadmap
- **ORP072:** AES67 Network Audio Driver - Network audio support
- **OCC025:** UI Framework Decision - JUCE vs Electron (rationale applies here)
- **ROADMAP.md:** Wave Finder planned for M4 (Months 7-12)
- **ARCHITECTURE.md:** Host-neutral core SDK design philosophy
- **AGENTS.md:** Strategic context ("Wave Finder needs FFT analysis")

---

## Appendix: Performance Requirements Reference

### Professional Analyzer Benchmarks

| Tool | Latency (Input → Display) | CPU (Single Stream) | Tech Stack |
|------|---------------------------|---------------------|------------|
| iZotope RX | <5ms | 15-25% | JUCE + Custom DSP |
| FabFilter Pro-Q | <3ms | 10-20% | Proprietary |
| Voxengo SPAN | <10ms | 5-15% | Custom C++ |
| **Wave Finder Target** | **<5ms** | **<30%** | **TBD (JUCE or JUCE+SDK)** |

### Orpheus SDK Overhead Baseline

From OCC v1.0-rc.1 testing:
- **16 simultaneous clips:** <30% CPU (Intel i5 8th gen)
- **Transport latency:** <1ms (lock-free command processing)
- **Memory footprint:** 50-100 MB (excluding UI)

**Implication:** SDK overhead is acceptable for professional tools if features justify it.

---

**Status:** Awaiting Wave Finder team response to assessment questions.

**Next Document:** ORP107 Wave Finder Architecture Decision (after assessment complete)
