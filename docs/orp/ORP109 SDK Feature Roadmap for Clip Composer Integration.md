# ORP109 - SDK Feature Roadmap for Clip Composer Integration

**Created:** 2025-11-11
**Status:** Planning
**Priority:** Strategic Roadmap
**OCC Version Context:** v0.2.0 complete, planning for >v0.2.0

---

## Executive Summary

This document identifies SDK features that would most benefit the Orpheus Clip Composer application build. Analysis is based on:

- OCC product vision (OCC021) and MVP plan (OCC026)
- Current SDK API surface (transport_controller.h, routing_matrix.h)
- OCC implementation patterns (OCC096 SDK Integration Patterns)
- User feedback from v0.2.0 release (OCC102)

**Total Features Identified:** 11
**Priority Breakdown:** 4 High, 4 Medium, 3 Low
**Critical Path:** Routing Matrix + Audio Device Selection (block MVP milestone)

---

## I. Gap Analysis

### Current SDK Capabilities (v1.0.0-rc.1)

**Transport Layer:**

- ✅ Clip playback control (start/stop/restart/seek)
- ✅ Metadata management (trim, fade, gain, loop)
- ✅ Sample-accurate timing
- ✅ Thread-safe command processing
- ✅ Event callbacks (started, stopped, looped, restarted, seeked)

**Audio I/O:**

- ✅ Dummy driver (testing)
- ✅ CoreAudio driver (macOS)
- ⚠️ ASIO/WASAPI drivers (exist but limited device control)
- ✅ WAV/AIFF file reading (libsndfile)

**Missing for OCC:**

- ❌ Routing matrix (4 Clip Groups → Master)
- ❌ Runtime audio device enumeration/selection
- ❌ Performance monitoring API
- ❌ Waveform pre-processing for UI
- ❌ Scene/preset system
- ❌ Cue points/markers
- ❌ Multi-channel routing (>stereo)
- ❌ MIDI control surface support
- ❌ Timecode sync (LTC/MTC)
- ❌ Extended audio formats (MP3/AAC/FLAC/OGG)
- ❌ Network audio (AES67/Dante)

---

## II. Feature Specifications

### HIGH PRIORITY (MVP Blockers)

#### Feature 1: Routing Matrix API

**Requirement:** OCC design calls for 4 Clip Groups with per-group gain/mute/solo controls, routing to Master bus (OCC021, OCC026 Month 4 milestone).

**Current Gap:** No SDK routing API exists. OCC096 shows placeholder `routingMatrix->setClipGroup()`.

**Proposed API:**

```cpp
/// Routing matrix for multi-group mixing
class IRoutingMatrix {
public:
  /// Assign clip to one of 4 Clip Groups (0-3)
  /// @param handle Clip handle
  /// @param groupIndex Clip Group index (0-3, or 255 for "no group")
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError assignClipToGroup(ClipHandle handle, uint8_t groupIndex) = 0;

  /// Set gain for entire Clip Group
  /// @param groupIndex Group index (0-3)
  /// @param gainDb Gain in decibels (-60 to +12 dB)
  /// @return SessionGraphError::OK on success
  /// @note Gain changes are smoothed over 10ms to prevent clicks
  virtual SessionGraphError setGroupGain(uint8_t groupIndex, float gainDb) = 0;

  /// Mute/unmute Clip Group
  /// @param groupIndex Group index (0-3)
  /// @param muted true = mute, false = unmute
  virtual SessionGraphError setGroupMute(uint8_t groupIndex, bool muted) = 0;

  /// Solo Clip Group (mutes all other groups)
  /// @param groupIndex Group index (0-3)
  /// @param soloed true = solo this group, false = unsolo
  virtual SessionGraphError setGroupSolo(uint8_t groupIndex, bool soloed) = 0;

  /// Enable/disable routing of group to master bus
  /// @param groupIndex Group index (0-3)
  /// @param enabled true = route to master, false = disable
  virtual SessionGraphError routeGroupToMaster(uint8_t groupIndex, bool enabled) = 0;

  /// Get current group assignment for clip
  /// @param handle Clip handle
  /// @return Group index (0-3), or 255 if not assigned
  virtual uint8_t getClipGroup(ClipHandle handle) const = 0;

  /// Get current group gain
  /// @param groupIndex Group index (0-3)
  /// @return Gain in dB, or 0.0 if group not initialized
  virtual float getGroupGain(uint8_t groupIndex) const = 0;

  /// Query if group is muted
  virtual bool isGroupMuted(uint8_t groupIndex) const = 0;

  /// Query if group is soloed
  virtual bool isGroupSoloed(uint8_t groupIndex) const = 0;
};

/// Create routing matrix instance
std::unique_ptr<IRoutingMatrix> createRoutingMatrix(
  core::SessionGraph* sessionGraph,
  uint32_t sampleRate
);
```

**Implementation Notes:**

