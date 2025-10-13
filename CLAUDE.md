# Orpheus SDK Development Guide for Claude Code

**Workspace:** This repo inherits general conventions from `~/chrislyons/dev/CLAUDE.md`

Professional audio SDK with host-neutral C++20 core for deterministic session, transport, and render management. This guide helps Claude Code maintain SDK quality while respecting core architectural principles.

## Core Principles (Non-Negotiable)

1. **Offline-first** — No runtime network calls for core features
2. **Deterministic** — Same input → same output, always (sample-accurate, bit-identical)
3. **Host-neutral** — Core SDK works across REAPER, standalone apps, plugins, embedded systems
4. **Professional-grade** — Broadcast-safe (24/7 reliability, no audio thread allocations)

## Quick Reference: Repository Structure

```
├── src/, include/       # Core SDK (C++20) - deterministic, portable, host-neutral
├── adapters/            # Optional host integrations (REAPER, minhost CLI, etc.)
│   ├── reaper_orpheus/  # REAPER extension (≤300 LOC ideal)
│   ├── minhost/         # CLI testing host
│   ├── realtime_engine/ # Real-time I/O wrappers (future)
│   └── clip_grid/       # Soundboard logic (future)
├── apps/                # Applications (Clip Composer, Wave Finder, FX Engine)
├── packages/shmui/      # UI demos (Next.js/React, local-mocked, no SaaS)
├── tests/               # GoogleTest suite
└── docs/                # Architecture, roadmap, adapter guides
```

**Layer Philosophy:**

- **Core SDK** = `/src`, `/include` — minimal, deterministic primitives
- **Adapters** = `/adapters/*` — optional, platform-specific integrations
- **Applications** = `/apps/*` — compose adapters into complete solutions
- **UI prototypes** = `/packages/shmui` — demos only, not production

## Build Commands (CMake + pnpm)

### C++ Core

```bash
# Configure, build, test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Optional: Enable JUCE demo host
cmake -S . -B build -DORPHEUS_ENABLE_APP_JUCE_HOST=ON
cmake --build build --target orpheus_demo_host_app

# Optional: clang-tidy (not CI-blocking, but encouraged)
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build
```

### JavaScript Workspace (Shmui)

**Requirements:** Node.js 20+ (required by package engines)

```bash
pnpm install                  # Bootstrap workspace
pnpm --filter www dev         # Launch demo site (localhost:4000)
```

### Demo: Render Click Track

```bash
./build/orpheus_minhost \
  --session tools/fixtures/solo_click.json \
  --render click.wav \
  --bars 2 \
  --bpm 100
```

## Critical Rules for Audio Code

### Sample-Accurate Determinism

- Use **64-bit sample counts**, never floating-point seconds
- Same session must produce **bit-identical output** across platforms
- Use `std::bit_cast` for float determinism, avoid undefined behavior
- Test across Windows/macOS/Linux before committing

### Broadcast-Safe Requirements

- **No allocations in audio threads** (use lock-free structures, pre-allocate)
- **No network calls in render path**
- Clock domain isolation (prevent drift, maintain sync)
- Graceful degradation (never crash, log diagnostics)

### Code Quality Standards

- **C++20** with Edition 2021
- Must pass `clang-format` (CI enforced)
- `clang-tidy` encouraged (not CI-blocking)
- All tests pass with AddressSanitizer and UBSan on Debug builds
- Keep adapters **≤300 LOC** when possible

## Product Context: What We're Building

**Orpheus SDK** is the foundation for a sovereign audio ecosystem — professional-grade infrastructure for DAWs, soundboards, broadcast tools, and analysis applications.

**Think:**

- Audio foundation layer (like ffmpeg for audio workflows, but real-time + DAW semantics)
- Host-neutral core (portable across Pro Tools, REAPER, Logic, standalone apps)
- Professional reliability (broadcast-safe, 24/7 operational)
- Open architecture (MIT licensed, no vendor lock-in)

**First-party applications:**

- **Clip Composer** — Professional soundboard for broadcast/theater/live performance
- **Wave Finder** — Harmonic calculator and frequency scope
- **FX Engine** — LLM-powered effects processing
- **System tools** — Routing matrix, meters, diagnostics

**Design for 10+ years** — Code written today should work in 2035. Avoid trendy dependencies.

## SDK Components

### Core Features (src/, include/)

- `SessionGraph` — tracks, clips, tempo, transport
- Deterministic render path (offline or real-time)
- `AbiVersion` — safe cross-version communication
- Transport management (play, stop, record, loop, sync)

### Industry Standards

- REAPER SDK (ReaScript, extension API)
- Pro Tools parity goals (deterministic editing, punch/loop record)
- ADM/OTIO interchange (Audio Definition Model, OpenTimelineIO)

