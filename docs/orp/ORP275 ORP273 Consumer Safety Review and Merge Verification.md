<!-- SPDX-License-Identifier: MIT -->

# ORP275 — ORP273 Consumer Safety Review and Merge Verification

**Date:** 2026-10-23  
**Scope:** SDK PR #277, `feat/orp273-sprint-remediation` against `main`  
**Baseline head:** `11b303ab03aaa676209fc4ffa04e8455e4426ec1`  
**Base:** `ad62b0f8ca1f4a705fd28451b9cd5e805a0cc2b2`

## Review contract

Review the optimization/refactoring work for downstream safety, not only local
compilation. Fix demonstrated regressions and ineffective gates; retain the
single cross-platform render golden and the existing half-deadline budget.
Merge only after the reviewed code and required CI checks pass. Consumer SDK
pins, consumer repositories and the governed ShmUI import stay untouched.

The original ORP274 observations remain historical. Its broad cross-platform
reproducibility statement was not supported by the initial PR's CI: the first
run failed Ubuntu and Windows. This record supersedes those claims with
observed review evidence; it does not turn the earlier failing run into proof.

## Initial CI findings and corrections

Evidence: GitHub Actions run `39755340663` for PR #277.

| Finding | Consumer/delivery risk | Correction |
| --- | --- | --- |
| Ubuntu Debug and Windows Debug/Release render golden failures | The pinned elementary functions did not pin surrounding floating-point contraction; platforms rendered different bytes. | Preserve the working-tree multiply/add correction, remove software `fma` dependence, and explicitly disable implicit contraction in transport/routing target compilation. Review and regenerate one golden, never platform-specific goldens. |
| Ubuntu Release `-Werror=mismatched-new-delete` | The allocation guard's header definitions were inlined into ordinary STL consumers and prevented the Release test target from compiling. | Move replacement allocation functions and their shared state into one linked `tests/support/rt_guard.cpp` translation unit per guarded test executable; keep warning and allocation checks enabled. |
| Windows Release package-consumer `sndfile.dll` permission denial | Parallel vcpkg app-local steps wrote the same runtime destination while building independent installed-package fixtures. | Give each fixture its own runtime output directory; keep parallel builds and all package checks. |
| Performance validator could still accept CTest's exit-zero/no-tests path | A configured source test name did not prove that this build tree actually ran the measurement. | Require the exact enforcing gate's result row to report `Passed`; missing, skipped, failed and not-run measurements fail validation. |
| Windows Release TruePeak p99 exceeded the gate | Reported p99 26,192.6 microseconds exceeded the unchanged 21,333.3-microsecond limit. Median 25,200.8 shows broad compute cost, not one isolated scheduling outlier. | Keep the gate and ratio intact; require production-path correction and hosted verification before merge. |

The inherited CI matrix excludes macOS Release; this PR did not remove that
leg. The existing five-runner evidence contract is retained. No runner is
removed to make this review pass.

## Deterministic arithmetic evidence

The pre-existing uncommitted change in `src/core/transport/pinned_math.h`
replaced explicit `std::fma` polynomial steps with separately rounded products
and adds. The review retained and integrated its intent, corrected stale
comments and bounded the accuracy claim.

- On the local unsanitized Release tree, that working-tree correction alone
  preserved ORP274's ARM golden `0x2885ce8fbb52d42d`.
- Disabling implicit contraction in surrounding transport/routing arithmetic
  changed the observed ARM golden to `0x2c62d3e2104b576d`. This is direct evidence
  that the previous pinned-function change alone did not pin render arithmetic.
- The same newly reviewed golden passes block sizes 256, 512, 1024 and 2048 and
  repeated renders locally. Hosted CI must prove the same constant, not select
  a different expected value per platform.
- A throwaway C++20 optimized smoke sampled 1,000,001 positions: maximum
  sin/cos absolute error `3.3306690738754696e-16` on `[0, pi/2]`; exponential
  relative error `1.4432899320127035e-15` over the corresponding gain range;
  float-rounded `pow10` differed by zero ulps from local libm on `[-4.8, 1.2]`.
- Accuracy against one host's libm is not universal correct-rounding proof.
  The render fixture also does not cover every enabled EQ/compressor program;
  those still contain other control/DSP mathematics. No universal DSP
  determinism claim follows from this fixture.

## CoreAudio and public/private boundary review

Callback terminal publication now uses atomics without waking a waiter; the
route worker uses the bounded control-side wait. Render admission observes the
terminal state without waiting for worker wakeup. The changed capacity check is
an intentional compatibility boundary: an advertised callback capacity above
4096 frames is refused with `NotSupported`, even if the requested buffer is
smaller. Consumers must surface that refusal rather than claim all prior
hardware configurations remain accepted. Physical-device acceptance is not
inferred from injected tests.

