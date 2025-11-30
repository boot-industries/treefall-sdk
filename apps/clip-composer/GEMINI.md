# Orpheus Clip Composer - Gemini 3 Pro Assistant Guide

**Target Model:** Google Gemini 3 Pro (multimodal)
**Location:** Nested app within orpheus-sdk (`apps/clip-composer/`)
**Documentation PREFIX:** OCC
**Framework:** JUCE 8.0.4 | **SDK:** Orpheus SDK M2

Professional soundboard for broadcast, theater, and live performance.

---

## Project Context

**Clip Composer (OCC)** is a desktop application for real-time audio playback in professional environments.

**5-Layer Architecture:**
1. **UI Components** (JUCE) - ClipGrid (960 buttons), waveforms, transport
2. **Application Logic** - SessionManager, ClipManager, RoutingManager
3. **SDK Integration** - ITransportController, IAudioFileReader, IRoutingMatrix
4. **Real-Time Audio** - CoreAudio/ASIO/WASAPI drivers
5. **Platform I/O** - Hardware audio interfaces

**Key Principle:** Layers 1-2 are OCC-specific. Layers 3-5 are Orpheus SDK (shared infrastructure).

**Status:**
- Version: v0.2.0 Sprint Complete (OCC093)
- Release: v0.1.0-alpha (October 22, 2025)

---

## Critical Constraints (UNIQUE TO OCC)

### 3-Thread Model

**1. Message Thread (JUCE MessageManager)**
- UI events, visual updates, SDK callbacks
- I/O operations allowed (session save/load)
- NO audio processing

**2. Audio Thread (IAudioDriver)**
- `processAudio()` callback (~10ms @ 512 samples)
- Mix clips, read pre-loaded audio, update transport
- ⛔ ZERO allocations, ZERO locks, ZERO I/O

**3. File I/O Thread (JUCE ThreadPool)**
- Pre-load audio for waveform display
- Scan directories, write recorded audio
- NO interaction with audio thread

**Thread Safety Verification:**
- Use `juce::MessageManager::callAsync()` for UI updates from background
- Never call JUCE UI components from audio thread
- SDK provides lock-free primitives (see `ITransportController`)

**For audio thread constraints (allocations, locks, I/O), see:** `../../GEMINI.md` (parent SDK guide)

---

## File Organization

```
apps/clip-composer/
├── GEMINI.md              # This file
├── .codex/AGENTS.md       # Codex CLI config
├── CLAUDE.md              # Claude Code config (reference)
├── CMakeLists.txt         # JUCE + SDK integration
├── Source/                # JUCE application code
│   ├── Main.cpp
│   ├── ClipGrid/          # 960-button grid
│   ├── AudioEngine/       # SDK integration layer
│   ├── Session/           # JSON session management
│   └── UI/                # Additional components
├── docs/occ/              # OCC-prefixed documentation
│   ├── OCC096.md          # SDK Integration Patterns
│   ├── OCC097.md          # Session Format
│   ├── OCC098.md          # UI Components
│   ├── OCC100.md          # Performance Requirements
│   ├── OCC101.md          # Troubleshooting Guide
│   └── OCC110.md          # Transport State and Loop Features
└── tests/                 # OCC-specific tests
```

**Parent SDK Documentation (accessible via relative paths):**
- `../../GEMINI.md` - Orpheus SDK guide for Gemini 3
- `../../docs/ARCHITECTURE.md` - SDK design rationale
- `../../docs/orp/` - ORP-prefixed SDK documentation

---

## Quick Commands

```bash
# Launch application (ALWAYS use script, not `open`)
../../scripts/relaunch-occ.sh

# Verify parent SDK
cd ../.. && ctest --test-dir build --output-on-failure
```

**Full reference:** `../../docs/repo-commands.html`

---

## Documentation Standards

**Pattern:** `{OCC###} {Verbose Title}.md`

**Examples:**
- ✅ `OCC096 SDK Integration Patterns.md`
- ✅ `OCC110 SDK Integration Guide - Transport State and Loop Features.md`
- ❌ `OCC096.md` (missing title)

**Find next number:**
```bash
ls -1 docs/occ/ | grep -E '^OCC[0-9]{3,4}\s+' | sort -V | tail -1
```

---

## Key Documentation (Read First)

**Implementation Reference:**
- `docs/occ/OCC096.md` - SDK Integration Patterns (code examples, anti-patterns)
- `docs/occ/OCC097.md` - Session Format (JSON schema, save/load)
- `docs/occ/OCC098.md` - UI Components (JUCE implementations)
- `docs/occ/OCC099.md` - Testing Strategy
- `docs/occ/OCC100.md` - Performance Requirements
- `docs/occ/OCC101.md` - Troubleshooting Guide
- `docs/occ/OCC110.md` - Transport State and Loop Features

**Product & Vision:**
- `docs/occ/OCC021.md` - Product Vision (market positioning)
- `docs/occ/OCC026.md` - MVP Definition (6-month plan)

**Technical Specifications:**
- `docs/occ/OCC027.md` - API Contracts (C++ interfaces OCC ↔ SDK)
- `docs/occ/OCC023.md` - Component Architecture (5-layer architecture)

**Parent SDK:**
- `../../GEMINI.md` - SDK guide for Gemini 3
- `../../docs/ARCHITECTURE.md` - System design, threading model
- `../../docs/orp/ORP068 Implementation Plan (v2.0).md` - Master SDK plan

---

## Performance Requirements

**From OCC100:**
- **Latency:** <5ms round-trip (audio input → processing → output)
- **CPU:** <30% with 16 simultaneous clips (Intel i5 8th gen)
- **Memory:** Stable after 1 hour operation (no leaks)
- **Stability:** Zero crashes in 24-hour test

---

## Gemini 3 Pro-Specific Guidance

**Leverage multimodal capabilities:**
- Architecture diagrams → Analyze layer boundaries, suggest improvements
- JUCE UI screenshots → Evaluate layout, threading implications
- Build errors → Parse errors, suggest fixes
- Profiler flamegraphs → Identify audio thread violations
- Waveform images → Analyze signal characteristics

**Network access for:**
- JUCE documentation: https://juce.com/learn/documentation
- JUCE forum: https://forum.juce.com/ (search for similar issues)
- Lock-free data structures: Research SPSC/MPSC queues
- Audio engineering: Buffer sizing, latency optimization

**Best practices:**
- FLAG immediately if solution allocates on audio thread
- ASK before suggesting changes if uncertain about thread context
- Prioritize broadcast-safe patterns over clever optimizations
- Explain *why* a solution is real-time safe

---

## See `CLAUDE.md` for:

- Detailed workflow conventions
- Git commit format and PR templates
- Multi-file editing strategy
- Custom slash commands
- Session management patterns

**Key docs:**
- `../../docs/ARCHITECTURE.md` - Threading model
- `../../docs/ROADMAP.md` - Timeline
- `../../docs/repo-commands.html` - Full command reference

---

**Version:** 2.0 (Lean, app-focused)
**Last Updated:** 2025-11-25
**Model Target:** Google Gemini 3 Pro (multimodal, November 2025)
**Environment:** Sandboxed to orpheus-sdk repository, network access enabled
**App Location:** Nested in `apps/clip-composer/`

**Remember:** Clip Composer is a professional tool for broadcast, theater, and live performance. Design for 24/7 reliability, ultra-low latency (<5ms), and zero crashes. ZERO allocations, ZERO locks, ZERO I/O on audio thread. When uncertain about threading or JUCE patterns, ask before implementing.
