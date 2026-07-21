# API Surface Index

This index catalogs public entry points exposed by the Orpheus SDK workspace. Update this document whenever new packages or
notable APIs are added.

**Last Updated:** 2026-07-16 (sequencer trigger voice primitive)
**SDK Version:** 0.6.3 — the authoritative version is `project(orpheus VERSION ...)`
in the repo-root `CMakeLists.txt`. ("Added" tags below cite the historical
release names in `CHANGELOG.md`, including the pre-renumbering `v1.0.0-rc.*`
labels.)

---

## C++ Public Headers

### Core Transport & Playback

| Header                                         | Primary Interface          | Description                                             | Added               |
| ---------------------------------------------- | -------------------------- | ------------------------------------------------------- | ------------------- |
| `include/orpheus/transport_controller.h`       | `ITransportController`     | Multi-clip transport with gain/loop/seek/restart        | v1.0.0-rc.1         |
| `include/orpheus/transport_controller.h`       | Cue point extensions       | In-clip markers with seek-to-cue operations             | ORP109 (unreleased) |
| `include/orpheus/transport_controller.h`       | Callback telemetry / active snapshot | Cumulative ring-loss detection and fixed-capacity voice reconciliation | ORP151 (`0.5.1`) |
| `include/orpheus/clip_dsp.h`                   | `ClipDspProgram`, `ClipDspProcessor` | Validated fixed-order, fixed-capacity per-clip realtime processing | ORP157 (`0.6.2`) |
| `include/orpheus/session_graph.h`              | `SessionGraph`             | In-memory session representation (tracks, clips, tempo) | v0.1.0-alpha        |
| `include/orpheus/audio_file_reader.h`          | `IAudioFileReader`         | Audio file decoding (WAV/AIFF/FLAC via libsndfile)      | v0.1.0-alpha        |
| `include/orpheus/audio_file_reader_extended.h` | `IAudioFileReaderExtended` | Waveform pre-processing for UI rendering                | ORP109 (unreleased) |
| `include/orpheus/audio_file_writer.h`          | `IAudioFileWriter`         | Audio file encoding (WAV/AIFF/FLAC via libsndfile)      | ORP134 G5 (FTR007)  |
| `include/orpheus/identity.h`                   | `StrongId`, `IdAllocator`  | Stable session/track/clip/media/lane identifiers        | ORP134 G2           |
| `include/orpheus/time_domain.h`                | `TimePoint`, `TimeRange`   | Sample-canonical time with seconds/beats/timecode views | ORP134 G2           |
| `include/orpheus/media_model.h`                | `MediaRegion`, `Take`      | Media/region vocabulary + launcher aggregates           | ORP134 G2           |
| `include/orpheus/audio_input.h`                | `IAudioInputStream`        | Lock-free capture ring + input-stream contract          | ORP134 G7           |
| `include/orpheus/audio_analysis.h`             | `orpheus::analysis`        | FFT/STFT, LUFS/RMS/peak, spectral features, onsets      | ORP134 G6           |
| `include/orpheus/audio_graph.h`                | `GraphDescription`         | Graph-neutral routing vocabulary + soundboard facade    | ORP134 G3           |
| `include/orpheus/trigger_voice.h`               | `ITriggerVoice`, `TriggerVoice` | RT-safe owned one-shot sample, bounded polyphony, sample-accurate trigger offsets, and pitch-ratio playback | ORP154 (`0.6.2`) |

### Routing & Mixing (ORP109)

| Header                             | Primary Interface        | Description                                                     | Added               |
| ---------------------------------- | ------------------------ | --------------------------------------------------------------- | ------------------- |
| `include/orpheus/routing_matrix.h` | `IRoutingMatrix`         | N×M routing plus coherent group-control snapshots               | ORP109, ORP153       |
| `include/orpheus/clip_routing.h`   | `IClipRoutingMatrix`     | Simplified clip-based routing (4 Clip Groups for OCC)           | ORP109 (unreleased) |
| `include/orpheus/clip_routing.h`   | Multi-channel extensions | Output bus assignment for 8-32 channel interfaces               | ORP109 (unreleased) |

### Audio I/O & Device Management

| Header                                   | Primary Interface     | Description                             | Added               |
| ---------------------------------------- | --------------------- | --------------------------------------- | ------------------- |
| `include/orpheus/audio_driver.h`         | `IAudioDriver`        | Platform-agnostic audio I/O abstraction | v0.1.0-alpha        |
| `include/orpheus/audio_driver_manager.h` | `IAudioDriverManager` | Runtime device enumeration and hot-swap | ORP109 (unreleased) |

### Performance & Diagnostics (ORP109)

| Header                                  | Primary Interface     | Description                             | Added               |
| --------------------------------------- | --------------------- | --------------------------------------- | ------------------- |
| `include/orpheus/performance_monitor.h` | `IPerformanceMonitor` | Real-time CPU/latency/underrun tracking | ORP109 (unreleased) |

### Workflow Management (ORP109)

