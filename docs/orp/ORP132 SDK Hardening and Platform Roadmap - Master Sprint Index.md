<!-- SPDX-License-Identifier: MIT -->

# ORP132 — SDK Hardening & Platform Roadmap: Master Sprint Index

**Document type:** Master planning index (read this first)
**Status:** Proposed — ready for execution
**Author:** Architecture review synthesis (Claude, Opus 4.8)
**Date:** 2026-07-09
**Supersedes prior stale planning references:** memory note "Next ORP doc: ORP127" is stale — highest existing doc is **ORP131**; this series begins at **ORP132**.

---

## 0. How to use this document set

This is the entry point for a multi-sprint effort to (a) harden the Orpheus SDK's
realtime and public-API contracts, and (b) lay the platform foundations that let
downstream apps — **FourTrack**, **Clip Composer**, **FreqFinder** — depend on
SDK-level primitives instead of app-specific workarounds.

The plan is split into five companion documents so each can be taken into its own
focused session:

| Doc | Title | Horizon | Take into session when… |
|-----|-------|---------|--------------------------|
| **ORP132** (this) | Master Sprint Index | — | You need scope, sequencing, boundaries |
| **ORP133** | NOW — Realtime & Contract Truth | Sprint 1 | Ready to start; lowest risk, highest urgency |
| **ORP134** | NEXT — Streaming Reader & Platform Primitives | Sprint 2 | ORP133 merged |
| **ORP135** | LATER — Platform Leadership Bets | Sprint 3+ | ORP134 primitives stable |
| **ORP136** | Verification & CI Framework | Cross-cutting | Alongside every sprint |

Each sprint doc is self-contained: it restates its own goals, file targets,
acceptance gates, and risks. ORP136 is the shared verification spec that all
sprints reference rather than duplicate.

---

## 1. Executive summary

### What this review confirmed

A read-only strategic architecture review was performed and **every code-level
finding was verified against the current source** (not just the docs). The review
is accurate. The headline realtime finding — the transport render path performs
file-reader/decoder work on the audio callback — is **real and reproducible**:

- `TransportController::processAudio()` calls `processCommands()` at callback entry
  (`src/core/transport/transport_controller.cpp:340-343`).
- The render loop calls `clip.reader->readSamples(...)` directly
  (`src/core/transport/transport_controller.cpp:475-477`), which reaches
  `sf_readf_float()` on the audio thread — with an explicit source comment
  "NO MUTEX LOCK HERE - this is called from the audio thread"
  (`src/core/audio_io/audio_file_reader_libsndfile.cpp:80-97`).
- Command handling calls `reader->seek(...)` from the audio thread
  (`src/core/transport/transport_controller.cpp:776,784`).

### The important nuance the review under-weighted

**The repository is already further along than the review implies.** This changes
sequencing and lowers risk:

1. **The realtime debt is already formally tracked.** `docs/REALTIME_AUDIT.md`
   lists "The transport render path still calls file readers directly" under
   *Known Architecture Debt*, and `tools/realtime_audit.py` encodes it as machine-
   checkable `KNOWN_DEBT_PATTERNS` (`readSamples(`, `->seek(`, `sf_readf_float`).
   A ctest already runs the audit (`tests/CMakeLists.txt:111`), and the tool has a
   `--fail-known-debt` flag described in-repo as *"the future strict gate after the
   streaming-reader migration lands."* **This gives every sprint a built-in, pre-
   agreed finish line.**
2. **`prepareClipAudio()` already exists** as the non-realtime prewarm hook
   (`transport_controller.h:256`) — the streaming-reader work extends an existing
   seam rather than inventing one.
3. **The inbound command queue is already POD.** ORP127 converted UI→audio
   commands to a fixed `TransportCommand` union over an SPSC ring. The remaining
   `std::function` hazard is only the **outbound** audio→UI callback ring
   (`transport_controller.h:368`) — a smaller, contained fix than the review frames.
4. **App-side remediation is already documented.** `docs/APP_REALTIME_DEBT_REMEDIATION.md`
   contains patch guidance for FourTrack and Clip Composer, gated on the SDK branch
   landing.
5. **A downstream primitive is already requested with a consumer waiting.**
   FourTrack's **FTR007** formally asks the SDK to adopt an `IAudioFileWriter`
   (FourTrack hand-rolled a `WavWriter` locally and will drop it on the next
   submodule bump).

**Conclusion:** This is not a rewrite. It is *converting already-tracked debt into
executed, verified fixes*, then adding a small number of high-leverage primitives
that named downstream apps are already asking for.

### What Orpheus should fix first (one line)

