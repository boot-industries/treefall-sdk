# Orpheus SDK Development Guide

Professional audio SDK with host-neutral C++20 core for deterministic session, transport, and render management.

**Documentation PREFIX:** ORP (SDK), OCC (Clip Composer)

---

## Core Principles

1. **Offline-first** — No runtime network calls for core features
2. **Deterministic** — Same input → same output, always (sample-accurate, bit-identical)
3. **Host-neutral** — Core SDK works across REAPER, standalone apps, plugins, embedded
4. **Broadcast-safe** — 24/7 reliability, no audio thread allocations

---

## Quick Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug   # Build
cmake --build build                              # Compile
ctest --test-dir build --output-on-failure       # Test
./scripts/relaunch-occ.sh                        # Run Clip Composer
```

**Full command list:** `docs/repo-commands.html`

---

## Audio Code Rules

**Determinism:** 64-bit sample counts (never float seconds), bit-identical across platforms, `std::bit_cast` for float determinism.

**Broadcast-Safe:** No audio thread allocations, no render path network calls, lock-free structures, pre-allocate, graceful degradation.

**Quality:** C++20, passes `clang-format` (CI enforced), AddressSanitizer + UBSan on Debug, adapters <=300 LOC.

---

## File Placement

| Type | Location | Example |
|------|----------|---------|
| Core | `src/`, `include/` | `SessionGraph.cpp` |
| Adapters | `adapters/` | `reaper_adapter.cpp` |
| Apps | `apps/` | `orpheus_clip_composer/` |
| Docs | `docs/` | `ARCHITECTURE.md` |

---

## Decision Framework

1. **Will this work offline?** → If no, wrong for core
2. **Is this deterministic?** → If no, not in render path
3. **Is this host-neutral?** → If no, belongs in adapter
4. **For all applications?** → If no, it's app-specific

---

## UX Package: shmui

**Status:** Planned | **Repo:** `~/dev/shmui`

JUCE components (AudioAnalyzer, WaveformVisualizer, BarVisualizer, OrbVisualizer, MatrixDisplay) for application-level UI. Thread-safe audio/UI communication. NOT for core SDK.

Integration: `packages/shmui-juce/` (planned).

---

## CI/CD (Phase 3 Complete)

Matrix builds (ubuntu/windows/macos x Debug/Release). 7 parallel jobs. Sanitizers on Debug. Performance budgets enforced. Chaos tests nightly.

---

## ORP068 Status

**Progress:** 55/104 tasks (52.9%) — Phases 0-3 complete. Phase 4 (docs/productionization) pending.
**Resume:** `.claude/implementation_progress.md` + `docs/ORP/ORP068 Implementation Plan (v2.0).md`

---

## OCC — Clip Composer

**Status:** v0.2.0 Sprint Complete (OCC093)
**Docs:** `apps/clip-composer/docs/OCC/` (12 docs, ~6,000 lines)

Key docs: OCC021 (product vision), OCC026 (6-month MVP plan), OCC027 (API contracts), OCC093 (v0.2.0 sprint).

---

## Multi-Instance Usage

| Instance | Working Dir | Focus |
|----------|-------------|-------|
| SDK | `~/dev/orpheus-sdk` | C++ core, adapters, transport/routing |
| Clip Composer | `~/dev/orpheus-sdk/apps/clip-composer` | Tauri app, JUCE UI, OCC features |

Separate `.claude/` directories prevent config collision.

---

**DON'T USE OPEN TO RUN CLIP COMPOSER** — Always run launch script after build.
**Application rebuilds should be performed manually by the user always.**
