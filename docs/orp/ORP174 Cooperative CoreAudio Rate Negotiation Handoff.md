<!-- SPDX-License-Identifier: MIT -->

# ORP174 — Cooperative CoreAudio Rate Negotiation Handoff

**Document type:** SDK implementation and downstream handoff  
**Status:** Implemented and merged; downstream adoption deferred to FTR079  
**Date:** 2026-08-08  
**Consumer:** FourTrack / EightTrack, FTR079  
**Implementation PR:** [orpheus-sdk#242](https://github.com/chrislyons/orpheus-sdk/pull/242)  
**Merged SDK revision:** `498c02222f11a81f3dcc3e726d0355bdb20866ae`  
**Pre-sprint SDK baseline:** `8333a04a47cd9c5f8a2dcd78fb185f6984b2069e`  
**Related:** ORP172, ORP173; FourTrack FTR078 and FTR079

---

## 1. Decision and adoption boundary

ORP174 replaces active CoreAudio rate reassertion with a bounded activation
transaction. The merged SDK now negotiates a requested rate only after the
ORP172 compatibility resolver accepts the route, monitors active physical
directions passively, and closes callback admission on terminal route or
callback-capacity failures.

FourTrack was not changed by PR #242. Its SDK gitlink, bridge, model, and UI
remain outside this implementation. FTR079 owns adoption from the exact merged
revision above and the application-level hardware matrix.

## 2. Delivered SDK contracts

- `AudioRouteRuntimeOutcome` is the sole public runtime-outcome taxonomy.
  `AudioIoTelemetry` contains only cumulative input-render failures and that
  route outcome. The C++ package version is `0.7.0`; `ORPHEUS_ABI_VERSION`
  remains `1.0`.
- The installed package fixtures accept the current minor and reject the
  previous minor. Separate expected-failure consumers reject the retired enum
  and retired telemetry field.
- CoreAudio property access is supplied through the injectable
  `ICoreAudioPropertyApi` read/write/listener/settable seam. The rate-only
  property monitor and its retired poll taxonomy are removed.
- `CoreAudioSampleRateTransaction` rereads nominal rate and settable state,
  registers listeners before writes, writes output before a distinct input,
  waits for listener-confirmed readback, and rolls back in reverse order only
  when fresh readback still equals the requested rate. Same-rate activation
  performs no rate write or listener registration.
- Automatic hog-mode permission is scoped to activation. A value changed by the
  driver is restored on pre-publication failure and normal stop; restoration
  failure publishes `BackendFailure`. Nominal rate is never restored by hog
  cleanup.
- Route monitoring preserves physical output, optional physical input, and
  private aggregate order, reports directional loss before aggregate loss, and
  uses passive atomic generation/state waiting. Terminal admission is latched
  once and later healthy observations cannot reopen it.
- Render facts publish sample rate, maximum hardware callback frames, client
  chunk size, and channel counts atomically before start. The callback validates
  the complete AudioBufferList, renders the full hardware span once, invokes
  the client in contiguous chunks, and copies every internal output frame once.

## 3. Implementation record

The implementation branch was based on `8333a04a47cd9c5f8a2dcd78fb185f6984b2069e`
and merged at `498c02222f11a81f3dcc3e726d0355bdb20866ae` with these three
commits:

- `4f1f804836bdab0600e20957573bbece7b704ea9` —
  `refactor(audio-io): unify route runtime outcomes`
- `7b1e45cb` — `refactor(coreaudio): negotiate sample rate cooperatively`
- `4ee8f2ad40357f7400a90ed342bcf7a8e902cc18` —
  `fix(coreaudio): deliver complete hardware callbacks`

## 4. Verification evidence

Observed on the macOS arm64 SDK checkout at the merged implementation tip:

- `cmake --build --preset sdk-debug --parallel` completed successfully.
- `ctest --test-dir build-sdk-debug --output-on-failure` completed successfully
  for the configured 80-test tree.
- Installed package and consumer checks passed, including current-minor
  acceptance, previous-minor rejection, telemetry contract assertions, retired
  enum rejection, and retired telemetry-field rejection.
- `PYTHONDONTWRITEBYTECODE=1 python3 tools/realtime_audit.py` passed with zero
  hard failures and zero tracked debt findings.
- CoreAudio deterministic coverage passed transaction ordering, same-rate and
  same-device cases, settable/write/timeout failure handling, reverse rollback,
  third-party rate preservation, delayed listener delivery, passive route loss,
  output-only stale-input auditing, hog-mode restoration, and complete callback
  chunk delivery.

## 5. Deferred hardware evidence

The following evidence is not claimed by this SDK-only handoff:

- Windows WASAPI compilation, execution, and real-device acceptance; no local
  Windows runner is available.
- FourTrack application-level manual validation after FTR079 adoption for
  built-in and USB routes, same-device duplex, distinct-device/private
  aggregate routes, directional default-device changes, unsupported-rate
  selection, external 44.1/48 kHz rate changes, buffer-size changes, permission
  denial, and disconnect/reconnect.

No FourTrack adoption result is inferred from this record.
