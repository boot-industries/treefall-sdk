<!-- SPDX-License-Identifier: MIT -->

# ORP134 — NEXT Sprint: Streaming Reader & Platform Primitives

**Horizon:** Sprint 2 · **Parent:** [[ORP132]] · **Depends on:** [[ORP133]] merged + [[ORP136]] runtime harness · **Verification:** [[ORP136]]
**Status:** Proposed
**Scope:** 100% local to `orpheus-sdk/`. Downstream app adoption is separate (see §8).
**Risk:** Medium–High (streaming reader is the highest-risk change in the program).
**Suggested branch:** `feat/orp134-streaming-and-primitives`

---

## 1. Goal

Make the transport **truly** realtime-safe by moving decode/seek off the audio
callback, and add the small set of SDK-level primitives that named downstream apps
already need — so FourTrack, Clip Composer, and FreqFinder can drop app-specific
workarounds.

**The finish line is pre-defined and machine-checkable:**
`tools/realtime_audit.py --fail-known-debt --include-adjacent` must pass — i.e. the
tool's `KNOWN_DEBT_PATTERNS` (`readSamples(`, `->seek(`, `sf_readf_float` on the
transport render path) must be eliminated. This is the repo's own "future strict
gate," and this sprint is the migration that unlocks it.

---

## 2. Prerequisite gate (do not start without this)

ORP136's **realtime runtime harness** (allocation / lock / blocking-syscall detector
around `processAudio()`) must exist first. Rationale: the entire value of this sprint
is *proving* the audio thread no longer does I/O — a static grep is necessary but not
sufficient. Start G1 only once the harness can red/green the claim.

---

## 3. Work items

### G1 — Prepared-media streaming reader (the core change)

**Current (verified):** `processAudio()` → per-clip `clip.reader->readSamples(...)`
(`transport_controller.cpp:475-477`) → `sf_readf_float()` on the audio thread
(`audio_file_reader_libsndfile.cpp:80-97`); `reader->seek(...)` from command handling
(`:776,784`).

**Target architecture** (already named in `REALTIME_AUDIT.md`): a *non-realtime
streamer feeding realtime-owned clip buffers*. Stage it behind the **existing**
`prepareClipAudio()` seam (`transport_controller.h:256`) so the public transport API
does not change.

**Design (staged, non-breaking):**

1. **Keep `IAudioFileReader` as a background/file-I/O abstraction** (unchanged
   contract; `open/close/seek/readSamples` are background-thread operations per its
   own header, `audio_file_reader.h:40-47`).
2. **Add a realtime playback-source contract** — an immutable, audio-thread-only
   view backed by either:
   - **prepared memory** (whole-file decoded to PCM in `prepareClipAudio`), for the
     common short-clip case (Clip Composer, most FourTrack takes), or
   - **fixed-size streaming pages** filled by a background worker for long files,
     with an explicit **underrun → emit silence + `BufferUnderrun` event** path
     (the event type added in ORP133 G1).
3. **Do all decode/resample/hash/waveform work off the audio thread** in
   `prepareClipAudio()` / the streaming worker.
4. **Publish immutable prepared sources atomically** to the transport (same
   release/acquire discipline as the ORP127 voice snapshot).
5. **Replace audio-thread `readSamples`/`seek`** with reads from the prepared source /
   page cache. Seek becomes a page-cache reposition (worker-driven), not an
   `sf_seek` on the callback.

**Hard invariants to preserve** (regression gates, from `SDK_POSITION_TRACKING.md`
ORP089, which is COMPLETE and authoritative):
- ±1 sample position accuracy; metadata changes visible within ~1 audio buffer;
  OUT-point enforcement unchanged; fade-in/out, loop-boundary, restart-crossfade,
  and gain-smoothing behavior bit-identical where inputs are identical.

**Staging discipline:** land behind a flag or as an opt-in prepared path first,
prove parity with golden render hashes (ORP136), then flip the default and delete the
audio-thread read path. Do **not** merge a half-migrated callback that reads *both*
ways in production.

### G2 — Stable identity & time-domain primitives (additive)

**Current (verified):** `Clip`/`Track`/`Marker` are pointer-identified and beats-only
(`session_graph.h:16-121`: `const Clip*`, `remove_clip(const Clip*)`, `start_beats`,
`length_beats`). No stable IDs, no sample/timecode domain.

**Add value/handle types *alongside* the existing graph (never rewrite it):**

