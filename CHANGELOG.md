# Changelog

All notable changes to the Orpheus SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Changed

- Synchronized `packages/shmui-juce` to ShmUI `0375ebf47329786924c61359c2aa4d9372456985`
  and token contract 0.5.0. The package now exposes six orthogonal Console
  mode/flavor material, illumination, display, physical-control, meter, and
  waveform recipes.
- Added message-thread default-theme notifications with safe listener lifetime,
  registration-order delivery, deferred additions, removal-safe iteration, and
  coalesced reentrant updates. Existing dark Console helpers retain their behavior;
  `ShmuiTheme::forProfile` selects explicit mode and flavor.

## [0.6.7] - 2026-07-24

Patch release correcting capture-channel selection for distinct CoreAudio input
and output endpoints.

### Fixed

- CoreAudio now maps AUHAL capture channels to the explicitly resolved input
  sub-device when a private aggregate also contains input lanes from the output
  endpoint.
- Initialization rejects requested capture channel counts that exceed the
  resolved input endpoint instead of surfacing an opaque AudioUnit setup error.

### Packaging

- Preserved the governed ShmUI-JUCE 0.3.0 token package and stable text-button
  identity contracts from SDK 0.6.6 in the mainline release history.

## [0.6.6] - 2026-07-22

Patch release preserving ShmUI text-button component identity.

### Fixed

- `shmui::TextButton` now initializes its JUCE component name from the
  constructor label, retaining stable action lookup and accessibility identity
  when the visible label is changed later.

## [0.6.5] - 2026-07-22

Patch release aligning the governed ShmUI-JUCE import provenance contract.

### Changed

- Updated the recorded ShmUI source revision after repairing the authoritative
  import generator to match the SDK's canonical path-NUL-content hash and JSON
  serialization contract.

## [0.6.4] - 2026-07-22

Patch release refreshing the governed ShmUI-JUCE package import.

### Changed

- Synchronized the flattened `packages/shmui-juce` source tree to ShmUI token
  contract 0.3.0, including Console themes, transport/text controls, and meter
  presentation APIs.
- Removed the stale nested package source copy so CMake compiles the governed
  imported files recorded by `shmui-juce-import.json`.

## [0.6.3] - 2026-07-21

Patch release adding prepared and offline tape-varispeed processing.

### Added

- `PreparedTapeVarispeed` provides positive-rate `0.5x...2.0x` interleaved
  tape processing with linked channel phase, fixed Kaiser-windowed sinc kernels,
  explicit capacity outcomes, and no allocation or locks after `prepare()`.
- `OfflineTapeVarispeed` provides the matching fully prepared deterministic
  offline path for export and render workflows.


## [0.6.2] - 2026-07-18

Patch release adding bounded per-clip DSP, one-shot trigger voices, and
physical-output meter telemetry.

### Added

- `ITriggerVoice`, `TriggerVoice`, and `VoicePolicy` provide an independently
  owned one-shot sample, retrigger-cut or bounded oldest-first polyphony,
  sample-accurate in-buffer event offsets, and linear pitch-ratio playback.
- `TriggerVoice` queues up to 64 trigger events per render call and performs no
  allocation, deallocation, locking, or I/O in `trigger()` or `render()`.
- `AudioIoTelemetry` and `IAudioDriver::getTelemetry()` expose host-neutral,
  factory-visible capture-render failure diagnostics.
- `ClipDspProgram` and `ClipDspProcessor` provide a validated, fixed-capacity
  gate → four-band EQ → compressor → stereo-width → limiter chain.
- Transport clip metadata carries the DSP program into each durable voice;
  default programs remain transparent and live updates replace prepared state
  at one audio-thread command boundary.

### Changed

- `AudioDriverConfig::device_id` is replaced by direction-specific
  `input_device_id` and `output_device_id` fields without a compatibility alias.
- CoreAudio resolves persistent DeviceUIDs independently, validates endpoint
  direction, and owns private aggregate creation and destruction for distinct
  duplex endpoints.
- CoreAudio capture-render failure accounting saturates at `UINT64_MAX` and
  resets only after successful initialization.
- Routing channel meters report isolated post-gain/pan contributions while
  group and master meters retain accumulated-stage readings.
- Realtime telemetry schema 2 publishes a fixed-capacity meter for every
  configured physical output lane.

### Verification

- `trigger_voice_test` covers owned interleaved PCM, offsets, retriggering,
  oldest-first stealing, pitch interpolation, a 480,000-frame sample, and
  guarded realtime allocation behavior.
- The installed `Orpheus::audio_utils` consumer compiles and runs the public
  trigger voice contract.
- CoreAudio endpoint lifecycle coverage exercised explicit distinct endpoints,
  same-device duplex, each directional default, unknown and incompatible UIDs,
  aggregate reuse/destruction, and successful capture with zero failures.
- Clip DSP unit, live transport-update, fixed-state, and guarded realtime
  allocation contracts pass.
- Routing output-meter tests pass with isolated per-lane assertions.
- All 152 configured SDK tests pass, including the installed exact-version
  package and runtime consumers documented in ORP157.

## [0.6.1] - 2026-07-16

Patch release exposing durable per-voice callback identity.

### Changed

- `ITransportCallback::onClipStarted()`, `onClipStopped()`, and
  `onClipLooped()` now include the nonzero SDK voice identity carried by the
  realtime event ring. Every retired voice publishes its own stop event;
  handle-level consumers reconcile presentation state through `getClipState()`.

## [0.6.0] - 2026-07-16

Minor pre-1.0 release adding a coherent public routing-control contract.
Custom `IRoutingMatrix` implementations must implement the two new snapshot
methods.

### Added

- `RoutingControlSnapshot` and `RoutingGroupControlState` expose fixed-capacity,
  schema-versioned, allocation-free configured gain, mute, solo, output route,
  effective mute, and monotonic revision state.
