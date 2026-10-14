# Realtime Audio Audit

This checklist is the first hardening gate for Treefall SDK audio code. It is
about realtime safety and architecture contracts, not micro-optimizing DSP.

## Callback Rules

- No locks, sleeps, blocking file/network I/O, hidden logging, heap allocation,
  container growth, or `std::function` ownership changes on realtime callbacks.
- Callback diagnostics must be opt-in and allocation-free. Production callbacks
  should record simple counters only.
- Audio-file open, decode setup, hashing, resampler setup, and cue/trim
  prewarming must happen before playback via non-realtime preparation APIs.
- Stop/shutdown must use explicit callback lifetime coordination. Fixed sleeps
  are not acceptable as a resource-safety boundary.
- Realtime entry points validate pointer arrays, exact configured channel
  counts, and bounded frame counts before draining commands or writing output.
  Invalid shapes are no-touch returns; a valid zero-frame block still advances
  bounded control state.
- Output-shape validation completes before the first write to caller-owned
  buffers, and a valid callback that fails admission or conversion clears
  exactly `frames * sizeof(float)` per configured output lane rather than each
  buffer's advertised byte size.
- Any-thread pending observations are clamped to queue capacity, and cumulative
  sequence/drop diagnostics saturate rather than wrap.
- Registered transport sources remain pinned by unread commands, active voices,
  and pending streaming page primes. Stop, replacement, panic, natural end, and
  teardown release each ownership pin exactly once.
- Borrowed callback targets are admitted with a single strong CAS. Control-side
  replacement closes admission, drains leases, and only then destroys the old
  target; callbacks do not wait, allocate, or notify waiters.
- Callback timing diagnostics are opt-in and default OFF. When enabled, the
  monitor lease remains held through timestamp conversion and publication.
- The control-to-audio command ingress is a fixed 255-node MPSC admission pool.
  Producers perform one bounded ownership scan and at most 32 publication CAS
  attempts; the audio consumer exchanges the whole pending chain and reverses
  it into a fixed scratch list for FIFO processing. Producer-side reuse may
  reclaim retained payloads, but `processCommands()` and its detach/release
  helpers must not allocate, lock, perform I/O, or invoke callbacks.
- Static auditing explicitly targets `processCommands`, `detachPendingCommands`,
  `releaseCommandNode`, and the command-lease/settlement cleanup helpers in
  addition to `processAudio`. This lexical gate is a bounded source audit, not
  a recursive call-graph proof.
- Whole command calls are non-realtime: registry preparation and retained-payload
  reclamation may lock, allocate, or perform I/O on the producer. The atomic
  admission primitives are bounded, not a wall-clock wait-free guarantee.
- Ingress counters saturate at `UINT32_MAX` with widened atomic backing and
  bounded compensation. Valid use allows at most `UINT32_MAX` simultaneous
  command calls per controller. Independent observations need not satisfy the
  admission partition until callers and consumer are quiescent.

## Diagnostic and Transition Contracts

- `PerformanceMetrics::latencyMs` is the estimated callback-buffer duration
  (`bufferSize / sampleRate * 1000`), not measured input, output, or round-trip
  latency. A zero sample rate retains the last estimate. Use the existing
  directional `AudioLatencyBreakdown` and `AudioRouteLatency::complete` for
  measured route terms; incomplete routes remain incomplete.
- Performance metrics, histogram buckets, and realtime diagnostic counters are
  independently sampled, eventually consistent aggregates, not a coherent
  same-callback snapshot. Uptime is calculated at observation time.
- Underrun reset linearizes at its atomic exchange: earlier increments are
  cleared and later increments are retained. Peak reset requests coalesce and
  take effect at the next callback boundary; the sole callback producer resets
  to the prior smoothed CPU value and includes that callback's instantaneous
  CPU. An idle monitor retains the previous peak until a callback arrives.
- `callback_over_budget_count` counts durations strictly above one quarter of
  the callback budget. The compatibility field `callback_p99_over_budget_count`
  counts durations strictly above one half of the budget; it is not a
  percentile. Duration zero means no timing observation. Normal transport
  telemetry supplies no duration, so zero threshold counts are not timing
  health evidence or an audio-onset measurement. Opt-in backend timing feeds
  the performance monitor only.