- IDs: `SessionId`, `TrackId`, `ClipId`, `MediaId`, `AutomationLaneId` (opaque,
  stable, serializable — e.g. strong typedef over `uint64_t`).
- Time: `TimePoint` / `TimeRange` carrying a sample count as the source of truth, with
  conversions to seconds/beats/timecode (sample-domain is canonical for determinism,
  per the SDK's own rules: "64-bit sample counts, never float seconds").
- Media/region: `MediaRegion`, and the app-shaped aggregates `Take`, `ClipSlot`,
  `LauncherScene` as thin structs over the primitives.

**Constraint:** existing pointer-based classes keep working; new code and serialization
prefer IDs. This directly honors the review's "Do Not Touch Yet — do not rewrite the
session graph before stable IDs exist."

### G3 — Graph-neutral routing seam (layer, don't replace)

**Current (verified):** transport constructs its own routing matrix with soundboard
defaults (2 channels/clip, 4 groups, stereo out — `transport_controller.cpp:35-60`);
`routing_matrix` already grew `SourceChannelPolicy`/`DownmixPolicy`.

**Add** a graph-neutral vocabulary *beneath* the existing matrix — sources,
processors, buses, sinks, sends, taps, channel layouts — and express the current
soundboard topology as one *facade* over it. Do **not** rip out the tested matrix
(review: "do not replace the routing matrix outright"). The deliverable here is the
seam + the soundboard facade proving it, not a full graph engine (that's ORP135).

**Downstream payoff:** FourTrack gets track/input/monitor/export buses; FreqFinder
gets analysis **taps**; Clip Composer keeps its cue/master/group facade.

### G4 — Deterministic render/bounce hash harness

**Current (verified):** `render_tracks`/`RenderSpec`/`render_click` exist
(`src/orpheus/render_tracks.cpp`, ABI `orpheus_render_api_v1`), and a hash helper
already ships (`tests/support/fnv1a64.hpp`) — but there is no golden-hash determinism
gate.

**Add** a render-job descriptor (sample rate, block size, channel layout, session/
graph snapshot, output format) and a **golden render-hash test** that renders the same
input at multiple block sizes and asserts a stable hash (with a documented cross-
platform tolerance strategy). This is both a determinism guarantee and the parity
oracle for G1's streaming migration.

**Downstream payoff:** FourTrack bounce/export; Clip Composer show render; FreqFinder
reproducible analysis; CI proves "same input → same output."

### G5 — `IAudioFileWriter` (FTR007 — requested, consumer waiting)

**Current:** the SDK has **no** writer; FourTrack hand-rolled a `WavWriter` and
**FTR007 formally requests** the SDK adopt one (libsndfile-backed for breadth: WAV/
AIFF/FLAC, path to Ogg/Opus). FourTrack will delete its local copy on the next
submodule bump.

**Add** `IAudioFileWriter` mirroring `IAudioFileReader`'s shape (open/write/close,
background-thread contract, error model). Libsndfile-backed implementation +
create-function, with a stub for platforms without libsndfile.

**Why here:** additive, low-risk, unblocks FourTrack export (FTR019) and future
Clip Composer recording. Can be pulled forward to a fast-follow after ORP133 if
capacity allows — it does not depend on G1.

### G6 — Analysis-primitive facade (build on what exists)

**Current (verified):** `LoudnessMeter` (LUFS/biquad), `AudioFileReaderExtended`
waveform peaks, `scrub_resampler`, `polyphase_resampler`, `true_peak_meter.h`
already exist. Missing: FFT/STFT and a unified analysis entry point.

**Add** a small analysis facade exposing: FFT/STFT, RMS/peak/LUFS (wrap existing
meter), spectral centroid/rolloff, onset/transient detection, and waveform proxy
generation (wrap existing peak code). **Do not duplicate** the existing meter/waveform
code — expose it through the facade.

**Downstream payoff:** FreqFinder (the real analyzer in `~/dev/freqfinder`, **not**
the in-tree `apps/wave-finder/` smoke-test shell) can drop app-specific DSP; Clip
Composer/FourTrack reuse metering + waveform.

### G7 — Recorder primitives (host-neutral building blocks)

**Current:** FourTrack implements its own input ring buffer, async disk writer, arm/
punch, and feedback-guard — all app-specific today; SDK offers only the raw driver
input hook.

**Add** the reusable, non-opinionated pieces (not a full recorder): a lock-free input
ring-buffer helper and an `IAudioInputStream`/pre-warmed-input contract so apps stop
hooking the raw driver callback directly. Take/punch/latency-compensation *policy*
stays app-side for now; the SDK provides the plumbing. Pairs with G5 (writer) to make
"capture → disk" an SDK-supported path.

**Downstream payoff:** FourTrack becomes a first-class SDK use case without making the
SDK app-specific.

### G8 — Wire the already-shipped `scene_manager.h`

`include/orpheus/scene_manager.h` exists but Clip Composer isn't linking it. Verify it
covers multi-snapshot session/page switching (Clip Composer models 8 tabs × 48
buttons); fill gaps; add tests. Small, high-signal win.

---

## 4. Sequencing within the sprint

1. **G5 (writer)** and **G4 (render-hash harness)** first — additive, and G4 becomes
   the parity oracle for G1.
2. **G2 (identity/time)** next — additive, unblocks serialization and later automation.
3. **G1 (streaming reader)** — the big one, gated on the ORP136 runtime harness and
   validated by G4's hashes + ORP089 regression tests.
4. **G3 (graph seam)**, **G6 (analysis)**, **G7 (recorder)**, **G8 (scenes)** —
   independent primitives, parallelizable after their prerequisites.

---

## 5. Acceptance gates (Definition of Done)

- [ ] `tools/realtime_audit.py --fail-known-debt --include-adjacent` **passes** (the
      defining gate — no `readSamples`/`seek`/`sf_readf_float` on the render path).
- [ ] ORP136 runtime harness shows **zero** allocations/locks/blocking calls across a
      multi-clip `processAudio()` stress run.
- [ ] Golden render-hash test green at ≥3 block sizes; documented tolerance strategy.
- [ ] All ORP089 position-tracking invariants regression-pass (±1 sample, ~10ms
      metadata, OUT-point, fades, loop, restart, gain smoothing).
- [ ] Long-file streaming + simulated cache-miss/underrun test passes (emits silence +
      `BufferUnderrun`, never blocks).
- [ ] Identity/time primitives compile-tested via installed headers; serialization
      round-trips by ID.
- [ ] `IAudioFileWriter` writes WAV/AIFF/FLAC; round-trip read-back test passes.
- [ ] Analysis facade returns correct FFT/LUFS/waveform on fixtures; no duplication of
      existing meter/waveform code.
- [ ] `scene_manager` covered by tests; gap list resolved or documented.
- [ ] `CHANGELOG`, `ARCHITECTURE.md`, and public header docs updated.

---

## 6. Risks & mitigations

| Risk | Mitigation |
|------|------------|
| Streaming reader becomes an unbounded rewrite | Stage behind `prepareClipAudio`; keep public API; prove parity by hash before flipping default; ban dual-path production merge. |
| Graph model over-abstracts without a real consumer | Ship only the seam + soundboard facade that exercises it; defer the full engine to ORP135. |
| Identity introduction breaks serialization/undo | Additive only; keep pointer classes; migrate serialization behind a version bump with a round-trip test. |
| Render-hash flakiness across platforms | Document tolerance; pin block sizes; use the existing `fnv1a64` helper; treat cross-platform drift as a determinism bug to file, not to paper over. |
| Analysis facade duplicates existing DSP | Facade *wraps* `LoudnessMeter`/waveform/`true_peak_meter`; code review rejects re-implementations. |

---

## 7. Respecting "Do Not Touch Yet"

This sprint deliberately **does not**: rewrite the session graph (adds IDs alongside);
replace the routing matrix (adds a seam + facade); freeze realtime internals into ABI
(ABI facades are ORP135, after these contracts settle); add plugin hosting or ML.

---

## 8. Downstream follow-up (separate sprints — NOT this session)

Filed in the app repos *after* the relevant SDK primitive merges and the submodule pin
is bumped:

- **FourTrack (`~/dev/fourtrack`):** switch `Engine::processAudio()` to the G1
  streaming source; delete local `WavWriter` for G5 `IAudioFileWriter`; adopt G7 input
  stream. (Guidance: `docs/APP_REALTIME_DEBT_REMEDIATION.md`, FTR007.)
- **Clip Composer (`~/dev/clip-composer`):** move in-callback analyzer to a telemetry
  ring; wire G8 `scene_manager`.
- **FreqFinder (`~/dev/freqfinder`):** adopt the G6 analysis facade.

*Next: [[ORP135]] once these primitives are stable.*
