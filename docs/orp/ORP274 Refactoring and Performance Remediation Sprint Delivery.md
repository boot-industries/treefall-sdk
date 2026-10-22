<!-- SPDX-License-Identifier: MIT -->

# ORP274 - Refactoring and Performance Remediation Sprint Delivery

**Document type:** Sprint delivery record for the ORP273 remediation
**Date:** 2026-10-22
**Sprint branch:** `feat/orp273-sprint-remediation`
**Base revision:** `ad62b0f8ca1f4a705fd28451b9cd5e805a0cc2b2` (branch `main`)
**Implements:** `docs/orp/ORP273 Refactoring and Performance Optimization Review.md` (findings F-01 … F-20)
**Scope:** `src/core/transport/`, `src/core/routing/`, `src/core/session/`,
`src/core/common/`, `src/platform/audio_drivers/coreaudio/`,
`include/orpheus/clip_dsp.h`, `include/orpheus/json.hpp`,
`include/orpheus/session_graph.h`, `tests/`, `scripts/`,
`.github/workflows/`, `tools/realtime_audit.py`, `ARCHITECTURE.md`,
`budgets.json`
**Status:** Complete with recorded deviations

## 1. Executive summary

ORP273 recorded twenty findings against the SDK's render path, realtime
backends, session layer, build tooling and documentation. This sprint lands a
remediation for every one of them, in the wave order ORP273 §5 prescribes:
Wave 0 (measurement hygiene) → Wave 1 (realtime correctness) → Wave 2
(render-path redundancy) → Wave 3 (structural decomposition) → Wave 4
(tooling, calibration and documentation).

The wave order is not cosmetic. Waves 1 through 3 change rendered bytes,
metering arithmetic and per-block work; Wave 0's job is to make the repository
able to *prove* what those waves did. Landing a performance change before a
registered, unsanitized, thresholded measurement exists would produce exactly
the kind of unmeasurable claim ORP273 §5 warns about. So the sprint starts by
making the performance-budget validator honest (F-07), by guarding timing
assertions under sanitizers (F-17), by registering the render-path deadline
measurement that already exists in the tree but was invisible (F-18), and by
bringing `ARCHITECTURE.md` back in line with the code (F-19).

Wave 1 then fixes the realtime-path defects: the CoreAudio device callback no
longer notifies a waiter (F-01), every CoreAudio route gets one explicit
callback-frame ceiling (F-11), and the render path's libm dependence is
replaced with pinned, platform-independent elementary functions so the
render-hash golden can finally be pinned as a constant (F-02).

Wave 2 removes the per-block and per-sample redundancy the review measured:
the whole-pool lane memset (F-03), the three out-of-line smoother calls per
sample (F-04), the redundant meter scratch clears (F-09), the second
full-sample TruePeak pass (F-10), the per-group headroom rescan (F-08), the
17,936-byte telemetry rebuild (F-05), the compressor's constant `pow` term
(F-06), the per-clip session sort/validate (F-13) and the worker's quadratic
identity scan (F-20).

Wave 3 is structural: explicit JSON and session-document limits (F-12), an
allocation-free transaction rollback (F-14), the extraction of the control-side
clip-source registry (F-15) and of the matrix-level meter state (F-16).

Wave 4 calibrates the F-18 threshold against the Wave 2 render path and
finalises the documentation.

Three scope decisions are taken here and are not reopened later. They are
stated in full in §6:

1. **F-02's scope is extended** beyond the EqualPower fade to the three libm
   dependencies that reach the rendered samples — the fade's `std::sin`, the
   pan law's `std::cos`/`std::sin` and the clip gain's `std::pow`. ORP273's own
   acceptance test for F-02 ("the same constant hash across … at least two
   platforms") is unsatisfiable while any of them remains libm-dependent.
2. **F-06 keeps the dB-domain gain law** and lands a bit-exact fast path,
   because ORP273 F-06 explicitly excludes a DSP-contract change.
3. **F-20 keeps `include/orpheus/clip_source.h` unchanged** by using the
   sort/`lower_bound` variant of the record's remedy; the slot-array variant
   is deferred because it would change an installed public header, which
   ORP273 §6.2 does not account for.

The sprint changes three installed public headers: `include/orpheus/clip_dsp.h`
(F-06), `include/orpheus/json.hpp` (F-12) and `include/orpheus/session_graph.h`
(F-13, F-14). F-02 additionally changes the rendered sample values, so any
consumer artefact that pins a rendered-audio hash must be regenerated. No
consumer repository is modified by this sprint.

## 2. Sprint plan and wave order

The wave assignment below is ORP273 §5's. Each finding lands as its own
focused commit; the commit SHA is filled in as the waves land and the
authoritative per-finding record is §3.

| Wave | Finding | What lands | Dependency | Blast radius | Commit |
| --- | --- | --- | --- | --- | --- |
| 0 | F-07 | The budget validator reports what it measured and fails when a declared budget could not be measured. | None | None | `728f956e2` |
| 0 | F-17 | Timing assertions are guarded under sanitizers, via a shared `tests/support/sanitizer_guard.hpp`. | None | None | `5b095b6d3` |
| 0 / 4 | F-18 | The already-existing worst-case deadline harness becomes a registered CTest gate with environment-overridable observations, warm-ups, rates and a clamped p99 ratio; thresholds are calibrated after Wave 2. | None / Wave 2 | None | `822a8b5a5` |
| 0 | F-19 | The five stale `ARCHITECTURE.md` statements are corrected against the tree. | None | None | `90692ecb6` |
| 1 | F-01 | The CoreAudio device callback publishes the route-monitor terminal state atomically and does not notify a waiter; the unreachable duplicate malformed-output block is removed. | None | None | `6b359c2d7` |
| 1 | F-02 | The render path's libm dependence is replaced by pinned elementary functions and the render-hash golden is pinned as a constant. | F-18 | Rendered sample values change; goldens regenerate | `36e7ceab4`, `a47fb4a1a` |
| 1 | F-11 | One explicit SDK callback-frame ceiling for all CoreAudio routes, enforced at initialisation before buffers are allocated. | None | None | `bc42c8dee` |
| 2 | F-03 | The per-block full-lane memset is replaced by a voice-lifetime lane-zero invariant. | F-18 | None | `adf783799` |
| 2 | F-04 | `GainSmoother::process()` is inlined; the smoother ramp state is hoisted out of the per-sample loop. | F-03 | None | `54bc76938` |
| 2 | F-09 | The redundant meter scratch clears are removed and the write paths made total. | F-05 | None | `a43f473fb` |
| 2 | F-10 | The true-peak metering path is fused into a single per-sample pass. | None | None | `1675a34a2` |
| 2 | F-08 | Headroom channel counts are accumulated once per block. | F-10 | None | `41f05df4d` |
| 2 | F-05 | The redundant meter telemetry copies are removed and replaced by field-wise writes. | F-09, F-10 | None | `46979a76d` |
| 2 | F-06 | The compressor's constant makeup term is precomputed on the control thread. | None | None | `2e393db24` |
| 2 | F-13 | Session parsing uses a bulk per-track construction path. | None | Public C++ header (`include/orpheus/session_graph.h`) | `cb332cfa8` |
| 2 | F-20 | The worker snapshot uses a sorted lookup instead of a nested identity scan. | None | None | `c0d8a8284` |
| 3 | F-12 | Explicit document-byte and structural limits for session and JSON parsing. | None | Public C++ header (`include/orpheus/json.hpp`) | `3cd0668b0` |
| 3 | F-14 | A no-throw, non-allocating transaction rollback. | None | Public C++ header (`include/orpheus/session_graph.h`) | `574c393ac` |
| 3 | F-15 | The control-side source registry is extracted from `TransportController`. | F-03, F-04, F-05 | None | `a9f75580d` |
| 3 | F-16 | Meter storage and publication are extracted from `RoutingMatrix`. | F-08, F-09, F-10 | None | `d73749180` |
| 4 | F-18 (calibration) | The registered measurement's p99 ratio is calibrated against the Wave 2 render path. | Wave 2 | None | recorded in §4 |
| 4 | F-19 (verification) | `ARCHITECTURE.md` corrections verified against the tree. | None | None | recorded in §7 |

**Wave 0 is the prerequisite for judging Waves 1 to 3.** No release-
representative measurement of the render path exists in this repository
today, so a performance change landed before F-18 would be an unmeasurable
claim.

**Wave 1 is safe to land without a downstream rebuild**, with one caveat:
F-02 changes rendered bytes, so a consumer that pins a rendered-audio hash
must regenerate it.

**Wave 2 is safe to land without a downstream rebuild** except for F-13, which
adds a public method to `include/orpheus/session_graph.h`, and F-06, which adds
one private member to `include/orpheus/clip_dsp.h`. Every rendered-output change
must keep the existing goldens valid.

**Wave 3 requires a Clip Composer rebuild and a FreqFinder package
re-consumption** for F-12 and F-14, because both change an installed public
header. F-15 and F-16 are private-header decompositions: they require an SDK
rebuild, and a re-pin or re-consumption for consumers that build from source,
but no consumer source change.

**Wave 4 is deliberately last**: the thresholds are calibrated against the
Wave 2 render path, not against the path being changed.

### 2.1 Scope decisions taken at sprint start

These three decisions are fixed at the start of the sprint and are recorded
here so that they are not reopened mid-implementation. Their consequences are
detailed in §6.

1. **F-02 scope extension.** The finding as recorded names the EqualPower
   fade's `std::sin`. The sprint also replaces the pan law's `std::cos`/
   `std::sin` and the clip gain's `std::pow`, because those reach the
   rendered samples and ORP273's own acceptance test for F-02 requires the
   same constant hash across platforms. Leaving any of them libm-dependent
   would make that acceptance test unsatisfiable. The six libm callsites in
   `src/core/transport/transport_controller.cpp` (`:109`, `:116`, `:361`,
   `:3025`, `:3239`, `:3377`) are the only ones in that translation unit, and
   `src/core/transport/clip_dsp.cpp:16-18` holds the seventh.
2. **F-06 keeps the dB-domain gain law.** The finding's remedy is applied as a
   bit-exact fast path: the per-frame `std::pow` is skipped only in the
   non-reducing case, where its result is provably the constant makeup gain.
   The `std::log10` remains, because removing it would change the DSP
   contract, which ORP273 F-06 explicitly excludes.
3. **F-20 keeps `include/orpheus/clip_source.h` unchanged.** The sorted
   `lower_bound` variant of the remedy is used instead of the slot-array
   variant, because the slot-array variant would change an installed public
   header and ORP273 §6.2 does not account for that consequence.

## 3. Implementation record

Every finding ORP273 recorded is listed here in finding order. The
`Status` column states what landed; `Wave` is ORP273 §5's wave assignment.
Anchors are post-sprint line numbers on the sprint branch.

### F-01 — CoreAudio device callback notifies a waiter from the render callback

| Field | Value |
| --- | --- |
| Finding | F-01 |
| Status | Landed |
| Severity / Class | Critical / Both |
| Wave | 1 |
| Anchors (post-sprint) | `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.h:69`, `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.h:109`, `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:115`, `src/platform/audio_drivers/coreaudio/coreaudio_route_monitor.cpp:129`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:708`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:795` |
| What changed | The route monitor's terminal transition is split into an atomic-only publication callable from the device callback (`closeAdmissionFromCallback()`) and a signalling step callable only from control or worker context (`closeAdmission()`). The generation counter and its `notify_one()` are replaced by a condition variable with a bounded 100 ms wait, because `std::atomic<uint64_t>::wait()` has no timed variant. The unreachable duplicate malformed-output block in the render callback is removed. |
| Cost mechanism | One futex wake per malformed callback becomes zero on the callback path. The worker observes a callback-published terminal state within one 100 ms bound instead of immediately. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^coreaudio_driver_test$'` → `1/1 Test #74: coreaudio_driver_test ............ Passed 10.86 sec`; `python3 tools/realtime_audit.py --root . --fail-known-debt` → `Realtime audit passed: 0 hard failures, 0 tracked debt findings.`; the two new regression tests `DeviceCallbackTerminalTransitionDoesNotNotify` and `WorkerObservesCallbackPublishedTerminalState` pass (the latter observes the terminal state in 106 ms, inside the 100 ms wait bound plus scheduling). |
| Downstream impact | None |
| Commit | `6b359c2d7` `fix(coreaudio): stop the device callback notifying a waiter` |

### F-02 — EqualPower fade evaluates `std::sin` per rendered sample, and the render-hash golden is unpinned because of it

| Field | Value |
| --- | --- |
| Finding | F-02 |
| Status | Landed |
| Severity / Class | Critical / Both |
| Wave | 1 |
| Anchors (post-sprint) | `src/core/transport/pinned_math.h:1`, `src/core/transport/transport_controller.cpp:36`, `src/core/transport/transport_controller.cpp:291`, `src/core/transport/transport_controller.cpp:3054`, `src/core/transport/clip_dsp.cpp:19`, `tests/transport/transport_render_hash_test.cpp:59` |
| What changed | A new private header `src/core/transport/pinned_math.h` provides libm-independent `sin`, `cos` and `exp` built on `std::fma`, which IEEE-754 requires to be correctly rounded. The EqualPower fade, the mono and stereo pan laws, and the three clip-gain conversions now use them, and the clip DSP's `dbToLinear` does too. The render-hash gate now pins the scenario hash as a constant and asserts it. |
| Cost mechanism | Up to 196,608 `std::sin` calls per block at 32 voices and 2,048 frames become zero libm calls on the render path. The pinned `sin` measured 1.83 ns per call against the host libm's 3.23 ns. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^transport_render_hash_test$'` → `[Render Hash] block 256: 0x2885ce8fbb52d42d` at all four block sizes, `100% tests passed`. The pinned functions were measured against the host libm: `double \|pinnedSin - sin\| <= 3.331e-16`, `double \|pinnedCos - cos\| <= 3.331e-16`, `double \|pinnedExp - exp\| <= 2.665e-15`; the float32 results differ from the host libm in 6,204 of 1,000,000 sampled gains and 13,252 of 1,000,000 sampled fade positions, always by at most one float32 ulp. |
| Downstream impact | Regenerate rendered-audio hash. The scenario hash moves from `0xfce554ccf7c66ab9` to `0x2885ce8fbb52d42d`. |
| Commit | `36e7ceab4` `fix(transport): pin the render path's libm dependence`; `a47fb4a1a` `test(transport): relax the fade-start silence assertion for the pinned fade` |

### F-03 — Every audio block clears all 256 voice-slot routing lanes, including lanes of voices that are not playing

| Field | Value |
| --- | --- |
| Finding | F-03 |
| Status | Landed |
| Severity / Class | High / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/transport/transport_controller.cpp:583`, `src/core/transport/transport_controller.cpp:1961`, `src/core/transport/transport_controller.cpp:2155` |
| What changed | The per-block whole-pool memset is replaced by a clear of only the lanes owned by active voices. The invariant that an inactive slot holds no stale audio is kept by zeroing the whole block window of a slot when a voice is admitted into it and when a voice is retired from it. Lane ownership is keyed on the slot index because voice retirement is a swap-remove. |
| Cost mechanism | The per-block clear drops from 256 lanes to the active voice count times the source channel count — up to 8x less at 32 active voices, and proportionally less below that. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^transport_render_hash_test$'` → `100% tests passed`; the pinned render hash is unchanged, so the rendered bytes are unchanged. `ctest --test-dir build --output-on-failure -R '^multi_clip_stress_test$\|^callback_queue_stress_test$\|^voice_state_tsan_test$'` → `100% tests passed`. |
| Downstream impact | None |
| Commit | `adf783799` `perf(transport): clear only the routing lanes that are in use` |

### F-04 — The routing inner loop advances three out-of-line gain/pan smoothers per channel per sample, two of which are discarded under the transport's default policy

| Field | Value |
| --- | --- |
| Finding | F-04 |
| Status | Landed |
| Severity / Class | High / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/routing/gain_smoother.h:66`, `src/core/routing/gain_smoother.h:109`, `src/core/routing/gain_smoother.h:139`, `src/core/routing/routing_matrix.cpp:1071` |
| What changed | `GainSmoother::process()` is defined in the class and the ramp state is hoisted out of the per-sample loop: `beginBlock()` adopts any pending target once per block, `next()` advances one sample without touching the atomics, and `endBlock()` commits the ramp. The group and master smoothers use the same pattern. The pan smoothers still adopt their pending targets under every source channel policy. |
| Cost mechanism | Two atomic loads per sample per channel become two per block per channel. At 256 channels and 2,048 frames that is 1,048,576 atomic loads per block replaced by 1,536. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^gain_smoother_test$'` → `100% tests passed`; `ctest --test-dir build --output-on-failure -R '^transport_render_hash_test$'` → the pinned render hash still matches. |
| Downstream impact | None |
| Commit | `54bc76938` `perf(routing): inline the gain smoothers and hoist their ramp state` |

### F-05 — The audio thread constructs and copies a 17,936-byte meter telemetry struct on every block

| Field | Value |
| --- | --- |
| Finding | F-05 |
| Status | Landed |
| Severity / Class | High / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/transport/transport_controller.cpp:1065`, `src/core/transport/transport_controller.cpp:1075` |
| What changed | The block's readings are collected into a small local that carries only what the block measures, and the published payload is written field by field. The group-output payload has a single holder — the scratch buffer — so it is no longer copied into a second 17,936-byte value before publication. |
| Cost mechanism | A 17,936-byte value initialisation plus a 16,792-byte copy plus a 17,936-byte copy per block become roughly 1.1 KB of field-wise copies plus the one published-payload copy. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^multichannel_transport_test$\|^callback_loss_telemetry_test$\|^transport_controller_test$'` → `100% tests passed`; the published telemetry bytes are asserted by the multichannel and callback-loss telemetry tests and are unchanged. |
| Downstream impact | None |
| Commit | `46979a76d` `perf(transport): stop rebuilding the meter telemetry struct per block` |

### F-06 — The enabled clip compressor evaluates `std::log10` and `std::pow` once per rendered frame per voice

| Field | Value |
| --- | --- |
| Finding | F-06 |
| Status | Landed |
| Severity / Class | High / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `include/orpheus/clip_dsp.h:122`, `src/core/transport/clip_dsp.cpp:80`, `src/core/transport/clip_dsp.cpp:207` |
| What changed | The compressor's constant makeup gain is precomputed on the control thread. Below the threshold the reduction in dB is exactly zero, so the per-frame `pow` is skipped and the constant makeup gain is applied instead. The reducing branch keeps the same arithmetic, and the dB-domain gain law is unchanged: the `log10` stays. |
| Cost mechanism | The per-frame transcendental count drops from two to one **only while the compressor is below its threshold**; while it is reducing, both remain. ORP273's claim that the change removes one of the two per-frame transcendentals holds only in the non-reducing case. |
| Contract surface touched | Public C++ header (`include/orpheus/clip_dsp.h`) — one private float added to `ClipDspProcessor` |
| Verification | `ctest --test-dir build --output-on-failure -R '^clip_dsp_test$'` → `100% tests passed`, including the new `CompressorBelowThresholdAppliesOnlyTheConstantMakeupGain`, which pins that the applied gain does not drift between frames and equals the makeup gain. `sizeof(ClipDspProcessor)` stays under the 4,096-byte assertion. |
| Downstream impact | Clip Composer rebuild (private member added to an installed class) |
| Commit | `2e393db24` `perf(transport): skip the compressor's per-frame pow when it is not reducing` |

### F-07 — The performance-budget validator reports success when the only configured budget was skipped and neither latency nor frequency was measured

| Field | Value |
| --- | --- |
| Finding | F-07 |
| Status | Landed |
| Severity / Class | High / Measurement hygiene |
| Wave | 0 |
| Anchors (post-sprint) | `budgets.json:4`, `budgets.json:12`, `scripts/validate-performance.js:60`, `scripts/validate-performance.js:115` |
| What changed | `budgets.json` now declares one budget that a CTest gate enforces: the render-path deadline distribution, named by `enforcedBy`. The validator resolves that name, proves the gate is registered in the repository's CTest inventory, runs it, and reports what it measured. A budget that cannot be measured is a failure, never a pass. The unenforced bundle-size, command-latency, event-frequency and degradation-tolerance entries are removed rather than left as claims nothing checks. |
| Cost mechanism | The validator's pass condition changes from "no violations were recorded" to "every declared budget's gate ran and passed". |
| Contract surface touched | Docs only (repository-root `budgets.json` and the validator script) |
| Verification | `node scripts/validate-performance.js --build-dir=build` before the gate was registered → `❌ renderBlockTime: realtime_render_deadline_distribution is not registered` and `❌ 1 budget(s) not met.`, exit 1. After the gate landed, the same command against the Debug tree reports `❌ renderBlockTime: realtime_render_deadline_distribution is not measurable in this configuration` because the measurement requires an unsanitized Release build, and exit 1. The release-tree result is recorded in §4. |
| Downstream impact | None |
| Commit | `728f956e2` `fix(perf): make the performance-budget validator report what it measured` |

### F-08 — Headroom compensation rescans every routing channel once per group on every block

| Field | Value |
| --- | --- |
| Finding | F-08 |
| Status | Landed |
| Severity / Class | Medium / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/routing/routing_matrix.h:316`, `src/core/routing/routing_matrix.cpp:975`, `src/core/routing/routing_matrix.cpp:991`, `src/core/routing/routing_matrix.cpp:1674` |
| What changed | The active channel counts are accumulated once per block from the route snapshot the render path already takes, and the headroom helper reads the cached counts. The counts use the same predicates as the rescan they replace, and the headroom arithmetic is unchanged. The two rescan helpers are removed. |
| Cost mechanism | Up to 8,192 route loads per block in the per-group modes and up to 16,384 in the global mode become one route load plus one mute load per channel per block — 512 loads instead of 16,384 at the maximum topology. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^routing_matrix_test$'` → `100% tests passed`, including `HeadroomModeNoneNoAttenuation`, `HeadroomModePerGroupAttenuates`, `HeadroomModeLogarithmicAttenuates` and the new `HeadroomModeGlobalAttenuatesAcrossGroups`, which pins that the global mode attenuates by the count across every group. |
| Downstream impact | None |
| Commit | `41f05df4d` `perf(routing): count the active channels once per block` |

### F-09 — Per-channel meter scratch lanes are cleared before being fully overwritten

| Field | Value |
| --- | --- |
| Finding | F-09 |
| Status | Landed |
| Severity / Class | Medium / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/routing/routing_matrix.cpp:1094`, `src/core/routing/routing_matrix.cpp:1110`, `src/core/routing/routing_matrix.cpp:1118` |
| What changed | The two `std::fill_n` clears of the per-channel meter scratch lanes are removed and the write paths are made total: every source channel policy writes both lanes. The muted path does not read the scratch lanes and is unchanged. |
| Cost mechanism | Two 8 KB memsets per channel per block become the writes the frame loop already performed, plus one zero store per frame under the policies that do not fill the right lane. |
| Contract surface touched | None |
| Verification | The published meter values were captured before and after the change and are byte-identical: `diff` of the routing matrix test's meter output before and after reported no difference beyond the test durations. `ctest --test-dir build --output-on-failure -R '^routing_matrix_test$\|^multi_channel_routing_test$'` → `100% tests passed`. |
| Downstream impact | None |
| Commit | `a43f473fb` `perf(routing): remove the redundant meter scratch clears` |

### F-10 — TruePeak metering performs a second full per-sample pass over every block

| Field | Value |
| --- | --- |
| Finding | F-10 |
| Status | Landed |
| Severity / Class | Medium / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/routing/routing_matrix.cpp:1225`, `src/core/routing/routing_matrix.cpp:1233`, `src/core/routing/routing_matrix.cpp:1594`, `src/core/routing/routing_matrix.cpp:1620` |
| What changed | The TruePeak estimator is fed inline during the single sample pass instead of in a second pass over the same block. The stereo path's two independent estimators are fed interleaved rather than one after the other, which changes only the call interleaving and not either meter's result. The `left == nullptr` quirk of the stereo path is preserved verbatim. The post-master output-lane true-peak path is untouched and remains a follow-up. |
| Cost mechanism | One full per-sample pass over every block is removed from the group-lane path and from the stereo path. |
| Contract surface touched | None |
| Verification | The published meter values were captured before and after the change and are byte-identical: `diff` of the routing matrix test's meter output before and after reported no difference beyond the test durations. `ctest --test-dir build --output-on-failure -R '^routing_matrix_test$\|^multi_channel_routing_test$'` → `100% tests passed`, including `TruePeakMeteringDetectsInterSamplePeaks` and `TruePeakMeterStateResetsOnSilenceMuteAndTopologyChange`. |
| Downstream impact | None |
| Commit | `1675a34a2` `perf(routing): fuse the TruePeak metering into one pass` |

### F-11 — Non-converting CoreAudio routes have no SDK-level callback-frame ceiling

| Field | Value |
| --- | --- |
| Finding | F-11 |
| Status | Landed |
| Severity / Class | Medium / Refactor |
| Wave | 1 |
| Anchors (post-sprint) | `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:22`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:366`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:407`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:1825`, `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp:1848` |
| What changed | The SDK's callback-frame ceiling is one named constant, and it is enforced unconditionally at initialisation instead of only on converting routes. The refusal still reports `SessionGraphError::NotSupported` and still happens before the render buffers are allocated and before the AudioUnit is started. The chunk size and the converter caps now name the same bound. |
| Cost mechanism | A non-converting route on a device whose advertised maximum buffer-frame capacity exceeds 4,096 is refused at initialisation instead of allocating render buffers and starting an AudioUnit on an unsupported route. |
| Contract surface touched | None |
| Verification | `ctest --test-dir build --output-on-failure -R '^coreaudio_driver_test$'` → `100% tests passed`. The new `RefusesDeviceAboveCallbackFrameCeiling` test raises the injected device's advertised maximum to 8,192 and asserts `SessionGraphError::NotSupported` with no render buffers allocated and no AudioUnit started; it fails on the pre-fix code, where the route proceeded past the capacity check. |
| Downstream impact | None |
| Commit | `bc42c8dee` `fix(coreaudio): apply one callback-frame ceiling to every route` |

### F-12 — Session documents and JSON parsing have no byte, depth, node, or collection budget

| Field | Value |
| --- | --- |
| Finding | F-12 |
| Status | Landed |
| Severity / Class | Medium / Refactor |
| Wave | 3 |
| Anchors (post-sprint) | `include/orpheus/json.hpp:31`, `include/orpheus/json.hpp:41`, `src/core/common/json_parser.cpp:71`, `src/core/common/json_parser.cpp:111`, `src/core/common/json_parser.cpp:135`, `src/core/common/json_parser.cpp:169`, `src/core/session/json_io.cpp:198` |
| What changed | The public JSON header declares `JsonParseLimits` and a parser constructor that takes them. The parser enforces a nesting-depth ceiling, a node ceiling, an object-member ceiling and an array-element ceiling, each with its own explicit error. The session layer adds a 64 MiB document ceiling in front of the parser. |
| Cost mechanism | An unbounded or deeply nested document can no longer consume unbounded memory or stack: the parser rejects the 65th nested container, the 1,000,001st node, the 1,025th object member and the 65,537th array element, and the session layer rejects a document larger than 67,108,864 bytes. |
| Contract surface touched | Public C++ header (`include/orpheus/json.hpp`) |
| Verification | `ctest --test-dir build --output-on-failure -R '^orpheus_tests$'` → `100% tests passed`, including the new `SessionJsonLimits.StructuralLimitsAreExplicitErrors` and `SessionJsonLimits.SessionDocumentByteLimitIsAnExplicitError`, which assert each distinct message and that a document at the limit still parses. The session golden fixtures under `tests/fixtures/session/` and `tools/fixtures/` parse unchanged. |
| Downstream impact | Clip Composer rebuild and FreqFinder package re-consumption (installed public header changed) |
| Commit | `3cd0668b0` `feat(session): bound session documents and JSON parsing` |

### F-13 — Session parsing sorts and re-validates the whole track on every clip insertion

| Field | Value |
| --- | --- |
| Finding | F-13 |
| Status | Landed |
| Severity / Class | Medium / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `include/orpheus/session_graph.h:89`, `include/orpheus/session_graph.h:376`, `include/orpheus/session_graph.h:387`, `src/core/session/session_graph.cpp:646`, `src/core/session/json_io.cpp:299` |
| What changed | `SessionGraph::add_clips()` appends every clip and then sorts and validates the track once. Every spec's numbers are validated before anything is appended, so a numeric rejection unwinds nothing; a rejected layout removes every clip the call added and re-sorts before it rethrows. The session loader uses the bulk path once per track. `Track::add_clip()` keeps its per-call behaviour for ordinary callers. |
| Cost mechanism | A track of N clips no longer performs N sorts and N layout validations; it performs one of each. |
| Contract surface touched | Public C++ header (`include/orpheus/session_graph.h`) — `SessionGraph::ClipSpec`, `SessionGraph::add_clips()` and `Track::append_clip()` |
| Verification | `ctest --test-dir build --output-on-failure -R '^orpheus_tests$'` → `100% tests passed`, including the new `BulkClipConstructionIsOneEditAndSortsTheTrack`, `BulkClipConstructionRejectsOverlapsWithoutSideEffects` and `EmptyBulkClipConstructionIsNotAnEdit`. |
| Downstream impact | Clip Composer rebuild and FreqFinder package re-consumption (installed public header changed) |
| Commit | `cb332cfa8` `perf(session): build a track's clips in one pass` |

### F-14 — Transaction rollback allocates inside a `noexcept` path

| Field | Value |
| --- | --- |
| Finding | F-14 |
| Status | Landed |
| Severity / Class | Medium / Refactor |
| Wave | 3 |
| Anchors (post-sprint) | `include/orpheus/session_graph.h:266`, `include/orpheus/session_graph.h:300`, `include/orpheus/session_graph.h:461`, `include/orpheus/session_graph.h:463`, `src/core/session/session_graph.cpp:241` |
| What changed | The rollback state is built once, while allocation is still permitted, into an owned `RollbackImage`, and the rollback applies it by moving. The rollback therefore performs no allocation and cannot fail to restore. The watermark rule is unchanged: a rollback never lowers a live ID watermark, and an ID issued and then rolled back stays spent. |
| Cost mechanism | The rollback's allocations — one track, one clip and one assignment vector per track, rebuilt from the snapshot — move to `begin_transaction()`, where a failure is reportable. The rollback itself allocates nothing. |
| Contract surface touched | Public C++ header (`include/orpheus/session_graph.h`) — the transaction representation |
| Verification | `ctest --test-dir build --output-on-failure -R '^orpheus_tests$'` → `100% tests passed`. The new `TransactionRollbackRestoresTheGraphWithoutAllocating` wraps the rollback in the repository's runtime allocation guard and asserts `allocViolations() == 0`, so the rollback is proven allocation-free at runtime rather than by inspection. The pre-existing transaction and rollback tests pass unchanged. |
| Downstream impact | Clip Composer rebuild and FreqFinder package re-consumption (installed public header changed) |
| Commit | `574c393ac` `perf(session): make the transaction rollback allocation-free` |

### F-15 — `TransportController` co-locates five distinct ownership domains in one class and one 3,874-line translation unit

| Field | Value |
| --- | --- |
| Finding | F-15 |
| Status | Landed |
| Severity / Class | Medium / Refactor |
| Wave | 3 |
| Anchors (post-sprint) | `src/core/transport/clip_source_registry.h:34`, `src/core/transport/clip_source_registry.h:37`, `src/core/transport/clip_source_registry.h:134`, `src/core/transport/transport_controller.h:751`, `src/core/transport/transport_controller.cpp:250` |
| What changed | The control-side clip-source registry — the registered readers, their prepared and streaming sources, the persistent metadata, the cue points, the registry mutex and the streaming worker — moves into an internal `ClipSourceRegistry`. The registry owns the worker's lifetime, so the worker is joined before any registered source is released, exactly as before. The registry mutex is taken exactly where it was taken before, and no allocation, lock or I/O moved onto the audio thread. The realtime audit's scan targets for the three registered-source lifetime helpers move with them, with the same hard realtime rules. |
| Cost mechanism | No runtime cost: the extracted state is the same state, reached through one more member. The change is a structural decomposition of a 3,874-line translation unit. |
| Contract surface touched | None (the extracted header is private; `include/orpheus/transport_controller.h` and `include/orpheus/clip_source.h` are byte-identical) |
| Verification | `ctest --test-dir build --output-on-failure -R '^streaming_seek_test$\|^callback_queue_stress_test$\|^command_ingress_test$\|^transport_render_hash_test$\|^multi_clip_stress_test$'` → `100% tests passed`; `python3 tools/realtime_audit.py --root . --fail-known-debt` → `Realtime audit passed: 0 hard failures, 0 tracked debt findings.` |
| Downstream impact | None |
| Commit | `a9f75580d` `refactor(transport): extract the control-side clip-source registry` |

### F-16 — `RoutingMatrix` co-locates configuration publication, render state, and meter state

| Field | Value |
| --- | --- |
| Finding | F-16 |
| Status | Landed |
| Severity / Class | Medium / Refactor |
| Wave | 3 |
| Anchors (post-sprint) | `src/core/routing/routing_meter_state.h:23`, `src/core/routing/routing_meter_state.h:80`, `src/core/routing/routing_matrix.h:272` |
| What changed | The thirteen matrix-level meter members move into an internal `RoutingMeterState`. The published values, the clip counters, the availability flags and the group-output seqlock protocol are unchanged; the seqlock keeps its before/after sequence loads and its coherence rule. The per-entity channel and group meters stay where they are, inside the channel and group vectors. |
| Cost mechanism | No runtime cost: the same state, in one more object. The change is a structural decomposition of `RoutingMatrix`. |
| Contract surface touched | None (`include/orpheus/routing_matrix.h` is byte-identical) |
| Verification | `ctest --test-dir build --output-on-failure -R '^routing_matrix_test$\|^multi_channel_routing_test$\|^scene_routing_test$\|^clip_routing_test$\|^transport_render_hash_test$'` → `100% tests passed`; the published meter values and the group-output snapshot's coherence and topology revision are asserted by the routing tests and are unchanged. |
| Downstream impact | None |
| Commit | `d73749180` `refactor(routing): extract the matrix-level meter state` |

### F-17 — Sanitizer CI jobs apply unsanitized timing thresholds to instrumented measurements

| Field | Value |
| --- | --- |
| Finding | F-17 |
| Status | Landed |
| Severity / Class | Medium / Measurement hygiene |
| Wave | 0 |
| Anchors (post-sprint) | `tests/support/sanitizer_guard.hpp:10`, `tests/support/sanitizer_guard.hpp:28`, `tests/common/performance_monitor_test.cpp:303`, `tests/common/performance_integration_test.cpp:186`, `.github/workflows/ci-pipeline.yml:146`, `.github/workflows/ci-pipeline.yml:323` |
| What changed | The sanitizer detection that six test translation units each carried as an in-file copy is centralised in `tests/support/sanitizer_guard.hpp` and applied at the two sites that asserted unsanitized timing thresholds without a guard. The CI sanitizer legs set `ORPHEUS_SANITIZED_BUILD=1`, so a sanitized build the preprocessor cannot detect follows the same policy. The thresholds themselves are unchanged; the unsanitized Release legs keep asserting them. |
| Cost mechanism | Two unguarded per-sample timing ceilings in the sanitizer legs become guarded ceilings; the number of atomic loads and comparisons in the guard is bounded by the number of guarded assertions, not by the workload. |
| Contract surface touched | CI workflow (`.github/workflows/ci-pipeline.yml`) |
| Verification | `ctest --test-dir build --output-on-failure -R '^performance_monitor_test$\|^performance_integration_test$\|^waveform_processor_test$\|^multi_clip_stress_test$\|^callback_queue_stress_test$\|^realtime_harness_test$'` → `100% tests passed, 0 tests failed out of 6`. |
| Downstream impact | None |
| Commit | `5b095b6d3` `fix(test): guard timing assertions under sanitizers` |

### F-18 — The clip DSP and routing findings have no in-repository measurement gate that would detect a regression

| Field | Value |
| --- | --- |
| Finding | F-18 |
| Status | Landed |
| Severity / Class | Medium / Measurement hygiene |
| Wave | 0 (registration), 4 (calibration) |
| Anchors (post-sprint) | `tests/transport/realtime_harness_test.cpp:998`, `tests/transport/realtime_harness_test.cpp:999`, `tests/transport/realtime_harness_test.cpp:1122`, `tests/transport/CMakeLists.txt:626`, `tests/transport/CMakeLists.txt:629` |
| What changed | The existing worst-case deadline harness becomes a registered CTest gate. Its observation count, warm-up count, rate list and p99 ratio are overridable from the environment, so the gate fits a CI budget while the full 10,000-block qualification stays available. The p99 ratio is clamped to 0.5, so no configuration can raise the bar above the documented ceiling. The CI Test step records the ctest log. |
| Cost mechanism | The registered gate measures 500 observations at 48 kHz; the full qualification is 10,000 observations across four rates and remains opt-in. |
| Contract surface touched | CI workflow (`.github/workflows/ci-pipeline.yml`) and the repository's CTest inventory (`tests/transport/CMakeLists.txt`) |
| Verification | `ctest --test-dir build -N -R realtime_render_deadline_distribution` → `Test #55: realtime_render_deadline_distribution`. The release-tree result is recorded in §4. |
| Downstream impact | None |
| Commit | `822a8b5a5` `test(perf): register the render-path deadline measurement` |

### F-19 — The branch carrying ORP266's Medium and Low remediations is not an ancestor of `main`, so the architecture-document corrections are still absent

| Field | Value |
| --- | --- |
| Finding | F-19 |
| Status | Landed |
| Severity / Class | Medium / Both |
| Wave | 0 |
| Anchors (post-sprint) | `ARCHITECTURE.md:253`, `ARCHITECTURE.md:254`, `ARCHITECTURE.md:263`, `ARCHITECTURE.md:267`, `ARCHITECTURE.md:277`, `ARCHITECTURE.md:368` |
| What changed | The five stale statements are corrected against the tree: the audio-file reader's memory and sample-rate claims, the WASAPI platform status, the driver and session-JSON file paths, and the `ActiveClip` sketch. The ORP266 branch's wording was reviewed and not applied wholesale, because it names `src/core/session/json_io.h`, which does not exist; the tree-verified wording is used instead. The `ALSA (Linux) – Planned for v1.0` line is left alone, because it is true today. |
| Cost mechanism | One wrong-path audit pass per reader of the document. The corrections are bounded to six statements. |
| Contract surface touched | Docs only (`ARCHITECTURE.md`) |
| Verification | `python3 tools/docs_path_audit.py --root .` → `Docs-path audit passed: 7 documents checked.` and exit 0. |
| Downstream impact | None |
| Commit | `90692ecb6` `docs(architecture): correct stale file map and contract claims` |

### F-20 — `MediaStreamWorker::snapshotSources` rebuilds the live-source vector every service round, with an O(S²) identity scan, while holding the worker mutex

| Field | Value |
| --- | --- |
| Finding | F-20 |
| Status | Landed |
| Severity / Class | Low / Performance |
| Wave | 2 |
| Anchors (post-sprint) | `src/core/transport/clip_source.cpp:1290`, `src/core/transport/clip_source.cpp:1298`, `src/core/transport/clip_source.cpp:1309` |
| What changed | The previous round's entries are sorted by source identity once and each source resolves its predecessor with a binary search, so the round costs O(S log S) instead of O(S²). The command-fill continuity, the expired-registration pruning, the rotating cursor's fairness and the worker's teardown are unchanged, and the installed public header is byte-identical. |
| Cost mechanism | Up to 500 rounds per second × S² comparisons becomes 500 × S log S. At S = 32 that is 512,000 comparisons per second replaced by 80,000. |
| Contract surface touched | None (`include/orpheus/clip_source.h` is byte-identical) |
| Verification | `ctest --test-dir build --output-on-failure -R '^streaming_seek_test$\|^transport_render_hash_test$'` → `100% tests passed`; the streaming-seek test drives `snapshotSources` directly and its expired-registration and command-fill expectations are unchanged. |
| Downstream impact | None |
| Commit | `c0d8a8284` `perf(transport): remove the quadratic identity scan from the worker snapshot` |

## 4. Measurement and evidence

### 4.1 Method

Two trees were measured with the same harness, the same shape and the same
host. The baseline is the sprint's base revision
`ad62b0f8ca1f4a705fd28451b9cd5e805a0cc2b2`, built as an unsanitized
Release tree in a detached worktree. The sprint tree is
`feat/orp273-sprint-remediation`, built as an unsanitized Release tree in
`build-release-clean`. The baseline tree carries one measurement-only patch:
the same environment overrides the sprint's F-18 change adds, so both trees
observe 20 warm-up blocks and 500 measured blocks at 48 kHz. No production
code differs between the two trees other than the sprint's own changes.

- Host: macOS, arm64, Apple LLVM 21.0.0 (clang-2100.0.123.102)
- Build: `-DCMAKE_BUILD_TYPE=Release -DORP_ENABLE_ASAN=OFF -DORP_ENABLE_UBSAN=OFF`
- Frame deadline at 48 kHz with 2,048-frame blocks: 42,666.7 µs
- Shape: 2,048 frames, 20 warm-up blocks, 500 measured blocks, 48 kHz

### 4.2 Release-tree before and after

Command, in both trees:

```
ORPHEUS_RUN_WORST_CASE_BENCHMARK=1 ORPHEUS_DEADLINE_OBSERVATIONS=500 \
ORPHEUS_DEADLINE_WARMUPS=20 ORPHEUS_DEADLINE_RATES=48000 \
./build-release-clean/tests/transport/realtime_harness_test \
  --gtest_filter='RealtimeHarnessTest.WorstCaseTopologyDeadlineDistribution'
```

| Mix | Metric | Before (`ad62b0f8c`) | After (sprint branch) | Change |
| --- | --- | --- | --- | --- |
| `transport-32voices-8source-255commands` | p50 wall | 4,108.67 µs | 3,468.83 µs | −15.6 % |
| `transport-32voices-8source-255commands` | p95 wall | 4,189.83 µs | 3,575.42 µs | −14.7 % |
| `transport-32voices-8source-255commands` | p99 wall | 4,258.42 µs | 3,644.54 µs | −14.4 % |
| `transport-32voices-8source-255commands` | max wall | 4,278.17 µs | 3,720.92 µs | −13.0 % |
| `routing-256-32-32-Peak` | p50 wall | 5,987.29 µs | 5,178.17 µs | −13.5 % |
| `routing-256-32-32-Peak` | p95 wall | 6,149.54 µs | 5,550.25 µs | −9.7 % |
| `routing-256-32-32-Peak` | p99 wall | 6,267.75 µs | 5,597.79 µs | −10.7 % |
| `routing-256-32-32-Peak` | max wall | 7,818.17 µs | 5,669.83 µs | −27.5 % |
| `routing-256-32-32-TruePeak` | p50 wall | 13,582.5 µs | 12,567.3 µs | −7.5 % |
| `routing-256-32-32-TruePeak` | p95 wall | 13,740.5 µs | 12,706.8 µs | −7.5 % |
| `routing-256-32-32-TruePeak` | p99 wall | 13,885.4 µs | 12,883.1 µs | −7.2 % |
| `routing-256-32-32-TruePeak` | max wall | 13,993.4 µs | 12,946.9 µs | −7.5 % |

Both trees report `allocations=0 deallocations=0` for every mix, so the render
path stayed allocation-free.

Run-to-run spread: two baseline runs of the same tree and shape gave a
transport-mix p99 of 4,322.79 µs and 4,258.42 µs — a 1.5 % spread. The
observed improvements are well outside that spread. The baseline's
`routing-256-32-32-Peak` maximum of 7,818.17 µs is a single outlier against
its own p99 of 6,267.75 µs; the sprint tree's maximum for that mix is
5,669.83 µs.

The worst p99 as a fraction of the frame deadline is **0.325 before the
sprint and 0.302 after it**, both on the TruePeak routing mix.

### 4.3 The gate and its threshold

The registered gate `realtime_render_deadline_distribution` runs the same
harness with 500 observations at 48 kHz and asserts the p99 is below the
frame deadline and below 0.5 of it. The measured worst p99 ratio on this
host is 0.302, so **the 0.5 threshold stands and was not lowered**; ORP274
records the calibration method rather than a moved threshold.

```
ctest --test-dir build-release-clean --output-on-failure -R '^realtime_render_deadline_distribution$'
```

```
Test project /Users/chrislyons/dev/orpheus-sdk/build-release-clean
    Start 55: realtime_render_deadline_distribution
1/1 Test #55: realtime_render_deadline_distribution ...   Passed   11.10 sec

100% tests passed, 0 tests failed out of 1
```

### 4.4 The calibration method

1. Build the sprint tree as an unsanitized Release tree.
2. Run the registered gate and record the p99 for every mix.
3. Compute the worst p99 ratio against the frame deadline.
4. If the worst ratio exceeds the declared ceiling, lower `p99_ratio` in the
   CTest property and `p99FractionOfDeadline` in `budgets.json` to a value at
   or above the observed ratio, never above 0.5, and record both numbers
   here.
5. A failure for any other reason is a regression to investigate, not a
   threshold to move.

Step 4 was not needed: the observed worst ratio is 0.302, below the 0.5
ceiling. The ratio is clamped in the harness
(`std::min(parsed, 0.5)`), so no configuration can raise the bar above the
documented ceiling.

### 4.5 The pinned mathematics

The pinned elementary functions were measured against the host libm over
the ranges the render path uses:

| Quantity | Measured maximum deviation from the host libm |
| --- | --- |
| `pinned::sin` vs `std::sin`, double precision | 3.331e-16 |
| `pinned::cos` vs `std::cos`, double precision | 3.331e-16 |
| `pinned::exp` vs `std::exp`, double precision | 2.665e-15 (relative 1.058e-15) |
| `pinned::pow10` vs `std::pow(10.0f, ·)`, float32 result | 6,204 of 1,000,000 sampled gains differ, always by at most 1 float32 ulp |
| `pinned::sin` vs `std::sin`, float32 result | 13,252 of 1,000,000 sampled fade positions differ, always by at most 1 float32 ulp |

The plan's estimate of "one sample per million" for the float32 divergence
is one to two orders of magnitude optimistic; the measured rate is 0.6 % for
the gain conversion and 1.3 % for the fade. The "at most one float32 ulp"
bound holds.

Cost on this host: the pinned `sin` measured 1.83 ns per call against the
host libm's 3.23 ns; the pinned `pow10` measured 11.93 ns per call against
the host libm's 1.85 ns. The gain conversions are control-thread operations,
so the latter does not reach the render path.

### 4.6 The render-hash golden

The pinned render-path math changes the rendered bytes by design. The
scenario hash moves from `0xfce554ccf7c66ab9` to `0x2885ce8fbb52d42d`, and
the golden is now pinned and asserted in
`tests/transport/transport_render_hash_test.cpp`. The rendered bytes are
otherwise unchanged: the transport_render_hash_test still passes unchanged
through F-03, F-04, F-05, F-08, F-09 and F-10.

## 5. Downstream consumer impact

The sprint changes three installed public headers and the rendered bytes.

| Change | Public header | Clip Composer | FreqFinder |
| --- | --- | --- | --- |
| F-06 — `ClipDspProcessor` gains one private float | `include/orpheus/clip_dsp.h` | Rebuild | No change |
| F-12 — `JsonParseLimits` and a parser constructor | `include/orpheus/json.hpp` | Rebuild | Package re-consumption |
| F-13 — `SessionGraph::ClipSpec` and `SessionGraph::add_clips()` | `include/orpheus/session_graph.h` | Rebuild | Package re-consumption |
| F-14 — the transaction representation | `include/orpheus/session_graph.h` | Rebuild | Package re-consumption |
| F-02 — the render path's libm dependence | None | Any artefact that pins a rendered-audio hash must be regenerated | No change |
| F-01, F-03, F-04, F-05, F-07, F-08, F-09, F-10, F-15, F-16, F-17, F-18, F-19, F-20 | None | No change | No change |

`include/orpheus/transport_controller.h`, `include/orpheus/clip_source.h`,
`include/orpheus/routing_matrix.h` and `include/orpheus/realtime_telemetry.h`
are byte-identical to their base-revision content, so F-01, F-03, F-04, F-05,
F-07, F-08, F-09, F-10, F-15, F-16, F-20 need no consumer rebuild.

**No consumer repository is modified by this sprint.** The only change
outside the SDK's own source trees is `tools/realtime_audit.py`, which is the
SDK's own gate (see §6).

## 6. Deviations and deferred items

1. **F-02's scope is extended** beyond the EqualPower fade to the pan law
   and the clip-gain conversions, because ORP273 F-02's own acceptance test
   — the same constant hash across at least two platforms — is unsatisfiable
   while any libm call remains on the rendered path. The seven libm callsites
   replaced are `src/core/transport/transport_controller.cpp:36`, `:44`,
   `:291`, `:2776`, `:2915`, `:3054` and `src/core/transport/clip_dsp.cpp:19`.
2. **F-06 keeps the dB-domain gain law** and lands a bit-exact fast path,
   because ORP273 F-06 excludes a DSP-contract change. The per-frame
   transcendental count drops from two to one **only while the compressor is
   below its threshold**; while it is reducing, both remain. ORP273's claim
   that the change removes one of the two per-frame transcendentals holds
   only in the non-reducing case.
3. **F-20 keeps `include/orpheus/clip_source.h` unchanged** by using the
   sorted `lower_bound` variant of the remedy; the slot-array variant is
   deferred because it would change an installed public header, which
   ORP273 §6.2 does not account for. The consequence of keeping the header
   frozen is that the sorted carry vector is a function-local, so the worker
   thread performs one small heap allocation and free per service round
   (at most 500 rounds per second) where it previously allocated nothing in
   steady state. A persistent member would change the installed public
   class layout, so it is left to the deferred slot-array variant.
4. **F-16 adds one method beyond the specified surface**,
   `RoutingMeterState::resetGroupOutputPublication()`. The specified surface
   has no way to zero the publication sequence, and `initialize()` must
   reset it to 0 — otherwise `GroupOutputMeterSnapshot::render_sequence`
   (= sequence / 2) would shift by one after every `initialize()` and the
   published telemetry bytes would change.
5. **F-16 changes `RoutingMatrix::processStereoMetering`'s peak and RMS
   parameters** from atomic references to float out-parameters, because the
   master peak and RMS atomics now live in `RoutingMeterState`. The
   finite-sanitisation, the values, the store order and the release memory
   order are unchanged; the caller performs the store.
6. **F-16 expresses the end-of-block topology-revision store** as a second
   `publishGroupOutputHeader()` call, because the revision must be published
   after the route-stability check, which is after the publication's header
   is written. The group count and availability are re-stored with identical
   values.
7. **F-15 changes one file outside the SDK's source trees**:
   `tools/realtime_audit.py` hard-codes the scan targets for the three
   registered-source lifetime helpers. The mandated move makes those
   patterns unmatchable, so the three targets were repointed at
   `src/core/transport/clip_source_registry.cpp` with the
   `ClipSourceRegistry::` prefix. The same hard realtime rules and the same
   coverage apply; the gate's result is unchanged.
8. **F-02's pinned fade no longer starts from a bit-exactly zero gain.**
   The pinned `sin` is exact to about 2e-17 near zero, so the EqualPower
   fade's first sample is 1.96e-17 rather than 0 — below the float32 noise
   floor. `tests/transport/clip_controls_test.cpp` now asserts silence with
   a tolerance instead of bit-exactness.
9. **F-18's registered gate runs 500 observations at 48 kHz.** The full
   10,000-observation, four-rate qualification stays available through
   `ORPHEUS_RUN_WORST_CASE_BENCHMARK=1` when the binary is run directly.
   The p99 ratio was **not** lowered: the measured worst ratio is 0.302.
10. **F-10's post-master output-lane true-peak path is untouched.** It runs
    the estimator unconditionally in every metering mode and is outside this
    finding's scope. It remains a follow-up.
11. **F-16's per-entity meter members stay where they are.** The channel and
    group meters live inside `std::vector<ChannelState>` and
    `std::vector<GroupState>`, whose move constructors copy every meter
    field. Extracting them into a parallel array is a larger refactor and
    is not this finding. It remains a follow-up.
12. **F-20's slot-array variant is deferred**, as described in item 3.
13. **The pinned mathematics depends on `std::fma`.** On a target with a
    hardware FMA instruction — arm64, and x86-64 with FMA enabled — the
    pinned `sin` is cheaper than the host libm (measured: 1.83 ns against
    3.23 ns). On a baseline x86-64 target the compiler emits a libm `fma`
    call, so the fade path's cost would be platform-dependent. This was not
    measured on an x86-64 host and is recorded as an evidence limit in §7.
    ORP273's own remedy for F-02 — a control-thread-prepared curve table —
    avoids the dependency and is the natural follow-up if the x86-64
    measurement shows a regression.
14. **One pre-existing test flake was fixed as delivery hygiene.** The first
    release-tree suite run reported `transport_integration_test` as failed
    while other verification commands were running concurrently. The test
    waited a fixed 100 ms for the dummy driver's first audio tick before
    asserting that the transport position had advanced, so a delayed tick
    failed it for a reason unrelated to the transport. The test now uses the
    polling helper the same file already uses for its callback waits. This
    is outside the finding scope of every ORP273 finding; it is recorded
    here because the sprint's delivery depends on the suite being reliable.

## 7. Verification performed

Every command below was run from the repository root on the sprint branch
unless stated otherwise. The Debug tree is `build` (Debug, ASan, UBSan); the
release tree is `build-release-clean` (Release, no sanitizers).

### 7.1 Repository gates

| Command | Result |
| --- | --- |
| `python3 tools/realtime_audit.py --root . --fail-known-debt` | `Realtime audit passed: 0 hard failures, 0 tracked debt findings.` — exit 0 |
| `python3 tools/docs_path_audit.py --root .` | `Docs-path audit passed: 7 documents checked.` — exit 0 |
| `python3 tools/version_contract.py --check` | `SDK version contract is consistent: 0.9.1` — exit 0 |
| `python3 tools/shmui_juce_manifest.py --check` | `ShmUI-JUCE manifest is consistent: 77 files, sha256 4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7` — exit 0 |
| `python3 tools/suite.py validate --json` | `{"errors": [], "manifest": "…/suite/orpheus-suite.json", "schema": "…/suite/schema/orpheus-suite.schema.json", "valid": true}` — exit 0 |
| `python3 tools/realtime_audit.py --root . --fail-known-debt --include-adjacent` | Exit 1 with exactly one known finding, outside this sprint's scope: `DEBT: Clip Composer AudioEngine callback: /Users/chrislyons/dev/clip-composer/Source/Audio/AudioEngine.cpp:3203: juce::AudioBuffer<float> tempBuffer (Clip Composer wraps/analyzes output in callback)` |

### 7.2 Test suites

| Command | Result |
| --- | --- |
| `ctest --test-dir build --output-on-failure` | `100% tests passed, 0 tests failed out of 86` (the deadline-distribution gate is `Skipped` in the sanitized Debug tree, by design) |
| `ctest --test-dir build --output-on-failure -R '^realtime_static_audit$\|^docs_path_audit$\|^multi_clip_stress_test$\|^transport_render_hash_test$\|^performance_monitor_test$\|^clip_dsp_test$\|^fade_processing_test$'` | `100% tests passed, 0 tests failed out of 7` |
| `ctest --test-dir build --output-on-failure -R '^realtime_harness_test$'` | `100% tests passed, 0 tests failed out of 1` |
| `ctest --test-dir build --output-on-failure -R '^cmake_find_package$'` | `1/1 Test #16: cmake_find_package ............... Passed 19.06 sec` — `100% tests passed, 0 tests failed out of 1` |
| `ctest --test-dir build-release-clean --output-on-failure` | `100% tests passed, 0 tests failed out of 86` |
| `ctest --test-dir build-release-clean --output-on-failure -R '^realtime_render_deadline_distribution$'` | `1/1 Test #55: realtime_render_deadline_distribution ... Passed 11.10 sec` — `100% tests passed, 0 tests failed out of 1` |

### 7.3 Behavioural checks

| Check | Result |
| --- | --- |
| The render hash is pinned and identical across block sizes | `[Render Hash] block 256: 0x2885ce8fbb52d42d` at block sizes 256, 512, 1024 and 2048 |
| The published routing meter values are unchanged by F-09 and F-10 | The routing matrix test's meter output was captured before and after each change and `diff` reported no difference beyond the test durations |
| The published telemetry bytes are unchanged by F-05 | The multichannel transport and callback-loss telemetry tests assert the published meter, availability and frame-window values and pass unchanged |
| The transaction rollback performs no allocation | `SessionGraphInvariants.TransactionRollbackRestoresTheGraphWithoutAllocating` wraps the rollback in the repository's runtime allocation guard and asserts `allocViolations() == 0` |
| A CoreAudio route above the callback-frame ceiling is refused | `CoreAudioOutputOnlyInjectedTest.RefusesDeviceAboveCallbackFrameCeiling` asserts `SessionGraphError::NotSupported` with no render buffers allocated and no AudioUnit started; the test fails against the pre-fix code |
| The device callback does not notify a waiter | `CoreAudioRouteMonitorTest.DeviceCallbackTerminalTransitionDoesNotNotify` asserts the notification count is unchanged by `closeAdmissionFromCallback()` and increased by `closeAdmission()` |
| The worker observes a callback-published terminal state | `CoreAudioRouteMonitorTest.WorkerObservesCallbackPublishedTerminalState` observes the terminal state in 106 ms, inside the 100 ms wait bound plus scheduling |
| Every JSON limit has its own distinct error | `SessionJsonLimits.StructuralLimitsAreExplicitErrors` asserts each message and that a document at the limit still parses; `SessionJsonLimits.SessionDocumentByteLimitIsAnExplicitError` asserts the document-size message |
| A bulk clip add is one edit and rejects an overlap without side effects | `SessionGraphInvariants.BulkClipConstructionIsOneEditAndSortsTheTrack`, `…RejectsOverlapsWithoutSideEffects`, `…EmptyBulkClipConstructionIsNotAnEdit` |
| The global headroom mode attenuates by the count across every group | `RoutingMatrixTest.HeadroomModeGlobalAttenuatesAcrossGroups` |
| The compressor's non-reducing gain does not drift | `ClipDspTest.CompressorBelowThresholdAppliesOnlyTheConstantMakeupGain` |

### 7.4 Smoke checks

| Command | Result |
| --- | --- |
| `scripts/check-determinism.sh` | `✓ Determinism check completed`, `SHA-256: 1a602fe4e879ec011bdf2b9ce25e0f2ad7f577d62c2c8ee937dc6bc38319d905` — exit 0 |
| `./build/adapters/minhost/orpheus_minhost render-click --session tools/fixtures/solo_click.json --out /tmp/orp274_probe.wav --range 0:8` | `Rendered click track to "/tmp/orp274_probe.wav"`, 705,644 bytes written — exit 0 |
| `node scripts/validate-performance.js --build-dir=build-release-clean` | `✅ renderBlockTime: realtime_render_deadline_distribution passed` and `✅ All performance budgets met!` — exit 0 |

### 7.5 Hygiene

`git diff --check` reports no whitespace errors; `git status --porcelain` is
empty at the delivery commit; `git log --oneline origin/main..HEAD` lists
the sprint branch's commits: one focused commit per finding, plus the sprint
plan, the delivery record, the fade-assertion follow-up and the
delivery-hygiene fix.

### 7.6 Evidence limits

1. **The measurement host is arm64 macOS.** The release-tree before/after
   numbers, the deadline gate and the pinned-math cost were measured on that
   host only. The x86-64 Linux and Windows legs are not measured here; the
   Windows and Ubuntu Release CI legs are what carry the cross-platform
   golden proof.
2. **The cross-platform render hash is a CI claim, not a local
   observation.** The golden `0x2885ce8fbb52d42d` is asserted locally against
   the same platform. The claim that it is identical on every platform rests
   on the render path no longer calling the host libm, which the pinned
   functions' correctness argument and their measured deviation from the
   host libm support, plus the Windows and Ubuntu Release CI legs.
3. **The pinned mathematics' `std::fma` cost was not measured on x86-64.**
   See §6 item 13.
4. **The routing matrix's channel and group meter members were not
   extracted.** See §6 item 11.
5. **The post-master output-lane true-peak path was not fused.** See §6
   item 10.
6. **The adjacent Clip Composer finding is out of scope.** It is reported by
   the adjacent audit and is this repository's neighbour's own callback.
7. **One pre-existing test flake was observed during the sprint and fixed.**
   See §6 item 14. The fix is a test-timing change only; no product
   behaviour was altered to make it pass.

## 8. References

- `docs/orp/ORP273 Refactoring and Performance Optimization Review.md` — the
  review this sprint remediates
- `docs/orp/ORP272 Suite Development Channel Convergence.md` — the suite
  channel this sprint's branch follows
- `docs/orp/ORP270 Transport Initial Metadata Registration Delivery.md` —
  the delivery-record format this record follows
- `docs/orp/ORP.md` — the record index
- `ARCHITECTURE.md` — the architecture document corrected by F-19
- `docs/REALTIME_AUDIT.md` — the realtime contract the render path upholds
- `docs/SUPPORT_MATRIX.md` — the platform support status F-19 references
