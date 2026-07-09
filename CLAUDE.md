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
```

> **Clip Composer moved:** the OCC app is now its own repo at `~/dev/clip-composer`
> (consumes this SDK as a submodule). Build/run it from there.

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

## CI/CD (All Phases Complete)

Matrix builds (ubuntu/windows/macos x Debug/Release). Sanitizers on Debug. Supply chain hardening: SHA-pinned Actions, `dep-audit.yml` (npm/PyPI age+download checks, bot detection), Husky dep-guard, `ignore-scripts` enforced.

---

## ORP068 Status

**Progress:** All phases complete. C++ SDK v1.0.0-rc.1 released.
**History:** `.claude/implementation_progress.md` + `docs/orp/ORP068 Implementation Plan (v2.0).md`

---

## OCC — Clip Composer (extracted to its own repo)

**As of 2026-07-09 the Clip Composer application lives in a standalone repository:**
`~/dev/clip-composer` (GitHub: `chrislyons/clip-composer`). It consumes this SDK as a
git submodule at `third_party/orpheus-sdk`. The former `apps/clip-composer/`
subdirectory has been **archived** to `~/archived-repos/clip-composer-sdk-subdir/`
(see `docs/orp/ORP131`).

- **OCC docs, build, and CI now live in the Clip Composer repo** — not here.
- SDK work that OCC depends on (transport, routing, audio_io, ABI) stays in this repo;
  bump the submodule pin in the OCC repo to pick up SDK changes.

---

## Multi-Instance Usage

| Instance | Working Dir | Focus |
|----------|-------------|-------|
| SDK | `~/dev/orpheus-sdk` | C++ core, adapters, transport/routing |
| Clip Composer | `~/dev/clip-composer` | Standalone JUCE app, OCC features (SDK as submodule) |

Separate `.claude/` directories prevent config collision.

---

**DON'T USE OPEN TO RUN CLIP COMPOSER** — Always run launch script after build.
**Application rebuilds should be performed manually by the user always.**