The ShmUI import and optional export boundary are unchanged by this PR. Default
ShmUI-off builds do not hide the source files that remain in the SDK checkout.
ORP276 is the actionable ShmUI-team handoff; a future source-ownership migration
requires coordinated SDK and consumer integration, not hand-editing this mirror.

## Tooling smoke evidence

`node scripts/validate-performance.js --build-dir=/tmp/orp277-empty-ctest`
ran real CTest against an empty throwaway inventory. CTest reported no tests;
the validator reported `not measurable` and exited 1. No success was inferred
from CTest's exit code.

## Additional consumer-safety findings resolved

- The block smoother could overwrite a pending producer update while adopting
  its predecessor. It now consumes the pending flag before reading the
  published target; `getTarget()` reads only the atomic publication.
- Active-only clearing left inactive backing lanes vulnerable to direct host
  route edits. Inactive channel pointers are now null, restored on admission
  and nulled on retirement. A nonzero playback → panic → host route edit
  regression proves silence without full-capacity command-path clears.
- The sorted worker identity lookup introduced repeated scratch allocation.
  Its carry vector now retains capacity across rounds without changing the
  public worker object's layout; its test driver uses the same scratch lifetime.
- Parser collection budgets originally ran after consuming the next value.
  Limits and duplicate-key rejection now precede value parsing; file input is
  capped before and during reads.
- Snapshot-based rollback quantized original beat values and lost pending grid
  dirtiness. The rollback image now clones exact internal beat state and retains
  that dirtiness; issued IDs stay monotonic, failed unpublished batches do not
  consume IDs, and rollback remains allocation-free.
- Serialization now preflights parser-compatible structures and escaped strings,
  validates marker numbers and rejects oversized output before touching the
  saved target. Regression coverage preserves the previous document on refusal.
  The final byte check follows stream construction, so numeric/formatting
  overhead can still temporarily exceed 64 MiB; disk publication remains atomic.
- Public gain APIs accept extreme finite numbers, beyond the ordinary documented
  range. Exponential conversion now saturates before narrowing the exponent to
  `int`; regressions exercise public extreme attenuation and positive overflow.
- The performance validator now forwards `--config` (default `Release`) to
  CTest so Visual Studio multi-configuration trees can run the enforcing gate.

Independent final realtime and math/CI reviewers reread the fixes and reported
no remaining actionable blockers in their slices. They did not run checks;
runtime evidence below was exercised separately.

## Local verification after review fixes

Only named targets were built, not the previously timed-out broad local build.
Release uses `build-release-clean` with sanitizers off; Debug uses `build` with
ASan/UBSan.

| Check | Observed result |
| --- | --- |
| Release session/DSP/allocation-guard batch | 6/6 CTest entries passed |
| Release render/routing/ingress/CoreAudio/static-contract batch | 12 correctness/contract entries passed; initial concurrent deadline entry failed as described below |
| Installed clean-prefix, package-runtime consumer and stable ABI link | 3/3 passed, including all four discovery orders in the package harness |
| Debug session/routing/smoother/streaming/hash/CoreAudio batch | 6/6 passed under ASan/UBSan |
| Debug extreme-gain, inactive-lane and hash batch | 3/3 passed under ASan/UBSan |
| Actual minhost render and `afinfo` inspection | C ABI session/clipgrid/render v1.0 negotiated; 4-second stereo 44.1-kHz PCM16 WAV, 176,400 frames / 705,600 audio bytes |
| Real Release performance validator | Passed the exact registered gate in 9.91 seconds, unchanged p99 ratio 0.5 |
| Empty CTest inventory validator | Exit 1: unmeasurable, not a false pass |

The first local deadline run overlapped the package-consumer compile. It failed
with large wall/CPU divergence (TruePeak wall p99 116,888 microseconds versus
thread-CPU p99 24,763). It is retained as contaminated-run evidence, not presented
as a passing run or a hardware measurement. After correcting the non-contracted
ARM scalar path with phase-parallel NEON and running the gate alone, it passed:

| Synthetic mix, 48 kHz / 2048 frames | Wall p99 (microseconds) | Maximum (microseconds) |
| --- | --- | --- |
| Transport 32 voices / 8 sources / 255 commands | 4,165.62 | 5,025 |
| Routing 256 channels / 32 groups / 32 outputs, Peak | 5,755.42 | 5,892.21 |
| Same routing, TruePeak | 10,352.7 | 16,344.3 |