- Loop-anchor preparation and rollback belong to the control thread. Admission
  transfers exclusive transition ownership to a command; bounded audio-safe
  commit releases the previous pin only when that command executes. Rejected
  commands roll back before transfer, and teardown drains after audio quiescence.
- Transport observers run only when the control thread calls
  `processCallbacks()`. A cache miss emits silence and queues an underrun;
  it must never invoke an observer from the audio render path.

## Control-Side Resilience and Measurement Limits

- Driver-manager device transitions serialize separately from short state
  snapshot/publication locks. Enumeration, factory construction, initialization
  and old-driver stop do not hold the state lock; getters may observe the
  retained route during preparation. Publication replaces driver and metadata
  together, and notifications occur outside both locks. Failed initialization
  or stop retains old pointer/metadata, not a guarantee that it is usable.
  `getActiveDriver()` is borrowed, not a lease; callers must coordinate lifetime.
- CoreAudio start, status getters, and stop are serialized by the driver mutex.
  Input priming has an explicit two-second polling deadline; the one-millisecond
  sleep is only cadence. That deadline does not bound backend calls, mutex
  waits, OS scheduling, or end-to-end start/stop. Physical route qualification
  remains separate from injected delayed-input and timeout measurements.
- A synchronous reader can block the media worker and destructor join.
  Command-demand deadlines do not bound an arbitrary virtual/OS read or
  cancellation acknowledgement. Deduplication and bounded service opportunities
  cannot establish hard blocked-reader fairness or teardown qualification.
- `RealtimeHarnessTest.WorstCaseTopologyDeadlineDistribution` is opt-in,
  unsanitized Release evidence only. Set
  `ORPHEUS_RUN_WORST_CASE_BENCHMARK=1`; its normal skip is not qualification.
  It preserves all 10,000 observations per synthetic rate/mix after 100 warmups,
  reports wall and available thread-CPU distributions, and checks actual
  frame/rate deadlines. Synthetic rates are not negotiated hardware evidence.
  Allocation guards do not prove zero I/O; `/proc/self/io` sampling includes
  observer reads, and callback-only Linux syscall attribution remains separate.

### Local resilience measurements

Host: Cloudkicker, Apple M2 Pro / Mac14,9, arm64, Darwin 25.5.0.
Compiler: Apple LLVM 21.0.0 (`clang-2100.0.123.102`). Base revision:
`e34c81b02` plus the resilience working changes. Debug checks passed five
selected CTest gates (37.12 s) and all four synthetic CoreAudio prime fixtures;
no physical playback was exercised.

- Actual service replay at 32 sources performed 160 reads. Full-window service
  had prefix-count skew 5 and last first-opportunity index 155; round-robin
  service had skew 1 and index 31. This is a controlled reader replay, not a
  cancellation or wall-clock fairness bound.
- CoreAudio delayed-input start maximum: 30.4661 ms over 24 samples; controlled
  timeout start maximum: 31.4467 ms over 24 samples. The real two-second prime
  poll sample took 2000.43 ms; concurrent route/latency/stop calls waited
  2000.44 / 2000.48 / 2000.56 ms. These synthetic measurements demonstrate
  serialization, not a hard end-to-end bound.

First unsanitized Release topology run, **before gain precomputation**:
`env ORPHEUS_RUN_WORST_CASE_BENCHMARK=1
build-release/tests/transport/realtime_harness_test
--gtest_filter=RealtimeHarnessTest.WorstCaseTopologyDeadlineDistribution`.
Result: failed, 961.095 s. Each mix/rate has 100 warmups and 10,000 measured
2048-frame blocks; rates are synthetic. Transport uses 32 MonoStrict voices,
8 source/output lanes and 255 admitted commands per block. Routing uses
256 channels / 32 groups / 32 outputs. All guarded allocation/deallocation
counts were zero; I/O attribution was unavailable. Units below are microseconds.

