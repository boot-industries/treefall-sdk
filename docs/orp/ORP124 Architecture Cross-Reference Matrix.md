# ORP124 Architecture Cross-Reference Matrix

**Status:** Authoritative
**Created:** 2026-02-27
**Purpose:** Maps the three-layer architecture (SDK Core, occ-app-platform, Applications) to source locations, CMake targets, and documentation.

---

## Package Architecture

```
orpheus-sdk/
  src/core/            orpheus_core         Base library (session graph, ADM, etc.)
  src/transport/       orpheus_transport    ITransportController, clip playback
  src/audio_io/        orpheus_audio_io     IAudioFileReader, file decoding
  src/routing/         orpheus_routing      IRoutingMatrix, channel routing
  src/audio_driver/    orpheus_audio_driver_coreaudio (macOS)

  packages/
    occ-app-platform/  occ_app_platform     Shared JUCE app services
    shmui-juce/        orpheus_shmui_juce   Audio visualization components

  apps/
    clip-composer/     orpheus_clip_composer_app
    wave-finder/       orpheus_wave_finder_app
```

---

## Layer 1: SDK Core Interfaces

| Interface | Header | CMake Target | Used By |
|-----------|--------|-------------|---------|
| `ITransportController` | `include/orpheus/transport_controller.h` | `orpheus_transport` | OCC AudioEngine, Wave Finder |
| `ITransportCallback` | `include/orpheus/transport_controller.h` | `orpheus_transport` | OCC AudioEngine |
| `IAudioFileReader` | `include/orpheus/audio_file_reader.h` | `orpheus_audio_io` | OCC AudioEngine |
| `IAudioFileReaderExtended` | `include/orpheus/audio_file_reader_extended.h` | `orpheus_audio_io` | OCC AudioEngine |
| `IRoutingMatrix` | `include/orpheus/routing_matrix.h` | `orpheus_routing` | OCC AudioEngine |
| `IClipRoutingMatrix` | `include/orpheus/clip_routing.h` | `orpheus_routing` | OCC AudioEngine |
| `IAudioDriver` | `include/orpheus/audio_driver.h` | platform-specific | OCC AudioEngine |
| `IAudioDriverManager` | `include/orpheus/audio_driver_manager.h` | platform-specific | OCC AudioEngine |
| `IPerformanceMonitor` | `include/orpheus/performance_monitor.h` | `orpheus_transport` | OCC MainComponent |
| `ISceneManager` | `include/orpheus/scene_manager.h` | `orpheus_core` | (future) |

---

## Layer 2: occ-app-platform Services

| Service | Header | Source | Dependencies |
|---------|--------|--------|-------------|
| `Command` | `include/orpheus/app/Command.h` | (header-only) | None |
| `UndoManager` | `include/orpheus/app/UndoManager.h` | `src/UndoManager.cpp` | Command |
| `ServiceContext` | `include/orpheus/app/ServiceContext.h` | (header-only) | None |
| `ApplicationPaths` | `include/orpheus/app/ApplicationPaths.h` | `src/ApplicationPaths.cpp` | juce_core |
| `Database` | `include/orpheus/app/Database.h` | `src/Database.cpp` | SQLite3 |
| `EventLogger` | `include/orpheus/app/EventLogger.h` | `src/EventLogger.cpp` | Database |
| `PlayoutLogger` | `include/orpheus/app/PlayoutLogger.h` | `src/PlayoutLogger.cpp` | Database |
| `DisplayPreferences` | `include/orpheus/app/DisplayPreferences.h` | `src/DisplayPreferences.cpp` | juce_data_structures |
| `ExternalToolManager` | `include/orpheus/app/ExternalToolManager.h` | `src/ExternalToolManager.cpp` | juce_core |

**CMake target:** `occ_app_platform` (static library)
**Link dependencies:** `juce::juce_core`, `juce::juce_events`, `juce::juce_data_structures`, `sqlite3`

---

## Layer 3: Application-Specific Code

### Clip Composer (`apps/clip-composer/`)

| Component | Location | Dependencies |
|-----------|----------|-------------|
| `AudioEngine` | `Source/Audio/` | SDK (Transport, AudioIO, Routing, Driver) |
| `SessionManager` | `Source/Session/` | Database, ApplicationPaths |
| `ClipGrid` / `ClipButton` | `Source/ClipGrid/` | shmui-juce (ClipButton) |
| `HotKeyManager` | `Source/Core/` | AudioEngine, ClipGrid |
| `MIDIDeviceManager` | `Source/Core/` | AudioEngine |
| `ClipCommands` | `Source/Core/` | Command (occ-app-platform), SessionManager |
| `GridConstants` | `Source/Core/` | (header-only, app-specific) |
| `MainComponent` | `Source/` | All of the above |

### Wave Finder (`apps/wave-finder/`)

| Component | Location | Dependencies |
|-----------|----------|-------------|
| `MainComponent` | `Source/` | Database, EventLogger, ServiceContext, ApplicationPaths |

---

## CMake Build Presets

| Preset | SDK | OCC | Wave Finder | Tests |
|--------|-----|-----|-------------|-------|
| `sdk-debug` | Yes | No | No | Yes |
| `sdk-release` | Yes | No | No | Yes |
| `occ-debug` | Yes | Yes | No | Yes |
| `occ-release` | Yes | Yes | No | Yes |
| `wave-finder-debug` | Yes | No | Yes | Yes |
| `all-apps-debug` | Yes | Yes | Yes | Yes |
| `ci-ubuntu` | Yes | No | No | Yes |
| `ci-macos` | Yes | Yes | Yes | Yes |

---

## Documentation Status Vocabulary

All ORP and OCC documents should use one of these status values:

| Status | Meaning | Example |
|--------|---------|---------|
| `Authoritative` | Current source of truth, actively maintained | Architecture decisions, API contracts |
| `Complete` | Finished work, still accurate | Sprint reports, implementation docs |
| `Draft` | Work in progress, subject to change | Planning documents |
| `Superseded` | Replaced by newer document (link to replacement) | Old plans after revision |
| `Historical` | Accurate at time of writing, context has changed | Old assessments |
| `Reference` | Stable reference material (lookup tables, guides) | API index, migration guides |

Documents in `archive/` subdirectories are implicitly `Historical` or `Superseded`.

---

## Key Documentation Cross-References

| Topic | ORP Doc | OCC Doc | Source |
|-------|---------|---------|--------|
| SDK Integration | ORP068 | OCC096, OCC110 | `include/orpheus/` |
| Session Format | ORP068 | OCC097 | `Source/Session/` |
| UI Components | - | OCC098 | `Source/ClipGrid/`, `Source/UI/` |
| Performance | ORP068 (M2) | OCC100 | `include/orpheus/performance_monitor.h` |
| Build System | ORP124 (this) | - | `CMakeLists.txt`, `CMakePresets.json` |
| Module Extraction | ORP124 (this) | - | `packages/occ-app-platform/` |
| shmui Integration | ORP119 | - | `packages/shmui-juce/` |