- `IRoutingMatrix::getRoutingControlSnapshot()` returns one coherent public
  control value without allocating or capturing a scene.
- `IRoutingMatrix::applyGroupControlSnapshot()` validates every configured group
  before mutation, rejects without changing state or revision, and publishes
  accepted batches to rendering at one buffer boundary.

### Changed

- Group gain targets now enter the smoother at the render boundary rather than
  during a concurrent control-thread write.
- Routing preset load and reset apply group controls through the validated batch
  contract.

### Verification

- All 150 configured SDK tests pass.
- The installed `Orpheus::routing` and transitively linked
  `Orpheus::transport` package consumers compile and exercise snapshot
  write/readback and rejection rollback through public headers.
- All routing-matrix tests pass under ThreadSanitizer, including concurrent
  render, transaction publication, and coherent query coverage.

## [0.5.3] - 2026-07-16

Patch release for host-correct route-latency placement and isolated per-channel
routing meters. Public C and C++ interfaces remain source-compatible.

### Fixed

- CoreAudio round-trip latency now queries the active physical input/output
  routes live, including device and stream latency, safety offsets, actual I/O
  buffer depth, and AudioUnit processing latency. Private aggregate endpoints no
  longer hide high-latency consumer outputs such as Bluetooth headsets.
- `IRoutingMatrix::getChannelMeter(i)` now reports channel \(i\)'s isolated
  effective contribution after gain, pan, mute, and channel-solo logic instead
  of the shared group accumulator. Null, unrouted, and effectively muted
  channels publish silence in the current processing call.
- Channel-meter stereo reduction is defined as maximum lane peak and RMS over
  the mean power of both lanes. Group and master meters retain their summed-stage
  behavior.

### Verification

- All 149 configured SDK tests pass.
- All 39 routing-matrix tests pass, including source-order, distinct gain/pan,
  mute, solo, null-input, and group/master stage regressions.
- All changed C++ ranges pass `clang-format --dry-run --Werror`. The
  repository-wide formatting audit still reports pre-existing drift in
  unrelated files.

## [0.5.2] - 2026-07-16

Patch release for reliable simultaneous CoreAudio capture and playback when the
system defaults resolve to different HAL devices. The public C and C++ audio
driver contracts remain source-compatible.

### Fixed

- The CoreAudio driver now resolves default input and output independently and
  creates a private aggregate endpoint when AUHAL must address both devices.
- Aggregate sub-devices sharing one hardware clock avoid redundant drift
  compensation; cross-clock devices retain drift correction.
- Capture render failures remain observable through the driver's cumulative
  diagnostic counter instead of being hidden behind a zero-filled input buffer.
- Aggregate-device setup avoids redundant clock-domain property reads.

### Verification

- All 149 configured SDK tests pass.
- `coreaudio_driver_test`, `driver_manager_test`, and `device_hot_swap_test`
  pass on macOS.
- The live cross-device capture regression observes callbacks with zero
  `AudioUnitRender` failures on separate default input and output devices.
- All files changed since `v0.5.1` pass `clang-format --dry-run --Werror` and
  `git diff --check`. The repository-wide formatting audit still reports
  pre-existing drift in unrelated files.

## [0.5.1] - 2026-07-15

Patch release for failure-atomic playout control and observable transport
callback loss. C++ consumers must recompile for the appended virtual query
methods; the stable C ABI remains at 1.0.

### Added

- `ITransportController::startClipWithGroupChoke()` admits a firing voice and
  same-`routingGroup` peer choke as one bounded command. Peer fades begin only
  after the firing voice is admitted.
- Cumulative callback-delivery telemetry exposes attempted, retained, and
  dropped event sequences without resetting on callback drains.
- Coherent fixed-capacity active-voice snapshots let hosts reconcile surviving
  per-handle state after a callback-ring gap.

### Fixed

- Metadata updates now enter the transport command ring before persistent
  metadata commits, so queue saturation cannot split active and registered
  routing state.
- Voice IDs skip zero and active collisions when the 32-bit allocator wraps;
  newest-voice aggregation no longer depends on zero as a sentinel.
- Snapshot publication precedes callback-loss visibility and carries an exact
  snapshot watermark, preventing hosts from accepting stale reconciliation
  state.

### Verification

- All 149 configured SDK tests pass, including atomic choke saturation,
  callback overflow/reconciliation, multi-voice aggregation, real-time static
  and runtime harnesses, installed `find_package` consumers, C ABI linkage,
  package version policy, and documentation audits.

## [0.5.0] - 2026-07-14

Release of the explicit multichannel source, logical group-bus, and physical
output-routing contracts required by broadcast playout hosts. The pre-1.0 minor
version advances because public transport/clip metadata structures gained
source-channel fields and interface implementers must provide the group-bus
route methods.

### Added

- `TransportConfig` now declares output width, maximum source lanes, block
  capacity, voice capacity, and the source-channel policy used by the immutable
  renderer.
- `ClipMetadata` publishes logical routing group, explicit `ChannelLayout`, and
  a fixed eight-speaker patch. The transport validates the declared layout
  against the decoded file before atomically committing metadata.
- `ITransportController::{setGroupOutputBus(), getGroupOutputBus()}` routes a
  logical group to one contiguous physical output bus without rebuilding the
  transport.
- CoreAudio, WASAPI, and dummy drivers publish and render their configured
  multichannel output width. Scene snapshots preserve group/output routing and
  source-channel metadata.

### Changed

- Transport voices render up to eight discrete planar source lanes into the
  routing matrix instead of forcing every decoded file through a stereo pair.
  Named group buses can share, isolate, or span physical outputs without a
  hidden fold-down.
- Installed-package consumers and examples construct the public
  `TransportConfig` explicitly and exercise the same transport render contract
  as source-tree hosts.