| Rate (Hz) | Mix | Wall p50 | Wall p95 | Wall p99 | Wall max | CPU p99 | CPU max | Deadline |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 44100 | Transport | 3943.04 | 4365.38 | 4484.88 | 5374.21 | 4415 | 4662 | 46439.9 |
| 44100 | Peak | 5640.21 | 6192.88 | 6315.75 | 7261.38 | 6280 | 6550 | 46439.9 |
| 44100 | TruePeak | 13920.4 | 14666 | 15635.8 | 26822.1 | 14610 | 16405 | 46439.9 |
| 48000 | Transport | 3904.71 | 4293.96 | 4367.71 | 5075.29 | 4362 | 4557 | 42666.7 |
| 48000 | Peak | 5726.79 | 6285.88 | 6378.83 | 6890.67 | 6371 | 6892 | 42666.7 |
| 48000 | TruePeak | 13180.1 | 14508.8 | 15368.7 | 21533.5 | 14551 | 16092 | 42666.7 |
| 96000 | Transport | 4014.67 | 5432.17 | 12583 | 96031.8 | 5142 | 8340 | 21333.3 |
| 96000 | Peak | 5633.08 | 6510.96 | 8203.92 | 24356.5 | 6443 | 7582 | 21333.3 |
| 96000 | TruePeak | 14020 | 15055.6 | 18456.2 | 122879 | 15013 | 23044 | 21333.3 |
| 192000 | Transport | 4205.83 | 4566.83 | 4776.42 | 22783.3 | 4434 | 5324 | 10666.7 |
| 192000 | Peak | 5596.67 | 6432.75 | 6770.75 | 14397.7 | 6313 | 7105 | 10666.7 |
| 192000 | TruePeak | 13010.1 | 14618.9 | 15437.5 | 26655.6 | 14522 | 15790 | 10666.7 |

The initial run retained distributions and maxima, not per-block miss indices.
Its complete emitted output is retained locally as
`build-release/worst-case-before-gain.log`. Subsequent runs emit each missed
block outside the timed region; a later result cannot erase these misses.
Maximum-topology timing remains unqualified. Moving exact gain conversion to
control publication cannot by itself resolve routing or scheduler misses.

Second run, after exact gain precomputation on control publication, used the
same command and `-O3 -DNDEBUG` Release flags; result failed (932.120 s).
Gain-command rendered transitions match the existing metadata-gain path
sample-for-sample. Allocation/deallocation guards again reported zero; I/O
attribution remained unavailable. All missed blocks are retained in
`build-release/worst-case-after-gain.log`.

| Rate (Hz) | Mix | Wall p50 | Wall p95 | Wall p99 | Wall max | CPU p99 | CPU max | Misses / 10000 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 44100 | Transport | 3910 | 4275.29 | 4337.08 | 5139.46 | 4331 | 4477 | 0 |
| 44100 | Peak | 5647.33 | 6123.88 | 6213.92 | 12422.4 | 6198 | 7008 | 0 |
| 44100 | TruePeak | 13258.5 | 14297.1 | 14454 | 23011 | 14404 | 15491 | 0 |
| 48000 | Transport | 3892.29 | 4235.96 | 4302.5 | 4949.38 | 4298 | 4581 | 0 |
| 48000 | Peak | 5663.25 | 6144.92 | 6622.71 | 20440.1 | 6346 | 7104 | 0 |
| 48000 | TruePeak | 13124 | 14169.4 | 14368.2 | 33419 | 14322 | 16100 | 0 |
| 96000 | Transport | 3879.46 | 4158.92 | 4274.33 | 7015.79 | 4269 | 4785 | 0 |
| 96000 | Peak | 5676.71 | 6125.17 | 6255.62 | 10999.8 | 6240 | 6984 | 0 |
| 96000 | TruePeak | 13190 | 14197.9 | 14385.6 | 29925.7 | 14364 | 15633 | 3 |
| 192000 | Transport | 3880.5 | 4214.12 | 4305.29 | 17708.2 | 4296 | 5509 | 1 |
| 192000 | Peak | 5696.38 | 6034.38 | 6216.46 | 18408.2 | 6204 | 8133 | 2 |
| 192000 | TruePeak | 13120 | 13558.5 | 14304 | 24728.8 | 14276 | 15124 | 10000 |