- Lock-free updates (atomic operations for group state)
- Gain smoothing (10ms ramp) to prevent clicks/pops
- Sample-accurate mute/solo (no mid-buffer discontinuities)
- Solo logic: When any group is soloed, all non-soloed groups are muted
- Thread-safe: UI thread can change routing while audio thread processes

**Testing Requirements:**

- Unit tests: Group assignment, gain smoothing, mute/solo logic, edge cases
- Integration test: 16 clips across 4 groups, verify independent gain control
- Stress test: Rapid group changes during playback (no dropouts)

**OCC Integration:**

- `RoutingPanel` UI component binds to these methods
- Session JSON includes `"clipGroup": 0-3` per clip
- Real-time metering per group (future enhancement)

**Estimated Effort:** 3-5 days

- Header/implementation: 2 days
- Unit tests: 1 day
- Integration testing: 1 day
- Documentation: 0.5 day

---

#### Feature 2: Audio Driver Enumeration & Selection

**Requirement:** OCC102 lists "audio device selection UI" as Issue #1 from v0.2.0 user feedback. Currently requires manual config file editing.

**Current Gap:** SDK uses `createDummyAudioDriver()` or hardcoded CoreAudio/ASIO. No runtime device enumeration or selection.

**Proposed API:**

```cpp
/// Audio device information
struct AudioDeviceInfo {
  std::string deviceId;        ///< Unique device identifier
  std::string name;            ///< Human-readable name
  std::string driverType;      ///< "CoreAudio", "ASIO", "WASAPI", "ALSA"
  uint32_t minChannels;        ///< Minimum output channels
  uint32_t maxChannels;        ///< Maximum output channels
  std::vector<uint32_t> supportedSampleRates;  ///< e.g., {44100, 48000, 96000}
  std::vector<uint32_t> supportedBufferSizes;  ///< e.g., {128, 256, 512, 1024}
  bool isDefaultDevice;        ///< true if system default
};

/// Audio driver manager for device enumeration and selection
class IAudioDriverManager {
public:
  /// Enumerate all available audio devices
  /// @return Vector of device info structures
  /// @note This may block briefly (10-100ms) while querying hardware
  virtual std::vector<AudioDeviceInfo> enumerateDevices() = 0;

  /// Get detailed information about specific device
  /// @param deviceId Device identifier from enumerateDevices()
  /// @return Device info, or std::nullopt if device not found
  virtual std::optional<AudioDeviceInfo> getDeviceInfo(const std::string& deviceId) = 0;

  /// Set active audio device (hot-swap)
  /// @param deviceId Device identifier
  /// @param sampleRate Desired sample rate (must be in supportedSampleRates)
  /// @param bufferSize Desired buffer size (must be in supportedBufferSizes)
  /// @return SessionGraphError::OK on success
  /// @note This stops playback, switches device, restarts audio thread
  /// @warning May cause brief audio dropout (~100ms)
  virtual SessionGraphError setActiveDevice(
    const std::string& deviceId,
    uint32_t sampleRate,
    uint32_t bufferSize
  ) = 0;

  /// Get currently active device
  /// @return Device ID, or std::nullopt if no device active
  virtual std::optional<std::string> getCurrentDevice() const = 0;

  /// Get current sample rate
  virtual uint32_t getCurrentSampleRate() const = 0;

  /// Get current buffer size
  virtual uint32_t getCurrentBufferSize() const = 0;

  /// Register callback for device changes (hot-plug events)
  /// @param callback Function called when devices added/removed
  virtual void setDeviceChangeCallback(std::function<void()> callback) = 0;
};

/// Create driver manager instance
std::unique_ptr<IAudioDriverManager> createAudioDriverManager();
```

**Implementation Notes:**

- Platform-specific implementations:
  - **macOS:** CoreAudio device enumeration via `AudioObjectGetPropertyData`
  - **Windows:** ASIO SDK device enumeration, WASAPI `IMMDeviceEnumerator`
  - **Linux:** ALSA `snd_device_name_hint`, PulseAudio device list
- Hot-swap logic:
  1. Fade out all clips (10ms)
  2. Stop audio callback
  3. Close current driver
  4. Open new driver with specified settings
  5. Restart audio callback
  6. Notify OCC via callback
- Device change detection (USB audio interfaces plugged/unplugged)

**Testing Requirements:**

- Unit tests: Device enumeration, info queries, error handling
- Integration test: Switch devices mid-playback, verify no crashes
- Manual test: Plug/unplug USB audio interface, verify callback fires

**OCC Integration:**

- `AudioDevicePanel` UI component (dropdown list of devices)
- Settings persistence in `preferences.json`
- User flow: Settings → Audio → Select Device → Apply

**Estimated Effort:** 5-7 days

- Header/implementation: 3 days
- Platform-specific code (CoreAudio/ASIO/WASAPI): 2 days
- Unit tests: 1 day
- Integration testing: 1 day
- Documentation: 0.5 day

---

#### Feature 3: Performance Monitoring API

**Requirement:** OCC needs CPU/latency display (OCC100 Performance Requirements). Currently OCC must measure this manually.

