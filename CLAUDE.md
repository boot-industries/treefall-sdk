# Orpheus SDK Development Guide

**Workspace:** Inherits conventions from `~/chrislyons/dev/CLAUDE.md`

Professional audio SDK with host-neutral C++20 core for deterministic session, transport, and render management.

## Core Principles

1. **Offline-first** — No runtime network calls for core features
2. **Deterministic** — Same input → same output, always (sample-accurate, bit-identical)
3. **Host-neutral** — Core SDK works across REAPER, standalone apps, plugins, embedded
4. **Broadcast-safe** — 24/7 reliability, no audio thread allocations

## Repository Structure

```
├── src/, include/       # Core SDK (C++20) - deterministic, portable
├── adapters/            # Host integrations (REAPER, minhost, realtime_engine, clip_grid)
├── apps/                # Applications (Clip Composer, Wave Finder, FX Engine)
├── packages/shmui/      # UI demos (Next.js/React, local-mocked)
├── tests/               # GoogleTest suite
└── docs/                # Architecture, roadmap, adapter guides
```

## Build Commands

### C++ Core

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Optional: JUCE demo
cmake -S . -B build -DORPHEUS_ENABLE_APP_JUCE_HOST=ON
```

### JavaScript (Node 20+)

```bash
pnpm install
pnpm --filter www dev  # localhost:4000
```

## Audio Code Rules

**Determinism:**

- 64-bit sample counts (never float seconds)
- Bit-identical output across platforms
- `std::bit_cast` for float determinism

**Broadcast-Safe:**

- No audio thread allocations
- No render path network calls
- Lock-free structures, pre-allocate
- Graceful degradation

**Quality:**

- C++20, passes `clang-format` (CI enforced)
- AddressSanitizer + UBSan on Debug
- Adapters ≤300 LOC when possible

## File Placement

| Type     | Location           | Example                  |
| -------- | ------------------ | ------------------------ |
| Core     | `src/`, `include/` | `SessionGraph.cpp`       |
| Adapters | `adapters/`        | `reaper_adapter.cpp`     |
| Apps     | `apps/`            | `orpheus_clip_composer/` |
| UI demos | `packages/shmui/`  | `DemoSession.tsx`        |
| Docs     | `docs/`            | `ARCHITECTURE.md`        |

## Decision Framework

1. **Will this work offline?** → If no, wrong for core
2. **Is this deterministic?** → If no, not in render path
3. **Is this host-neutral?** → If no, belongs in adapter
4. **For all applications?** → If no, it's app-specific

## Do / Don't

### ✅ Do

- Maintain host-neutral determinism
- Sample-accurate rendering, clock isolation
- Keep UI self-contained/mockable
- Document flags/ports/env vars
- Write unit tests, profile first
- Follow `.clang-format`/`.clang-tidy`
- Add Doxygen for public APIs

### ❌ Don't

- Break core/adapter abstraction
- Depend on SaaS for functionality
- Allocate in audio threads
- Use floating-point time
- Commit secrets/binaries
- Add network to core features
- Modify CMake/CI for network access

## CI/CD (Phase 3 Complete ✅)

**Pipeline:** Matrix builds (ubuntu/windows/macos × Debug/Release)

- 7 parallel jobs: C++ build/test, lint, native driver, TypeScript, integration, deps, perf
- Sanitizers (ASan/UBSan) on Debug
- Performance budgets enforced
- Chaos tests (nightly)
- Pre-commit hooks (Husky)

## ORP068 Status

**Progress:** 55/104 tasks (52.9%)

- ✅ Phase 0-3 (Repo, Driver, UI, CI)
- ⏳ Phase 4: Docs/productionization (0/14)

**Resume:** Check `.claude/progress.md` + `docs/ORP/ORP068 Implementation Plan (v2.0).md`

## OCC - Clip Composer (Design Complete ✅)

**Docs:** `apps/clip-composer/docs/OCC/` (11 docs, ~5,300 lines)

**Key:**

- `OCC021` - Product vision (broadcast/theater, €500-1,500)
- `OCC026` - 6-month MVP plan
- `OCC027` - API contracts
- `OCC029` - 5 SDK modules required (transport, file reader, drivers, routing, perf monitor)

**Timeline:** MVP at 6mo, v1.0 at 12mo

## Token Efficiency Rules

**Debugging:**

- 3 causes + ONE grep per cause BEFORE reading files
- Incremental: `cmake --build build --target file.cpp.o`
- ASan crash on valid code? `rm -rf build` first

**Git:**

- `git log --oneline -10` (not `--all`)
- Pick ONE commit from message

**Files:**

- `grep -rn 'symbol'` before `Read()`
- Never read same file twice

---

**DON'T USE OPEN TO RUN CLIP COMPOSER** - Always run launch script after build.