Move file/decoder work off the audio callback behind the existing `prepareClipAudio`
seam until `tools/realtime_audit.py --fail-known-debt` passes; replace the outbound
`std::function` callback ring with POD events; and make the public docs/version
tell the truth.

### What Orpheus should become (one line)

A host-neutral, deterministic audio SDK whose realtime-safe streaming playback,
stable identity/time primitives, graph-neutral routing, deterministic render/bounce,
and shared analysis/recorder/writer primitives let FourTrack, Clip Composer, and
FreqFinder ship thin app layers on a dependable core.

---

## 2. Scope boundary — the question that gates hand-off

> **The user's requirement:** confirm which changes are local to `orpheus-sdk/`.
> Anything requiring changes *outside* this repo must be run as a **separate sprint**.

### Verified boundary fact

**The Orpheus SDK has no git submodules** (`.gitmodules` absent). The dependency
arrow points one way: **apps consume the SDK**. FourTrack and Clip Composer each
vendor the SDK at `third_party/orpheus-sdk/` and bump the pin to pick up changes.

Therefore:

#### ✅ IN SCOPE — local to `orpheus-sdk/` (this repo; ORP133–ORP136)

All of the following are entirely within this repository:

- Transport realtime refactor (streaming reader, POD event ring, StopGroup).
- Command-queue producer contract + debug assertions.
- `IAudioFileWriter` primitive (FTR007) and input-capture abstraction.
- Identity/time-domain primitives; graph-neutral routing seam.
- Deterministic render/bounce hash harness.
- Analysis-primitive facade (FFT/STFT on top of existing `LoudnessMeter`/waveform).
- ABI facades, versioning/SemVer source of truth, migration guides.
- Documentation truth pass (README/CMake/ARCHITECTURE/ROADMAP/API index).
- CI gates: realtime strict gate, docs-path validation, render-hash, TSAN, fuzzing,
  benchmark budgets, installed-header compile test, downstream conformance harness
  (the harness *definition* lives here; apps opt in from their own repos).

#### ⛔ OUT OF SCOPE — separate sprints in app repos (NOT this hand-off)

These require edits in other repositories and must be their own sprints, sequenced
*after* the SDK provides the primitive:

- **FourTrack (`~/dev/fourtrack`):** move `Engine::processAudio()` off direct
  `readers_[i]->readSamples(...)` onto the SDK streaming source; drop local
  `WavWriter` once `IAudioFileWriter` lands. Guidance already in
  `docs/APP_REALTIME_DEBT_REMEDIATION.md` + FTR007. **→ FourTrack sprint.**
- **Clip Composer (`~/dev/clip-composer`):** move the in-callback analyzer work to
  a telemetry ring; wire the already-shipped `scene_manager.h`. **→ OCC sprint.**
- **FreqFinder (`~/dev/freqfinder`):** adopt SDK analysis primitives once the facade
  exists. (Note: in-tree `apps/wave-finder/` is an app-platform smoke-test shell,
  **not** the real analyzer — do not scope FFT work against it.) **→ FreqFinder sprint.**
- Submodule-pin bumps in each app repo after each SDK release.

**Every deliverable in ORP133–ORP136 is confirmed SDK-local and safe to hand to a
single agent working only in `orpheus-sdk/`.** Cross-repo follow-ups are flagged
inline in each doc under a "Downstream follow-up (separate sprint)" heading.

---

## 3. Fold-in — how this plan reconciles with existing SDK docs

This series **does not replace** existing planning docs; it sequences and completes
them. Reconciliation:

| Existing doc | Relationship to this plan |
|--------------|---------------------------|
| `docs/REALTIME_AUDIT.md` | **Authoritative source of the realtime contract.** ORP133/134 execute against its "Known Architecture Debt" list and make its `--fail-known-debt` gate pass. Do not contradict it; extend it. |
| `docs/APP_REALTIME_DEBT_REMEDIATION.md` | **App-side companion.** Its patches are the OUT-OF-SCOPE follow-ups triggered once ORP134's streaming reader lands. |
| `docs/SDK_POSITION_TRACKING.md` (ORP089, COMPLETE) | Position-tracking semantics are **settled**; the streaming reader must preserve them (±1 sample, ~10ms metadata visibility, OUT-point enforcement). ORP134 lists this as a regression gate. |
| `tools/realtime_audit.py` | **The finish-line oracle.** `KNOWN_DEBT_PATTERNS` is the checklist ORP134 must empty. |
| `ROADMAP.md` (M1–M6) | High-level milestones; **stale in places** (still references in-tree `apps/clip-composer/docs/OCC/`). ORP133 corrects the stale paths and re-expresses M4/M6 items as ORP135 bets. |
| `docs/SDK_SPRINT_SUMMARY.md`, `SDK_TEAM_HANDOFF.md` (ORP074) | Trim/fade metadata sprint; **orthogonal** and already scoped. Left untouched. |
| ORP125/126/127 | Completed refactor/voice-model sprints this plan builds on (POD command union, snapshot buffers). |