**Current Gap:** No SDK performance metrics API.

**Proposed API:**

```cpp
/// Performance metrics for audio processing
struct PerformanceMetrics {
  float cpuUsagePercent;       ///< CPU usage (0-100%)
  float latencyMs;             ///< Round-trip latency in milliseconds
  uint32_t bufferUnderrunCount; ///< Total dropout count since start
  uint32_t activeClipCount;    ///< Currently playing clips
  uint64_t totalSamplesProcessed; ///< Lifetime sample count
  double uptimeSeconds;        ///< Time since audio thread started
};

/// Performance monitor for diagnostics and metering
class IPerformanceMonitor {
public:
  /// Get current performance metrics
  /// @return Metrics structure (atomic snapshot)
  /// @note Thread-safe, <100 CPU cycles
  virtual PerformanceMetrics getMetrics() const = 0;

  /// Reset buffer underrun counter
  virtual void resetUnderrunCount() = 0;

  /// Get peak CPU usage since last reset
  /// @return Peak CPU % (useful for worst-case profiling)
  virtual float getPeakCpuUsage() const = 0;

  /// Reset peak CPU usage tracker
  virtual void resetPeakCpuUsage() = 0;

  /// Get audio callback timing histogram
  /// @return Vector of {bucketMs, count} pairs
  /// @note Useful for profiling callback jitter
  virtual std::vector<std::pair<float, uint32_t>> getCallbackTimingHistogram() const = 0;
};

/// Create performance monitor instance
std::unique_ptr<IPerformanceMonitor> createPerformanceMonitor(
  core::SessionGraph* sessionGraph
);
```

**Implementation Notes:**

- CPU measurement:
  - Audio thread: Measure callback duration with `std::chrono::high_resolution_clock`
  - Calculate `cpuUsage = (callbackDuration / bufferDuration) * 100`
  - Use exponential moving average (α = 0.1) to smooth readings
- Latency calculation:
  - Input latency: Driver-reported input buffer size
  - Processing latency: SDK internal buffer size
  - Output latency: Driver-reported output buffer size
  - Total: `latency = (inputBuf + processingBuf + outputBuf) / sampleRate * 1000`
- Atomic operations for all counters (no locks in audio thread)

**Testing Requirements:**

- Unit tests: Metric calculation accuracy, atomic safety
- Integration test: Verify CPU usage matches external profiler (±5%)
- Stress test: 16 clips, verify metrics update correctly

**OCC Integration:**

- `PerformanceMonitor` UI component (gauges, graphs)
- 30 Hz polling from UI (via timer callback)
- Warning indicators (CPU >80%, underruns >0)

**Estimated Effort:** 2-3 days

- Header/implementation: 1.5 days
- Unit tests: 0.5 day
- Integration testing: 0.5 day
- Documentation: 0.5 day

---

#### Feature 4: Waveform Data Pre-Processing

**Requirement:** OCC needs fast waveform rendering for Edit Dialog and clip grid thumbnails (OCC098 UI Components).

**Current Gap:** `IAudioFileReader` reads samples sequentially, but no downsampling/peak extraction API for efficient UI rendering.

**Proposed API:**

```cpp
/// Waveform data for UI rendering
struct WaveformData {
  std::vector<float> minPeaks;  ///< Minimum sample values per pixel
  std::vector<float> maxPeaks;  ///< Maximum sample values per pixel
  uint32_t pixelWidth;          ///< Number of pixels (samples per pixel varies)
  uint32_t channelIndex;        ///< Channel this data represents
  int64_t startSample;          ///< First sample in range
  int64_t endSample;            ///< Last sample in range
};

/// Extended audio file reader with waveform pre-processing
class IAudioFileReaderExtended : public IAudioFileReader {
public:
  /// Generate waveform data for UI rendering
  /// @param startSample Start of range (0-based)
  /// @param endSample End of range (exclusive)
  /// @param pixelWidth Target width in pixels
  /// @param channelIndex Channel to extract (0 = left, 1 = right, etc.)
  /// @return Waveform data (min/max peaks per pixel)
  /// @note This may block (10-100ms for long files), call on background thread
  virtual WaveformData getWaveformData(
    int64_t startSample,
    int64_t endSample,
    uint32_t pixelWidth,
    uint32_t channelIndex
  ) = 0;

  /// Get peak level for entire file (for normalization)
  /// @param channelIndex Channel to analyze (0 = left, 1 = right, etc.)
  /// @return Peak absolute value (0.0 to 1.0+)
  virtual float getPeakLevel(uint32_t channelIndex) = 0;

  /// Pre-compute waveform data on background thread
  /// @param callback Called when pre-processing complete
  /// @note This spawns background thread, returns immediately
  virtual void precomputeWaveformAsync(std::function<void()> callback) = 0;
};

/// Create extended audio file reader
std::unique_ptr<IAudioFileReaderExtended> createAudioFileReaderExtended();
```

**Implementation Notes:**