### Future Expansion

- ADM authoring (object-based audio metadata)
- OSC control integration
- Clip-grid scheduling (soundboard semantics)
- Real-time I/O adapters (CoreAudio, ASIO, WASAPI)

## "Agent" Semantics (Important for AI Assistants)

**"Agent" = optional orchestration layer EXTERNAL to core**

### Acceptable Uses

- Mocking transport state in Shmui demos
- Demonstrating how external services _could_ integrate (always optional, mocked)
- LLM integration hooks in FX Engine (application-level, not core)

### Unacceptable Uses

- Replacing Orpheus render pipeline with SaaS audio streams
- Committing API credentials
- Making core features dependent on external services
- Adding network calls to render path

**Rule:** If it's not audio graph/transport/render logic, it doesn't belong in core. Put it in an adapter, application, or Shmui demo.

## Security & Privacy Defaults

- **Localhost bind only** (127.0.0.1, never 0.0.0.0)
- **No telemetry without explicit consent**
- Tokens only via `.env.local`, never committed
- Guard networked features behind env vars (e.g., `VITE_ENABLE_AGENT=0`)
- Code signing for releases (applications, not core SDK)

## File Placement Guide

| Type            | Location                   | Example                                                           |
| --------------- | -------------------------- | ----------------------------------------------------------------- |
| Core primitives | `src/`, `include/`         | `SessionGraph.cpp`, `TransportManager.h`                          |
| Host adapters   | `adapters/`                | `reaper_adapter.cpp`, `minhost/main.cpp`                          |
| Applications    | `apps/`                    | `orpheus_clip_composer/`, `orpheus_wave_finder/`                  |
| UI demos        | `packages/shmui/apps/www/` | `DemoSession.tsx`, `OrbVisualization.tsx`                         |
| Documentation   | `docs/`                    | `ARCHITECTURE.md`, `ADAPTERS.md`, `ORP068 Implementation Plan.md` |

## Decision Framework for AI Assistants

When proposing changes, ask:

1. **Will this work offline?** — If no, probably wrong for core SDK
2. **Is this deterministic?** — If no, doesn't belong in render path
3. **Is this host-neutral?** — If no, belongs in an adapter
4. **Would this make sense for all applications?** — If no, it's application-specific

## Expected Output by Request Type

| Request Type           | Expected Output                                                      |
| ---------------------- | -------------------------------------------------------------------- |
| Add core feature       | C++20 code + CMake + unit tests (ctest), determinism verified        |
| Create UI demo         | Next.js/React mock using pnpm workspace, no external API             |
| Add background service | Localhost-only, behind flag, documented env vars                     |
| Update docs            | Link back to README + Integration Plan                               |
| Add adapter            | Thin wrapper (≤300 LOC ideal), optional CMake flag, clear separation |
| Optimize performance   | Profile first, preserve determinism, document trade-offs             |

## Do / Don't Checklist

### ✅ Do

- Maintain host-neutral determinism
- Respect sample-accurate rendering and clock domain isolation
- Keep UI examples self-contained and mockable
- Document all flags, ports, and env vars
- Keep CI/build green across OSes (Windows/macOS/Linux)
- Write unit tests for new features
- Profile before optimizing
- Preserve backward compatibility (ABI versioning)
- Follow `.clang-format` and `.clang-tidy` standards
- Add Doxygen comments for public APIs

### ❌ Don't

- Break core/adapter abstraction
- Depend on SaaS runtimes for functionality
- Add unverified external libraries
- Bypass style or test gates
- Allocate in audio threads
- Use floating-point time (sample counts only)
- Assume platform specifics (abstract behind adapters)
- Commit secrets, credentials, or binary files
- Add network calls to core features
- Modify CMake/CI to require network access

## Success Criteria for Contributions

### Good SDK Contribution

- [ ] Compiles on Windows, macOS, Linux without warnings
- [ ] Passes all existing tests (`ctest`)
- [ ] Adds new tests for new functionality
- [ ] Preserves determinism (same input → same output across runs/platforms)
- [ ] Maintains sample-accurate timing
- [ ] Keeps core minimal (no unnecessary dependencies)
- [ ] Documents APIs clearly (Doxygen comments)
- [ ] Follows code style (clang-format, clang-tidy compliant)

### Good Application Contribution

