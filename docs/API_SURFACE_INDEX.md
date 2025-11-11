# API Surface Index

This index catalogs public entry points exposed by the Orpheus SDK workspace. Update this document whenever new packages or
notable APIs are added.

**Last Updated:** 2025-11-11 (ORP109 features)
**SDK Version:** v1.0.0-rc.2 (unreleased)

---

## C++ Public Headers

### Core Transport & Playback

| Header                                         | Primary Interface          | Description                                             | Added               |
| ---------------------------------------------- | -------------------------- | ------------------------------------------------------- | ------------------- |
| `include/orpheus/transport_controller.h`       | `ITransportController`     | Multi-clip transport with gain/loop/seek/restart        | v1.0.0-rc.1         |
| `include/orpheus/transport_controller.h`       | Cue point extensions       | In-clip markers with seek-to-cue operations             | ORP109 (unreleased) |
| `include/orpheus/session_graph.h`              | `SessionGraph`             | In-memory session representation (tracks, clips, tempo) | v0.1.0-alpha        |
| `include/orpheus/audio_file_reader.h`          | `IAudioFileReader`         | Audio file decoding (WAV/AIFF/FLAC via libsndfile)      | v0.1.0-alpha        |
| `include/orpheus/audio_file_reader_extended.h` | `IAudioFileReaderExtended` | Waveform pre-processing for UI rendering                | ORP109 (unreleased) |

### Routing & Mixing (ORP109)

| Header                             | Primary Interface        | Description                                                     | Added               |
| ---------------------------------- | ------------------------ | --------------------------------------------------------------- | ------------------- |
| `include/orpheus/routing_matrix.h` | `IRoutingMatrix`         | Professional N×M routing (64 channels → 16 groups → 32 outputs) | ORP109 (unreleased) |
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

- `ChannelConfig`, `GroupConfig`, `RoutingConfig`, `AudioMeter`, `RoutingSnapshot`, `SoloMode`, `MeteringMode`

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

**Note:** TypeScript packages previously in `packages/` have been archived. See [DECISION_PACKAGES.md](DECISION_PACKAGES.md) for rationale (C++ SDK focus).

| Package                  | Entry Point                   | Notes                                          | Status   |
| ------------------------ | ----------------------------- | ---------------------------------------------- | -------- |
| `@orpheus/shmui`         | `packages/shmui/src/index.js` | React components and UI helpers                | Archived |
| `@orpheus/engine-native` | `packages/engine-native/`     | Node/Electron bindings wrapping the C++ engine | Archived |
| `@orpheus/engine-wasm`   | _Planned_                     | WebAssembly bundle                             | Archived |
| `@orpheus/client`        | _Planned_                     | Contract negotiation and command helpers       | Archived |

---

## C++ Components

| Component   | Location    | Description                                                        |
| ----------- | ----------- | ------------------------------------------------------------------ |
| Core Engine | `src/`      | Primary C++ source compiled into static libraries.                 |
| Adapters    | `adapters/` | Integration points for external hosts (minhost, REAPER).           |
| Tests       | `tests/`    | GoogleTest-driven validation (270+ unit tests).                    |
| Apps        | `apps/`     | Applications built on SDK (Clip Composer, Wave Finder, FX Engine). |

---

## Documentation Cross-Reference

- [Architecture Overview](../ARCHITECTURE.md) – System design and threading model
- [Migration Guide](MIGRATION_v0_to_v1.md) – v0.x → v1.0 upgrade guide (includes ORP109 features)
- [Driver Architecture](DRIVER_ARCHITECTURE.md) – Runtime-specific integration notes
- [Contract Guide](CONTRACT_DEVELOPMENT.md) – Command/event schemas shared across drivers
- [ORP109 Roadmap](../docs/ORP/ORP109%20SDK%20Feature%20Roadmap%20for%20Clip%20Composer%20Integration.md) – Feature specifications
- [ORP110 Implementation Report](../docs/ORP/ORP110%20ORP109%20Implementation%20Report.md) – Complete feature documentation

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