- Downsampling algorithm:
  - Calculate `samplesPerPixel = (endSample - startSample) / pixelWidth`
  - For each pixel, read `samplesPerPixel` samples
  - Find min/max in that range
  - Store in `minPeaks[pixel]` and `maxPeaks[pixel]`
- Multi-threaded pre-processing:
  - Load entire file into memory buffer (if <100MB)
  - Process in parallel chunks (one thread per channel)
  - Cache results for fast subsequent queries
- Memory optimization:
  - For large files (>100MB), use streaming reads
  - Generate waveform at multiple resolutions (LOD pyramid)

**Testing Requirements:**

- Unit tests: Downsampling accuracy, peak detection, edge cases (short files)
- Performance test: 10-minute WAV → 800px waveform in <100ms
- Visual test: Render waveform, compare to reference (Audacity/Reaper)

**OCC Integration:**

- `WaveformDisplay` component calls `getWaveformData()` on background thread
- Cache waveform data in `ClipMetadata` structure
- Real-time zoom: Re-query with different `pixelWidth`

**Estimated Effort:** 3-4 days

- Header/implementation: 2 days
- Multi-threading: 1 day
- Unit tests: 0.5 day
- Integration testing: 0.5 day
- Documentation: 0.5 day

---

### MEDIUM PRIORITY (Post-MVP Enhancements)

#### Feature 5: Scene/Preset System

**Requirement:** Theater/broadcast users need to save/recall button states (which clips loaded on which buttons, ready to trigger).

**Current Gap:** OCC handles session save/load in JSON (OCC097), but SDK has no scene snapshot API.

**Proposed API:**

```cpp
/// Scene snapshot (lightweight metadata)
struct SceneSnapshot {
  std::string sceneId;         ///< Unique identifier
  std::string name;            ///< User-friendly name
  uint64_t timestamp;          ///< Creation time (Unix epoch)
  std::vector<ClipHandle> assignedClips;  ///< Clips loaded per button
  std::vector<uint8_t> clipGroups;        ///< Group assignment per clip
  std::vector<float> groupGains;          ///< Gain per Clip Group
  // Note: Does not store audio file data, only metadata
};

/// Scene manager for preset workflows
class ISceneManager {
public:
  /// Capture current session state as scene
  /// @param name User-friendly scene name
  /// @return Scene ID (UUID)
  virtual std::string captureScene(const std::string& name) = 0;

  /// Recall scene (restore button states, routing)
  /// @param sceneId Scene identifier from captureScene()
  /// @return SessionGraphError::OK on success
  /// @note This stops all playback, reconfigures session, does NOT reload audio files
  virtual SessionGraphError recallScene(const std::string& sceneId) = 0;

  /// List all saved scenes
  /// @return Vector of scene snapshots
  virtual std::vector<SceneSnapshot> listScenes() const = 0;

  /// Delete scene
  /// @param sceneId Scene identifier
  virtual SessionGraphError deleteScene(const std::string& sceneId) = 0;

  /// Export scene to JSON file
  /// @param sceneId Scene identifier
  /// @param filePath Output file path
  virtual SessionGraphError exportScene(const std::string& sceneId, const std::string& filePath) = 0;

  /// Import scene from JSON file
  /// @param filePath Input file path
  /// @return Scene ID of imported scene
  virtual std::string importScene(const std::string& filePath) = 0;
};

/// Create scene manager instance
std::unique_ptr<ISceneManager> createSceneManager(core::SessionGraph* sessionGraph);
```

**OCC Integration:**

- "Scenes" menu in main menu bar
- Scene selector dropdown in toolbar
- Hotkey support (F1-F12 → recall scene 1-12)

**Estimated Effort:** 4-5 days

---

#### Feature 6: Cue Points / Markers

**Requirement:** Complex shows need in-clip markers (e.g., "vocal starts at 5s", "chorus at 30s").

**Current Gap:** Clips only have trim IN/OUT. No internal markers.

**Proposed API:**

```cpp
/// Cue point within a clip
struct CuePoint {
  int64_t position;            ///< Position in samples (file offset)
  std::string name;            ///< User label (e.g., "Verse 1")
  uint32_t color;              ///< RGBA color for UI (0xRRGGBBAA)
};

/// Extend ClipMetadata with cue points
struct ClipMetadataExtended : public ClipMetadata {
  std::vector<CuePoint> cuePoints;  ///< Ordered list of markers
};

/// Extend ITransportController with cue point methods
class ITransportControllerExtended : public ITransportController {
public:
  /// Add cue point to clip
  /// @param handle Clip handle
  /// @param position Position in samples (0-based file offset)
  /// @param name User label
  /// @param color RGBA color
  /// @return Cue point index, or -1 on error
  virtual int addCuePoint(ClipHandle handle, int64_t position, const std::string& name, uint32_t color) = 0;

  /// Get all cue points for clip
  /// @param handle Clip handle
  /// @return Vector of cue points (ordered by position)
  virtual std::vector<CuePoint> getCuePoints(ClipHandle handle) const = 0;

  /// Seek to specific cue point
  /// @param handle Clip handle
  /// @param cueIndex Index in cue points array
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError seekToCuePoint(ClipHandle handle, uint32_t cueIndex) = 0;

  /// Remove cue point
  /// @param handle Clip handle
  /// @param cueIndex Index to remove
  virtual SessionGraphError removeCuePoint(ClipHandle handle, uint32_t cueIndex) = 0;
};
```