### Fixed

- Channel-solo and group-solo are independent domains. Soloing a group no
  longer mutes every source channel before group summing, and soloing a channel
  no longer mutes every logical group after summing.
- A stopping loop no longer wraps from its OUT point back to IN. Stop fades now
  complete across the loop boundary, so host-level group chokes and Stop All
  cannot leave a looping voice active indefinitely.

### Verification

- The complete 147-test SDK suite passes, including eight-output
  channel-order/render-hash, group-route, scene, device-swap, driver, and
  installed-package consumer coverage.
- Real multichannel CoreAudio hardware remains a downstream release gate; this
  release does not infer hardware validation from dummy-driver buffers.

## [0.4.0] - 2026-07-14

Release of the public transport-rendering contract required by host applications
that must not depend on SDK-private transport headers. The minor version
acknowledges the source migration required for interface implementers.

### Added

- `TransportRenderConfig` and
  `ITransportController::{getRenderConfig(), processAudio(), processCallbacks()}`
  publish the renderer configuration, audio-thread render entry point, and
  control-thread event/tempo pump through `<orpheus/transport_controller.h>`.
- `getRenderConfig()` reports the constructor sample rate, the routing matrix's
  current output count, and the concrete renderer's hard maximum block size.

### Changed

- Factory callers remain source-compatible after recompilation, but custom
  `ITransportController` subclasses and test doubles must implement
  `getRenderConfig() const noexcept`, `processAudio(float**, size_t, size_t)
  noexcept`, and `processCallbacks()`. This implementer source break is why the
  pre-1.0 minor version advances to `0.4.0`.
- The new virtual methods are appended after every pre-`0.4.0` interface slot,
  preserving the existing vtable entry order for factory consumers.
- The transport's routing topology remains fixed after construction in this
  release. Hosts must not call `getRoutingMatrix()->initialize(...)` on a live
  transport; doing so would discard the transport's stereo channel layout.
- Installed package version matching uses `SameMinorVersion` while the SDK is
  pre-1.0, so `0.4.x` is not selected for a source-incompatible `0.3.x`
  request. Stable major releases use `SameMajorVersion`.

### Fixed

- The realtime allocation-guard self-test now calls the replaceable allocation
  functions explicitly, so optimized Release builds cannot elide its deliberate
  violation and produce a false detector failure.

### Integration contract

- `processAudio()` is non-reentrant, is called by exactly one audio thread, and
  is the sole consumer of the control-to-audio SPSC command ring. Its realtime
  path permits no allocation, locks, blocking, I/O, or host callback dispatch.
- `processCallbacks()` is called by exactly one control/message thread and is
  the sole consumer of the audio-to-control event ring. It must run even without
  a callback object because it also republishes session tempo.
- Source-tree tests exercise the complete runtime contract through
  `ITransportController`, and an installed-package CTest configures, builds, and
  runs a public-header-only consumer linked only to `Orpheus::transport`.
- Clip Composer adoption remains downstream work: it must own the factory result
  as `std::unique_ptr<orpheus::ITransportController>`, render and pump callbacks
  through that interface, and remove its SDK-private transport header include
  and concrete-controller downcast. This release preparation does not claim that
  application migration has occurred.

## [0.3.3] - 2026-07-14

Release for downstream ORP141 adoption. This is the first tagged package that
combines generated release/version metadata, stable installed CMake target
aliases, media SHA-256 references, routing-snapshot provenance, scene-recall
transport stop semantics, fixed-capacity active-voice diagnostics, CoreAudio
performance-monitor attachment, and the merged realtime reliability fixes.

### Added

- `ITransportController::getTotalActiveVoiceCount()` and
  `ITransportCallback::onActiveClipLimitReached()` expose bounded voice-pool
  admission truth without allocations or locks on the audio thread.
- `MediaFingerprint`, `sha256File()`, and `resolveMediaReference()` provide one
  verified media-identity and relative-path security contract.
- Generated `orpheus/version.h`, package inventory JSON, stable
  `Orpheus::*` installed-target aliases, and release-artifact metadata.
- A self-contained `createSceneManager()` factory lets installed-package
  consumers exercise scene capture/recall without private `SessionGraph`
  headers.

### Changed

- Scene recall stops attached transport before applying captured routing/session
  state, and routing snapshots retain optional control-time/audio-position
  provenance.
- CoreAudio reports total active voices and callback timing through an attached
  performance monitor.
- Realtime command/callback, transport snapshot, and routing reliability
  contracts now fail explicitly instead of publishing partial state.

## [0.3.1] - 2026-07-10

Release tag cut for Clip Composer's submodule bump (OCC155 Ask #1). Rolls up all
work landed on `main` since `v0.3.0` (ORP127 voice/SRC, ORP132–136 hardening,
ORP133/134 realtime safety, FTR live-tempo/routing fixes) and adds the transport
API OCC requested below.

### Added - OCC155 Transport API Requests (2026-07-10)