| Header                            | Primary Interface        | Description                                        | Added               |
| --------------------------------- | ------------------------ | -------------------------------------------------- | ------------------- |
| `include/orpheus/scene_manager.h` | `ISceneManager`          | Lightweight preset snapshots for theater/broadcast | ORP109 (unreleased) |
| `include/orpheus/session_json.h`  | `session_json` utilities | Session serialization and filesystem helpers       | v0.1.0-alpha        |

### Metadata & Configuration

| Header                            | Primary Types       | Description                                  | Added        |
| --------------------------------- | ------------------- | -------------------------------------------- | ------------ |
| `include/orpheus/clip_metadata.h` | `ClipMetadata`      | Trim/fade/gain/loop settings (persistent)    | v1.0.0-rc.1  |
| `include/orpheus/abi_version.h`   | `AbiVersion`        | Version negotiation and compatibility checks | v0.1.0-alpha |
| `include/orpheus/error_codes.h`   | `SessionGraphError` | SDK error code enumeration                   | v0.1.0-alpha |

---

## ORP109 Feature Summary (Unreleased)

**Added:** 2025-11-11
**Status:** Complete, awaiting OCC integration
**Test Coverage:** 165+ new tests (98%+ pass rate)

### New Public Interfaces (7)

1. **IRoutingMatrix** (`routing_matrix.h`) - Professional N×M audio routing
2. **IClipRoutingMatrix** (`clip_routing.h`) - Simplified clip-based routing
3. **IAudioDriverManager** (`audio_driver_manager.h`) - Device enumeration and hot-swap
4. **IPerformanceMonitor** (`performance_monitor.h`) - Real-time diagnostics
5. **IAudioFileReaderExtended** (`audio_file_reader_extended.h`) - Waveform pre-processing
6. **ISceneManager** (`scene_manager.h`) - Preset/snapshot management
7. **Transport Extensions** (`transport_controller.h`) - Cue points and multi-channel routing

### New Data Structures (23+)

**Routing:**

- `ChannelConfig`, `GroupConfig`, `RoutingConfig`, `AudioMeter`, `RoutingSnapshot`, `RoutingControlSnapshot`, `RoutingGroupControlState`, `SoloMode`, `MeteringMode`

**Device Management:**

- `AudioDeviceInfo`

**Performance:**

- `PerformanceMetrics`

**Waveform:**

- `WaveformData`

**Scene Management:**

- `SceneSnapshot`

**Cue Points:**

- `CuePoint`, `ClipMetadataExtended`

---

## JavaScript Packages (Archived)

**Note:** The former TypeScript packages have been **archived and removed from
the tree** — nothing under `packages/` is JavaScript today. See
[DECISION_PACKAGES.md](orp/_process/archive/DECISION_PACKAGES.md) for rationale (C++ SDK focus).

| Package (archived)       | Former Role                                    | Status   |
| ------------------------ | ---------------------------------------------- | -------- |
| `@orpheus/shmui`         | React components and UI helpers                | Archived |
| `@orpheus/engine-native` | Node/Electron bindings wrapping the C++ engine | Archived |
| `@orpheus/engine-wasm`   | WebAssembly bundle (never shipped)             | Archived |
| `@orpheus/client`        | Contract negotiation and command helpers       | Archived |

---

## C++ Components

| Component   | Location    | Description                                                                  |
| ----------- | ----------- | ---------------------------------------------------------------------------- |
| Core Engine | `src/`      | Primary C++ source compiled into static libraries.                            |
| Adapters    | `adapters/` | Integration points for external hosts (minhost, REAPER).                      |
| Packages    | `packages/` | Active C++/JUCE app packages (`occ-app-platform`, `shmui-juce`).               |
| Tests       | `tests/`    | GoogleTest-driven validation (270+ unit tests).                                |
| Apps        | `apps/`     | In-tree dev apps (wave-finder smoke shell, juce-demo-host). Production apps (Clip Composer, FourTrack, FreqFinder) are external repos. |

---

## Documentation Cross-Reference

- [Architecture Overview](../ARCHITECTURE.md) – System design and threading model
- [Migration Guide](MIGRATION_v0_to_v1.md) – v0.x → v1.0 upgrade guide (historical; includes ORP109 features)
- [Driver Architecture](orp/_process/archive/DRIVER_ARCHITECTURE.md) – Runtime-specific integration notes (archived)
- [Contract Guide](orp/_process/archive/CONTRACT_DEVELOPMENT.md) – Command/event schemas shared across drivers (archived)
- [ORP109 Roadmap](orp/ORP109%20SDK%20Feature%20Roadmap%20for%20Clip%20Composer%20Integration.md) – Feature specifications
- [ORP110 Implementation Reports](orp/ORP110A%20App-Level%20Integration%20Report.md) – Complete feature documentation (see also ORP110B)
- [ORP150 Atomic Clip-Group Choke Admission](orp/ORP150%20Atomic%20Clip-Group%20Choke%20Admission.md) – One-command metadata-group start/choke semantics and failure atomicity
- [ORP151 Callback Loss Telemetry and Active Voice Reconciliation](orp/ORP151%20Callback%20Loss%20Telemetry%20and%20Active%20Voice%20Reconciliation.md) – Counter lifetime, snapshot semantics, realtime publication, and installed-host reconciliation
- [ORP154 Sequencer Trigger Voice Primitive](orp/ORP154%20Sequencer%20Trigger%20Voice%20Primitive.md) – Owned PCM, bounded voice policy, in-buffer scheduling, pitch interpolation, and realtime guarantees