**OCC Integration:**

- Edit Dialog: Click waveform with Ctrl+M to add marker
- Waveform display: Draw vertical lines at cue points
- Keyboard shortcuts: Cmd+1-9 → seek to cue 1-9

**Estimated Effort:** 3-4 days

---

#### Feature 7: Multi-Channel Routing (Beyond Stereo)

**Requirement:** Professional audio interfaces have 8-32 channels. OCC currently limited to stereo output.

**Current Gap:** SDK processes stereo (2 channels) only.

**Proposed API:**

```cpp
/// Extended routing matrix for multi-channel workflows
class IRoutingMatrixMultiChannel : public IRoutingMatrix {
public:
  /// Set output bus for clip (beyond stereo)
  /// @param handle Clip handle
  /// @param outputBus Bus index (0 = channels 1-2, 1 = channels 3-4, etc.)
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError setClipOutputBus(ClipHandle handle, uint8_t outputBus) = 0;

  /// Map clip channel to output channel
  /// @param handle Clip handle
  /// @param clipChannel Clip channel (0 = L, 1 = R for stereo clip)
  /// @param outputChannel Output channel (0-31)
  /// @return SessionGraphError::OK on success
  virtual SessionGraphError mapChannels(ClipHandle handle, uint8_t clipChannel, uint8_t outputChannel) = 0;

  /// Get clip output bus
  virtual uint8_t getClipOutputBus(ClipHandle handle) const = 0;
};
```

**OCC Integration:**

- Advanced routing panel (matrix view)
- Per-clip output assignment (e.g., clip → channels 5-6)

**Estimated Effort:** 5-6 days

---

#### Feature 8: MIDI Control Surface Support

**Requirement:** OCC roadmap mentions "remote control" (OCC021). Hardware controllers are standard in broadcast.

**Current Gap:** SDK has no MIDI input handling.

**Proposed API:**

```cpp
/// MIDI device information
struct MidiDeviceInfo {
  std::string deviceId;
  std::string name;
  bool isInput;
  bool isOutput;
};

/// MIDI controller interface
class IMidiController {
public:
  /// Enumerate MIDI devices
  virtual std::vector<MidiDeviceInfo> enumerateMidiDevices() = 0;

  /// Open MIDI input device
  /// @param deviceId Device identifier
  virtual SessionGraphError openMidiInput(const std::string& deviceId) = 0;

  /// Bind MIDI note to clip trigger
  /// @param noteNumber MIDI note (0-127)
  /// @param handle Clip handle
  virtual SessionGraphError bindMidiNote(uint8_t noteNumber, ClipHandle handle) = 0;

  /// Bind MIDI CC to parameter
  /// @param ccNumber MIDI CC (0-127)
  /// @param parameter Parameter type (gain, fade, etc.)
  /// @param handle Clip handle
  virtual SessionGraphError bindMidiCC(uint8_t ccNumber, const std::string& parameter, ClipHandle handle) = 0;

  /// Register callback for MIDI events
  virtual void setMidiCallback(std::function<void(uint8_t status, uint8_t data1, uint8_t data2)> callback) = 0;
};
```

**OCC Integration:**

- MIDI Learn mode (click button, press MIDI key → bind)
- Settings panel: MIDI device selection, binding list

**Estimated Effort:** 6-8 days (cross-platform MIDI I/O is complex)

---

### LOW PRIORITY (Future / V2.0+)

#### Feature 9: Timecode Sync (LTC/MTC)

**Requirement:** Broadcast environments sync to timecode (e.g., lock to video playback).

**Proposed API:**

```cpp
/// Timecode format
enum class TimecodeFormat {
  LTC_24fps,
  LTC_25fps,
  LTC_30fps,
  MTC_24fps,
  MTC_25fps,
  MTC_30fps
};

/// Timecode sync interface
class ITimecodeSync {
public:
  /// Enable timecode input
  /// @param format Timecode format
  /// @param audioInputChannel Audio channel for LTC (0-based)
  virtual SessionGraphError enableTimecode(TimecodeFormat format, uint8_t audioInputChannel) = 0;

  /// Get current timecode position
  /// @return Timecode in HH:MM:SS:FF format
  virtual std::string getCurrentTimecode() const = 0;

  /// Lock transport to timecode
  /// @param enabled true = lock, false = freewheel
  virtual void setTimecodeLock(bool enabled) = 0;
};
```

**Estimated Effort:** 8-10 days

---

#### Feature 10: Extended Audio Format Support

**Requirement:** Users want MP3/AAC/FLAC/OGG support. SDK currently uses libsndfile (WAV/AIFF only).

