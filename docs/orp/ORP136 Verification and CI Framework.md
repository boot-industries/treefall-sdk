<!-- SPDX-License-Identifier: MIT -->

# ORP136 — Verification & CI Framework (Cross-Cutting)

**Applies to:** every sprint in this program · **Parent:** [[ORP132]]
**Companions:** [[ORP133]] (NOW), [[ORP134]] (NEXT), [[ORP135]] (LATER)
**Status:** Proposed · **Scope:** 100% local to `orpheus-sdk/` (the downstream
conformance *harness* is defined here; apps opt in from their own repos).

---

## 1. Purpose

One place that defines *how we prove each change is safe*, so the sprint docs
reference gates instead of restating them. Pro-audio SDK risk is concentrated in
realtime safety, determinism, and cross-thread correctness — the gates below target
exactly those.

**Anchor:** the repo already ships the spine of this framework —
`tools/realtime_audit.py` (+ `realtime_static_audit` ctest, `tests/CMakeLists.txt:111`),
sanitizer CI (`.github/workflows/ci-pipeline.yml`), a TSAN baseline
(`tests/transport/voice_state_tsan_baseline.md`), a hash helper
(`tests/support/fnv1a64.hpp`), and stress tests (`callback_queue_stress_test.cpp`,
`multi_clip_stress_test.cpp`). This doc **extends** that spine; it does not rebuild it.

---

## 2. Gate catalog

### 2.1 Realtime safety — static (exists; tighten)

- **Have:** `tools/realtime_audit.py` fails on forbidden driver-callback patterns and
  *reports* `KNOWN_DEBT_PATTERNS` (`readSamples(`, `->seek(`, `sf_readf_float`) on the
  transport render path.
- **ORP133:** confirm no `std::function` finding remains after the POD event ring.
- **ORP134 finish line:** `--fail-known-debt --include-adjacent` must **pass** — this
  is the defining acceptance gate for the streaming-reader migration. Flip the CI job
  from "report" to "fail" the moment G1 lands.

### 2.2 Realtime safety — runtime (NEW; **blocks ORP134**)

A static grep cannot prove the absence of allocation/locking. Add a runtime harness:

- Wrap a multi-clip `processAudio()` stress run with an allocation hook (override
  `operator new`/`malloc` interpose in a test build) that **fails on any allocation**
  inside the callback window.
- Detect locks/blocking syscalls on the audio thread (TSAN annotations + a blocking-
  call assert shim in debug).
- Assert callback-duration outliers stay within budget; count underruns.
- **This harness must exist before ORP134 G1 starts** (ORP134 §2 prerequisite).

### 2.3 Determinism & offline render (NEW)

- Golden render-hash test using `tests/support/fnv1a64.hpp`: render identical input at
  **≥3 block sizes**, assert stable hash.
- Fixed seed/state snapshot render jobs; verify output metadata.
- Documented cross-platform tolerance strategy (bit-identical target; any drift is a
  determinism bug to file, not to mask).
- Doubles as the **parity oracle** for ORP134 G1 (streaming vs. current reader).

### 2.4 Threading & concurrency (extend TSAN)

- **Have:** `voice_state_tsan_test.cpp` + baseline.
- Add TSAN tests for: multiple control producers hitting the ORP133 producer assertion;
  audio callback + concurrent UI queries; register/prepare/start/stop interleavings;
  device hot-swap (ties to ORP128 sample-rate resilience); command- and callback-queue
  overflow.

### 2.5 Audio I/O & codecs — fuzzing + fixtures (NEW)

- Fixture matrix: WAV/AIFF/FLAC × 44.1/48/96 kHz × mono/stereo/multichannel × short/
  long/corrupted, plus trim/seek/loop edge cases.
- Fuzz the decoder/metadata path (corrupted/truncated files must fail gracefully, never
  crash or read OOB). Pairs with ORP134 G1's underrun path.
- When ORP134 G5 `IAudioFileWriter` lands: writer round-trip (write → read-back → hash).

### 2.6 Routing & graph (extend)