The maximum-topology criterion remains open: TruePeak at 192 kHz exceeds the
deadline even in thread CPU, and other mixes retain wall-max misses. No outlier
was removed and no support tier or release qualification follows from this run.

## Host, Package and Input-Boundary Evidence

Local Debug verification at `e737bbddb` plus the host/package/validation working
changes passed 14 selected CTest gates (34.11 s): aggregate native, identity/time,
scene management/routing, ingress, voice policy, callback loss, both package
consumers, ABI link, and the four targeted compile negatives.
`cmake_find_package` exercised legacy, Treefall, legacy-first and Treefall-first
discovery. Both manifests cover public link capabilities; internal ABI/support
exports and the explicitly checked minhost/performance-tool executables are not
advertised link capabilities. ASIO stays excluded.

- Installed session I/O exercised parse, serialize/save, primary load, backup
  recovery, future-primary hard rejection and unchanged persisted documents.
  Parsing retains the existing clip-grid normalization; it is not a byte-copy
  operation on an uncommitted graph.
- Compiler rejection requires an actual source-located error naming the retired
  symbol, missing private header, or invalid constant expression. Source echoes,
  notes, configure errors and generic link/build failures do not qualify.
- Actual module-resolved ABI callbacks confirmed per-DSO logger/telemetry
  isolation and same-module Orpheus/Treefall alias sharing on this host. Static
  builds retain final-link-unit scope; independent platform execution remains
  required, not inferred from this macOS run.
- Numeric/duplicate-key/reference tests reject invalid values before graph,
  revision, scene timeline, routing or playback mutation. Signed sample
  conversion validates the rounded value in `[-2^63, 2^63)`; fractional integer
  settings and dangling assignment/scene members are refused.
- Borrowed graph and committed clip pointers are invalidated by removal or
  rebuild/restore/rollback. Re-resolve stable IDs; scene definitions and runtime
  scene state remain outside transaction snapshots. Borrowed driver pointers
  are not lifetime leases or evidence of usability after failed stop.
- Same-file A/B renders verified independent polyphony, tags, voice identities,
  policy change, stop/refire fades and A unregister while B stays live, including
  per-sample output attribution.

The actual native demo ran `--self-smoke tools/fixtures/two_tracks.json
build/t4-demo-stems tests/fixtures/reconform/insert.json` without opening an audio
device; it exercised the shared UI/controller paths and exited 0 without JUCE
assertions after explicit UTF-8 conversion. Native window observation succeeded,
but exact-window pixel capture failed and `CGPreflightScreenCaptureAccess()`
returned false. Pixel review is unavailable; the temporary capture hold/helper
source was removed. Minhost rendered the same stems through C ABI 1.0:

| Stem | Canonical C++ / ABI SHA-256 |
|---|---|
| `two_tracks_bass_48k_24b.wav` | `109e7e7e0a0dd8eef6daee6836d57e714c86ef556c4faa11923b7546ac858d5f` |
| `two_tracks_drums_48k_24b.wav` | `34ae09c68965b59b65ee64b216cf644ecd8b09ebc2b280967fadbcd153a68f61` |

The real click command used `{"tempo_bpm":100,"bars":2}` and produced stereo
Int16, 44100 Hz, 211680 frames / 4.8 s. An out-of-range bar override exited 1
with `spec.parse` / `bars out of range` and preserved the existing output hash.
REAPER application/vendor-SDK acceptance remains unavailable.

Retained earlier result: during a concurrent JUCE build, the aggregate gate
failed `OscillatorTest.ProcessesEfficiently` at 388663.43 samples/s against its
unchanged 500000 samples/s requirement. The subsequent quiet aggregate passed;
this does not erase the observed failure or establish its scheduling cause.
Windows/GCC/MSVC diagnostic portability, Linux TSan, physical routes and release
temporal-coherence acceptance are still unqualified.

## Integrated Local Checks and External Gates

Base revision `15e26480d` plus qualification working changes; same attributed
Cloudkicker/M2 Pro/Apple LLVM host above. Native Debug build completed.