**Implementation Notes:**

- Integrate additional codec libraries:
  - **FLAC:** libFLAC (BSD license, deterministic)
  - **OGG Vorbis:** libvorbis (BSD license, deterministic with fixed-point decode)
  - **MP3:** mpg123 (LGPL license, patents expired)
  - **AAC:** fdkaac (commercial license required for distribution)
- Maintain deterministic decode behavior (sample-accurate, bit-identical)
- Add format detection logic to `IAudioFileReader::open()`

**Estimated Effort:** 6-8 days (licensing research + integration)

---

#### Feature 11: Network Audio (AES67/Dante)

**Requirement:** OCC021 mentions "network audio" as differentiator. Broadcast studios use Dante/AES67.

**Implementation Notes:**

- **HUGE SCOPE** - requires:
  - AES67/Ravenna protocol implementation (PTP clock sync)
  - Audinate Dante SDK integration (commercial licensing ~$10k-50k)
  - Sub-millisecond jitter handling
  - Network stack integration (UDP multicast)
- **Licensing:** Dante requires Audinate SDK license (commercial agreement)
- **Alternative:** Open-source AES67 (Merging Ravenna protocol, no licensing)

**Recommendation:** Defer to v2.0+ or commercial tier only

**Estimated Effort:** 30-60 days (full-time engineer, 2-3 months)

---

## III. Recommended Implementation Roadmap

### Phase 1: Critical Path (OCC >v0.2.0)

**Goal:** Unblock OCC MVP milestone (Month 4 equivalent from OCC026)

**Features:**

1. **Routing Matrix API** (5 days)
2. **Audio Device Enumeration & Selection** (7 days)

**Total Effort:** 12 days (2.5 weeks)

**Acceptance Criteria:**

- OCC can route clips to 4 groups with independent gain
- OCC users can select audio interface from UI (no config files)
- All unit tests passing (routing + device management)
- OCC integration complete (RoutingPanel + AudioDevicePanel components)

---

### Phase 2: UX Enhancements (OCC >v0.2.0 + 1 release)

**Goal:** Improve user experience with diagnostics and waveform polish

**Features:** 3. **Performance Monitoring API** (3 days) 4. **Waveform Pre-Processing API** (4 days)

**Total Effort:** 7 days (1.5 weeks)

**Acceptance Criteria:**

- OCC displays real-time CPU/latency metrics
- Edit Dialog waveforms render in <100ms
- Performance overhead <2% (monitoring should be cheap)

---

### Phase 3: Power User Features (OCC >v0.2.0 + 2-3 releases)

**Goal:** Enable advanced workflows (scenes, cue points, multi-channel)

**Features:** 5. **Scene/Preset System** (5 days) 6. **Cue Points/Markers** (4 days) 7. **Multi-Channel Routing** (6 days)

**Total Effort:** 15 days (3 weeks)

**Acceptance Criteria:**

- Users can save/recall scene presets
- Edit Dialog supports in-clip markers
- Advanced users can route to 8+ channel interfaces

---

### Phase 4: Integration & Control (OCC v1.0+)

**Goal:** Hardware integration and professional workflows

**Features:** 8. **MIDI Control Surface Support** (8 days)

**Total Effort:** 8 days (1.5 weeks)

**Acceptance Criteria:**

- Launchpad/APC40 integration working
- MIDI Learn mode functional
- Latency <5ms (MIDI note → clip trigger)

---

### Phase 5: Future/Advanced (OCC v2.0+)

**Features:** 9. **Timecode Sync** (10 days) 10. **Extended Audio Formats** (8 days) 11. **Network Audio** (60 days)

**Total Effort:** 78 days (15+ weeks)

**Note:** These features require significant research, licensing, and validation. Consider for commercial tier or enterprise version.

---

## IV. Implementation Priorities by Dependency

### Critical Path Dependencies

```
Routing Matrix (Feature 1)
  ↓
OCC Month 4 Milestone (16-clip demo with routing)
  ↓
OCC MVP Complete (Month 6)
```

**Blocker:** Routing Matrix must be complete before OCC can ship MVP with routing panel.

```
Audio Device Selection (Feature 2)
  ↓
OCC User Feedback Issue #1 (from v0.2.0)
  ↓
Professional User Adoption
```

**Blocker:** Users cannot use OCC professionally without runtime device selection (config files are not acceptable).

### Enhancement Dependencies

```
Performance Monitoring (Feature 3)
  ↓
OCC Diagnostics Panel (OCC100 Performance Requirements)
  ↓
Beta Testing & Validation
```

**Nice-to-Have:** Diagnostics help with beta testing, but not MVP blocker.

```
Waveform Pre-Processing (Feature 4)
  ↓
Edit Dialog UX Polish
  ↓
User Experience Improvement
```

**Nice-to-Have:** Waveforms work now (OCC handles rendering), but SDK can do it faster.

---

## V. Resource Allocation

### Effort Summary