- **Have:** 5 routing tests.
- Add: mono/stereo/discrete-multichannel routing; output/cue/audition buses; solo/mute/
  choke; metering; sends; taps; high voice- and channel-count stress. Validates the
  ORP134 G3 seam + soundboard facade.

### 2.7 Docs & API gates (NEW; ORP133 G6)

- Installed-header compile test: configure/install the SDK, compile a tiny consumer
  against installed headers (catches include leaks — the lint job already has a
  "Windows includes" check to extend).
- Public-API inventory check: enumerate installed headers; fail on undocumented new
  public symbols.
- Docs-path validation: fail on internal links to nonexistent paths, on
  `apps/clip-composer` references (now external), and on removed CMake options.

### 2.8 Packaging & supply chain (maintain)

- Keep the existing binary-artifact rejection, SHA-pinned Actions, `dep-audit.yml`.
- Add benchmark budgets (there's a `budgets.json`): fail CI on regressions beyond
  threshold for the multi-clip render path and prepared-media decode.
- **Later (ORP135 B7):** ABI diff check once ABI facades exist.

### 2.9 Downstream conformance harness (NEW; definition here, opt-in from apps)

Define a conformance suite in this repo that app repos can run against their pinned
submodule. The **suite lives here**; **running it against an app is an app-repo
concern** (out of scope for SDK sessions). Scenarios per app:

- **Clip Composer:** large-grid load, prewarm, trigger bursts, stop/restart/loop/cue,
  cue/master routing, session restore, realtime-health reporting.
- **FourTrack:** record-arm, input monitor, punch in/out, overdub-play-while-recording,
  take creation, latency compensation, mixdown/export.
- **FreqFinder:** file analysis, scrub/preview region, FFT/harmonic outputs, waveform
  proxy, analysis-cache determinism.

---

## 3. Per-sprint gate mapping

| Gate | ORP133 NOW | ORP134 NEXT | ORP135 LATER |
|------|:---------:|:-----------:|:------------:|
| 2.1 Static realtime | ✅ (no `std::function`) | ✅ **`--fail-known-debt` passes** | ✅ maintain |
| 2.2 Runtime realtime | — | ✅ **prerequisite + pass** | ✅ per bet |
| 2.3 Determinism hashes | — | ✅ (parity oracle) | ✅ (esp. B1, B8) |
| 2.4 TSAN concurrency | ✅ (producer assert) | ✅ extend | ✅ per bet |
| 2.5 Codec fuzz/fixtures | — | ✅ (+ writer round-trip) | — |
| 2.6 Routing/graph | — | ✅ (seam + facade) | ✅ (B3 spatial) |
| 2.7 Docs & API | ✅ (docs-path CI) | ✅ (installed-header) | ✅ (API inventory) |
| 2.8 Packaging/budgets | ✅ (version single-source) | ✅ (benchmark budgets) | ✅ (ABI diff, B7) |
| 2.9 Downstream conformance | — | ✅ (define suite) | ✅ (extend) |

---

## 4. Standing CI principles

- **No silent caps:** if a gate samples or bounds coverage, `log` what was skipped —
  a green check must not imply coverage it didn't provide.
- **Fail closed on realtime:** once ORP134 flips `--fail-known-debt`, a reintroduced
  audio-thread read is a hard CI failure, not a warning.
- **Determinism is non-negotiable:** cross-platform hash drift is a tracked bug.
- **Docs are part of done:** contract-changing PRs update headers + `ARCHITECTURE.md` +
  `CHANGELOG.md` or fail review.

---

## 5. What to build first (verification bootstrap)

1. **ORP133:** docs-path CI (2.7) + producer-assert TSAN (2.4) — cheap, immediate.
2. **Before ORP134:** the runtime realtime harness (2.2) + golden render-hash (2.3).
   These two unblock and de-risk the streaming reader.
3. **During ORP134:** codec fuzzing (2.5), routing/graph (2.6), conformance suite (2.9).

*Parent: [[ORP132]]. This framework is referenced by every sprint rather than duplicated.*