| Evidence | Observed result |
|---|---|
| Nonhardware native/package/audit suite | 83 CTest entries passed, 200.76 s |
| Local Debug ASan + UBSan | 15 selected entries passed, 132.34 s; four synthetic CoreAudio prime cases also passed |
| Production portable WASAPI format algorithm | Strict C++20 ASan/UBSan smoke passed PCM16/float32, extensible, mask/count, width/alignment/valid-bits, overflow and zero-boundary cases |
| Enabled optional ShmUI export discovery | Four namespace modes passed with OpenGL OFF, 212.34 s; optional flag restored OFF afterward |
| Linux TSan configuration | YAML parsed; nine configured targets confirmed by actual CTest inventory; execution unavailable |

The nonhardware command was:
`env GTEST_FILTER=-DeviceHotSwapTest.HotSwap_DummyToCoreAudio:DeviceHotSwapTest.HotSwap_CoreAudioToDummy
ctest --test-dir build --output-on-failure -E '^coreaudio_driver_test$'`.
Thus this is not a full physical-device suite pass. The opt-in Release
worst-case measurement still skips by default in Debug; that skip does not
qualify timing. macOS `/proc/self/io` is unavailable, so no callback-only syscall
proof is inferred. Logs remain under `build/t5-integrated-nonhardware-suite.log`
and `build/t5-native-sanitizer-evidence.log`; the portable smoke source was
removed after execution.

### Review split and hosted execution evidence

The four WASAPI format/runtime/test files are excluded from the portable
hardening delta and retained on `fix/pr265-platform-qualification`. The existing
WASAPI support tier is unchanged. The nine-target Linux TSan selection stays
with the portable realtime-concurrency changes.

Hosted run `38326895539` exercised merge revision
`96d5cfa307776eb2c9a13d67194700a6d490b5ee`, whose parents are base `d2d61b28c`
and reviewed head `b3bc1e3d7`. Downloaded artifacts show nine TSan targets passed
in 262.61 s with `halt_on_error=1:exitcode=66` and `-j 1`. The separate syscall
artifact records `execution=0`, `parser=0` for
`ConcurrentIngressAt96k64DoesNoIo` and its marked consumer window. This is
selected-consumer evidence, not every callback/mix or worst-case I/O proof [1].

Windows Debug and Release each passed all 24 expanded WASAPI cases, including
native descriptor validation and fake output/failure paths. Both configurations
failed the scene fixture because it deleted the WAV while the transport's
reader still owned it. Cleanup now destroys the scene manager and transport
before removing media, including assertion-failure teardown. Neither that old
run nor local macOS verification establishes green required checks for the
amended portable or counterpart revisions [1].

Review corrections also exercise true Treefall-first header inclusion and
reject a zero graph-render sample rate before synthesis. CPU percentiles are
omitted on unsupported platforms and when any observation is invalid. A reduced
real-render smoke used two warmups/four observations per mix: 12 CPU reports
with the sampler, zero with the unsupported-sampler branch. Both smoke runs
retained deadline failures; they are reporting proof, not timing qualification.
A throwaway getter fault injection compiled the actual graph renderer, observed
the zero-rate rejection and no output creation; normal public graph setters
already reject zero. The ten relevant local CTest gates passed in 52.01 s,
including installed package, ABI, scene, renderer and realtime coverage.

The partial-command failure/cancellation fixture now advances at most two
worker rounds to its first actual read, verifies page 1 remains unpublished,
then fails the next read at page 2 or cancels without another read. The
production request starts at `firstMissing`; no resident-page round is assumed.
Read-offset traces and final guards verify rollback releases new ownership,
retains the original page-0 anchor, and permits a successful retry. Production
scheduling is unchanged. The focused native fixture passed (11 ms), the full
streaming and realtime-audit CTest gates passed (10.30 s), and all four worker
service fixtures passed with ASan/UBSan. These local results do not certify
current-head hosted CI or authorize merging either PR.

[1] GitHub Actions, "Treefall SDK CI Pipeline," run 38326895539, Oct. 13, 2026.
[Online]. Available:
https://github.com/boot-industries/treefall-sdk/actions/runs/38326895539.

