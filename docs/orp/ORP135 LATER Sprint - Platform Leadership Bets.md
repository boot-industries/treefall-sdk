<!-- SPDX-License-Identifier: MIT -->

# ORP135 — LATER: Platform Leadership Bets (2026+)

**Horizon:** Sprint 3+ (post-ORP134) · **Parent:** [[ORP132]] · **Depends on:** [[ORP134]] primitives stable
**Status:** Directional — each bet graduates into its own ORP doc when picked up
**Scope:** All SDK-local *when built*; several enable — but do not require — app-repo work.
**Label:** These are **speculative 2026+ directions**, not committed sprint work. Ship
ORP133/ORP134 first; revisit this doc when the identity/time/graph/render contracts
are stable and a concrete downstream pull exists for a given bet.

---

## 1. Purpose

Capture where Orpheus should go *after* the realtime contract is hardened and the core
primitives exist, so the near-term work in ORP133/ORP134 is built in the right shape.
Each item below is a **candidate** with a gating dependency, a rationale, and the
downstream app that would justify it. Nothing here should be started until its
dependency is green and a real consumer is asking.

**Governing rule:** every bet must preserve the four principles — offline-first,
deterministic, host-neutral, broadcast-safe — or it does not belong in the core SDK.

---

## 2. Bets (each becomes its own ORP doc when scheduled)

### B1 — Sample-accurate automation
**Depends on:** ORP134 identity/time primitives (`AutomationLaneId`, `TimePoint`).
**What:** automation lanes with sample-accurate breakpoints, block-boundary-correct
evaluation, realtime-safe parameter application (precompute per-block ramps off the
audio thread, apply on it — the pattern already used for gain smoothing, ORP127 G4).
**Why / who:** foundational for mixing in all three apps; prerequisite for plugin
parameter automation (B4). **Determinism gate:** automation must render bit-identical.

### B2 — Take / comping / editing subsystem
**Depends on:** ORP134 identity (`Take`, `MediaRegion`) + writer (G5) + recorder plumbing (G7).
**What:** SDK-level take management, comp lanes, punch ranges, non-destructive edits
referencing media by `MediaId`/`TimeRange`.
**Why / who:** **FourTrack** multitrack recording passes; keeps edit state serializable
and undoable via stable IDs rather than pointers.

### B3 — Spatial / multichannel graph readiness
**Depends on:** ORP134 graph seam (G3) + channel-layout primitives.
**What:** channel layouts, speaker maps, object/bed model, ADM-like metadata,
downmix/upmix policy (the routing matrix already carries `DownmixPolicy`; extend, don't
restart). Note `src/core/adm/entity_graph.cpp` already exists — align with it.
**Why / who:** broadcast/immersive futures; Clip Composer surround shows. **Governance
gate:** careful channel-layout governance before exposing publicly.

### B4 — Plugin processor API (then optional plugin *hosting*)
**Depends on:** B1 (automation) + ORP134 graph seam, both stable.
**What:** start with **generic in-SDK processor nodes** + parameter automation. Defer
VST3/AU/LV2 *hosting* until graph, automation, and realtime rules are proven — the
review is explicit: "do not add plugin hosting before graph, automation, and realtime
contracts are stable." Hosting, if ever, is an **adapter**, not core.
**Why / who:** FX-Engine-style processing; opens third-party extension. **Risk:** high
distraction potential — gate hard.

### B5 — MIDI / OSC / control-surface mapping
**Depends on:** ORP133 G3 command-producer contract + an SDK MPSC dispatcher (below).
**What:** host-neutral mapping from control events → transport/routing/session commands.
Device-specific code stays **outside** core. Includes the deferred **MPSC control
dispatcher** that ORP133 documented as future work (funnels UI+MIDI+OSC into the
single audio-thread consumer safely).
**Why / who:** Clip Composer iOS/OSC remote (MVP spec); FourTrack hardware control.

### B6 — Cross-language ABI facades
**Depends on:** ORP133/134 realtime + identity contracts *frozen* (do not freeze
unstable internals — review's explicit warning).
**What:** stable C ABI facades for the high-value, now-stable surfaces: prepared media
+ metadata, offline render jobs, the transport command/event protocol, routing graph
snapshots, diagnostics/perf counters. Keep the ergonomic C++ SDK primary.
**Why / who:** external hosts, language bindings, long-lived compatibility; the app
repos' submodule model benefits from stable boundaries.

### B7 — Migration tooling & ABI compatibility matrix
**Depends on:** B6 + a published SemVer/ABI policy (seeded in ORP133 G4).
**What:** installed-header compile tests (partly in ORP136), ABI diff checks in CI,
versioned migration guides, a compatibility matrix.
**Why / who:** partner/downstream confidence as the platform surface grows.

### B8 — Offline AI/ML artifact & provenance layer
**Depends on:** ORP134 render/determinism harness.
**What:** SDK-level support **only** for deterministic *offline* artifacts — analysis
caches, embeddings, model-output provenance, stem metadata, undoable edits. **No
network calls and no nondeterministic ML in realtime or render paths** (review's hard
constraint). ML runtime stays outside core.
**Why / who:** future assisted-editing workflows without compromising determinism.

### B9 — WASM / mobile portability
**Depends on:** cleaner threading/I/O/dependency/ABI seams from ORP134 + B6. (`.emscripten-version`
already present, so a WASM target is contemplated.)
**What:** portability pass after the seams are clean — not before.
**Why / who:** FourTrack iOS; web demos/tools.

---

## 3. Sequencing sketch (only when pulled)

```
ORP134 stable
   ├─ B1 automation ──┬─ B4 plugin processor API
   │                  └─ B2 take/comping (also needs G5+G7)
   ├─ B3 spatial (needs G3)
   ├─ B5 MIDI/OSC + MPSC dispatcher (needs ORP133 G3)
   ├─ B6 ABI facades ─ B7 migration/compat matrix
   ├─ B8 offline ML artifacts (needs G4 determinism)
   └─ B9 WASM/mobile (needs B6)
```

---

## 4. What stays OUT of core regardless

- Plugin *hosting* runtimes (VST3/AU/LV2 scan+load) → adapter layer if ever.
- Device-specific control-surface drivers → outside core.
- ML inference runtimes / network calls → outside core; only deterministic artifacts in.
- Any nondeterministic behavior on realtime or render paths.

---

## 5. Graduation checklist (per bet, before it becomes a sprint)

- [ ] Its dependency in ORP133/ORP134 is merged and stable.
- [ ] A concrete downstream consumer (FourTrack / Clip Composer / FreqFinder) is asking.
- [ ] It can preserve offline-first + determinism + host-neutral + broadcast-safe.
- [ ] It gets its own ORP1NN doc with acceptance gates before code starts.

*This document is a compass, not a commitment. Parent: [[ORP132]]. Verification norms: [[ORP136]].*