All three mixes report zero guarded allocations/deallocations. The frame
deadline is 42,666.7 microseconds; the p99 ceiling is 21,333.3. The x64 and arm64
SIMD paths load a compile-time transpose of canonical coefficients and retain
per-phase tap order; scalar-reference tests guard numerical parity. These are
synthetic CPU measurements, not physical-device or downstream UI qualification.
Windows acceptance still requires hosted CI against the same unchanged budget.

## Hosted review follow-through

Run `39932281768` at reviewed head `51c1cc79f` still failed the golden on Linux
and Windows, plus the unsanitized x86 deadlines. The unique runtime-directory
change fixed the Windows package-consumer failure and the isolated allocation
hooks fixed GCC compilation; those checks reached/passed their intended paths.
Lint, macOS Debug, Linux TSan and syscall evidence passed. The failing run is
not acceptance evidence.

The exact golden exercises soft clipping: already around frame 20, left-channel
input exceeds the 0.794 knee threshold. Thus `std::tanh(float)` still changed
audio bytes by platform/build even after contraction was pinned. The correction
keeps clipping enabled for every consumer, moves shared numeric helpers to
`src/core/common/pinned_math.h` without an old-path shim and evaluates the knee
with the seven-level continued-fraction rational [6]. The float argument/return
boundaries remain unchanged. For knee arguments >= 1 the original expression
already exceeds the final 0.9999 ceiling; the bounded branch preserves that
saturated output without evaluating a wider transcendental.

The exercised 1,000,001-point final-output sweep over magnitudes `[0,4]` reports
maximum error one float ULP (`5.9604644775390625e-08`) against local libm,
monotonic output and unchanged ceiling. The permanent routing test covers signed
threshold/nextafter, finite extremes, inactive right output and clipping count;
it replaces a previous nonnegative-counter tautology. The deliberately reviewed
new scenario golden is `0x212b8a01988459a9`. Release routing/hash tests pass,
Debug ASan/UBSan routing/hash/gain tests pass (3/3), the unchanged Release budget
validator passes in 9.90 seconds and the real minhost render succeeds.

The hosted deadline failures were compute cost, not dismissed as scheduling:
Linux TruePeak wall/thread-CPU p99 was 28,448/28,450 microseconds, Windows wall
p99 33,578. SSE2 now uses the same ordered phase-parallel kernel as x64, with
vector absolute/max reduction instead of four scalar lane reductions. VS 2022
uses `/fp:precise`, which Microsoft documents as non-contracting by default,
without `/fp:strict`'s floating-environment exception/trap overhead [7].
Older MSVC retains strict mode.

A throwaway Rosetta x86_64 AppleClang smoke compared finite and zero/Inf/NaN
kernel checksums with the preceding implementation: both matched. A separate
maximum-topology routing smoke produced output lane 0 = 0.32 with p99 13,242.38
microseconds, below the unchanged 21,333.33 ceiling. This is emulated x86 evidence,
not GCC/MSVC or hardware acceptance. Small paired per-sample timing differences
were inconsistent by branch and are not claimed as a measured speedup.
Hosted checks must still pass the single golden and unchanged budgets.

## Consumer obligations

- Rebuild C++ SDK libraries and consumers against the matching headers. Public
  parser/session and clip-DSP class layouts changed; this is not a safe binary
  replacement under old prebuilt C++ objects. Stable C ABI 1.0 remains unchanged.
- Re-consume the installed package, not private `src/` headers. SDK-owned clean
  prefix fixtures exercise the host-neutral session, audio, DSP, routing,
  transport, telemetry and compatibility target contracts.
- Regenerate consumer-owned rendered-byte artifacts deliberately after review;
  the transport fixture golden changes from the pre-sprint value and ORP274's
  intermediate value. Do not blindly replace unrelated consumer goldens.
- Handle bounded JSON/session refusal as an error, not truncation; preserve the
  last valid session and future-schema/recovery distinctions.
- No app pin, child-app CI, hardware-support promotion, version bump, or ShmUI
  migration is part of this branch.

## References

[1] PR #277, https://github.com/boot-industries/treefall-sdk/pull/277.  
[2] Initial CI run, https://github.com/boot-industries/treefall-sdk/actions/runs/39755340663.  
[3] ORP273, original review requirements.  
[4] ORP274, original remediation and local measurement record.  
[5] ORP276, ShmUI private-source compartmentalization handoff.
[6] NIST, “§4.39 Continued Fractions,” *Digital Library of Mathematical
Functions*, Eq. 4.39.1. https://dlmf.nist.gov/4.39.E1 (accessed 2026-10-23).  
[7] Microsoft, “/fp (Specify floating-point behavior),” VS 2022 `/fp:precise`
and `/fp:strict`. https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170 (accessed 2026-10-23).