Physical explicit-UID, Bluetooth, aggregate teardown, unplug/replug and
rate/buffer-change acceptance still needs named device/operator access and
playback authorization. Child acceptance, meter-source reconciliation,
same-callback native/app attribution and temporal coherence remain separate
gates. Hard blocked-reader cancellation and failed maximum-topology deadlines
also stay open. ASIO stays source-only; there is no release tag or support-tier
promotion from these local checks.

### Platform counterpart merge gates

The counterpart restores exactly the four WASAPI source/test files from
`b3bc1e3d7` atop the corrected portable branch. Require current-revision Windows
Debug/Release native descriptor/fake, installed package and ABI checks before
merging; historical 24-case results are not a new-revision pass. Retain the
nine-target Linux TSan and marked selected-consumer syscall gates on the stack;
the previous artifacts qualify only their recorded merge revision. Full
maximum-topology deadline/I/O qualification remains open after the observed
misses. Real-device `wasapi-hardware.json`, physical route/operator access and
temporal-coherence acceptance remain separate release/support-promotion gates.


## Boundary and Lifetime Gates

- `transport_controller_test`, `routing_matrix_test`, and `audio_input_test`
  cover malformed no-touch boundaries, bounded observations, saturating
  counters, and checked input-capacity arithmetic.
- `realtime_borrowed_target_test` covers admission, replacement, and lease
  draining without a retry loop.
- The WASAPI fake runtime covers terminal worker outcomes, rollback, stop/join,
  exact output-channel negotiation, and explicit reinitialization.


## Current Gates

- `ctest -R realtime_static_audit` runs `tools/realtime_audit.py
  --fail-known-debt` — the STRICT in-repo gate. Since the ORP134 G1
  streaming-reader migration, the transport render path performs no file
  reads/seeks, so any reintroduced audio-thread read is a hard CI failure.
- The runtime side is enforced by `realtime_harness_test` (ORP136 §2.2):
  allocation hooks + `/proc/self/io` sampling prove zero allocations and zero
  media I/O across multi-clip `processAudio()` stress runs, and that streaming
  cache misses emit silence + `BufferUnderrun` instead of blocking.
- `tools/realtime_audit.py --fail-known-debt --include-adjacent` is the
  cross-repo strict gate. It still reports the ADJACENT-repo debt below and
  will pass once the app-side sprints land in those repos.

## Resolved Architecture Debt

- **The transport render path no longer calls file readers (ORP134 G1).**
  `prepareClipAudio()`/`startClip()` build an immutable realtime source on the
  control thread — whole-file PCM for short clips, a worker-fed page ring for
  long files — and `processAudio()` memcpy-reads it position-explicitly. Seek
  became a prefetch hint (no `sf_seek` anywhere near the callback); a
  streaming cache miss renders silence and reports `BufferUnderrun`, never
  blocking. Parity with the old reading path is proven bit-exact by the
  golden render-hash gate.

## Known Architecture Debt

- The routing matrix now carries explicit `SourceChannelPolicy` and
  `DownmixPolicy`, but transport clip rendering still uses stereo pair buffers.
- Clip Composer still performs app-level analyzer work in its audio callback in
  the adjacent repo. The SDK-side target is decimated telemetry copied out of
  callback buffers and consumed by UI/message-thread code. **→ OCC sprint.**
- FourTrack's engine still calls `readSamples` in its own callback in the
  adjacent repo; it should adopt the SDK's prepared/streaming sources.
  **→ FourTrack sprint.**
- See `docs/APP_REALTIME_DEBT_REMEDIATION.md` for Clip Composer and FourTrack
  patch guidance.

## Platform Matrix

- macOS: CoreAudio is the only shipped production device backend and must
  remain the strictest callback-safety gate.
- Windows: WASAPI remains source/fake-test capable but unpromoted until package,
  ABI, and physical-device evidence exists. ASIO is optional source-only
  integration requiring an external SDK and is excluded from install/export.
- Linux: Dummy is the only advertised driver. ALSA, JACK, and PipeWire remain
  distinct future providers rather than one generic Linux backend.
- iOS: RemoteIO remains a future provider and should share the same callback and
  preparation contracts, with platform-specific capability limits exposed
  through `AudioDriverCapabilities`.
