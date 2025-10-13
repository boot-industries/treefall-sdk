# ORP070 - Orpheus Clip Composer MVP Sprint

**Document ID:** ORP070
**Version:** 1.0
**Status:** Authoritative
**Date:** October 12, 2025
**Author:** SDK Core Team
**Related Plans:** ORP069 (OCC-Aligned SDK Enhancements)

---

## Executive Summary

This sprint defines all Orpheus SDK tasks required to support the **Orpheus Clip Composer (OCC) MVP** over the next 6 months. The OCC application is currently in Month 1 implementation with UI foundation complete. This sprint ensures the SDK provides all necessary real-time audio infrastructure to enable OCC's professional soundboard capabilities.

**Sprint Goal:** Deliver production-ready SDK modules enabling OCC to achieve <5ms latency, 16 simultaneous clips, and 24/7 broadcast-safe operation.

**Timeline:** Months 1-6 (October 2025 - April 2026)
**Critical Path:** Platform audio drivers (Month 2) → Routing matrix (Month 4) → OCC MVP beta (Month 6)

---

## Table of Contents

1. [Sprint Overview](#sprint-overview)
2. [OCC Requirements Summary](#occ-requirements-summary)
3. [Current SDK Status](#current-sdk-status)
4. [Sprint Phases](#sprint-phases)
5. [Phase 1: Audio Integration (Months 1-2)](#phase-1-audio-integration-months-1-2)
6. [Phase 2: Real-Time Mixing (Months 3-4)](#phase-2-real-time-mixing-months-3-4)
7. [Phase 3: Production Hardening (Months 5-6)](#phase-3-production-hardening-months-5-6)
8. [Testing Strategy](#testing-strategy)
9. [Success Criteria](#success-criteria)
10. [Risk Mitigation](#risk-mitigation)

---

## Sprint Overview

### Objectives

1. **Complete real-time audio infrastructure** for professional soundboard workflows
2. **Enable OCC MVP development** with robust SDK foundation
3. **Maintain broadcast-safe quality** (<30% CPU, >100hr MTBF, zero audio thread allocations)
4. **Support cross-platform deployment** (macOS, Windows with identical behavior)

### Scope

**In Scope:**
- Platform audio drivers (CoreAudio, WASAPI)
- Multi-channel routing matrix (4 Clip Groups → Master)
- Performance monitoring and diagnostics
- Real-time audio mixing in transport controller
- Sample-accurate timing guarantees (±1 sample @ 48kHz)
- Lock-free UI↔Audio thread communication
- Comprehensive test coverage (unit + integration + stress)

**Out of Scope (Deferred to v1.0+):**
- ASIO driver (optional, Month 6 if time permits)
- VST3/AU plugin hosting
- Recording functionality
- Time-stretching/pitch-shifting DSP
- Network streaming
- Advanced spatial audio (VBAP)

### Dependencies

**OCC Design Documentation:** `apps/clip-composer/docs/OCC/`
- **OCC027** - API Contracts (interface specifications)
- **OCC029** - SDK Enhancement Recommendations (module requirements)
- **OCC030** - SDK Status Report (current state)

**Related Plans:**
- **ORP069** - OCC-Aligned SDK Enhancements (parent plan)
- **ROADMAP.md** - Milestone M2 timeline

---

## OCC Requirements Summary

### Functional Requirements (from OCC027)

**1. Real-Time Clip Playback**
- Start/stop 16+ simultaneous clips
- Sample-accurate triggering (±1 sample)
- Fade-out on stop (10ms default, configurable)
- Transport callbacks (clip started/stopped/looped)

**2. Audio File Support**
- Formats: WAV, AIFF, FLAC (via libsndfile)
- Sample rates: 44.1kHz, 48kHz, 96kHz
- Channels: Mono, stereo, multi-channel (up to 32)
- File integrity verification (SHA-256)

**3. Platform Audio I/O**
- CoreAudio (macOS): <10ms latency
- WASAPI (Windows): <10ms latency
- Device enumeration and configuration
- Latency reporting and buffer management

**4. Multi-Channel Routing**
- 4 Clip Groups → Master Output
- Per-clip group assignment
- Group gain control with smoothing (10ms ramp)
- Mute/solo functionality

**5. Performance Diagnostics**
- Real-time CPU usage tracking
- Buffer underrun detection
- Latency measurement
- Memory usage reporting

### Non-Functional Requirements (from OCC026)

**Performance Targets:**
- Latency: <5ms (ASIO), <10ms (CoreAudio/WASAPI)
- CPU Usage: <30% with 16 active clips (Intel i5 8th gen)
- Memory: <500MB with 960 clips loaded
- MTBF: >100 hours continuous operation

**Quality Standards:**
- Zero audio thread allocations
- Zero warnings in Debug build
- 100% test pass rate (ctest)
- AddressSanitizer/UBSan clean
- Cross-platform determinism (bit-identical audio)

---

## Current SDK Status

### ✅ Completed (Phase 0 - Foundation)

**Module 1: ITransportController** (15 unit tests + 6 integration tests passing)
- Lock-free command queue (UI → Audio thread)
- Callback system (Audio → UI thread)
- Multi-clip state management (up to 32 clips)
- Fade-out on stop (10ms configurable)
- Sample-accurate position tracking (64-bit atomic)

**Module 2: IAudioFileReader** (5 unit tests passing)
- libsndfile integration (WAV/AIFF/FLAC)
- Metadata extraction (sample rate, channels, duration)
- Direct buffer reading (streaming deferred)
- File validation (basic checks, SHA-256 stubbed)

**Module 4: IAudioDriver - Dummy** (11 unit tests passing)
- Simulated real-time audio callback
- Configurable sample rate/buffer size
- Latency reporting (basic)
- Thread-safe start/stop

**Integration Complete:**
- TransportAudioAdapter connects transport to driver
- Full pipeline verified: driver → transport → callbacks
- All 46/47 tests passing (98% coverage)

### ⏳ Pending (This Sprint)

**Platform Audio Drivers** (Month 1-2)
- CoreAudio driver (macOS)
- WASAPI driver (Windows)
- Device enumeration and configuration
- Latency optimization (<10ms)

**Audio Mixing** (Month 2)
- Real audio processing in transport controller
- Multi-clip summing (16+ simultaneous)
- Clip-to-output routing (basic, 1:1)

**Routing Matrix** (Month 3-4)
- IRoutingMatrix interface and implementation
- 4 Clip Groups with gain control
- Mute/solo functionality
- Click-free gain smoothing

**Performance Monitor** (Month 4-5)
- IPerformanceMonitor interface and implementation
- CPU usage tracking (real-time)
- Buffer underrun detection
- Memory profiling

---

## Sprint Phases

### Phase 1: Audio Integration (Months 1-2)
**Goal:** Real audio playback working on macOS and Windows

**Duration:** 8 weeks (October-November 2025)
**Priority:** CRITICAL (blocks OCC Month 2)
**Deliverables:**
1. CoreAudio driver (macOS, <10ms latency)
2. WASAPI driver (Windows, <10ms latency)
3. Real audio mixing in transport controller
4. Integration tests (16 simultaneous clips)

### Phase 2: Real-Time Mixing (Months 3-4)
**Goal:** Multi-channel routing with 4 Clip Groups

**Duration:** 8 weeks (December 2025 - January 2026)
**Priority:** HIGH (required for OCC routing panel)
**Deliverables:**
1. IRoutingMatrix interface and implementation
2. 4 Clip Groups → Master routing
3. Gain smoothing (10ms ramp, click-free)
4. Mute/solo controls

### Phase 3: Production Hardening (Months 5-6)
**Goal:** Diagnostics, optimization, and stability

**Duration:** 8 weeks (February-March 2026)
**Priority:** HIGH (required for OCC MVP beta)
**Deliverables:**
1. IPerformanceMonitor implementation
2. CPU optimization (<30% with 16 clips)
3. 24-hour stability testing (>100hr MTBF)
4. Cross-platform validation (macOS + Windows)

---

## Phase 1: Audio Integration (Months 1-2)

### Milestone 1.1: CoreAudio Driver (Weeks 1-3)

**Objective:** Implement production-ready CoreAudio driver for macOS

**Tasks:**

#### 1.1.1 CoreAudio Driver Implementation
**Estimate:** 5 days
**Files to Create:**
- `src/platform/audio_drivers/coreaudio/coreaudio_driver.h`
- `src/platform/audio_drivers/coreaudio/coreaudio_driver.cpp`
- `src/platform/audio_drivers/CMakeLists.txt`

**Requirements:**
- Implement IAudioDriver interface (see `include/orpheus/audio_driver.h`)
- Use AudioUnit API (kAudioUnitSubType_HALOutput)
- Support device enumeration (AudioObjectGetPropertyData)
- Configurable buffer size (64-1024 samples)
- Configurable sample rate (44.1kHz, 48kHz, 96kHz)
- Latency reporting (GetProperty: kAudioDevicePropertyLatency)

**Acceptance Criteria:**
- [ ] Driver initializes without errors on macOS 11+
- [ ] Audio callback fires at correct sample rate
- [ ] Measured latency <10ms @ 512 samples, 48kHz
- [ ] Device enumeration lists all available audio devices
- [ ] Hot-plug support (device changes handled gracefully)
- [ ] Zero allocations in audio callback (verified with Instruments)

#### 1.1.2 CoreAudio Driver Tests
**Estimate:** 2 days
**Files to Create:**
- `tests/audio_io/coreaudio_driver_test.cpp`

**Test Cases:**
1. Initialization success/failure (device not found)
2. Audio callback invocation (verify sample rate)
3. Device enumeration (list all devices)
4. Latency measurement (compare reported vs measured)
5. Hot-plug handling (device disconnect/reconnect)
6. Multi-channel support (stereo, 8-channel)
7. Buffer underrun recovery
8. Thread safety (start/stop from UI thread)

**Acceptance Criteria:**
- [ ] 10+ test cases passing
- [ ] Tests run on CI (macOS runner)
- [ ] No flaky tests (100 consecutive runs pass)

#### 1.1.3 CMake Integration
**Estimate:** 1 day
**Files to Modify:**
- `CMakeLists.txt` (root)
- `src/CMakeLists.txt`
- `src/platform/audio_drivers/CMakeLists.txt` (new)

**Requirements:**
- Add `ORPHEUS_ENABLE_COREAUDIO` option (default ON for macOS)
- Link CoreAudio framework (macOS only)
- Conditional compilation (`#ifdef ORPHEUS_ENABLE_COREAUDIO`)
- Install headers for driver interface

**Acceptance Criteria:**
- [ ] Builds successfully on macOS with CoreAudio enabled
- [ ] Builds successfully on Windows with CoreAudio disabled
- [ ] No warnings in Debug or Release build

---

### Milestone 1.2: WASAPI Driver (Weeks 4-6)

**Objective:** Implement production-ready WASAPI driver for Windows

**Tasks:**

#### 1.2.1 WASAPI Driver Implementation
**Estimate:** 6 days
**Files to Create:**
- `src/platform/audio_drivers/wasapi/wasapi_driver.h`
- `src/platform/audio_drivers/wasapi/wasapi_driver.cpp`

**Requirements:**
- Implement IAudioDriver interface
- Use WASAPI Exclusive Mode (lowest latency)
- Support device enumeration (IMMDeviceEnumerator)
- Configurable buffer size (64-1024 samples)
- Configurable sample rate (44.1kHz, 48kHz, 96kHz)
- Latency reporting (IAudioClient::GetStreamLatency)
- COM initialization (CoInitializeEx in audio thread)

**Acceptance Criteria:**
- [ ] Driver initializes without errors on Windows 10+
- [ ] Audio callback fires at correct sample rate
- [ ] Measured latency <10ms @ 512 samples, 48kHz
- [ ] Device enumeration lists all available audio devices
- [ ] Exclusive mode works (shared mode fallback if exclusive fails)
- [ ] Zero allocations in audio callback (verified with ETW)

#### 1.2.2 WASAPI Driver Tests
**Estimate:** 2 days
**Files to Create:**
- `tests/audio_io/wasapi_driver_test.cpp`

**Test Cases:**
1. Initialization success/failure (device not found)
2. Audio callback invocation (verify sample rate)
3. Device enumeration (list all devices)
4. Latency measurement (compare reported vs measured)
5. Exclusive mode success/failure (fallback to shared)
6. Multi-channel support (stereo, 8-channel)
7. Buffer underrun recovery
8. COM initialization (verify CoInitializeEx called)

**Acceptance Criteria:**
- [ ] 10+ test cases passing
- [ ] Tests run on CI (Windows runner)
- [ ] No flaky tests (100 consecutive runs pass)

#### 1.2.3 CMake Integration
**Estimate:** 1 day
**Files to Modify:**
- `CMakeLists.txt` (root)
- `src/platform/audio_drivers/CMakeLists.txt`

**Requirements:**
- Add `ORPHEUS_ENABLE_WASAPI` option (default ON for Windows)
- Link Windows audio libraries (Ole32.lib, Avrt.lib)
- Conditional compilation (`#ifdef ORPHEUS_ENABLE_WASAPI`)

**Acceptance Criteria:**
- [ ] Builds successfully on Windows with WASAPI enabled
- [ ] Builds successfully on macOS with WASAPI disabled
- [ ] No warnings in Debug or Release build

---

### Milestone 1.3: Real Audio Mixing (Weeks 7-8)

**Objective:** Implement real audio processing in transport controller

**Tasks:**

#### 1.3.1 Audio Mixing Implementation
**Estimate:** 4 days
**Files to Modify:**
- `src/core/transport/transport_controller.cpp`
- `src/core/transport/transport_controller.h` (if needed)

**Requirements:**
- Replace stub audio processing with real mixing
- Read audio from IAudioFileReader for each active clip
- Sum multiple clips into output buffer (simple mix: A + B)
- Apply fade-out during stop command (10ms linear ramp)
- Handle clip trim points (IN/OUT from metadata)
- Respect playback position (start offset, loop points)

**Acceptance Criteria:**
- [ ] Single clip plays real audio (not silence)
- [ ] Multiple clips sum correctly (2 clips audible simultaneously)
- [ ] Fade-out works (no clicks/pops on stop)
- [ ] Trim points respected (clip plays from IN to OUT)
- [ ] No buffer overruns or underruns during playback
- [ ] Zero allocations in processAudio() (verified with sanitizers)

#### 1.3.2 Multi-Clip Stress Test
**Estimate:** 2 days
**Files to Create:**
- `tests/transport/multi_clip_stress_test.cpp`

**Test Cases:**
1. 16 simultaneous clips (verify all audible)
2. Rapid start/stop (100 clips/second)
3. CPU usage measurement (<30% target)
4. Memory usage tracking (no leaks)
5. Long-duration test (1 hour continuous playback)

**Acceptance Criteria:**
- [ ] 16 clips play simultaneously without dropouts
- [ ] CPU usage <30% (Intel i5 8th gen equivalent)
- [ ] No memory leaks (AddressSanitizer clean)
- [ ] 1-hour test passes (no crashes or underruns)

#### 1.3.3 Integration with OCC
**Estimate:** 1 day
**Files to Modify:**
- `apps/clip-composer/Source/AudioEngine/AudioEngine.cpp` (OCC app code)

**Requirements:**
- Replace dummy driver with CoreAudio (macOS) or WASAPI (Windows)
- Verify real audio output through speakers
- Test with OCC UI (button click → audio playback)

**Acceptance Criteria:**
- [ ] OCC plays real audio on macOS (CoreAudio)
- [ ] OCC plays real audio on Windows (WASAPI)
- [ ] Transport callbacks fire correctly
- [ ] UI updates in sync with audio playback

**Phase 1 Completion Criteria:**
- [ ] All tasks above completed
- [ ] All tests passing (CoreAudio, WASAPI, mixing)
- [ ] OCC Month 2 milestone achieved (real audio playback)
- [ ] Documentation updated (integration guide, API docs)

---

## Phase 2: Real-Time Mixing (Months 3-4)

### Milestone 2.1: Routing Matrix Interface (Weeks 9-10)

**Objective:** Define and implement IRoutingMatrix interface

**Tasks:**

#### 2.1.1 Interface Definition
**Estimate:** 2 days
**Files to Create:**
- `include/orpheus/routing_matrix.h` (public API)

**Interface Specification (from OCC027):**
```cpp
namespace orpheus {

/// Routing configuration for a clip group
struct ClipGroupConfig {
    std::string name;           ///< Group name (e.g., "Music", "SFX")
    float gain_db;              ///< Group gain in dB (-inf to +12)
    bool mute;                  ///< Mute flag
    bool solo;                  ///< Solo flag
    uint8_t output_channel;     ///< Output channel assignment (0-31)
};

/// Routing matrix configuration
struct RoutingConfig {
    std::array<ClipGroupConfig, 4> clip_groups;  ///< 4 clip groups
    uint8_t master_output_channels;              ///< Total output channels (2-32)
};

/// Callback interface for routing events
class IRoutingCallback {
public:
    virtual ~IRoutingCallback() = default;

    /// Called when group gain changes (UI thread)
    virtual void onGroupGainChanged(uint8_t group_index, float gain_db) = 0;

    /// Called when mute/solo changes (UI thread)
    virtual void onGroupMuteChanged(uint8_t group_index, bool muted) = 0;
};

/// Routing matrix interface
class IRoutingMatrix {
public:
    virtual ~IRoutingMatrix() = default;

    /// Initialize with configuration
    virtual SessionGraphError initialize(const RoutingConfig& config) = 0;

    /// Assign clip to group (UI thread, lock-free)
    virtual SessionGraphError setClipGroup(ClipHandle clip, uint8_t group_index) = 0;

    /// Set group gain (UI thread, applied with smoothing in audio thread)
    virtual SessionGraphError setGroupGain(uint8_t group_index, float gain_db) = 0;

    /// Set group mute (UI thread, lock-free)
    virtual SessionGraphError setGroupMute(uint8_t group_index, bool mute) = 0;

    /// Set group solo (UI thread, lock-free)
    virtual SessionGraphError setGroupSolo(uint8_t group_index, bool solo) = 0;

    /// Process audio routing (Audio thread, lock-free)
    /// @param inputs Array of clip outputs (16 clips × 2 channels)
    /// @param output Mixed output buffer (2 channels)
    /// @param num_frames Number of frames to process
    virtual void processRouting(
        const float* const* inputs,
        float** output,
        uint32_t num_frames
    ) = 0;

    /// Set routing callback (UI thread)
    virtual void setCallback(IRoutingCallback* callback) = 0;
};

/// Factory function
std::unique_ptr<IRoutingMatrix> createRoutingMatrix();

} // namespace orpheus
```

**Acceptance Criteria:**
- [ ] Interface header compiles without errors
- [ ] Doxygen comments complete (all methods documented)
- [ ] Follows existing SDK naming conventions
- [ ] Thread safety documented (UI thread vs Audio thread)

#### 2.1.2 Implementation Skeleton
**Estimate:** 1 day
**Files to Create:**
- `src/core/routing/routing_matrix.h` (implementation header)
- `src/core/routing/routing_matrix.cpp`
- `src/core/routing/CMakeLists.txt`

**Requirements:**
- Stub all interface methods
- Add basic structure (member variables for groups, gain state)
- Implement factory function (createRoutingMatrix)

**Acceptance Criteria:**
- [ ] Compiles and links successfully
- [ ] Factory function returns valid instance
- [ ] All methods return success (stubbed)

---

### Milestone 2.2: Gain Smoothing (Weeks 11-12)

**Objective:** Implement click-free gain changes

**Tasks:**

#### 2.2.1 Gain Smoother Implementation
**Estimate:** 3 days
**Files to Create:**
- `src/core/routing/gain_smoother.h` (internal utility)
- `src/core/routing/gain_smoother.cpp`

**Requirements:**
- Linear ramp over 10ms (configurable)
- Thread-safe target gain updates (atomic)
- Per-sample gain calculation in audio thread
- No allocations or locks in audio thread

**Algorithm:**
```cpp
class GainSmoother {
public:
    void setTargetGain(float target_db);  // UI thread, lock-free
    float processSample(float input);     // Audio thread, per-sample

private:
    std::atomic<float> target_gain_{1.0f};
    float current_gain_{1.0f};
    float ramp_increment_{0.0f};
    uint32_t ramp_samples_remaining_{0};
};
```

**Acceptance Criteria:**
- [ ] Gain changes complete in 10ms @ 48kHz (480 samples)
- [ ] No clicks/pops audible (verified by listening test)
- [ ] Zero allocations in processSample() (verified with sanitizers)
- [ ] Thread-safe (UI thread sets target, audio thread reads)

#### 2.2.2 Gain Smoother Tests
**Estimate:** 2 days
**Files to Create:**
- `tests/routing/gain_smoother_test.cpp`

**Test Cases:**
1. Gain ramp completes in 10ms
2. Multiple rapid gain changes (queue properly)
3. Extreme gain values (0 dB, -inf dB, +12 dB)
4. Thread safety (concurrent set/process)
5. Click detection (FFT analysis for artifacts)

**Acceptance Criteria:**
- [ ] 5+ test cases passing
- [ ] Click detection test passes (no artifacts >-60dB)

---

### Milestone 2.3: Routing Implementation (Weeks 13-14)

**Objective:** Complete routing matrix with 4 groups

**Tasks:**

#### 2.3.1 Routing Logic
**Estimate:** 4 days
**Files to Modify:**
- `src/core/routing/routing_matrix.cpp`

**Requirements:**
- Implement 4 clip groups (Music, SFX, Voice, Backup)
- Route each group to master output (sum)
- Apply per-group gain (with smoothing)
- Implement mute (gain = 0)
- Implement solo (mute all other groups)
- Handle group assignment changes (UI thread → Audio thread)

**Algorithm:**
```cpp
void RoutingMatrix::processRouting(
    const float* const* clip_outputs,  // [16][2] stereo clips
    float** master_output,              // [2] stereo master
    uint32_t num_frames
) {
    // Clear master output
    std::memset(master_output[0], 0, num_frames * sizeof(float));
    std::memset(master_output[1], 0, num_frames * sizeof(float));

    // Process each group
    for (uint8_t group = 0; group < 4; ++group) {
        if (groups_[group].mute || (solo_active_ && !groups_[group].solo)) {
            continue;  // Skip muted or non-solo groups
        }

        float group_gain = groups_[group].gain_smoother.getCurrentGain();

        // Sum all clips assigned to this group
        for (auto clip_handle : group_clip_assignments_[group]) {
            const float* clip_L = clip_outputs[clip_handle * 2 + 0];
            const float* clip_R = clip_outputs[clip_handle * 2 + 1];

            for (uint32_t i = 0; i < num_frames; ++i) {
                master_output[0][i] += clip_L[i] * group_gain;
                master_output[1][i] += clip_R[i] * group_gain;
            }
        }
    }
}
```

**Acceptance Criteria:**
- [ ] 4 groups route correctly to master
- [ ] Gain changes apply smoothly (no clicks)
- [ ] Mute works (group output = 0)
- [ ] Solo works (only solo group audible)
- [ ] Zero allocations in processRouting() (verified)

#### 2.3.2 Routing Matrix Tests
**Estimate:** 3 days
**Files to Create:**
- `tests/routing/routing_matrix_test.cpp`

**Test Cases:**
1. Single group routing (clips → group → master)
2. Multi-group routing (4 groups active)
3. Gain control (verify dB to linear conversion)
4. Mute functionality (verify silence)
5. Solo functionality (verify isolation)
6. Clip assignment changes (move clip between groups)
7. Click-free verification (FFT analysis)

**Acceptance Criteria:**
- [ ] 7+ test cases passing
- [ ] Click-free verification passes (<-60dB artifacts)
- [ ] CPU usage <10% for routing alone (measured)

#### 2.3.3 Integration with Transport
**Estimate:** 2 days
**Files to Modify:**
- `src/core/transport/transport_controller.cpp` (add routing support)

**Requirements:**
- Connect IRoutingMatrix to transport controller
- Pass clip outputs to routing matrix
- Use routed output as final audio
- Update OCC integration guide

**Acceptance Criteria:**
- [ ] Transport controller uses routing matrix
- [ ] OCC can assign clips to groups via UI
- [ ] Group controls (gain, mute, solo) work in OCC

**Phase 2 Completion Criteria:**
- [ ] All tasks above completed
- [ ] All tests passing (routing, gain smoothing)
- [ ] OCC Month 4 milestone achieved (routing panel working)
- [ ] Documentation updated (routing guide)

---

## Phase 3: Production Hardening (Months 5-6)

### Milestone 3.1: Performance Monitor (Weeks 17-18)

**Objective:** Implement real-time performance diagnostics

**Tasks:**

#### 3.1.1 Interface Definition
**Estimate:** 1 day
**Files to Create:**
- `include/orpheus/performance_monitor.h` (public API)

**Interface Specification (from OCC027):**
```cpp
namespace orpheus {

/// Performance metrics snapshot
struct PerformanceMetrics {
    float cpu_usage_percent;        ///< CPU usage (0-100%)
    uint32_t buffer_underruns;      ///< Total underrun count
    float latency_ms;               ///< Current latency (milliseconds)
    uint64_t memory_used_bytes;     ///< Memory usage (bytes)
    uint32_t active_clips;          ///< Number of active clips
    float peak_level_db;            ///< Peak audio level (dBFS)
};

/// Performance monitor interface
class IPerformanceMonitor {
public:
    virtual ~IPerformanceMonitor() = default;

    /// Start monitoring (call before audio starts)
    virtual void start() = 0;

    /// Stop monitoring
    virtual void stop() = 0;

    /// Update metrics (call from audio thread)
    virtual void updateMetrics(
        uint64_t frames_processed,
        uint32_t active_clips,
        const float* audio_buffer,
        uint32_t num_frames
    ) = 0;

    /// Get current metrics (UI thread, thread-safe)
    virtual PerformanceMetrics getMetrics() const = 0;

    /// Reset counters (UI thread)
    virtual void reset() = 0;
};

/// Factory function
std::unique_ptr<IPerformanceMonitor> createPerformanceMonitor(uint32_t sample_rate);

} // namespace orpheus
```

**Acceptance Criteria:**
- [ ] Interface header compiles
- [ ] Doxygen comments complete
- [ ] Thread safety documented

#### 3.1.2 Performance Monitor Implementation
**Estimate:** 4 days
**Files to Create:**
- `src/core/diagnostics/performance_monitor.h`
- `src/core/diagnostics/performance_monitor.cpp`
- `src/core/diagnostics/CMakeLists.txt`

**Requirements:**
- CPU usage tracking (system-specific: mach_task_self on macOS, GetProcessTimes on Windows)
- Buffer underrun detection (check for glitches)
- Latency measurement (driver latency + processing latency)
- Memory tracking (malloc_zone_statistics on macOS, GlobalMemoryStatusEx on Windows)
- Peak level detection (RMS and peak)

**Acceptance Criteria:**
- [ ] CPU usage reports correctly (verified against Activity Monitor/Task Manager)
- [ ] Underrun detection works (verified by inducing underruns)
- [ ] Latency measurement accurate (within ±1ms)
- [ ] Memory tracking functional
- [ ] Zero allocations in updateMetrics() (verified)

#### 3.1.3 Performance Monitor Tests
**Estimate:** 2 days
**Files to Create:**
- `tests/diagnostics/performance_monitor_test.cpp`

**Test Cases:**
1. CPU usage measurement (verify against system tools)
2. Underrun detection (simulate underrun)
3. Latency measurement (verify reported latency)
4. Memory tracking (verify allocation reporting)
5. Peak level detection (verify dBFS calculation)

**Acceptance Criteria:**
- [ ] 5+ test cases passing
- [ ] Measurements accurate (within 5% of system tools)

---

### Milestone 3.2: Optimization (Weeks 19-20)

**Objective:** CPU optimization to meet <30% target

**Tasks:**

#### 3.2.1 CPU Profiling
**Estimate:** 2 days
**Tools:** Instruments (macOS), Visual Studio Profiler (Windows)

**Steps:**
1. Run OCC with 16 simultaneous clips
2. Profile with native tools (Instruments Time Profiler, VS Performance Profiler)
3. Identify hot spots (>5% CPU in any function)
4. Document findings

**Acceptance Criteria:**
- [ ] Profiling complete on macOS and Windows
- [ ] Hot spots identified and documented
- [ ] Baseline CPU usage measured (<30% target)

#### 3.2.2 Optimization Implementation
**Estimate:** 4 days
**Files to Modify:** (Based on profiling results)

**Common Optimizations:**
- SIMD vectorization (SSE/AVX for mixing, routing)
- Cache-friendly data layouts (array-of-structs → struct-of-arrays)
- Reduce atomic operations (batch updates)
- Optimize file reading (pre-buffer more samples)

**Acceptance Criteria:**
- [ ] CPU usage <30% with 16 clips (verified)
- [ ] No audio glitches or dropouts
- [ ] Determinism preserved (bit-identical output)

#### 3.2.3 Performance Regression Tests
**Estimate:** 1 day
**Files to Create:**
- `tests/performance/cpu_benchmark_test.cpp`

**Test Cases:**
1. 16-clip CPU benchmark (measure and enforce <30%)
2. Memory benchmark (measure and enforce <500MB)
3. Latency benchmark (measure and enforce <10ms)

**Acceptance Criteria:**
- [ ] All benchmarks pass
- [ ] CI enforces performance thresholds

---

### Milestone 3.3: Stability Testing (Weeks 21-22)

**Objective:** 24-hour stress test, >100hr MTBF

**Tasks:**

#### 3.3.1 Long-Duration Stress Test
**Estimate:** 3 days (mostly unattended running)
**Files to Create:**
- `tests/stress/long_duration_test.cpp`

**Test Scenario:**
- Run OCC continuously for 24 hours
- Play 16 clips in rotation (random start/stop)
- Monitor for crashes, leaks, underruns
- Log all errors and warnings

**Acceptance Criteria:**
- [ ] 24-hour test completes without crashes
- [ ] No memory leaks (AddressSanitizer clean)
- [ ] No buffer underruns (performance monitor confirms)
- [ ] CPU usage stable (no gradual increase)

#### 3.3.2 Edge Case Testing
**Estimate:** 2 days
**Files to Create:**
- `tests/stress/edge_case_test.cpp`

**Test Cases:**
1. Rapid start/stop (1000 clips/second)
2. Invalid audio files (corrupted headers)
3. Device hot-plug (disconnect audio device mid-playback)
4. Out-of-memory scenario (load 1000+ clips)
5. Extreme latency (4096 sample buffer)

**Acceptance Criteria:**
- [ ] All edge cases handled gracefully (no crashes)
- [ ] Error messages logged appropriately
- [ ] System recovers automatically

#### 3.3.3 Cross-Platform Validation
**Estimate:** 2 days
**Platforms:** macOS, Windows

**Validation Steps:**
1. Run full test suite on macOS (ctest)
2. Run full test suite on Windows (ctest)
3. Verify bit-identical audio output (offline render)
4. Manual testing on both platforms

**Acceptance Criteria:**
- [ ] 100% test pass rate on macOS
- [ ] 100% test pass rate on Windows
- [ ] Bit-identical offline renders (verify with diff)
- [ ] Manual testing confirms identical behavior

**Phase 3 Completion Criteria:**
- [ ] All tasks above completed
- [ ] All tests passing (performance, stress, cross-platform)
- [ ] OCC Month 6 milestone achieved (MVP ready for beta)
- [ ] Documentation complete (API docs, integration guide, troubleshooting)

---

## Testing Strategy

### Unit Tests (GoogleTest)

**Coverage Target:** 90%+ line coverage

**Test Categories:**
1. **Interface compliance** - Verify all SDK interfaces work as specified
2. **Edge cases** - Invalid inputs, null pointers, boundary conditions
3. **Thread safety** - Concurrent access from UI and audio threads
4. **Determinism** - Same input produces same output
5. **Performance** - CPU, memory, latency benchmarks

**Tools:**
- GoogleTest framework
- AddressSanitizer (memory errors)
- UndefinedBehaviorSanitizer (UB detection)
- ThreadSanitizer (race conditions, macOS/Linux only)

### Integration Tests

**Test Scenarios:**
1. **Full pipeline** - Driver → Transport → Routing → Output
2. **Multi-clip playback** - 16 simultaneous clips
3. **UI integration** - OCC button click → audio playback
4. **Device changes** - Hot-plug, format changes
5. **Callback flow** - Audio thread → UI thread callbacks

### Stress Tests

**Long-Duration Tests:**
- 24-hour continuous playback
- 1000 clips loaded (memory stress)
- Rapid start/stop (1000/second)

**Performance Tests:**
- CPU benchmark (16 clips, enforce <30%)
- Memory benchmark (enforce <500MB)
- Latency benchmark (enforce <10ms)

### Manual Testing

**OCC Integration Testing:**
1. Load OCC with 960 clips
2. Trigger 16 simultaneous clips
3. Adjust routing (4 groups)
4. Monitor performance (CPU meter)
5. Run for 1 hour (verify stability)

**Platforms:**
- macOS 11+ (Intel and Apple Silicon)
- Windows 10+ (x64)

---

## Success Criteria

### Phase 1 Success (Months 1-2)
- [ ] CoreAudio driver complete and tested (<10ms latency)
- [ ] WASAPI driver complete and tested (<10ms latency)
- [ ] Real audio mixing working (16 clips, <30% CPU)
- [ ] OCC plays real audio on macOS and Windows
- [ ] All unit tests passing (CoreAudio, WASAPI, mixing)
- [ ] Integration tests passing (driver → transport → output)

### Phase 2 Success (Months 3-4)
- [ ] IRoutingMatrix interface complete
- [ ] 4 Clip Groups → Master routing working
- [ ] Gain smoothing click-free (verified by listening test)
- [ ] Mute/solo functionality working
- [ ] OCC routing panel functional
- [ ] All routing tests passing

### Phase 3 Success (Months 5-6)
- [ ] IPerformanceMonitor complete and tested
- [ ] CPU usage <30% with 16 clips (verified)
- [ ] 24-hour stability test passes (>100hr MTBF)
- [ ] Cross-platform validation complete (macOS, Windows)
- [ ] OCC MVP ready for beta testing (10 users)
- [ ] All documentation complete

### Overall Sprint Success
- [ ] All 3 phases complete on schedule
- [ ] OCC MVP achieves all acceptance criteria (from apps/clip-composer/docs/OCC/OCC026)
- [ ] Zero P0/P1 bugs remaining
- [ ] Documentation published (API docs, integration guide)
- [ ] Beta program launched (10 users recruited)

---

## Risk Mitigation

### Risk 1: Platform Driver Latency >10ms
**Probability:** Medium
**Impact:** High (blocks OCC Month 2 milestone)

**Mitigation:**
- Start CoreAudio/WASAPI work in Week 1 (not Week 4)
- Profile latency early (Week 2)
- Have fallback to larger buffer sizes if needed
- Consider ASIO as backup (Windows only, Week 6)

### Risk 2: CPU Usage >30% with 16 Clips
**Probability:** Medium
**Impact:** High (OCC performance target missed)

**Mitigation:**
- Profile early and often (Week 3, 7, 11, 15, 19)
- Implement SIMD optimizations from start (Week 7)
- Have backup plan: reduce max clips to 12 if needed

### Risk 3: Routing Matrix Complexity Underestimated
**Probability:** Low
**Impact:** Medium (delay OCC Month 4 milestone)

**Mitigation:**
- Start routing design in Week 9 (early)
- Implement gain smoothing separately (Week 11)
- Test incrementally (group by group)
- Defer advanced features (aux sends, EQ) to v1.0

### Risk 4: Cross-Platform Determinism Issues
**Probability:** Medium
**Impact:** High (professional quality compromised)

**Mitigation:**
- Use std::bit_cast for float determinism
- Test on both platforms weekly (not just at end)
- Run offline render comparison in CI
- Document any platform-specific behavior

### Risk 5: OCC Integration Delays SDK Work
**Probability:** Low
**Impact:** Medium (sprint timeline slips)

**Mitigation:**
- SDK team and OCC team work in parallel (separate repos/branches)
- Weekly sync meetings (Fridays, 30 min)
- Clear SDK API contracts (OCC027) prevent rework
- Stub implementations allow OCC to proceed without blocking

---

## Appendix A: File Inventory

### New Files Created (This Sprint)

**Platform Drivers:**
- `src/platform/audio_drivers/coreaudio/coreaudio_driver.{h,cpp}`
- `src/platform/audio_drivers/wasapi/wasapi_driver.{h,cpp}`
- `src/platform/audio_drivers/CMakeLists.txt`
- `tests/audio_io/coreaudio_driver_test.cpp`
- `tests/audio_io/wasapi_driver_test.cpp`

**Routing Matrix:**
- `include/orpheus/routing_matrix.h`
- `src/core/routing/routing_matrix.{h,cpp}`
- `src/core/routing/gain_smoother.{h,cpp}`
- `src/core/routing/CMakeLists.txt`
- `tests/routing/routing_matrix_test.cpp`
- `tests/routing/gain_smoother_test.cpp`

**Performance Monitor:**
- `include/orpheus/performance_monitor.h`
- `src/core/diagnostics/performance_monitor.{h,cpp}`
- `src/core/diagnostics/CMakeLists.txt`
- `tests/diagnostics/performance_monitor_test.cpp`

**Testing:**
- `tests/transport/multi_clip_stress_test.cpp`
- `tests/performance/cpu_benchmark_test.cpp`
- `tests/stress/long_duration_test.cpp`
- `tests/stress/edge_case_test.cpp`

**Total:** ~22 new files, ~8,000 lines of code (estimated)

### Modified Files

- `CMakeLists.txt` (add ORPHEUS_ENABLE_COREAUDIO, ORPHEUS_ENABLE_WASAPI)
- `src/CMakeLists.txt` (link platform drivers, routing, diagnostics)
- `src/core/transport/transport_controller.cpp` (add real mixing, routing integration)
- `apps/clip-composer/Source/AudioEngine/AudioEngine.cpp` (OCC integration)

---

## Appendix B: Dependencies

### External Libraries

**libsndfile** (LGPL 2.1)
- Already integrated (Phase 0)
- Used by IAudioFileReader

**Platform APIs:**
- **macOS:** CoreAudio framework (system)
- **Windows:** WASAPI (Ole32.lib, Avrt.lib, system)

### Build Tools

- CMake 3.22+
- C++20 compiler (Clang 13+, GCC 11+, MSVC 2019+)
- GoogleTest (FetchContent)

### CI Infrastructure

- GitHub Actions (macOS, Windows runners)
- AddressSanitizer, UBSanitizer
- Code coverage (lcov, codecov)

---

## Appendix C: Communication Plan

### Weekly Sync Meetings
**When:** Fridays, 30 minutes
**Who:** SDK Core Team + OCC Team
**Agenda:**
- Sprint progress update (completed tasks, blockers)
- OCC integration status (what's working, what's blocked)
- Risk review (any new risks identified)
- Next week priorities

### Slack Channels
- **#orpheus-sdk-integration** - General SDK development
- **#orpheus-occ-integration** - OCC-specific discussions
- **#orpheus-releases** - Release coordination

### GitHub Issues
**Tags:**
- `orp070` - This sprint's tasks
- `occ-blocker` - Urgent issues blocking OCC
- `performance` - Performance-related issues
- `platform-specific` - macOS or Windows specific

### Documentation Updates
**Frequency:** End of each phase
**Deliverables:**
- API documentation (Doxygen)
- Integration guide (Markdown)
- Troubleshooting guide (Markdown)
- Release notes (Markdown)

---

## Appendix D: References

### OCC Design Documentation
**Location:** `apps/clip-composer/docs/OCC/`

**Key Documents:**
- **OCC021** - Product Vision (market positioning, competitive analysis)
- **OCC026** - MVP Definition (6-month plan, acceptance criteria)
- **OCC027** - API Contracts (C++ interfaces, thread safety)
- **OCC029** - SDK Enhancement Recommendations (module requirements)
- **OCC030** - SDK Status Report (current state, blockers)

### Related Plans
- **ORP069** - OCC-Aligned SDK Enhancements (parent plan)
- **ROADMAP.md** - Milestone M2 timeline

### SDK Documentation
- **README.md** - Repository overview
- **ARCHITECTURE.md** - Design rationale
- **CLAUDE.md** - Claude Code development guide
- **AGENTS.md** - AI assistant guidelines

---

**Document Status:** Authoritative
**Next Review:** End of Phase 1 (Month 2)
**Maintained By:** SDK Core Team
**Last Updated:** October 12, 2025
