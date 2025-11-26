# Orpheus SDK - Gemini 3 Pro Assistant Guide

**Target Model:** Google Gemini 3 Pro (multimodal)
**Purpose:** Guide Gemini 3 Pro in assisting with professional audio SDK development
**Repo:** orpheus-sdk (Professional C++20 audio SDK)
**Documentation PREFIX:** ORP (SDK), OCC (Clip Composer app)

Professional audio SDK with host-neutral C++20 core for deterministic session, transport, and render management.

---

## Project Context

### Core Principles (NON-NEGOTIABLE)

1. **Offline-first** — No runtime network calls for core features
2. **Deterministic** — Same input → same output, always (sample-accurate, bit-identical)
3. **Host-neutral** — Core SDK works across REAPER, standalone apps, plugins, embedded
4. **Broadcast-safe** — 24/7 reliability, zero audio thread allocations

**Decision Framework:** Before suggesting any change, ask:

- Will this work offline? (If no → wrong for core SDK)
- Is this deterministic? (If no → cannot be in render path)
- Is this host-neutral? (If no → belongs in adapter layer)
- For all applications? (If no → app-specific, goes in `apps/`)

---

## Repository Structure

```
orpheus-sdk/
├── src/, include/       # Core SDK (C++20) - deterministic, portable
├── adapters/            # Host integrations (REAPER, minhost, realtime_engine)
├── apps/                # Applications (Clip Composer, Wave Finder, FX Engine)
│   └── clip-composer/   # Desktop app (JUCE 8.0.4)
│       └── GEMINI.md    # Clip Composer-specific guidance
├── packages/
│   └── shmui-juce/      # UX package - JUCE audio visualization components
├── tests/               # GoogleTest suite
├── docs/                # Architecture, roadmap, guides
│   └── orp/             # ORP-prefixed SDK documentation
├── CLAUDE.md            # Claude Code config (reference for conventions)
└── GEMINI.md            # This file
```

## UX Package: shmui-juce

**Location:** `packages/shmui-juce/` | **Upstream:** `~/dev/shmui` (dual-stack library)

Orpheus SDK integrates **shmui JUCE components** as a first-party UX package for application-level audio visualization. The upstream shmui repository maintains both React (web) and JUCE (native) components.

**Available Components:**

- `AudioAnalyzer` - FFT, RMS, frequency band analysis (thread-safe, broadcast-safe compatible)
- `WaveformVisualizer` - Multiple waveform display variants
- `BarVisualizer` - Frequency band bar display with state animations
- `OrbVisualizer` - OpenGL shader-based 3D orb visualization
- `MatrixDisplay` - LED-style matrix display with animations

**Threading Model (CRITICAL for Gemini):**

- `AudioAnalyzer` uses lock-free atomics → safe for Orpheus audio threads
- Visualization components require JUCE message thread → use `juce::MessageManager::callAsync()`
- NO allocations on audio thread (compatible with Orpheus broadcast-safe constraints)

**When to Suggest shmui:**

- ✅ App-level UI (Clip Composer, Wave Finder, FX Engine)
- ✅ Audio feedback, level meters, waveform displays
- ✅ User-facing visualizations
- ❌ Core SDK (SDK is UI-agnostic, host-neutral)

**See Also:**

- Full shmui docs: `~/dev/shmui/GEMINI.md`, `~/dev/shmui/.codex/AGENTS.md`
- JUCE source: `packages/shmui-juce/ShmUI.h` (after integration)
- Upstream shmui: https://ui.elevenlabs.io/ (React components)

**Documentation Prefixes:**

- **ORP** (`docs/orp/`) - Orpheus SDK core documentation
- **OCC** (`apps/clip-composer/docs/occ/`) - Clip Composer app documentation

---

## Audio Code Rules (CRITICAL)

**Broadcast-Safe Threading:**

- **ZERO allocations** on audio thread (no `new`, `malloc`, `std::vector::push_back`, `std::string`)
- **ZERO locks** on audio thread (no `std::mutex`, `std::lock_guard`)
- **ZERO I/O** on audio thread (no file reads, network calls)
- **ZERO system calls** (no `time()`, `rand()`, non-deterministic sources)