---

## Usage Examples

### Basic Transport (v1.0.0-rc.1)

```cpp
#include <orpheus/transport_controller.h>

auto transport = createTransportController();
transport->registerClipAudio(handle, "audio.wav");
transport->startClip(handle);
transport->updateClipGain(handle, -6.0f);
transport->setClipLoopMode(handle, true);
```

### Atomic Metadata-Group Choke (ORP150)

```cpp
auto metadata = transport->getClipMetadata(handle).value();
metadata.routingGroup = 1;
metadata.voiceMode = VoiceMode::MonoWithFadeOverlap;
transport->updateClipMetadata(handle, metadata);

// One SPSC command: admit this start first, then fade only active peers whose
// registered routingGroup is also 1. Check the immediate queue-admission result.
const auto result = transport->startClipWithGroupChoke(handle);
```

Do not replace this operation with multiple `stopClip()` calls: queue
saturation could admit only a prefix. `SessionGraphError::OK` means the atomic
command entered the ring; a later voice-pool refusal is reported through
`ITransportCallback::onActiveClipLimitReached` and leaves peers untouched.

### Durable Voice-Aware Callbacks

Hosts that persist one row per accepted playout receive a `voiceId` in
`onClipStarted()`, `onClipStopped()`, and `onClipLooped()`. The value is the
fixed-capacity SDK voice instance identity and is nonzero for accepted starts.
An in-place `MonoStrict` or `MonoWithFadeOverlap` refire retains the existing
identity; a newly allocated overlap voice receives a distinct identity.
Every retired voice emits `onClipStopped()`, including an old fade tail when a
fresh sibling remains live. Durable hosts close the row matching `voiceId`;
handle-level UI state must reconcile through `getClipState(handle)` rather than
interpreting every retirement as an aggregate Stopped transition.

### Callback Loss Detection and Reconciliation (ORP151)

```cpp
auto previousDrops = uint64_t{0};

// In the single control/message-thread pump:
const auto before = transport->getCallbackDeliveryTelemetry();
transport->processCallbacks();
const auto after = transport->getCallbackDeliveryTelemetry();

const bool lossDuringPump =
    after.cumulativeDroppedCount != before.cumulativeDroppedCount;
if (lossDuringPump ||
    after.cumulativeDroppedCount != previousDrops) {
  // Retained callbacks are incomplete. Reconcile current surviving state by
  // ClipHandle; keep the historical interval marked indeterminate.
  ActiveVoiceSnapshot active;
  do {
    active = transport->getActiveVoiceSnapshot();
  } while (active.publicationSequence <
           after.activeVoiceSnapshotSequence);
  for (uint32_t i = 0; i < active.entryCount; ++i) {
    reconcile(active.entries[i].handle,
              active.entries[i].activeVoiceCount,
              active.entries[i].state,
              active.entries[i].newestPosition.samples);
  }
  previousDrops = after.cumulativeDroppedCount;
}
```

Counters are cumulative for one controller lifetime and are not reset by
`processCallbacks()`. Polling detects loss even when the dropped event was the
last event in a burst. See ORP151 for the stable-copy pattern when audio remains
active during reconciliation.

`ITransportController` is a C++ virtual interface. The new queries have defaults
for source compatibility, but `0.5.1` C++ consumers and custom implementations
must recompile; the stable ABI promise in the repository README applies to the C
ABI, not binary compatibility of this C++ vtable.

### Routing Matrix (ORP109)

```cpp
#include <orpheus/clip_routing.h>

auto routing = createClipRoutingMatrix(sessionGraph, 48000);
routing->assignClipToGroup(clipHandle, 0);  // Group 0
routing->setGroupGain(0, -3.0f);
routing->setGroupSolo(1, true);
```

### Audio Device Selection (ORP109)

```cpp
#include <orpheus/audio_driver_manager.h>

auto driverManager = createAudioDriverManager();
auto devices = driverManager->enumerateDevices();
driverManager->setActiveDevice(deviceId, 48000, 512);
```

### Performance Monitoring (ORP109)

```cpp
#include <orpheus/performance_monitor.h>

auto perfMonitor = createPerformanceMonitor(sessionGraph);
auto metrics = perfMonitor->getMetrics();
// metrics.cpuUsagePercent, metrics.latencyMs, metrics.bufferUnderrunCount
```

---

**Maintained By:** SDK Core Team
**Next Review:** After v1.0.0 stable release