| Priority  | Features | Total Days | Calendar Weeks |
| --------- | -------- | ---------- | -------------- |
| High      | 1-4      | 19         | ~4 weeks       |
| Medium    | 5-7      | 15         | ~3 weeks       |
| Low       | 8-11     | 86         | ~17 weeks      |
| **Total** | **11**   | **120**    | **24 weeks**   |

### Recommended Staffing

**Single Developer (part-time on SDK features):**

- Phase 1 (Critical): 2.5 weeks
- Phase 2 (UX): 1.5 weeks
- Phase 3 (Power Users): 3 weeks
- **Subtotal:** 7 weeks (Critical + UX + Power Users)

**Full-time SDK Engineer:**

- All phases: 24 weeks (~6 months)
- Includes Low priority features (MIDI, timecode, network audio)

**Recommendation:** Prioritize Phase 1-2 (4 weeks total) to unblock OCC MVP, defer Phase 3-5 to post-launch.

---

## VI. Testing Strategy

### Unit Testing

**Per Feature:**

- API correctness (valid inputs, edge cases, error handling)
- Thread safety (concurrent access, atomic operations)
- Performance (latency <100 CPU cycles for query methods)

**Coverage Target:** 80%+ for new SDK features

### Integration Testing

**Multi-Feature Scenarios:**

- Routing + Device Selection: Switch audio device mid-playback with 4 active groups
- Performance Monitoring + 16 clips: Verify CPU metrics accurate under load
- Waveform Pre-Processing + Large Files: 60-minute WAV → waveform in <200ms

### OCC Integration Testing

**End-to-End Workflows:**

- Load session → Assign clips to groups → Adjust group gains → Verify audio correct
- Change audio device → Verify playback continues (brief dropout acceptable)
- Monitor CPU during 16-clip stress test → Verify metrics match external profiler

### Acceptance Testing

**User Acceptance (Beta Testers):**

- Professional users test routing workflows (theater show setup)
- Audio device selection tested on variety of interfaces (USB, Thunderbolt, PCIe)
- Performance monitoring validated (CPU readings match Activity Monitor/Task Manager)

---

## VII. Documentation Requirements

### API Documentation

**For Each Feature:**

- Doxygen comments for all public methods
- Usage examples in header files
- Thread safety guarantees documented

### Migration Guides

**SDK API Changes:**

- Add to `docs/MIGRATION_v0_to_v1.md` (or new version file)
- Breaking changes highlighted
- Code examples (before/after)

### OCC Integration Guides

**For OCC Developers:**

- Update `apps/clip-composer/docs/occ/OCC096.md` (SDK Integration Patterns)
- Add routing examples, device selection examples
- Update session JSON schema (OCC097) with new fields

---

## VIII. Risk Assessment

### Technical Risks

| Risk                                              | Probability | Impact | Mitigation                                                         |
| ------------------------------------------------- | ----------- | ------ | ------------------------------------------------------------------ |
| Platform-specific audio I/O bugs (ASIO/CoreAudio) | Medium      | High   | Comprehensive testing on target hardware, fallback to dummy driver |
| Performance regression (routing overhead)         | Low         | Medium | Profile early, maintain <5% CPU overhead target                    |
| Thread safety bugs (race conditions)              | Medium      | High   | Use lock-free primitives, AddressSanitizer/ThreadSanitizer in CI   |
| Device hot-swap instability                       | Medium      | Medium | Graceful degradation (stop playback if swap fails)                 |

### Schedule Risks

| Risk                                               | Probability | Impact | Mitigation                                                   |
| -------------------------------------------------- | ----------- | ------ | ------------------------------------------------------------ |
| Feature creep (scope expansion)                    | High        | Medium | Strict prioritization (Critical path only for Phase 1)       |
| Underestimated effort (MIDI/timecode)              | Medium      | Low    | Low priority features can be deferred to v2.0                |
| OCC dependency blocking (waiting for SDK features) | Low         | High   | Parallel development (OCC uses placeholders until SDK ready) |

### Business Risks

| Risk                                  | Probability | Impact | Mitigation                                                                   |
| ------------------------------------- | ----------- | ------ | ---------------------------------------------------------------------------- |
| Licensing costs (Dante SDK)           | Medium      | High   | Use AES67 open-source alternative, or defer network audio to commercial tier |
| User adoption slow (missing features) | Low         | Medium | Phase 1-2 addresses top user requests (routing + device selection)           |

---

## IX. Success Metrics

### Implementation Phase

**For Each Feature:**

- [ ] Public API defined in header file (Doxygen comments complete)
- [ ] Implementation complete (compiles without warnings)
- [ ] Unit tests passing (80%+ coverage)
- [ ] Integration tests passing (multi-feature scenarios)
- [ ] OCC integration complete (UI components functional)
- [ ] Documentation updated (API docs, migration guide)

### OCC Integration Phase

**For OCC >v0.2.0 Release:**

- [ ] Routing Panel functional (4 groups, gain/mute/solo controls)
- [ ] Audio Device Panel functional (device list, hot-swap working)
- [ ] Performance Monitor panel functional (CPU/latency display)
- [ ] Edit Dialog waveforms render fast (<100ms for 10-minute file)

