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

## Public SDK / Private UI Boundary

The SDK owns host-neutral audio, transport, routing, diagnostics, and platform
drivers. It does not contain or export Shmui. Private applications pin Shmui as
an independent application-owned artifact and link the `Shmui::*` namespace.
See `docs/orp/ORP163 Public SDK and Private Shmui Package Boundary.md`.

---

## CI/CD

Workflow definitions cover Linux, macOS, and Windows builds plus sanitizer and
supply-chain checks. Repository GitHub Actions are currently disabled; local
configured build/CTest, installed-package, realtime-audit, and hardware evidence
remain required before support claims.

---

## Current SDK Status

**Version:** 0.7.0 pre-1.0 SDK with stable C ABI 1.0.
**Historical milestone:** ORP068 phases are complete; the durable work log is
`.claude/implementation_progress.md`.

Current FourTrack-facing contracts:

- ORP154: standalone allocation-free trigger voice.
- ORP155: directional CoreAudio endpoints, isolated routing meters, and public
  capture-failure telemetry.
- ORP156: ORP155 implementation and verification handoff.
- ORP162: CoreAudio aggregate capture-channel mapping and downstream 0.6.7 pin handoff.
- ORP163: public SDK/private Shmui package boundary and 0.7.0 migration.

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