**Rule for the executing agent:** when a task in ORP133–ORP136 overlaps an existing
doc, treat the existing doc as the source of truth for *contract semantics* and this
plan as the source of truth for *sequencing and acceptance*. If they conflict on a
fact, stop and reconcile — do not silently diverge.

---

## 4. Sprint sequence & dependency map

```
ORP133 NOW ──────────────► ORP134 NEXT ──────────────► ORP135 LATER
(contract truth,           (streaming reader,           (automation, spatial,
 POD event ring,            identity/time,               plugin API, MIDI/OSC,
 StopGroup, docs)           graph seam, render-hash,     ABI facades, migration)
      │                     writer, analysis, recorder)         │
      │                            │                            │
      └────────────► ORP136 Verification & CI (spans all three) ◄┘
                     realtime gate · determinism · TSAN ·
                     fuzzing · benchmarks · conformance
```

**Hard dependencies:**

- ORP134's streaming reader **depends on** ORP133's event ring only loosely (they
  touch the same file; do ORP133 first to avoid churn), but **must not** begin until
  the ORP136 realtime runtime harness (allocation/lock detector) exists, so "off the
  audio thread" is *proven*, not asserted.
- ORP135 plugin/automation work **depends on** ORP134's graph seam and identity
  primitives being merged and stable.
- `IAudioFileWriter` (FTR007) can be pulled **forward into ORP134** (or even a fast-
  follow to ORP133) because it's additive, low-risk, and has a waiting consumer.

**Parallelizable within a sprint:** documentation truth pass (ORP133) is independent
of the code changes and can run concurrently.

---

## 5. Risk & sequencing rationale

- **Why NOW is docs + outbound ring + StopGroup (not the streaming reader):** these
  are low-blast-radius, ship confidence, and make the public contract honest before
  deeper surgery. The streaming reader is the single highest-risk change in the whole
  program and deserves its own sprint with the runtime harness already in place.
- **Why the streaming reader is NEXT, not NOW:** it can become an unbounded rewrite
  if unscoped. ORP134 stages it behind `prepareClipAudio`, preserves the public
  transport API, and gates completion on the pre-existing `--fail-known-debt` oracle
  plus ORP089 position-tracking regression tests.
- **Why identity/time primitives are additive, not a session rewrite:** the review's
  "Do Not Touch Yet" is respected — new `*Id`/`TimePoint`/`TimeRange` value types are
  introduced *alongside* the existing pointer-based `SessionGraph`, never by rewriting
  it. (Confirmed today: `Clip`/`Track` are pointer-identified, beats-only —
  `session_graph.h:16-63` — so additive layering is the correct, low-risk path.)

---

## 6. Deliverables checklist (for the whole program)

- [ ] ORP133 merged: audit-honest docs, POD event ring, StopGroup resolved,
      command-producer contract documented + asserted, version aligned.
- [ ] ORP136 realtime runtime harness merged (unblocks ORP134).
- [ ] ORP134 merged: `--fail-known-debt` passes, identity/time primitives,
      graph seam, render-hash harness, `IAudioFileWriter`, analysis facade,
      recorder primitives.
- [ ] ORP135 bets scoped into their own follow-on ORP docs as they mature.
- [ ] Downstream follow-up sprints filed in `~/dev/fourtrack`, `~/dev/clip-composer`,
      `~/dev/freqfinder` (out of this repo).

---

## 7. Conventions for the executing agent

- **Git:** new work on a working branch per sprint (e.g. `feat/orp133-contract-truth`);
  commit at each milestone with `type(scope): imperative` + Co-Authored-By trailer.
  Never commit directly to `main`.
- **C++ floor:** stay C++20 unless a sprint doc explicitly raises it (none do).
- **Realtime paths are high-risk:** no allocations, locks, I/O, logging, or unstable
  cross-thread state on any audio callback. Prove it with ORP136's harness, don't
  assert it.
- **Docs are part of "done":** every code change that alters a public contract updates
  the relevant header doc, `ARCHITECTURE.md`, and `CHANGELOG.md` in the same PR.
- **PREFIX/numbering:** new SDK docs are `ORP1NN`, next free number after **ORP136**
  is **ORP137**. Update `docs/orp/ORP.md` index and reindex the vault after adding docs.
- **Do not touch app repos from this session.** Flag cross-repo work; don't perform it.

---

*Companion docs: ORP133 (NOW), ORP134 (NEXT), ORP135 (LATER), ORP136 (Verification).*
