# FRQ004: FreqFinder and Orpheus SDK Relationship Assessment

**Status:** Architecture Decision Record
**Created:** 2025-11-09
**Author:** Claude Code
**Context:** Response to ORP106 architecture assessment for the Claude Code Web agent
**Related Documents:** ORP106 (Orpheus SDK), FRQ001 (Product Spec), FRQ003 (Implementation)

---

## Executive Summary

FreqFinder is marketed as part of the "Orpheus SDK Applications" family but **does not use the Orpheus SDK** as a dependency. It is a **JUCE-only** application that shares branding and strategic positioning with the Orpheus ecosystem but maintains complete architectural independence.

**Key Finding:** FreqFinder's architecture decision (JUCE-only, no SDK) is appropriate and validated by ORP106's decision framework. The application's requirements do not justify the complexity overhead of Orpheus SDK integration.

---

## 1. Current Architecture Status

### 1.1 Repository Structure

**FreqFinder Location:** Separate repository at `~/dev/freqfinder/`

**NOT integrated into:**

- `~/dev/orpheus-sdk/` monorepo
- `~/dev/orpheus-sdk/apps/` directory
- Orpheus SDK build system

### 1.2 Dependency Analysis

**From CMakeLists.txt (line 12-20, 76-85):**

```cmake
# ONLY JUCE dependency
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)

# Links ONLY to JUCE modules
target_link_libraries(FreqFinder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
)
```

**No Orpheus SDK dependencies found:**

- ❌ No `orpheus-sdk` in FetchContent
- ❌ No `orpheus::core` in target_link_libraries
- ❌ No `#include "orpheus/..."` in source files
- ❌ No references to Orpheus SDK classes (SessionGraph, TransportController, etc.)

### 1.3 Branding vs. Architecture

**Brand Association (FRQ001:4):**

```
Product Family: Orpheus SDK Applications
```

**Company Name (CMakeLists.txt:24):**

```cmake
COMPANY_NAME "OrpheusSDK"
```

**Key Insight:** FreqFinder uses "OrpheusSDK" as a **brand identity**, not a **technical dependency**. This is analogous to how "Microsoft Office" applications share branding but have independent architectures.

---

## 2. Answering ORP106's Key Questions

ORP106 (Orpheus SDK research document) posed critical questions to determine whether FreqFinder should use JUCE-only or JUCE+SDK. Here are FreqFinder's answers:

### 2.1 Section 1: Architecture Clarification

**Q1.1: When you say "JUCE-only," do you mean...**

**Answer:** **Option A** - JUCE for UI + JUCE's audio classes for everything (no Orpheus SDK at all)

**Evidence:**

- CMakeLists.txt links only to `juce::juce_audio_utils`, `juce::juce_dsp`, `juce::juce_gui_extra`
- Source code uses `juce::dsp::Oscillator`, `juce::dsp::FFT`, `juce::AudioProcessor`
- No Orpheus SDK classes present in codebase

**Q1.2: What's the current implementation status?**

**Answer:** Code exists, Phases 1 & 2 complete (per IMPLEMENTATION.md)

- ✅ Prototype exists (formerly "Hcalc")
- ✅ Design docs written (FRQ001, FRQ002, FRQ003)
- ✅ Code implemented (~2,500 lines C++20)
- ✅ Current version: 0.2.1

**Q1.3: Where is the code located?**

**Answer:** Separate repository: `~/dev/freqfinder/`

---

### 2.2 Section 2: Feature Requirements vs. SDK Capabilities

**Q2.1: What are FreqFinder's primary features?**

From FRQ001 and implementation:

- ✅ Real-time oscillator synthesis (7 sine wave generators)
- ✅ FFT analysis (8192-point via `juce::dsp::FFT`)
- ✅ Harmonic detection (fundamental + overtones/undertones)
- ✅ MIDI input handling (note-on sets frequency)
- ❌ Multi-channel analysis (NO - single stream only)
- ❌ Network audio analysis (NO - not required)
- ❌ Recording/bouncing (NO - synthesis only)
- ❌ Session management (NO - standalone tool)