### User Acceptance

**Beta Testing Results:**

- [ ] Zero critical bugs reported (crashes, data loss)
- [ ] <3 minor bugs per feature (cosmetic, UX polish)
- [ ] User satisfaction: "Routing workflow is professional-grade" (qualitative feedback)
- [ ] Performance targets met: <30% CPU with 16 clips + 4 groups active

---

## X. Next Steps

### Immediate Actions

1. **Review & Approve Roadmap** (User decision)
   - Confirm priorities (High/Medium/Low)
   - Adjust schedule based on OCC release timeline
   - Allocate engineering resources

2. **Create Feature Specifications** (SDK team)
   - Detailed design docs for Feature 1-4 (High priority)
   - API review (C++ team feedback)
   - Finalize header interfaces

3. **Set Up Development Branch** (SDK team)
   - Branch: `feature/occ-routing-matrix` (Feature 1)
   - Branch: `feature/occ-device-selection` (Feature 2)
   - Parallel development if resources allow

### Week 1-2: Feature 1 (Routing Matrix)

- Day 1-2: Header design, API review
- Day 3-5: Implementation (lock-free routing, gain smoothing)
- Day 6-7: Unit tests
- Day 8-10: Integration testing
- Day 11-12: OCC integration (RoutingPanel component)

### Week 3-4: Feature 2 (Audio Device Selection)

- Day 1-3: Header design, platform research (CoreAudio/ASIO/WASAPI)
- Day 4-7: Implementation (device enumeration, hot-swap)
- Day 8-9: Unit tests
- Day 10-12: Integration testing
- Day 13-14: OCC integration (AudioDevicePanel component)

### Week 5: Testing & Documentation

- Day 1-2: End-to-end OCC testing (routing + device selection)
- Day 3-4: Documentation updates (API docs, migration guide)
- Day 5: Code review, merge to main

### Week 6+: Phase 2 (Performance + Waveform)

- Repeat process for Feature 3-4
- OCC beta release with Phase 1 features

---

## XI. Related Documents

**OCC Planning:**

- **OCC021** - Product Vision and Market Positioning
- **OCC026** - 6-Month MVP Development Plan
- **OCC102** - v0.2.0 Release & v0.2.1 Planning

**OCC Technical Reference:**

- **OCC096** - SDK Integration Patterns (code examples)
- **OCC097** - Session Format (JSON schema)
- **OCC098** - UI Components (JUCE implementations)
- **OCC099** - Testing Strategy
- **OCC100** - Performance Requirements and Optimization

**SDK Documentation:**

- **ORP068** - Implementation Plan v2.0 (Orpheus SDK × Shmui Integration)
- **ORP099** - SDK Track Phase 4 Completion and Testing
- **ORP101** - Phase 4 Completion Report
- **docs/ARCHITECTURE.md** - SDK design rationale
- **docs/API_SURFACE_INDEX.md** - Public API catalog

---

## XII. Appendix: API Surface Summary

### New Headers Required

1. `include/orpheus/routing_matrix.h` - Routing Matrix API (Feature 1)
2. `include/orpheus/audio_driver_manager.h` - Device Enumeration (Feature 2)
3. `include/orpheus/performance_monitor.h` - Performance Metrics (Feature 3)
4. `include/orpheus/audio_file_reader_extended.h` - Waveform API (Feature 4)
5. `include/orpheus/scene_manager.h` - Scene/Preset API (Feature 5)
6. `include/orpheus/midi_controller.h` - MIDI API (Feature 8)
7. `include/orpheus/timecode_sync.h` - Timecode API (Feature 9)

### New Source Files Required

1. `src/core/routing/routing_matrix.cpp` - Routing implementation
2. `src/platform/audio_drivers/driver_manager.cpp` - Device management
3. `src/core/common/performance_monitor.cpp` - Performance tracking
4. `src/core/audio_io/waveform_processor.cpp` - Waveform pre-processing
5. `src/core/session/scene_manager.cpp` - Scene snapshots
6. `src/platform/midi/midi_controller.cpp` - MIDI I/O (CoreMIDI/WinMIDI/ALSA)
7. `src/core/transport/timecode_sync.cpp` - Timecode parsing

### Test Files Required

1. `tests/routing/routing_matrix_test.cpp`
2. `tests/audio_io/driver_manager_test.cpp`
3. `tests/common/performance_monitor_test.cpp`
4. `tests/audio_io/waveform_processor_test.cpp`
5. `tests/session/scene_manager_test.cpp`
6. `tests/midi/midi_controller_test.cpp`
7. `tests/transport/timecode_sync_test.cpp`

---

**Document Version:** 1.0
**Created:** 2025-11-11
**Status:** Planning - Awaiting User Approval
**Next Review:** After OCC v0.2.0 release (user feedback integration)

---

_This roadmap is a living document and will be updated as priorities shift based on OCC user feedback and SDK development progress._
