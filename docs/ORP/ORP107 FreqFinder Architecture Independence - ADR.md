# ORP107: FreqFinder Architecture Independence (ADR)

**Status:** Architecture Decision Record
**Created:** 2025-11-09
**Author:** Claude (Orpheus SDK Research Agent)
**Context:** Clarifying FreqFinder's relationship to Orpheus SDK ecosystem
**Related Documents:** ORP106 (Architecture Assessment), FRQ004 (FreqFinder's response)

---

## Decision

**FreqFinder** (Harmonic Calculator & Frequency Scope) is a member of the **"Orpheus SDK Applications"** brand family but **does NOT use Orpheus SDK** as a technical dependency.

**Architecture:** JUCE-only (no SDK integration)

**Repository:** Separate (`~/dev/freqfinder/`)

---

## Context

FreqFinder was initially listed in Orpheus SDK roadmap documents as a planned application, creating potential confusion about whether it would integrate with the SDK core (like Clip Composer) or remain architecturally independent.

**ORP106** provided an assessment framework to evaluate whether applications should use:
- **JUCE-only** (simple, lightweight)
- **JUCE UI + Orpheus SDK audio core** (complex, feature-rich)

FreqFinder's development team provided a comprehensive response in **FRQ004** (FreqFinder repo), analyzing their requirements against ORP106's decision matrix.

---

## Rationale

### FreqFinder's Requirements

**What FreqFinder does:**
- Real-time oscillator synthesis (7 sine wave generators)
- FFT analysis (8192-point harmonic detection)
- MIDI input handling (note-on sets frequency)
- Single fundamental frequency at a time
- Educational/mixing workflow tool

**What FreqFinder does NOT need:**
- ❌ Multi-stream session management (single tone generator)
- ❌ Network audio support (standard device I/O sufficient)
- ❌ Deterministic rendering (real-time synthesis only)
- ❌ Sample-accurate transport control (immediate MIDI response)
- ❌ Session compatibility with OCC (independent workflow)
- ❌ Broadcast-safe architecture (not mission-critical)

### Why JUCE-Only is Appropriate

**JUCE provides everything FreqFinder needs:**
- ✅ `juce::dsp::FFT` - Real-time FFT analysis
- ✅ `juce::dsp::Oscillator` - Sine wave synthesis
- ✅ `juce::MidiMessage` - MIDI input handling
- ✅ `AudioProcessorValueTreeState` - Parameter automation
- ✅ `AudioDeviceManager` - Standard audio I/O

**Orpheus SDK would add complexity without value:**
- SessionGraph - Not needed (no multi-clip coordination)
- TransportController - Not needed (no playback transport)
- RoutingMatrix - Not needed (simple 7-oscillator sum)
- AES67 Driver - Not needed (standard device I/O)
- Deterministic rendering - Not needed (real-time only)

### Validation via ORP106 Decision Matrix

From ORP106's use case matrix:

| Use Case | Recommendation | FreqFinder Match |
|----------|----------------|------------------|
| **Simple file analyzer** | JUCE-only sufficient | ✅ (synthesis, not file) |
| **Live input analyzer** | JUCE-only sufficient | ✅ **MATCH** |
| **Multi-stream analyzer** | JUCE + SDK recommended | ❌ (single stream) |
| **Network audio analyzer** | SDK required | ❌ (not required) |
| **Session-aware analyzer** | SDK required | ❌ (no sessions) |
| **Deterministic analysis** | JUCE + SDK recommended | ❌ (real-time only) |
| **Broadcast-safe analyzer** | JUCE + SDK recommended | ❌ (not broadcast) |

**Verdict:** FreqFinder maps to **"JUCE-only sufficient"** use cases.

---

## Comparison to Clip Composer (OCC)

### Why Different Architectures Make Sense

| Aspect | Clip Composer (OCC) | FreqFinder |
|--------|---------------------|------------|
| **Architecture** | JUCE UI + SDK audio core | JUCE-only |
| **Use Case** | Multi-clip broadcast soundboard | Harmonic synthesis tool |
| **Session Management** | YES (save/load complex sessions) | NO (plugin parameters only) |
| **Multi-Stream** | YES (16+ clips simultaneously) | NO (7 oscillators, single output) |
| **Network Audio Compatibility** | YES (via manufacturer drivers) | NO (standard I/O) |
| **Deterministic** | YES (broadcast-safe) | NO (real-time synthesis) |
| **Transport Control** | YES (sample-accurate cues) | NO (immediate MIDI) |
| **Complexity** | High (routing matrix, transport) | Low (simple signal flow) |

**Key Insight:** Different tools, different requirements, different architectures. **Both decisions are correct.**

---

## Consequences

### Positive

✅ **Faster development** - Phases 1-2 complete (~2,500 lines C++20)
- JUCE is well-documented, widely adopted
- No SDK learning curve required
- Straightforward codebase

✅ **Lower maintenance burden**
- No SDK release cycle dependency
- Simpler codebase to maintain
- Independent release schedule

✅ **Smaller binary size** - ~50-100 MB (vs ~100-200 MB with SDK)
- Better download/install experience
- Lower disk footprint

✅ **Appropriate complexity**
- Architecture matches problem scope
- No over-engineering
- Easier onboarding for contributors

### Neutral

⚪ **Brand association without technical coupling**
- FreqFinder is an "Orpheus SDK Application" (brand)
- But architecturally independent
- Users see cohesive ecosystem, developers see independent codebases

### Trade-offs

⚠️ **No session compatibility with OCC**
- Users cannot "open OCC session in FreqFinder"
- But: No use case requires this (FreqFinder generates tones, OCC plays clips)

⚠️ **No network audio support**
- Cannot analyze AES67/Dante streams
- But: Not a FreqFinder requirement (sidechain planned for Phase 4 is simple local input)

⚠️ **Migration effort if requirements change**
- If FreqFinder v2.0 needs SDK features (multi-stream analysis, network audio)
- Migration estimated at 2-4 weeks for experienced developer
- But: Feasible if needed, not locked into JUCE-only forever

---

## Migration Path (If Requirements Change)

**Hypothetical scenario:** FreqFinder v2.0 adds network audio analysis (AES67 streams)

**Migration steps:**
1. Add Orpheus SDK to CMakeLists.txt (`FetchContent` or submodule)
2. Replace `AudioDeviceManager` with SDK's AES67 network driver
3. Add `SessionGraph` if multi-stream session management needed
4. Keep JUCE UI components (no changes required)
5. Refactor audio callback to use SDK's lock-free command pattern

**Estimated effort:** 2-4 weeks (experienced developer)

**Key point:** Starting JUCE-only does not prevent future SDK integration if requirements evolve.

---

## Implementation Status

**FreqFinder v0.2.1:**
- ✅ Phase 1 complete - Oscillator synthesis (7 sine waves)
- ✅ Phase 2 complete - FFT analysis (8192-point)
- ⏳ Phase 3 in progress - Visualization polish
- 📋 Phase 4 planned - Sidechain input (simple local input, no network audio)

**Dependencies:**
```cmake
# From FreqFinder's CMakeLists.txt
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)

target_link_libraries(FreqFinder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
)
```

**No Orpheus SDK references found.**

---

## Ecosystem Positioning

### Brand Family: "Orpheus SDK Applications"

```
First-party applications:
├── Clip Composer (OCC)  — Professional soundboard [JUCE + SDK]
├── FreqFinder           — Harmonic calculator & frequency scope [JUCE-only]
└── FX Engine (planned)  — LLM-powered effects [Architecture TBD]
```

### Shared (Brand)

- ✅ Brand identity: "Orpheus SDK Applications"
- ✅ Company name: "OrpheusSDK"
- ✅ Design philosophy: Professional audio tools
- ✅ Target audience: Broadcast engineers, sound designers, educators

### Independent (Technical)

- ❌ Codebase: Separate repositories
- ❌ Build system: Independent CMake configurations
- ❌ Dependencies: JUCE-only vs JUCE+SDK
- ❌ Architecture: Simple (FreqFinder) vs complex (OCC)
- ❌ Release cycle: Independent versioning

### Workflow Integration (Optional)

**Potential use case:**
1. Engineer uses **Clip Composer** for broadcast audio playout
2. Engineer uses **FreqFinder** to:
   - Generate reference tones at specific frequencies
   - Identify problematic frequencies (via sidechain, Phase 4)
   - Explore harmonic relationships for sound design

**Key point:** Tools work **alongside** each other, not **integrated** with each other.

**Analogy:** Adobe Creative Cloud
- Photoshop + Illustrator share brand/ecosystem
- But have independent architectures
- Users leverage both in workflows without technical integration

---

## Framework for Future Applications

This decision establishes precedent for future Orpheus applications.

**Decision Framework (from ORP106):**

For each new app, evaluate:
1. **Multi-stream session management needed?** → SDK helpful
2. **Network audio (AES67/Dante) required?** → SDK required
3. **Deterministic rendering needed?** → SDK helpful
4. **Sample-accurate transport control?** → SDK helpful
5. **Session compatibility with OCC?** → SDK required

**If most answers are NO:**
- Use JUCE-only (FreqFinder pattern)
- Faster development, simpler codebase
- Maintain brand association without technical coupling

**If most answers are YES:**
- Use JUCE + SDK (OCC pattern)
- Leverage SDK's session management and transport
- Maintain ecosystem consistency

---

## References

**External Documents (FreqFinder repo):**
- FRQ001: FreqFinder Product Specification v0.2.1
- FRQ003: Prototype Implementation and Float Multiplier Migration
- FRQ004: FreqFinder and Orpheus SDK Relationship Assessment (comprehensive analysis)
- IMPLEMENTATION.md: FreqFinder Implementation Progress (Phases 1-2)

**Orpheus SDK Documents:**
- ORP106: Wave Finder Architecture Assessment - JUCE vs SDK Integration (assessment framework)
- ORP068: Implementation Plan (v2.0) - Orpheus SDK roadmap
- ROADMAP.md: Orpheus ecosystem applications
- ARCHITECTURE.md: Host-neutral core SDK design philosophy

---

## Approval

**Status:** Final
**Approved By:** Orpheus SDK planning team (pending formal review)
**Next Review:** When FreqFinder requirements change OR when new applications begin planning

---

## Key Takeaway

**Brand consistency ≠ Technical coupling**

The "Orpheus SDK Applications" family is a **brand ecosystem**, not a **technical requirement**. Each application uses the architecture that best fits its requirements:

- **OCC** needs SDK (multi-clip playback, session management, broadcast-safe)
- **FreqFinder** doesn't (simple synthesis, single stream, educational tool)
- **Future apps** will decide based on ORP106 framework

This architectural flexibility enables:
- Faster development for simple tools
- Appropriate complexity for each use case
- Unified brand without monolithic architecture
- Independent release cycles and maintenance

**Both decisions are correct for their respective contexts.**

---

**Document Version:** 1.0
**Last Updated:** 2025-11-09