- [ ] Uses SDK as a library (doesn't modify core for app-specific features)
- [ ] Composes adapters appropriately
- [ ] Handles errors gracefully (never crashes, logs diagnostics)
- [ ] Provides user-facing documentation
- [ ] Meets performance targets (latency, CPU usage, memory footprint)

## Tooling & CI

### Continuous Integration (Phase 3 Complete ✅)

**Unified CI Pipeline** (`.github/workflows/ci-pipeline.yml`):

- **Matrix builds:** ubuntu/windows/macos × Debug/Release (6 combinations)
- **7 parallel jobs:** C++ build/test, C++ lint, native driver, TypeScript, integration tests, dependency check, performance validation
- **Optimized caching:** PNPM store and CMake builds for faster execution
- **Target duration:** <25 minutes per run

**Quality Gates:**

- **Sanitizers:** AddressSanitizer and UBSan enabled automatically for Debug builds (non-MSVC)
- **Formatting:** `clang-format` enforced on C++ sources
- **Linting:** Workspace linting scripts for TypeScript (see `package.json`)
- **Performance budgets:** `scripts/validate-performance.js` validates bundle sizes against `budgets.json`
- **Dependency integrity:** Madge checks for circular dependencies (783 files scanned)
- **Security:** Manual audits available (`pnpm audit --audit-level=high`, `pnpm run dep:check`)

**Chaos Testing** (`.github/workflows/chaos-tests.yml`):

- **Nightly runs:** 2 AM UTC, tests failure recovery scenarios
- **3 scenarios:** Service driver crash, WASM worker restart, client reconnection
- **Auto-issue creation:** GitHub issues created on failure with priority labels

**Pre-merge Validation:**

- **Husky hooks:** Pre-commit (lint-staged) and commit-msg (commitlint)
- **Conventional commits:** Enforced via commitlint configuration
- **PR template:** Comprehensive checklist for C++ and TypeScript changes

### Local Development Tools

- **Optional local analysis:** `clang-tidy -p build` (project-wide `.clang-tidy` config available)
- **Performance validation:** `pnpm run perf:validate` checks bundle sizes locally
- **Dependency check:** `pnpm run dep:check` validates no circular dependencies
- **Dependency graph:** `pnpm run dep:graph` generates SVG visualization
- **Security audits:** `pnpm audit --audit-level=high` checks for high severity vulnerabilities (manual, run as needed)

## Platform Support

Regularly built and tested:

- **Windows** — MSVC toolchains (x64)
- **macOS** — Clang (x86_64 and arm64)
- **Linux** — GCC and Clang

**JavaScript/TypeScript Requirements:**

- **Node.js** — 20+ (required by package engines in monorepo)
- **pnpm** — 8.15.4 (managed via packageManager field)

Other platforms may work but are not part of automated coverage.

## Key Documentation

- `README.md` — Repository overview, getting started, build instructions
- `ARCHITECTURE.md` — Design rationale, component relationships
- `ROADMAP.md` — Planned features, milestones, timeline
- `docs/ADAPTERS.md` — Available adapters, build flags, integration guides
- `OCC021` — Orpheus Clip Composer product vision (flagship application)

## Out-of-Scope by Default

**Do NOT add these without explicit approval:**

- Embedding third-party SaaS/voice runtimes
- Hard-coded analytics or telemetry
- Non-deterministic "agent orchestration" inside core engine
- Modifying CMake or CI to require network access

**All "agent" examples must:**

- Compile without network access
- Pass CI in offline mode
- Be disabled by default (opt-in via env vars)
- Live in `packages/shmui` (not core SDK)

## Strategic Context

**This is infrastructure, not an end-user app:**

- Multiple applications depend on this SDK
- Clip Composer needs real-time triggering
- Wave Finder needs FFT analysis
- FX Engine needs LLM integration hooks
- Design APIs for flexibility, not convenience

**Long-term vision:** 10+ years of stability. Favor simplicity, determinism, and user autonomy over short-term convenience.

## Entry Points for Common Tasks

- `src/lib.cpp`, `include/orpheus/core.h` — Core APIs
- `adapters/minhost/main.cpp` — CLI testing host reference
- `adapters/reaper_orpheus/` — REAPER extension integration
- `packages/shmui/apps/www/` — UI prototype demos
- `apps/` — Application code (Clip Composer, Wave Finder, etc.)
- `tests/` — GoogleTest suite

## ORP068 Implementation Progress

**Current status tracked in:** `.claude/progress.md`

**Phase Status:**

- ✅ Phase 0: Repository consolidation (15/15 tasks)
- ✅ Phase 1: Driver development (23/23 tasks)
- ✅ Phase 2: Expanded contract + UI (11/11 tasks)
- ✅ Phase 3: CI/CD infrastructure (6/6 tasks) **JUST COMPLETED**
- ⏳ Phase 4: Documentation & productionization (0/14 tasks)

**Overall Progress:** 55/104 tasks complete (52.9%)

**What's New in Phase 3:**

- Unified CI pipeline with matrix builds across all platforms
- Performance budget enforcement (bundle size validation)
- Chaos testing (nightly failure scenario testing)
- Dependency graph integrity checks (no circular dependencies)
- Manual security audit tooling (npm audit commands, dependency checks)
- Pre-merge validation (Husky hooks, commitlint, lint-staged)

When resuming ORP068 implementation work, always check:

1. `.claude/progress.md` - Current phase, completed tasks, next steps
2. `docs/ORP/ORP068 Implementation Plan (v2.0).md` - Full task breakdown
3. `git log --oneline -5` - Recent commits and context

**Quick resume:** Just say "Continue with ORP068" and reference `.claude/progress.md`

---

## Orpheus Clip Composer (OCC) - Design Phase Complete (October 2025)

**Status:** ✅ Design phase complete, ready for implementation

The Orpheus Clip Composer design package is comprehensive and implementation-ready. This flagship application drives SDK evolution by identifying real-world requirements for professional audio workflows.

### OCC Design Documentation

**Location:** `apps/clip-composer/docs/OCC/` (11+ documents, ~5,300 lines)

**Key Documents:**

- `OCC021` - Product Vision (authoritative) - Market positioning, competitive analysis
- `OCC026` - Milestone 1 MVP Definition - 6-month plan with deliverables
- `OCC027` - API Contracts - C++ interfaces between OCC and SDK
- `OCC029` - SDK Enhancement Recommendations - 5 critical modules required
- `PROGRESS.md` - Complete design phase report

**Quick Reference:**

- Product vision: Broadcast/theater soundboard, €500-1,500 price point, vs SpotOn/QLab/Ovation
- Technical: JUCE framework, Rubber Band DSP, JSON sessions, 960 clip buttons (10×12 grid, 8 tabs)
- Performance: <5ms ASIO latency, <30% CPU with 16 clips, >100hr MTBF
- Timeline: 6-month MVP (Months 1-6), v1.0 at 12 months, v2.0 at 18 months

### SDK Enhancements Required for OCC MVP

**5 Critical Modules (Milestone M2 - Real-Time Infrastructure):**

1. **Real-Time Transport Controller** (Months 1-2)
   - Sample-accurate clip playback, lock-free audio thread
   - Interface: `ITransportController` (see `apps/clip-composer/docs/OCC/OCC027`)

2. **Audio File Reader** (Months 1-2)
   - WAV/AIFF/FLAC decoding with libsndfile
   - Interface: `IAudioFileReader` (see `apps/clip-composer/docs/OCC/OCC027`)

3. **Platform Audio Drivers** (Months 1-2)
   - CoreAudio (macOS), ASIO/WASAPI (Windows)
   - Interface: `IAudioDriver` (see `apps/clip-composer/docs/OCC/OCC029`)

4. **Multi-Channel Routing Matrix** (Months 3-4)
   - 4 Clip Groups → Master Output, gain smoothing
   - Interface: `IRoutingMatrix` (see `apps/clip-composer/docs/OCC/OCC027`)

5. **Performance Monitor** (Months 4-5)
   - CPU tracking, buffer underruns, latency reporting
   - Interface: `IPerformanceMonitor` (see `apps/clip-composer/docs/OCC/OCC027`)

**Implementation Strategy:**

- Parallel development: OCC uses stub implementations while SDK builds real modules
- Integration: Month 3 (SDK modules ready)
- Critical path: SDK Months 1-4 (blocking for OCC)

**Detailed Specifications:**

- See `apps/clip-composer/docs/OCC/OCC029 SDK Enhancement Recommendations` for complete interface specs
- See `apps/clip-composer/docs/OCC/OCC027 API Contracts` for thread safety guarantees and error handling
- See `ROADMAP.md` Milestone M2 for timeline and success criteria

### Working with OCC Documentation

**For SDK Development:**

- Read `apps/clip-composer/docs/OCC/OCC029` to understand required SDK modules
- Read `apps/clip-composer/docs/OCC/OCC027` for exact interface signatures
- Follow implementation phases (M2: Months 1-6)

**For OCC Application Development:**

- Read `apps/clip-composer/docs/OCC/OCC021` for product vision and market positioning
- Read `apps/clip-composer/docs/OCC/OCC023` for component architecture (5 layers, threading model)
- Read `apps/clip-composer/docs/OCC/OCC024` for user workflows (8 complete flows)
- Read `apps/clip-composer/docs/OCC/OCC026` for MVP timeline and acceptance criteria

**Design Governance:**

- See `apps/clip-composer/docs/OCC/CLAUDE.md` for design documentation standards
- See `apps/clip-composer/docs/OCC/README.md` for complete documentation index

---

**Remember:** Orpheus SDK is infrastructure for a sovereign audio ecosystem. We're building tools that professionals can rely on for decades, not chasing trends. When in doubt, favor simplicity, determinism, and user autonomy.

- I'm expecting you to run all of ORP068 -- you don't need to check in with me so often.