**Determinism:**

- 64-bit sample counts (never float seconds)
- Bit-identical output across platforms
- `std::bit_cast` for float determinism
- No system clock or timing in render path

**Quality:**

- C++20 standard, passes `clang-format` (CI enforced)
- AddressSanitizer + UBSan on Debug builds
- Adapters ≤300 LOC when possible

**Why this matters:** Audio threads have microsecond deadlines. Any violation causes glitches, dropouts, crashes.

---

## Quick Commands

**Full reference:** `docs/repo-commands.html` (open in browser)

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Test
ctest --test-dir build --output-on-failure

# Run Clip Composer (ALWAYS use script, not `open`)
./scripts/relaunch-occ.sh
```

---

## File Placement

| Type     | Location           | Example                  |
| -------- | ------------------ | ------------------------ |
| Core     | `src/`, `include/` | `SessionGraph.cpp`       |
| Adapters | `adapters/`        | `reaper_adapter.cpp`     |
| Apps     | `apps/`            | `orpheus_clip_composer/` |
| Docs     | `docs/`            | `ARCHITECTURE.md`        |

---

## Do / Don't

### ✅ Do

- Maintain host-neutral determinism
- Sample-accurate rendering, clock isolation
- Pre-allocate all audio thread data structures
- Write unit tests, profile before optimizing
- Follow `.clang-format` and `.clang-tidy`
- Add Doxygen for public APIs

### ❌ Don't

- Break core/adapter abstraction
- Suggest SaaS dependencies for core features
- Allocate in audio threads
- Use floating-point time (use sample counts)
- Commit secrets or large binaries
- Add network calls to core features

---

## Gemini 3 Pro-Specific Guidance

### Leveraging Your Multimodal Capabilities

**When user provides images:**

- Architecture diagrams → Analyze component relationships
- Build errors → Parse error messages, suggest fixes
- Profiler output (flamegraphs) → Identify bottlenecks
- UI mockups → JUCE implementation guidance

**When working with audio:**

- Audio files with glitches → Describe artifacts, suggest causes
- Waveform images → Analyze signal characteristics

**Best practices:**

- Be explicit about uncertainty (especially audio thread safety)
- Prioritize correctness over cleverness
- Audio code must be simple, predictable, debuggable
- Use network access for C++20 docs, audio engineering standards

---

## Example Workflows

### Debugging Audio Thread Issues

1. Review threading model in `docs/ARCHITECTURE.md`
2. Search for allocations: `grep -rn 'new\|malloc\|push_back' src/`
3. Check ORP docs: `docs/orp/ORP068 Implementation Plan (v2.0).md`
4. Test with multiple buffer sizes (128, 256, 512, 1024)
5. Run with sanitizers: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && ctest --test-dir build`

### Suggesting Code Changes

1. **Read existing code first** - understand current patterns
2. **Check docs** - is there architectural rationale?
3. **Provide reasoning** - explain _why_ it's better
4. **Consider implications** - determinism, offline-first, host-neutrality

---

## Current Status

**ORP068 Progress:** 55/104 tasks (52.9%)

- ✅ Phase 0-3 (Repo, Driver, UI, CI)
- ⏳ Phase 4: Docs/productionization (0/14)

**Key Documents:**

- `.claude/implementation_progress.md` - Current sprint status
- `docs/orp/ORP068 Implementation Plan (v2.0).md` - Master plan
- `docs/ARCHITECTURE.md` - System design
- `docs/ROADMAP.md` - Timeline

### Clip Composer (OCC)

**Status:** v0.2.0 Sprint Complete (OCC093) ✅
**Docs:** `apps/clip-composer/docs/occ/`
**See:** `apps/clip-composer/GEMINI.md` for app-specific guidance

---

## Documentation Standards

### PREFIX Naming (CRITICAL)

**Pattern:** `{PREFIX###} {Verbose Title}.md`

**Examples:**

- ✅ `ORP082 Session Report.md`
- ✅ `OCC103 QA v020 Results.md`
- ❌ `ORP082.md` (missing title)