- **`ITransportController::getRoutingMatrix()` (OCC155 Ask #3).** Public accessor
  returning the transport's `IRoutingMatrix` (group gains/mutes/solos/meters).
  Hosts that expose group faders/meters no longer need to reach into the internal
  `transport_controller.h` with `#define private public`. The pointer is owned by
  the transport and stable for its lifetime.
- **`ITransportController::panic()` (OCC155 Ask #5).** Immediate hard-cut: drops
  every active voice with no fade and silences output on the next audio block,
  unlike `stopAllClips()` which rides each voice's fade-out envelope. Intended for
  emergency "immediate mute" controls. Control-thread callable (SPSC command
  queue), RT-safe on the audio side.
- **`ITransportController::unregisterClipAudio(handle)` (OCC155 Ask #4).** Inverse
  of `registerClipAudio()`: frees the clip's reader and prepared/streaming source.
  No-op returning `NotReady` while any voice for the handle is still active
  (caller must `stopClip`/`panic` first); idempotent `OK` on an unregistered
  handle; `InvalidHandle` for handle 0. Prevents unbounded reader retention across
  a long broadcast day of clear/reload cycles.

  > **Implementer note:** these are new pure-virtuals on `ITransportController`.
  > For consumers that use the interface through `createTransportController()`
  > (the supported path — OCC included) this is source-additive: recompile only.
  > Any code that *subclasses* `ITransportController` (e.g. a test double) must add
  > stubs for the three methods.

- **Confirmed: ORP134 G1 resolves the fade-overlap shared-cursor bug (OCC155
  Ask #2).** In `v0.3.0`, the two voices of a `MonoWithFadeOverlap` clip shared a
  single `IAudioFileReader` cursor during the fade-overlap window, corrupting each
  other's file position on re-fire. ORP134 G1's position-explicit `IClipSource`
  reads make multi-voice playback of one clip correct by construction. New
  regression `occ155_api_test.FadeOverlapReFireHasIndependentCursors` fires,
  stops into a fade tail, re-fires during the tail, and asserts the fresh voice
  plays from near trim IN while the deep tail lives on independently — proving the
  cursors no longer share state.

### Changed - ORP133 Realtime Callback Safety & Contract Truth (2026-07-09)

- **Audio→UI event ring is now POD (ORP133 G1).** The transport's internal
  callback ring no longer stores `std::function<void()>` payloads; the audio
  thread enqueues trivially-copyable `TransportEvent`s and
  `processCallbacks()` translates them into the unchanged public
  `ITransportCallback` virtuals on the UI thread. Emission points, ordering,
  and timing are 1:1 with the previous behavior. No public API change.
- **Command-queue threading contract documented and enforced (ORP133 G3).**
  Control-mutating transport methods are single-producer: exactly one control
  thread may call them (funnel UI/MIDI/OSC through one dispatcher). Queries
  remain lock-free and callable from any thread. Debug builds now assert if
  commands are posted from more than one thread. All control entry points
  funnel through a single `postCommand()` choke point.
- **Version realigned to the build's truth (ORP133 G4).** The SDK version is
  **0.3.0**, sourced from `project(orpheus VERSION 0.3.0)` in
  `CMakeLists.txt`; README and docs now reference CMake as the single source
  of truth. The October 2025 "v1.0.0-rc.1"/"v1.0.0-rc.2" README/docs banners
  predated the 0.x renumbering (downstream apps pin `v0.3.0`); those labels
  remain in this changelog as historical release names only.
- **Documentation truth pass + CI gate (ORP133 G5/G6).** README,
  ARCHITECTURE.md, ROADMAP.md, docs/INDEX.md, and docs/API_SURFACE_INDEX.md no
  longer describe the extracted `apps/clip-composer` subdirectory, the removed
  `ORPHEUS_ENABLE_APP_CLIP_COMPOSER` option, or the archived TypeScript driver
  layer as live; `packages/occ-app-platform` and `packages/shmui-juce` are
  documented as active C++ packages. A new `tools/docs_path_audit.py` gate
  (ctest `docs_path_audit` + CI lint step) fails on broken internal doc links,
  unlabeled references to the extracted `apps/clip-composer` subdirectory, and
  references to CMake options that no longer exist.

### Added - ORP134 Platform Primitives (2026-07-09)

- **`IAudioFileWriter` (ORP134 G5 — FourTrack's FTR007 request).** New public
  interface `include/orpheus/audio_file_writer.h` mirroring
  `IAudioFileReader`'s shape and error model: `open(path, config)` /
  `writeSamples(interleaved float32)` / `close()`, background-thread contract
  (never the audio thread — capture feeds a ring buffer that a writer thread
  drains). Libsndfile-backed implementation supports WAV/AIFF in
  Int16/Int24/Float32 and FLAC in Int16/Int24 (Float32 is rejected — FLAC is
  integer-only); out-of-range floats saturate rather than wrap
  (`SFC_SET_CLIPPING`). `createAudioFileWriter()` returns nullptr when built
  without libsndfile (stub, same pattern as the reader). Round-trip tests
  prove bit-exact float32 WAV and ≤1 LSB integer encodings via the SDK's own
  reader. FourTrack can drop its local `WavWriter` on the next submodule bump.

- **The audio thread no longer reads files (ORP134 G1 — the program's
  defining migration).** `processAudio()` used to call
  `IAudioFileReader::readSamples()`/`seek()` per clip — blocking libsndfile
  decode on the audio callback (tracked debt since ORP121). The transport now
  renders from immutable, position-explicit `IClipSource` views built off the
  audio thread: whole-file PCM decoded in `prepareClipAudio()`/`startClip()`
  (control thread) for short clips, and a background-worker-fed fixed page
  ring for long files (> ~30 s; internal threshold). A streaming cache miss
  NEVER blocks: the clip renders silence for that buffer and the transport
  emits the (previously never-fired) `onBufferUnderrun` callback while the
  worker refills. Seeks became prefetch hints — there is no file cursor
  anywhere near the callback — which also makes multi-voice playback of one
  clip correct by construction (voices no longer fight over a shared reader
  position). The public transport API is unchanged.
  **Verified:** golden render-hash parity is bit-exact against the old
  reading path (same hash, all block sizes); the runtime harness measures the
  file-backed callback at ~0 read syscalls (was ~2,400 / ~4.9 MB per 300
  buffers) with zero allocations; streaming prefill plays gap-free and
  seek-past-window underruns recover; `tools/realtime_audit.py
  --fail-known-debt` now PASSES in-repo and runs as the strict CI gate
  (`realtime_static_audit`). Remaining `--include-adjacent` findings are
  app-repo debt for the downstream sprints (REALTIME_AUDIT.md).
- **Stable identity & time-domain primitives (ORP134 G2 — additive).** New
  public headers `identity.h` (opaque `SessionId`/`TrackId`/`ClipId`/
  `MediaId`/`AutomationLaneId` strong types over `uint64_t`, plus a
  deterministic monotonic `IdAllocator` with a serializable watermark),
  `time_domain.h` (`TimePoint`/`TimeRange` with the 64-bit sample count as
  the canonical value and seconds/beats/timecode as derived views), and
  `media_model.h` (`MediaRegion`, `Take`, `ClipSlot`, `LauncherScene` thin
  aggregates). The pointer-based `SessionGraph` is untouched (ORP134 §7);
  these types are the vocabulary new code and serialization should prefer.
  IDs serialize as decimal strings (the SDK JSON number type is a double —
  53-bit mantissa); tests round-trip IDs > 2^53 through the SDK JSON parser
  and the installed-header compile gate covers the new headers.
- **Recorder plumbing (ORP134 G7).** New `audio_input.h`: `AudioInputRing`
  (lock-free SPSC ring of interleaved frames — audio-thread producer never
  blocks, drops whole buffers on overflow and counts them) and
  `IAudioInputStream` (the capture contract hosts consume instead of hooking
  the raw driver callback; `createAudioInputStream()` returns the ring-backed
  implementation). Paired with `IAudioFileWriter` this makes "capture → disk"
  an SDK-supported path (integration test drives audio-thread capture through
  a background writer to a WAV and verifies the samples). Take/punch/latency
  policy stays host-side by design.
- **Analysis facade (ORP134 G6).** New `audio_analysis.h`
  (`orpheus::analysis`): radix-2 FFT/STFT (new), RMS/peak, integrated LUFS
  (WRAPS the existing `LoudnessMeter` — parity asserted by test, not
  re-implemented), spectral centroid/rolloff, spectral-flux onset detection,
  and an in-memory min/max waveform proxy (file-backed proxies remain
  `IAudioFileReaderExtended`'s job). Offline/background utilities only.
- **Graph-neutral routing seam (ORP134 G3).** New `audio_graph.h`:
  sources/processors/buses/sinks/sends/taps/channel-layout vocabulary as a
  validated `GraphDescription`, plus the soundboard facade
  (`makeSoundboardGraph`) and `toRoutingConfig()` mapping onto the EXISTING
  routing matrix (not replaced — ORP134 §7). Tests prove the facade's
  translation equals the transport's hard-wired topology (64 ch / 4 groups /
  stereo out) and that the vocabulary expresses FourTrack's bus shapes and
  FreqFinder's analysis taps. The executing graph engine remains an ORP135
  candidate.
- **Scene manager wired + tested at launcher scale (ORP134 G8).**
  `ISceneManager::setRoutingMatrix()` is now on the public interface (it
  previously existed only on the hidden concrete class, making routing
  capture/recall unreachable through `createSceneManager()`). New
  `scene_routing_test` drives capture/recall of group assignments and gains
  end-to-end, including an 8-tab page-switching round-trip. Gap documented
  by design: `SceneSnapshot::assignedClips` is host presentation state the
  SDK cannot derive — hosts persist grids with `media_model.h`'s
  `ClipSlot`/`LauncherScene` shapes.
- **Offline render determinism gate completed (ORP134 G4).** `RenderSpec` is
  the offline render-job descriptor (sample rate, bit depth, channel layout,
  dither + seed, output target). New `DitheredRenderIsSeedDeterministic` test
  proves the same job descriptor reproduces byte-identical output including
  the dither noise, and that changing the seed changes the bytes. Combined
  with the pre-existing undithered golden hashes (`render_tracks_basic`) and
  the transport-side block-size-invariance gate (ORP136 bootstrap), the
  "same input → same output, always" loop is closed for both render paths.

### Added - ORP136 Verification Bootstrap (2026-07-09)

- **Runtime realtime-safety harness** (`tests/transport/realtime_harness_test.cpp`
  + `tests/support/rt_guard.hpp`, ORP136 §2.2). Global allocation hooks count
  any C++ alloc/dealloc performed while the calling thread is inside a marked
  audio-callback section, and Linux `/proc/self/io` sampling observes file I/O
  at the syscall level. Proven red/green: the pure (reader-less) 16-clip render
  path performs ZERO allocations across 500 callbacks, while the file-backed
  path is pinned as KNOWN DEBT (~2,400 read syscalls observed on the audio
  thread) with an explicit flip-point marker for the ORP134 streaming reader.
  This is the runtime gate ORP134 G1 is blocked on.
- **Golden render-hash determinism gate**
  (`tests/transport/transport_render_hash_test.cpp`, ORP136 §2.3). The
  transport render path (trims, Linear/EqualPower fades, gain, OUT-point stop
  fades, mono→stereo, routing + limiter) is proven **bit-identical across
  block sizes 256/512/1024/2048** and across repeated runs (FNV-1a 64 over
  raw sample bytes). Serves as the parity oracle for the ORP134 G1
  streaming-reader migration. Cross-platform golden constants are deferred
  until the fade math is pinned (ORP134 G4); drift is a bug to file, not mask.

### Deprecated - ORP133

- **`ITransportController::stopAllInGroup()` (ORP133 G2).** This method was a
  silent no-op in every release (the transport has no clip→group mapping;
  grouping is a host concern). It now returns
  `SessionGraphError::NotSupported` and is documented as deprecated. Hosts
  implement scoped group-stop with their own group model on top of
  `stopOtherClips()` (ORP127 G7 choke primitive). The method is retained for
  source compatibility and may be removed in a future major version.

### Added - ORP109 Professional Features (2025-11-11)

#### Feature 1: Routing Matrix API (ORP109, ORP110)

- **Routing Matrix (`routing_matrix.h`)** - Professional N×M audio routing system
  - N×M routing: 64 channels → 16 groups → 32 outputs
  - Multiple solo modes (SIP, AFL, PFL, Destructive)
  - Per-channel gain/pan/mute/solo controls
  - Per-group gain/mute/solo controls
  - Real-time metering (Peak/RMS/TruePeak/LUFS)
  - Snapshot/preset system for instant recall
  - Lock-free audio thread processing
  - Sample-accurate gain smoothing (10ms ramps)
  - Clipping protection (soft-clip before 0 dBFS)

- **Simplified Clip Routing API (`clip_routing.h`)** - Clip-based routing for OCC
  - `assignClipToGroup()` - Assign clip to one of 4 Clip Groups
  - `setGroupGain()`, `setGroupMute()`, `setGroupSolo()` - Per-group controls
  - `routeGroupToMaster()` - Enable/disable group routing
  - Query methods: `getClipGroup()`, `getGroupGain()`, `isGroupMuted()`, `isGroupSoloed()`

- **Unit Tests:** 40+ routing tests passing (routing_matrix_test, gain_smoother_test, clip_routing_test)
- **Implementation:** `src/core/routing/routing_matrix.cpp` (896 LOC), `src/core/routing/clip_routing.cpp` (342 LOC)
- **Performance:** <1% CPU overhead per clip, lock-free operation

#### Feature 2: Audio Device Selection (ORP109, ORP110)

- **Audio Driver Manager (`audio_driver_manager.h`)** - Runtime device management
  - `enumerateDevices()` - List all available output devices
  - `getDeviceInfo()` - Query device capabilities (channels, sample rates, buffer sizes)
  - `setActiveDevice()` - Hot-swap audio device without app restart
  - `getCurrentDevice()`, `getCurrentSampleRate()`, `getCurrentBufferSize()` - Query active state
  - `setDeviceChangeCallback()` - Hot-plug event detection (USB interfaces)

- **Platform Support:**
  - ✅ macOS: Full CoreAudio device enumeration + hot-swap
  - ⚠️ Windows: WASAPI/ASIO stubs (returns dummy driver, planned for v1.1)
  - ⚠️ Linux: ALSA stubs (returns dummy driver, planned for v1.1)

- **Unit Tests:** 25+ device manager tests passing (driver_manager_test, device_hot_swap_test)
- **Implementation:** `src/platform/audio_drivers/driver_manager.cpp` (687 LOC), `src/platform/audio_drivers/coreaudio_device_enumerator.mm` (345 LOC)
- **Performance:** N/A (not in audio thread), device switch causes brief ~100ms dropout

#### Feature 3: Performance Monitoring API (ORP109, ORP110)

- **Performance Monitor (`performance_monitor.h`)** - Real-time diagnostics
  - `getMetrics()` - CPU usage, latency, underrun count, active clip count, uptime
  - `getPeakCpuUsage()` - Peak CPU since last reset (worst-case profiling)
  - `resetUnderrunCount()`, `resetPeakCpuUsage()` - Reset diagnostics
  - `getCallbackTimingHistogram()` - Callback jitter profiling

- **Metrics Provided:**
  - CPU usage (0-100%, exponential moving average)
  - Round-trip latency (input + processing + output)
  - Buffer underrun count (dropout detection)
  - Active clip count, total samples processed, uptime

- **Unit Tests:** 20+ performance monitor tests passing (performance_monitor_test, performance_integration_test)
- **Implementation:** `src/core/common/performance_monitor.cpp` (167 LOC)
- **Performance:** <100 CPU cycles per query, <1% audio thread overhead

#### Feature 4: Waveform Pre-Processing (ORP109, ORP110)

- **Audio File Reader Extended (`audio_file_reader_extended.h`)** - Fast waveform extraction
  - `getWaveformData()` - Downsampled min/max peaks per pixel
  - `getPeakLevel()` - Peak level for normalization
  - `precomputeWaveformAsync()` - Background pre-computation for instant queries

- **Capabilities:**
  - Multi-resolution LOD pyramid (zoom levels)
  - Multi-channel support (stereo, 5.1, etc.)
  - Efficient streaming reads (large files >100MB)
  - Sample-accurate downsampling algorithm

- **Unit Tests:** 15+ waveform processor tests passing (waveform_processor_test)
- **Implementation:** `src/core/audio_io/waveform_processor.cpp` (302 LOC)
- **Performance:** 1-min WAV → 800px in ~10ms, 10-min WAV → 800px in ~80ms

#### Feature 5: Scene/Preset System (ORP109, ORP110)

- **Scene Manager (`scene_manager.h`)** - Lightweight preset snapshots
  - `captureScene()` - Save current session state (metadata only)
  - `recallScene()` - Restore session state without reloading audio files
  - `listScenes()` - Query all saved scenes (ordered by timestamp)
  - `deleteScene()`, `clearAllScenes()` - Scene management
  - `exportScene()`, `importScene()` - JSON export/import for portability

- **Scene Format:**
  - UUID-based scene identification (timestamp + counter)
  - Stores clip assignments, routing, group gains
  - Does NOT store audio file paths (assumes clips already loaded)

- **Unit Tests:** 40+ scene manager tests passing (scene_manager_test)
- **Implementation:** `src/core/session/scene_manager.cpp` (460 LOC)
- **Performance:** Metadata only (~1 KB per scene), instant capture/recall

#### Feature 6: Cue Points/Markers (ORP109, ORP110)

- **Transport Controller Extensions (`transport_controller.h`)** - In-clip markers
  - `addCuePoint()` - Add named marker at sample position
  - `getCuePoints()` - Query all cue points for clip (ordered by position)
  - `seekToCuePoint()` - Jump to specific cue point (sample-accurate)
  - `removeCuePoint()` - Delete cue point by index

- **Cue Point Structure:**
  - `position` - Sample offset (file-relative, 0-based)
  - `name` - User label (e.g., "Verse 1", "Chorus")
  - `color` - RGBA color for UI rendering (0xRRGGBBAA)

- **Unit Tests:** 15+ cue point tests (clip_cue_points_test, 1 known abort issue)
- **Implementation:** Extended `src/core/transport/transport_controller.cpp` (+120 LOC)
- **Performance:** <0.1% CPU overhead per cue, sample-accurate seek

#### Feature 7: Multi-Channel Routing (ORP109, ORP110)

- **Clip Routing Extensions (`clip_routing.h`)** - Multi-channel support
  - `setClipOutputBus()` - Route clip to specific output bus (0 = channels 1-2, 1 = channels 3-4, etc.)
  - `mapChannels()` - Fine-grained channel mapping (clip channel → output channel)
  - `getClipOutputBus()` - Query current bus assignment

- **Bus Mapping:** Support for up to 32 output channels (16 stereo buses)
- **Unit Tests:** 10+ multi-channel routing tests passing (multi_channel_routing_test)
- **Implementation:** Extended `src/core/routing/clip_routing.cpp` (+80 LOC)
- **Performance:** <0.5% CPU overhead per clip

### Technical Details - ORP109

- **Files Created:** 23 files (7 headers, 9 implementations, 7 test files)
- **Lines of Code:** ~5,000+ production code, ~4,350+ test code
- **Test Coverage:** 165+ new tests (98%+ pass rate)
- **Zero Regressions:** All existing tests continue to pass
- **Platform Support:** macOS complete, Windows/Linux partial (stubs)
- **Binary Size Impact:** +600 KB (+21% from 2.8 MB → 3.4 MB)
- **Memory Overhead:** ~10-15 KB per active session (excluding waveform cache)
- **CPU Overhead:** <5% with all features active (16 clips, 4 groups, metering enabled)

### Documentation - ORP109

- **Migration Guide:** Extended `docs/MIGRATION_v0_to_v1.md` with ORP109 feature section
- **README:** Updated with ORP109 features in "What's New" section
- **API Surface Index:** Updated with 7 new public headers
- **Architecture:** Updated with new SDK capabilities (routing, device management, diagnostics)
- **Implementation Report:** Complete documentation in `docs/ORP/ORP110 ORP109 Implementation Report.md`
- **Roadmap:** Original specification in `docs/ORP/ORP109 SDK Feature Roadmap for Clip Composer Integration.md`

---

## [1.0.0-rc.1] - 2025-10-31

First release candidate for Orpheus SDK v1.0. This release introduces comprehensive clip playback control, metadata persistence, and transport features for professional broadcast and live performance applications.

### Added

#### Gain Control API (ORP074, ORP100)

- `updateClipGain(ClipHandle, float gainDb)` - Per-clip gain adjustment (-96 to +12 dB)
- Gain conversion: `linear = 10^(gainDb / 20)` (0 dB = unity, -6 dB = 0.5, +6 dB = 2.0)
- Thread-safe: Immediate effect for active clips, on next start for stopped clips
- Validated: Must be finite (not NaN or Inf)
- Unit tests: `tests/transport/clip_gain_test.cpp` (11/11 passing)

#### Loop Mode API (ORP074, ORP090, ORP100)

- `setClipLoopMode(ClipHandle, bool shouldLoop)` - Enable seamless clip looping
- `isClipLooping(ClipHandle)` - Query loop state for UI indicators
- Loop behavior: Seek to trim IN at trim OUT (no fade-out on loop)
- `onClipLooped()` callback fires on each loop iteration
- Unit tests: `tests/transport/clip_loop_test.cpp` (11/11 passing)

#### Persistent Metadata Storage (ORP074, ORP100)

- `updateClipMetadata(ClipHandle, ClipMetadata)` - Batch update all metadata
- `getClipMetadata(ClipHandle)` - Query all metadata in single call
- `setSessionDefaults(SessionDefaults)` - Global defaults for new clips
- `getSessionDefaults()` - Query session-level defaults
- Metadata persists through stop/start cycles (stored in `AudioFileEntry`)
- Fields: trim points, fade curves, gain, loop mode, stop others mode
- Unit tests: `tests/transport/clip_metadata_test.cpp` (10/10 passing)

#### Seamless Clip Restart (ORP087)

- `restartClip(ClipHandle)` - Gap-free restart from trim IN point
- Sample-accurate: ±0 samples (not ±1 sample tolerance)
- Unlike `startClip()`: ALWAYS restarts even if already playing
- Use case: Clip editor preview with < > trim nudge buttons
- Fires `onClipRestarted()` callback

#### Clip Seek API (ORP088)

- `seekClip(ClipHandle, int64_t position)` - Sample-accurate position seeking
- Thread-safe: Callable from UI thread
- Real-time safe: Seek happens in audio thread (no allocations)
- Use case: Waveform click-to-jog (SpotOn/Pyramix UX)
- Fires `onClipSeeked()` callback

#### Trim Boundary Enforcement (ORP091, ORP093, ORP094)

- Strict enforcement of trim IN/OUT points (no playback beyond boundaries)
- Non-looping clips: Stop at trim OUT (fade-out applied)
- Looping clips: Seek to trim IN at trim OUT (seamless)
- Validation: Returns `SessionGraphError::InvalidClipTrimPoints` if invalid
- Fixed illegal loop-to-zero bug (ORP092)

#### Transport Callbacks

- `onClipRestarted(ClipHandle, TransportPosition)` - Restart event
- `onClipSeeked(ClipHandle, TransportPosition)` - Seek event

#### Testing & Validation

- `tests/transport/multi_clip_stress_test.cpp` - Enhanced 16-clip integration test
- Test coverage: 60 seconds runtime, varied gain/loop/trim settings
- 32/32 unit tests passing (100% success rate)
- AddressSanitizer clean (no memory leaks detected)

#### Documentation

- `docs/MIGRATION_v0_to_v1.md` - Comprehensive migration guide
- `docs/orp/ORP100 Unit Tests Implementation Report.md` - Test coverage report
- `docs/orp/ORP099 SDK Track Phase 4 Completion & Testing.md` - Release plan

### Changed

#### Trim Point Behavior

- **Breaking (behavioral):** Trim OUT now strictly enforced (clips cannot play beyond OUT)
- **Before v1.0:** Trim points were advisory (clips could exceed boundaries)
- **v1.0:** Trim boundaries mandatory (no opt-out)

#### Performance

- **Memory:** +64 bytes per clip (ClipMetadata structure)
- **CPU:** <1% overhead for gain/loop/metadata features (16 clips)
- **Audio Thread:** No allocations, lock-free command processing

### Fixed

#### Audio Engine

- Fix use-after-free bug in AudioEngine destructor (ORP096)
- Fix zigzag fade distortion by pre-computing gains (ORP097)
- Disable routing matrix gain smoothing (caused distortion)
- Remove file I/O from audio callback (ORP097)
- Fix fade timing bug (ORP097)

#### Transport

- Fix illegal loop-to-zero bug for non-loop clips (ORP092)
- Fix OUT point enforcement not working (ORP091)
- Eliminate transport spam with `seekClip()` API (ORP088)

#### Testing

- Add `orpheus_audio_io` library link to transport tests

### Deprecated

None. v1.0 is fully backward compatible with v0.x.

### Performance

**Measured (16 simultaneous clips, 60s runtime):**

- Callback accuracy: 74.9% (dummy driver)
- Clips started: 16/16
- Looping clips still playing: 8/8
- Memory: Stable (no leaks)
- CPU: <10% estimated (Intel i5 8th gen)

**Overhead:**

- Gain control: ~10 CPU cycles per sample
- Loop mode: ~50 CPU cycles per loop iteration
- Metadata: Zero overhead in audio thread (cached)

---

## [0.2.0-alpha] - 2025-10-28

### Added

- Clip Composer v0.2.0 (OCC application)
- Multi-tab transport isolation (OCC)
- Clip Edit Dialog redesign with color picker (OCC)
- Keyboard navigation and shortcuts (OCC)

### Fixed

- Multi-tab transport isolation bug (critical)
- Clip metadata persistence issues (fades, loop, stop-others)
- Audio I/O Settings dialog layout issues
- Trim point edit laws on Cmd+Click waveform
- OUT point edit law enforcement

---

## [0.1.0-alpha] - 2025-10-22

### Added

- Initial alpha release of Orpheus SDK
- Core transport controller
- Audio file I/O (WAV, AIFF, FLAC)
- Routing matrix (4 Clip Groups → Master)
- CoreAudio driver (macOS)
- Dummy audio driver (testing)
- Sample-accurate timing (±1 sample tolerance)
- Fade-in/out with curve types (Linear, EqualPower, Exponential)
- Trim points (IN/OUT samples)
- Stop Others mode

---

## Release Notes

### v1.0.0-rc.1 Highlights

**For SDK Users:**

- **Gain Control:** Normalize clips, create dynamic mixes, automate levels
- **Loop Mode:** Seamless music beds, ambience, sound effects
- **Persistent Metadata:** Clip settings survive stop/start cycles
- **Seamless Restart:** Gap-free preview in clip editors
- **Clip Seek:** Sample-accurate timeline scrubbing

**Migration:**

- **Backward compatible:** v0.x code works unchanged
- **New APIs:** Optional enhancements (not required)
- **See:** `docs/MIGRATION_v0_to_v1.md` for complete guide

**Testing:**

- **Unit tests:** 32/32 passing (11 gain, 11 loop, 10 metadata)
- **Integration test:** 16 clips, 60s runtime, 100% success
- **Memory:** ASan clean (no leaks)
- **Performance:** <1% CPU overhead

**Known Issues:**

- Doxygen documentation not yet generated (Task 3.1 pending)
- Performance profiling with Instruments/perf pending (Tasks 2.1-2.2)
- Latency validation pending (Task 2.2)

**Next Steps:**

1. Review this changelog
2. Read migration guide (`docs/MIGRATION_v0_to_v1.md`)
3. Update your code (optional, backward compatible)
4. Test with your workload
5. Report issues via GitHub

---

## References

- [ORP068] Implementation Plan v2.0 - SDK architecture and milestones
- [ORP074] SDK Enhancement Sprint - Gain/loop/metadata features
- [ORP087] Seamless Clip Restart API - Gap-free restart implementation
- [ORP088] Edit Law Enforcement - Clip seek and position tracking
- [ORP091] SDK OUT Point Enforcement Not Working - Boundary bug fix
- [ORP092] Fix Non-Loop Clips Illegal Loop-to-Zero Bug
- [ORP093] Sprint - Trim Point Boundary Enforcement
- [ORP094] Fix - Trim Point Boundary Enforcement
- [ORP095] Hot Buffer Size Change Implementation
- [ORP096] Fix Use-After-Free Bug in AudioEngine Destructor
- [ORP097] SDK Transport/Fade Bug Fixes for OCC v0.2.0
- [ORP099] SDK Track: Phase 4 Completion & Testing
- [ORP100] Unit Tests Implementation Report

---

## Contributors

- Orpheus SDK Team
- Claude Code (AI-assisted development)

---

**Support:**

- Documentation: `docs/`
- Issues: https://github.com/yourusername/orpheus-sdk/issues
- Discussions: https://github.com/yourusername/orpheus-sdk/discussions

**License:** MIT