**Q2.2: What audio sources does FreqFinder need to handle?**

Current implementation:

- ✅ Pure synthesis (internal oscillators)
- ✅ MIDI input (controller support)
- 🔜 Sidechain input (planned Phase 4, but simple passthrough)
- ❌ Audio file playback (NOT NEEDED)
- ❌ System audio capture (NOT NEEDED)
- ❌ Network audio streams (NOT NEEDED)
- ❌ Multiple sources simultaneously (NOT NEEDED)

**Verdict:** JUCE audio classes are **sufficient** for all requirements.

---

### 2.3 Section 3: Orpheus SDK Value Proposition

**Q3.1: Does FreqFinder need deterministic rendering?**

**Answer:** ❌ **NO**

Rationale:

- FreqFinder is a real-time synthesis tool, not a renderer
- No "analyze this file → get same result" requirement
- No compliance/reproducibility mandates
- Real-time audio output is non-deterministic by nature (buffer timing varies)

**Conclusion:** SDK's determinism is **not valuable** here.

---

**Q3.2: Does FreqFinder need multi-file/multi-stream session management?**

**Answer:** ❌ **NO**

Rationale:

- Single fundamental frequency at a time
- No "compare 4 audio files" use case
- No session graph (just 7 oscillators)
- No multi-track coordination

**Conclusion:** SDK's SessionGraph is **not useful** here.

---

**Q3.3: Does FreqFinder need network audio support (AES67/Dante)?**

**Answer:** ❌ **NO**

Rationale:

- Target users: Sound designers, mixing engineers, educators
- Use cases: EQ work, sound design, education (FRQ001:17-23)
- No mention of broadcast workflows or network audio in spec
- No installation/integration use cases

**Conclusion:** SDK's AES67 driver (ORP072) is **not required**.

---

**Q3.4: Does FreqFinder need sample-accurate transport control?**

**Answer:** ❌ **NO**

Rationale:

- No playback (it's a synthesizer)
- No "play from sample X to Y" requirement
- No loop points or cues
- MIDI note-on is immediate, not sample-accurate

**Conclusion:** SDK's TransportController is **not applicable**.

---

**Q3.5: Is FreqFinder purely a passive analyzer?**

**Answer:** ❌ **NO** - It's an **active synthesizer**

But this doesn't require SDK:

- Synthesis via `juce::dsp::Oscillator` (simple, efficient)
- No complex transport needed
- Just start/stop oscillators

**Conclusion:** JUCE audio classes handle synthesis easily.

---

**Q3.6: Is FreqFinder a single-instance tool?**

**Answer:** ✅ **YES**

Rationale:

- One fundamental frequency at a time
- One waveform display
- One spectrum analyzer
- No multi-stream comparison

**Conclusion:** JUCE-only is **sufficient**.

---

**Q3.7: Does FreqFinder need offline rendering or export features?**

**Answer:** ❌ **NO**

Rationale:

- Real-time synthesis only
- No "render to file" feature in spec
- No export requirements (except future CSV spectrum data)

**Conclusion:** SDK's deterministic rendering is **not needed**.

---

### 2.4 Section 4: Strategic Alignment

**Q4.1: Should FreqFinder follow the JUCE UI + Orpheus SDK audio core pattern (like OCC)?**

**Answer:** ❌ **NO**

**Rationale:**

**Clip Composer (OCC) Requirements:**
| Feature | OCC | FreqFinder |
|---------|-----|------------|
| Multi-clip playback | ✅ YES (16+ clips) | ❌ NO (single tone) |
| Sample-accurate cues | ✅ YES (broadcast) | ❌ NO (real-time only) |
| Session management | ✅ YES (save/load) | ❌ NO (just parameters) |
| Network audio | ✅ YES (AES67) | ❌ NO (not required) |
| Deterministic rendering | ✅ YES (broadcast) | ❌ NO (live synthesis) |
| Complex routing | ✅ YES (matrix) | ❌ NO (7 oscillators) |

**Conclusion:** OCC and FreqFinder have **different architectural needs**. OCC benefits from SDK's session management and broadcast-safe architecture. FreqFinder does not.

---

**Q4.2: Does FreqFinder need to share sessions with OCC?**

**Answer:** ❌ **NO**

Rationale:

- No use case for "open OCC session in FreqFinder"
- FreqFinder doesn't analyze clips (it generates tones)
- No workflow integration required
- Separate tool with separate purpose

**Conclusion:** Session compatibility is **not needed**.

---

**Q4.3: Does FreqFinder need to integrate with future Orpheus apps?**

**Answer:** ❌ **NO** (not technically)

**Clarification:**

- **Brand integration:** YES (shared "Orpheus SDK Applications" family)
- **Technical integration:** NO (no shared sessions, no plugin chaining)
- **Workflow integration:** MAYBE (could be used alongside OCC, but independently)

**Conclusion:** Brand positioning ≠ technical dependency.

---

**Q4.4: What's the current team bandwidth?**

**Answer:** Development is nearly complete (Phases 1 & 2 done, ~2,500 lines)

- JUCE-only approach has proven **fast to develop**
- No SDK learning curve required
- Code is straightforward and maintainable
- Adding SDK now would add complexity without benefit

**Conclusion:** JUCE-only was the **right choice** for rapid development.

---

**Q4.5: What's the timeline pressure?**

**Answer:** Phases 1-2 complete, moving toward Phase 3 (visualization polish)

- JUCE-only approach enabled **fast prototyping**
- No roadmap dependency on SDK (unlike OCC)
- Can ship independently

**Conclusion:** JUCE-only accelerated development as intended.

---

## 3. Decision Matrix Application

Using ORP106's decision matrix:

| Use Case                    | Recommendation         | FreqFinder Match                                |
| --------------------------- | ---------------------- | ----------------------------------------------- |
| **Simple file analyzer**    | JUCE-only sufficient   | ✅ **MATCH** (but synthesis, not file analysis) |
| **Live input analyzer**     | JUCE-only sufficient   | ✅ **MATCH** (real-time synthesis + analysis)   |
| **Multi-stream analyzer**   | JUCE + SDK recommended | ❌ No match (single stream)                     |
| **Network audio analyzer**  | SDK required           | ❌ No match (no network audio)                  |
| **Session-aware analyzer**  | SDK required           | ❌ No match (no session management)             |
| **Deterministic analysis**  | JUCE + SDK recommended | ❌ No match (no determinism needed)             |
| **Broadcast-safe analyzer** | JUCE + SDK recommended | ❌ No match (not broadcast-safe)                |

**Verdict:** FreqFinder maps to **"JUCE-only sufficient"** use cases.

---

## 4. Why FreqFinder Does NOT Use Orpheus SDK

### 4.1 Technical Reasons

**1. No Session Management Requirements**

FreqFinder doesn't need:

- Multi-file coordination
- Session save/load (just plugin parameters)
- Clip scheduling
- Transport control

**JUCE provides:** `AudioProcessorValueTreeState` for parameter automation (sufficient)

---

**2. No Network Audio Requirements**

FreqFinder doesn't need:

- AES67/Dante streams
- RTP audio
- Network device discovery
- PTP clock sync

**JUCE provides:** Standard audio device I/O via `AudioDeviceManager` (sufficient)

---

**3. No Deterministic Rendering Requirements**

FreqFinder doesn't need:

- Bit-identical analysis results
- Offline rendering
- Reproducible output for compliance

**JUCE provides:** Real-time synthesis with `juce::dsp::Oscillator` (sufficient)

---

**4. Simple Signal Flow**

FreqFinder's architecture:

```
MIDI Input → Note Processor
                  ↓
         Oscillator Bank (7 oscillators)
                  ↓
            Master Gain
                  ↓
         ┌────────┴────────┐
         ↓                 ↓
   Spectrum Analyzer   Audio Output
```

This is **trivial** compared to OCC's complex routing matrix. JUCE handles this easily.

---

**5. No Multi-Stream Coordination**

- OCC: Plays 16+ clips simultaneously with sample-accurate sync
- FreqFinder: 7 oscillators, all internally generated

**Conclusion:** JUCE's audio callback is sufficient. No need for SDK's TransportController.

---

### 4.2 Strategic Reasons

**1. Faster Development**

- JUCE is well-documented, widely known
- No SDK learning curve
- Phases 1-2 completed rapidly (~2,500 lines)

---

**2. Lower Maintenance Burden**

- JUCE updates are predictable
- No SDK release cycle dependency
- Simpler codebase to maintain

---

**3. Smaller Binary Size**

- JUCE-only: ~50-100 MB plugin
- JUCE + SDK: ~100-200 MB (estimated)

For a simple tool, smaller is better.

---

**4. Independent Release Cycle**

- FreqFinder can ship without waiting for SDK releases
- No breaking changes from SDK updates
- No API stability concerns

---

**5. Appropriate Architecture for Scope**

**Orpheus SDK is designed for:**

- Multi-clip playback
- Session management
- Network audio
- Broadcast-safe operation
- Deterministic rendering

**FreqFinder needs:**

- 7 oscillators
- FFT analysis
- Parameter automation

**Verdict:** Using SDK would be **architectural over-engineering**.

---

## 5. FreqFinder's Position in the Orpheus Ecosystem

### 5.1 Brand Family

```
Orpheus SDK Applications (First-Party):
├── Clip Composer (OCC)  — Professional soundboard [JUCE + SDK]
├── FreqFinder           — Harmonic calculator & frequency scope [JUCE-only]
└── FX Engine (planned)  — LLM-powered effects [Architecture TBD]
```

### 5.2 Ecosystem Positioning

**Shared:**

- ✅ Brand identity ("Orpheus SDK Applications")
- ✅ Company name ("OrpheusSDK")
- ✅ Design philosophy (professional audio tools)
- ✅ Target audience (broadcast engineers, sound designers)

**Independent:**

- ❌ Codebase (separate repositories)
- ❌ Build system (FreqFinder uses standalone CMake)
- ❌ Dependencies (JUCE-only vs JUCE+SDK)
- ❌ Architecture (simple vs complex)
- ❌ Use cases (synthesis vs playback)

---

### 5.3 Workflow Integration (Optional)

**Potential Use Case:**

1. Engineer uses **Clip Composer** to play back audio clips in a broadcast environment
2. Engineer uses **FreqFinder** to:
   - Generate reference tones at specific frequencies
   - Identify problematic frequencies in a mix (via sidechain)
   - Explore harmonic relationships for sound design

**Key Point:** These tools work **alongside** each other, not **integrated** with each other.

Analogy:

- **Adobe Photoshop** (image editing) + **Adobe Illustrator** (vector graphics)
- Same brand, same suite, but separate applications with independent architectures
- Users can use both in a workflow, but they don't share file formats or session data

---

## 6. Comparison to Orpheus Clip Composer (OCC)

### 6.1 Architecture Comparison

| Aspect                 | Clip Composer (OCC)                       | FreqFinder                             |
| ---------------------- | ----------------------------------------- | -------------------------------------- |
| **Architecture**       | JUCE UI + Orpheus SDK audio core          | JUCE-only                              |
| **Audio Engine**       | SDK's SessionGraph, TransportController   | JUCE's Oscillator, FFT                 |
| **Use Case**           | Multi-clip playback, broadcast soundboard | Harmonic synthesis, frequency analysis |
| **Session Management** | YES (save/load complex sessions)          | NO (just plugin parameters)            |
| **Multi-Stream**       | YES (16+ clips simultaneously)            | NO (7 oscillators, single output)      |
| **Network Audio**      | YES (AES67/Dante planned)                 | NO (not required)                      |
| **Deterministic**      | YES (broadcast-safe)                      | NO (real-time synthesis)               |
| **Complexity**         | High (complex routing, transport)         | Low (simple signal flow)               |
| **Binary Size**        | ~100-200 MB (estimated)                   | ~50-100 MB (estimated)                 |
| **Development Time**   | Longer (SDK integration)                  | Faster (JUCE-only)                     |

---

### 6.2 Why Different Architectures Make Sense

**Clip Composer NEEDS SDK because:**

- Multi-clip session management (SessionGraph)
- Sample-accurate transport (TransportController)
- Network audio support (AES67 driver)
- Broadcast-safe architecture (zero-allocation audio thread)
- Deterministic rendering (reproducible output)

**FreqFinder DOESN'T NEED SDK because:**

- No session management (just 7 oscillators)
- No transport control (immediate synthesis)
- No network audio (standard device I/O)
- No broadcast requirements (educational/mixing tool)
- No deterministic rendering (real-time only)

**Conclusion:** Different tools, different requirements, different architectures. **Both decisions are correct.**

---

## 7. Validation Against ORP106's Decision Framework

### 7.1 Section 5: Make/Buy Decision Framework

From ORP106's table:

| Feature                   | JUCE Built-in               | Orpheus SDK | FreqFinder Choice |
| ------------------------- | --------------------------- | ----------- | ----------------- |
| **FFT computation**       | ✅ `juce::dsp::FFT`         | ?           | ✅ **JUCE**       |
| **Audio file reading**    | ✅ `AudioFormatReader`      | ✅ SDK      | ❌ **Not needed** |
| **Live audio input**      | ✅ `AudioDeviceManager`     | ✅ SDK      | ✅ **JUCE**       |
| **Transport control**     | ✅ `AudioTransportSource`   | ✅ SDK      | ❌ **Not needed** |
| **Multi-stream mixing**   | ✅ `AudioSourceChannelInfo` | ✅ SDK      | ❌ **Not needed** |
| **Session management**    | ⚠️ Custom                   | ✅ SDK      | ❌ **Not needed** |
| **Network audio (AES67)** | ❌ Not available            | ✅ SDK      | ❌ **Not needed** |

**Verdict:** For every feature FreqFinder needs, **JUCE provides a sufficient solution**.

---

### 7.2 Section 6: Risk Assessment

**Q6.1: If FreqFinder uses JUCE-only, what features become harder/impossible?**

- Network audio support (AES67)? → **Not required**
- Session compatibility with OCC? → **Not required**
- Deterministic analysis? → **Not required**
- Multi-stream handling? → **Not required**

**Conclusion:** No features are lost that FreqFinder actually needs.

---

**Q6.2: If FreqFinder uses Orpheus SDK, what's the overhead?**

- Learning curve for SDK APIs? → **High** (unnecessary complexity)
- Binary size increase? → **~2x larger** (50MB → 100MB)
- Additional dependencies? → **Yes** (SDK build dependency)
- Maintenance burden? → **Higher** (SDK release cycle)

**Conclusion:** SDK adds overhead **without providing value**.

---

**Q6.3: What's the migration path if FreqFinder starts JUCE-only and needs SDK later?**

**Answer:** Easy to add if requirements change

Hypothetical scenario: FreqFinder v2.0 adds network audio analysis (AES67 streams)

Migration path:

1. Add Orpheus SDK to CMakeLists.txt
2. Replace `AudioDeviceManager` with SDK's network audio driver
3. Keep JUCE UI components (no change needed)
4. Refactor audio callback to use SDK's lock-free command pattern

**Estimated effort:** 2-4 weeks for experienced developer

**Conclusion:** Starting JUCE-only doesn't lock FreqFinder into that architecture forever. Migration is **feasible if needed**.

---

## 8. Answers for Claude Code Web Agent

### 8.1 Core Questions

**Q: How does FreqFinder fit into the Orpheus family?**

**A:** FreqFinder is a **brand member** of the "Orpheus SDK Applications" family but **not a technical member** of the Orpheus SDK codebase. It shares:

- Brand identity ("OrpheusSDK" company name)
- Target audience (professional audio users)
- Design philosophy (professional tools)

But maintains:

- Separate repository (`~/dev/freqfinder/`)
- Independent architecture (JUCE-only)
- No code dependencies on Orpheus SDK

**Analogy:** Like Microsoft Office applications—same brand, same ecosystem, but Word and Excel have different architectures under the hood.

---

**Q: Does FreqFinder make use of the Orpheus SDK?**

**A:** ❌ **NO**

**Evidence:**

- CMakeLists.txt shows **only JUCE dependencies** (no Orpheus SDK)
- Source code uses **only JUCE classes** (`juce::dsp::Oscillator`, `juce::dsp::FFT`, etc.)
- No Orpheus SDK headers included
- No SDK classes used (no SessionGraph, TransportController, etc.)

---

**Q: Why doesn't FreqFinder use Orpheus SDK?**

**A:** Because its requirements don't justify the complexity

**FreqFinder is:**

- A simple oscillator synthesizer (7 sine waves)
- A real-time FFT analyzer
- A single-stream tool
- An educational/mixing aid

**Orpheus SDK provides:**

- Multi-clip session management → **Not needed** (single stream)
- Sample-accurate transport → **Not needed** (immediate synthesis)
- Network audio (AES67) → **Not needed** (standard device I/O)
- Deterministic rendering → **Not needed** (real-time only)
- Broadcast-safe architecture → **Nice to have, but not required**

**Conclusion:** JUCE provides everything FreqFinder needs. SDK would add complexity without benefit.

---

**Q: Is this the right decision?**

**A:** ✅ **YES**, validated by ORP106's decision framework

From ORP106's decision matrix:

- FreqFinder maps to **"Simple/Live analyzer"** use case → **JUCE-only sufficient** ✅
- Does NOT map to **"Multi-stream/Network/Session-aware"** use cases → **SDK not required** ✅

**Supporting evidence:**

- Development completed rapidly (Phases 1-2 done, ~2,500 lines)
- Architecture is simple and maintainable
- No features are missing that users require
- Binary size is smaller (better user experience)
- Release cycle is independent (faster iteration)

---

### 8.2 Strategic Justification

**Why have an "Orpheus SDK Applications" family if they don't all use the SDK?**

**Answer:** Brand positioning ≠ Technical architecture

**Brand Strategy:**

- **"Orpheus SDK"** is a professional audio tools ecosystem
- **"Applications"** are first-party tools built by the Orpheus team
- Some apps use the SDK (Clip Composer), some don't (FreqFinder)
- Users see a cohesive family of tools, not individual technologies

**Technical Strategy:**

- Each tool uses the **right architecture** for its requirements
- OCC needs SDK (complex playback, session management)
- FreqFinder doesn't (simple synthesis, no session complexity)
- Future apps (FX Engine?) will decide based on their needs

**Analogy:** Adobe Creative Cloud

- All apps are "Adobe" brand
- Photoshop, Illustrator, Premiere Pro have **different architectures**
- Some share libraries (PDF rendering), others don't
- Users see a unified suite, developers see independent codebases

---

## 9. Recommendations for Orpheus Development Planning

### 9.1 Documentation Clarity

**Issue:** ORP106 lists "Wave Finder (formerly FreqFinder, prototyped as Hcalc)" in the Orpheus SDK roadmap, implying integration

**Recommendation:** Update Orpheus SDK roadmap to clarify:

```markdown
# Orpheus SDK Roadmap

## First-Party Applications

### Applications Using Orpheus SDK

- **Clip Composer (OCC)** — Professional soundboard
  - Architecture: JUCE UI + Orpheus SDK audio core
  - Status: v1.0-rc.1 complete

### Applications Using JUCE-Only

- **FreqFinder** — Harmonic calculator & frequency scope
  - Architecture: JUCE-only (no SDK dependencies)
  - Status: v0.2.1, Phases 1-2 complete
  - Rationale: Simple synthesis tool, SDK overhead not justified

### Applications (Architecture TBD)

- **FX Engine** — LLM-powered effects processing
  - Decision pending requirements analysis
```

This clarifies that "Orpheus SDK Applications" is a **brand family**, not a **technical requirement**.

---

### 9.2 Future Architecture Decisions

For future Orpheus applications (FX Engine, DAW, etc.), use ORP106's decision framework:

**Questions to ask:**

1. Does it need multi-file/multi-stream session management? → **SDK helpful**
2. Does it need network audio (AES67/Dante)? → **SDK required**
3. Does it need deterministic rendering? → **SDK helpful**
4. Does it need sample-accurate transport? → **SDK helpful**
5. Does it need to share sessions with OCC? → **SDK required**

**If most answers are NO:**

- Use JUCE-only (like FreqFinder)
- Faster development, simpler codebase
- Maintain brand association without technical coupling

**If most answers are YES:**

- Use JUCE + SDK (like OCC)
- Leverage SDK's session management and transport
- Maintain ecosystem consistency

---

### 9.3 Repository Organization

**Current Structure:**

```
~/dev/
├── orpheus-sdk/           # SDK repository
│   ├── apps/
│   │   └── clip-composer/ # OCC (uses SDK)
│   └── sdk/               # Core SDK
└── freqfinder/            # Separate repo (JUCE-only)
```

**Recommendation:** Consider organizing as:

**Option A: Keep separate (current approach)**

- ✅ FreqFinder has independent release cycle
- ✅ No SDK build dependency
- ❌ Appears less "integrated" in ecosystem

**Option B: Monorepo with optional SDK**

```
~/dev/orpheus/
├── sdk/                   # Orpheus SDK core
├── apps/
│   ├── clip-composer/     # Uses SDK
│   └── freqfinder/        # JUCE-only (no SDK dependency)
```

- ✅ Unified ecosystem appearance
- ✅ Shared build infrastructure (optional)
- ❌ FreqFinder might get SDK updates it doesn't need

**Verdict:** Current approach (separate repos) is **fine** given architectural independence. No change needed unless ecosystem grows significantly (5+ applications).

---

## 10. Conclusion

### 10.1 Summary

**FreqFinder's relationship to Orpheus SDK:**

- **Brand:** Member of "Orpheus SDK Applications" family ✅
- **Technical:** No code dependencies on Orpheus SDK ❌
- **Architecture:** JUCE-only (appropriate for requirements) ✅
- **Ecosystem:** Works alongside OCC, not integrated with it ✅

**Decision validation:**

- Matches ORP106's "JUCE-only sufficient" use case ✅
- Development completed rapidly (Phases 1-2 done) ✅
- No missing features that users require ✅
- Lower complexity and maintenance burden ✅

---

### 10.2 Key Takeaways for Claude Code Web Agent

1. **Brand ≠ Architecture:** "Orpheus SDK Applications" is marketing, not a technical requirement
2. **Right tool for the job:** OCC needs SDK (complex playback), FreqFinder doesn't (simple synthesis)
3. **Validated decision:** ORP106's framework confirms JUCE-only is appropriate
4. **Independent but related:** FreqFinder shares ecosystem positioning but not codebase
5. **Migration possible:** If requirements change, adding SDK is feasible but currently unnecessary

---

### 10.3 Recommended Actions

**For Orpheus SDK documentation:**

- Update roadmap to clarify which apps use SDK vs JUCE-only
- Add architecture decision records (like this document)
- Document brand vs technical relationship clearly

**For FreqFinder development:**

- Continue with JUCE-only approach (no changes needed)
- Complete Phases 3-5 as planned (no SDK integration)
- Maintain brand association via company name and ecosystem positioning

**For future Orpheus apps:**

- Use ORP106's decision framework for architecture choices
- Don't assume all apps must use SDK (evaluate requirements)
- Maintain brand consistency while optimizing technical architecture

---

## References

[1] ORP106: Wave Finder Architecture Assessment - JUCE vs SDK Integration (Orpheus SDK repo)
[2] FRQ001: Freqfinder Product Specification v0.2.1
[3] FRQ003: Prototype Implementation and Float Multiplier Migration
[4] IMPLEMENTATION.md: Freqfinder Implementation Progress (Phases 1-2)
[5] CMakeLists.txt: FreqFinder build configuration (JUCE 8.0.4, no SDK)

---

**Document Version:** 1.0
**Status:** Final
**Next Review:** When FreqFinder requirements change OR when ORP106 is updated
**Approval:** Awaiting review by Orpheus SDK planning team

---

**END OF DOCUMENT**