**Before creating docs:**

```bash
# Find highest ORP number
ls -1 docs/orp/ | grep -E '^ORP[0-9]{3,4}\s+' | sort -V | tail -1

# Find highest OCC number
ls -1 apps/clip-composer/docs/occ/ | grep -E '^OCC[0-9]{3,4}\s+' | sort -V | tail -1
```

### Automatic Documentation

When completing significant work:

1. Update `.claude/implementation_progress.md`
2. Create session report (if appropriate): `docs/orp/ORP### Title.md`
3. Update `docs/ARCHITECTURE.md` if structure changed

**What to document:**

- Completed work (files, line counts, components)
- Technical decisions and rationale
- Challenges and solutions
- Metrics (build time, test results)
- Next steps and blockers

---

## Multi-Instance Context

This repo supports separate contexts for SDK vs Application work:

**SDK Context:** Repository root (`~/dev/orpheus-sdk`)

- Focus: C++ core, adapters, transport/routing/session
- Docs: `docs/orp/` (ORP prefix)

**Clip Composer Context:** `apps/clip-composer/` subdirectory

- Focus: JUCE UI, desktop app, end-user workflows
- Docs: `apps/clip-composer/docs/occ/` (OCC prefix)
- See: `apps/clip-composer/GEMINI.md` for app-specific guidance

---

## File Boundaries

### Never Read

- `build-*/`, `apps/*/build/`, `orpheus_clip_composer_app_artefacts/`
- `Third-party/*/build/`, `*.o`, `*.a`, `*.dylib`, `*.so`

### Read First (Priority)

1. `.claude/implementation_progress.md` (current sprint)
2. `docs/orp/ORP068 Implementation Plan (v2.0).md` (master plan)
3. `docs/ARCHITECTURE.md` (system design)
4. This `GEMINI.md` file

---

## Context Efficiency

**Debugging:**

- Hypothesize 3 causes BEFORE reading files
- Search symbols first: `grep -rn 'SymbolName' src/`
- Build incrementally: `cmake --build build --target file.cpp.o`
- ASan crash on valid code? Try `rm -rf build` (stale runtime)

**Git:**

- `git log --oneline -10` (not `--all`)
- Focus on ONE commit at a time

**Files:**

- Search before reading: `grep -rn 'pattern' src/`
- Never read same file twice

---

## Git Workflow

**When committing:**

```bash
git status
git diff
git log --oneline -5  # Check message style
```

**Commit format:**

```
Brief descriptive message

Optional context.

🤖 Generated with Gemini 3 Pro
```

**Verify:** No secrets (`.env`, `credentials.json`, API keys)

---

## Using Network Access

**C++ Standard Library:**

- https://en.cppreference.com/ - C++20 reference
- https://isocpp.org/ - ISO C++ standards

**Audio Engineering:**

- https://tech.ebu.ch/publications - EBU specs
- https://www.smpte.org/ - SMPTE broadcast standards

**Build Systems:**

- https://cmake.org/documentation/ - CMake docs
- https://google.github.io/googletest/ - GoogleTest docs

**When to use:**

- Uncertain about C++20 behavior
- Need audio engineering formulas (gain curves, timecode)
- Looking up CMake patterns
- Researching JUCE APIs

---

## Additional Resources

**See `CLAUDE.md` for:**

- Detailed workflow conventions
- Cross-repo awareness patterns
- Sprint completion protocols
- Skills loading configuration

**Key docs:**

- `docs/ARCHITECTURE.md` - System design, threading model
- `docs/ROADMAP.md` - Development timeline
- `docs/repo-commands.html` - Full command reference

---

**Version:** 2.0 (Lean, project-focused)
**Last Updated:** 2025-11-25
**Model Target:** Google Gemini 3 Pro (multimodal)
**Environment:** Sandboxed to repository, network access enabled

**Remember:** Orpheus SDK is a professional tool for broadcast and live performance. Prioritize determinism, offline-first design, and broadcast-safety (ZERO allocations on audio thread). When uncertain about threading or audio implications, ask before implementing.
