# ORP117: Routing Matrix Technical Explainer

**Status:** Reference Documentation
**Created:** 2025-11-19
**Component:** `IRoutingMatrix`, `IClipRoutingMatrix`
**Files:** `include/orpheus/routing_matrix.h`, `include/orpheus/clip_routing.h`, `src/core/routing/`

---

## Executive Summary

The Routing Matrix is the audio signal routing and mixing subsystem of the Orpheus SDK. It provides professional N×M routing capabilities with lock-free, sample-accurate processing suitable for broadcast and real-time applications. Two implementations exist: the full-featured `IRoutingMatrix` for professional scenarios and the simplified `IClipRoutingMatrix` for Clip Composer's 4-group architecture.

---

## 1. Architecture Overview

### 1.1 Signal Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    ROUTING MATRIX SIGNAL FLOW                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Channels (1-64)         Groups (1-16)         Outputs (2-32)│
│  ┌─────────┐            ┌─────────┐            ┌─────────┐  │
│  │ Ch 0    │──┐         │ Group 0 │──┐         │ Out L   │  │
│  │ gain    │  │         │ gain    │  │         │ (0)     │  │
│  │ pan     │  ├─────────▶│ mute    │  ├─────────▶│         │  │
│  │ mute    │  │         │ solo    │  │         │         │  │
│  │ solo    │  │         └─────────┘  │         └─────────┘  │
│  └─────────┘  │                      │                      │
│  ┌─────────┐  │         ┌─────────┐  │         ┌─────────┐  │
│  │ Ch 1    │──┼─────────▶│ Group 1 │──┼─────────▶│ Out R   │  │
│  └─────────┘  │         └─────────┘  │         │ (1)     │  │
│      ...      │             ...      │         └─────────┘  │
│  ┌─────────┐  │         ┌─────────┐  │             ...      │
│  │ Ch N    │──┘         │ Group M │──┘         ┌─────────┐  │
│  └─────────┘            └─────────┘            │ Out K   │  │
│                                                └─────────┘  │
│                                                              │
│           ┌─────────────────────────────────┐                │
│           │        MASTER SECTION           │                │
│           │  gain │ mute │ clipping prot.   │                │
│           └─────────────────────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 Design Philosophy

- **Professional Console Model**: Mirrors traditional mixing console routing (channels → buses → master)
- **Lock-Free Audio**: All audio-thread operations use atomics and pre-allocated buffers
- **Sample-Accurate**: No mid-buffer discontinuities; all parameter changes are smoothed
- **Broadcast-Safe**: 24/7 reliability with clipping protection and deterministic output

### 1.3 Configuration Limits

| Parameter | Range | Default |
|-----------|-------|---------|
| Channels | 1-64 | 32 |
| Groups | 1-16 | 8 |
| Outputs | 2-32 | 2 (stereo) |
| Gain Range | -∞ to +12 dB | 0 dB |
| Pan Range | -1.0 to +1.0 | 0.0 (center) |
| Smoothing Time | 1.0-100.0 ms | 10 ms |

---

## 2. Core Data Structures

### 2.1 Configuration Types

```cpp
// Primary routing configuration
struct RoutingConfig {
    uint8_t num_channels;         // [1-64] Number of input channels
    uint8_t num_groups;           // [1-16] Number of group buses
    uint8_t num_outputs;          // [2-32] Number of output channels
    SoloMode solo_mode;           // Solo behavior mode
    MeteringMode metering_mode;   // Metering algorithm
    float gain_smoothing_ms;      // [1.0-100.0] Gain ramp time
    float dim_amount_db;          // [-24 to -6] Dim attenuation
    bool enable_metering;         // Enable/disable meter updates
    bool enable_clipping_protection; // Soft/hard limiter
};
```

### 2.2 Channel and Group Configuration

```cpp
// Per-channel configuration
struct ChannelConfig {
    std::string name;       // Display name (e.g., "Kick", "Snare")
    uint8_t group_index;    // Target group (0-15, or 255=unassigned)
    float gain_db;          // Channel gain [-∞ to +12 dB]
    float pan;              // Stereo pan [-1.0 to +1.0]
    bool mute;              // Mute state
    bool solo;              // Solo state
    uint32_t color;         // UI hint (RGBA)
};

// Per-group configuration
struct GroupConfig {
    std::string name;       // Display name (e.g., "Drums", "Music")
    float gain_db;          // Group gain [-∞ to +12 dB]
    bool mute;              // Mute state
    bool solo;              // Solo state
    uint8_t output_bus;     // Output assignment (0=master, 1-15=aux)
    uint32_t color;         // UI hint (RGBA)
};
```

### 2.3 Solo Modes

```cpp
enum class SoloMode : uint8_t {
    SIP = 0,        // Solo-in-Place: Mutes all non-soloed channels
    AFL = 1,        // After-Fader-Listen: Routes to dedicated AFL bus
    PFL = 2,        // Pre-Fader-Listen: Ignores fader position
    Destructive = 3 // Ableton-style: Stops non-soloed clips entirely
};
```

### 2.4 Metering

```cpp
enum class MeteringMode : uint8_t {
    Peak = 0,       // Fast, responsive peak detection
    RMS = 1,        // Average energy (root mean square)
    TruePeak = 2,   // ITU-R BS.1770 oversampled peak
    LUFS = 3        // Loudness Units Full Scale (broadcast)
};

struct AudioMeter {
    float peak_db;      // [-∞ to 0.0] dBFS peak level
    float rms_db;       // [-∞ to 0.0] dBFS RMS level
    bool clipping;      // True if clipping detected
    uint32_t clip_count; // Number of clipped samples
};
```

### 2.5 Routing Snapshot (Presets)

```cpp
struct RoutingSnapshot {
    std::string name;                    // Preset name
    uint32_t timestamp_ms;               // Creation timestamp
    std::vector<ChannelConfig> channels; // All channel states
    std::vector<GroupConfig> groups;     // All group states
    float master_gain_db;                // Master gain
    bool master_mute;                    // Master mute state
};
```

---

## 3. Public API Reference

### 3.1 IRoutingMatrix Interface

**Location:** `include/orpheus/routing_matrix.h:181-367`

#### Initialization

```cpp
// Initialize with configuration (UI thread only)
virtual SessionGraphError initialize(const RoutingConfig& config) = 0;

// Set callback for state change notifications
virtual void setCallback(IRoutingCallback* callback) = 0;
```

#### Channel Configuration (UI Thread, Lock-Free)

| Method | Signature | Purpose |
|--------|-----------|---------|
| `setChannelGroup` | `(uint8_t channel, uint8_t group) → Error` | Assign channel to group |
| `setChannelGain` | `(uint8_t channel, float db) → Error` | Set gain with smoothing |
| `setChannelPan` | `(uint8_t channel, float pan) → Error` | Set constant-power pan |
| `setChannelMute` | `(uint8_t channel, bool mute) → Error` | Mute/unmute channel |
| `setChannelSolo` | `(uint8_t channel, bool solo) → Error` | Solo channel |
| `configureChannel` | `(uint8_t channel, ChannelConfig) → Error` | Batch configuration |

#### Group Configuration (UI Thread, Lock-Free)

| Method | Signature | Purpose |
|--------|-----------|---------|
| `setGroupGain` | `(uint8_t group, float db) → Error` | Set group gain |
| `setGroupMute` | `(uint8_t group, bool mute) → Error` | Mute/unmute group |
| `setGroupSolo` | `(uint8_t group, bool solo) → Error` | Solo group |
| `configureGroup` | `(uint8_t group, GroupConfig) → Error` | Batch configuration |

#### Master Configuration

| Method | Signature | Purpose |
|--------|-----------|---------|
| `setMasterGain` | `(float db) → Error` | Set master output gain |
| `setMasterMute` | `(bool mute) → Error` | Mute master output |

#### State Queries (Any Thread, Atomic Reads)

```cpp
bool isSoloActive() const;                    // Any channel/group soloed?
bool isChannelMuted(uint8_t channel) const;   // Effective mute state
bool isGroupMuted(uint8_t group) const;       // Effective mute state
AudioMeter getChannelMeter(uint8_t channel) const;
AudioMeter getGroupMeter(uint8_t group) const;
AudioMeter getMasterMeter() const;
```

#### Preset Management

```cpp
RoutingSnapshot saveSnapshot(const std::string& name) const;
SessionGraphError loadSnapshot(const RoutingSnapshot& snapshot);
void reset();  // Reset to defaults
```

#### Audio Processing (Audio Thread Only)

```cpp
// Process audio through routing matrix
// MUST be called from audio callback only
void processRouting(
    const float* const* channel_inputs,  // Per-channel input buffers
    float** master_output,               // Master output buffers
    size_t num_frames                    // Frames to process
);
```

### 3.2 IClipRoutingMatrix Interface (Simplified)

**Location:** `include/orpheus/clip_routing.h:15-158`

Simplified 4-group architecture optimized for Clip Composer:

```cpp
// Group assignment
SessionGraphError assignClipToGroup(ClipHandle clip, uint8_t group);
uint8_t getClipGroup(ClipHandle clip) const;

// Group controls
SessionGraphError setGroupGain(uint8_t group, float db);  // [-60, +12]
SessionGraphError setGroupMute(uint8_t group, bool mute);
SessionGraphError setGroupSolo(uint8_t group, bool solo);
SessionGraphError routeGroupToMaster(uint8_t group, bool enabled);

// Multi-channel routing (Feature 7)
SessionGraphError setClipOutputBus(ClipHandle clip, uint8_t bus);
SessionGraphError mapChannels(ClipHandle clip, uint8_t src, uint8_t dst);
```

---

## 4. Audio Processing Pipeline

### 4.1 Processing Steps

The `processRouting()` method executes a 6-step pipeline on the audio thread:

```
Step 1: Clear Group Buffers
    └─ memset(group_buffers[g], 0, num_frames × sizeof(float))

Step 2: Process Channels → Groups
    ├─ For each channel:
    │   ├─ Check effective mute (explicit mute OR solo exclusion)
    │   ├─ For each sample:
    │   │   ├─ gain = channel.gain_smoother->process()
    │   │   ├─ pan_l = channel.pan_left->process()
    │   │   ├─ pan_r = channel.pan_right->process()
    │   │   ├─ sample *= gain
    │   │   └─ group_buffer[frame] += sample × pan
    │   └─ Update channel meters

Step 3: Process Groups → Master
    ├─ Clear master output buffers
    ├─ For each group:
    │   ├─ Check effective mute
    │   ├─ For each sample:
    │   │   ├─ gain = group.gain_smoother->process()
    │   │   ├─ sample *= gain
    │   │   └─ master_output[out][frame] += sample
    │   └─ Update group meters

Step 4: Apply Master Gain/Mute
    └─ For each sample:
        ├─ gain = master_gain_smoother->process()
        ├─ if (master_muted) gain = 0.0f
        └─ master_output[out][frame] *= gain

Step 5: Update Master Meters
    └─ processMetering(master_output, ...)

Step 6: Clipping Protection
    ├─ Soft-knee limiter: tanh(sample × 0.9) / 0.9
    └─ Hard clip: clamp(sample, -1.0, +1.0)
```

### 4.2 Gain Smoothing

The `GainSmoother` class provides click-free parameter changes:

**Location:** `src/core/routing/gain_smoother.h`

```cpp
class GainSmoother {
    float m_current;                    // Current gain value
    float m_target;                     // Target gain value
    float m_increment;                  // Per-sample delta
    std::atomic<float> m_pending_target; // UI thread writes here
    std::atomic<bool> m_has_pending;    // Update pending flag

public:
    void setTarget(float target);  // UI thread (lock-free)
    float process();               // Audio thread (per-sample)
};
```

**Smoothing Calculation:**
```
smoothing_samples = (smoothing_ms / 1000.0) × sample_rate
increment = 1.0 / smoothing_samples

Example: 48kHz, 10ms smoothing
  → 480 samples per ramp
  → increment ≈ 0.00208
```

### 4.3 Pan Law

Constant-power pan law maintains perceived loudness across the stereo field:

```cpp
// Convert pan [-1.0, +1.0] to left/right gains
void updatePanLaw(float pan, float& left_gain, float& right_gain) {
    // Map pan to [0, π/2] range
    float angle = (pan + 1.0f) * 0.25f * M_PI;

    // Constant-power: L² + R² = 1
    left_gain = cos(angle);   // 0.707 at center (-3 dB)
    right_gain = sin(angle);  // 0.707 at center (-3 dB)
}
```

### 4.4 Clipping Protection

Two-stage limiter prevents digital distortion (OCC109 implementation):

```cpp
// Stage 1: Soft-knee limiter (smooth compression)
if (std::abs(sample) > 0.9f) {
    sample = std::tanh(sample * 0.9f) / 0.9f;
}

// Stage 2: Hard clip safety (broadcast-safe guarantee)
sample = std::clamp(sample, -1.0f, 1.0f);
```

---

## 5. Thread Safety Model

### 5.1 Thread Ownership

| Operation Type | Thread | Mechanism |
|----------------|--------|-----------|
| `initialize()`, `loadSnapshot()` | UI only | Protected by caller |
| `setChannel*()`, `setGroup*()` | UI only | Atomic writes |
| `processRouting()` | Audio only | Lock-free reads |
| `get*()`, `isMuted()` | Any | Atomic reads |

### 5.2 Lock-Free Patterns

**Double-Buffer Configuration:**
```cpp
RoutingConfig m_config_buffers[2];    // Two config copies
std::atomic<int> m_active_config_idx; // 0 or 1

// UI thread: write to inactive buffer, then swap
void updateConfig(const RoutingConfig& config) {
    int inactive = 1 - m_active_config_idx.load();
    m_config_buffers[inactive] = config;
    m_active_config_idx.store(inactive);
}

// Audio thread: read from active buffer
const RoutingConfig& getConfig() const {
    return m_config_buffers[m_active_config_idx.load()];
}
```

**Atomic State Updates:**
```cpp
// Channel state uses atomic members for thread-safe access
struct ChannelState {
    std::atomic<bool> mute;
    std::atomic<bool> solo;
    std::atomic<float> peak_level;
    std::atomic<float> rms_level;
    std::atomic<uint32_t> clip_count;
    GainSmoother* gain_smoother;  // Internal lock-free design
};
```

### 5.3 Memory Ordering

All atomic operations use sequential consistency (`memory_order_seq_cst`) for safety. Critical paths could be optimized with relaxed ordering if profiling indicates need.

---

## 6. Integration Points

### 6.1 TransportController Integration

**Location:** `src/core/transport/transport_controller.cpp`

The TransportController owns and manages the routing matrix:

```cpp
TransportController::TransportController() {
    m_routingMatrix = createClipRoutingMatrix(m_sessionGraph, sampleRate);
    m_routingMatrix->initialize(MAX_ACTIVE_CLIPS);
}

void TransportController::processAudio(float** output, size_t frames) {
    // Process clips into per-channel buffers
    for (auto& clip : m_activeClips) {
        clip->render(m_channelBuffers[clip->channel], frames);
    }

    // Route channels through matrix to master output
    m_routingMatrix->processRouting(m_channelBuffers, output, frames);
}
```

### 6.2 SceneManager Integration

**Location:** `include/orpheus/scene_manager.h`

Scene snapshots include routing state:

```cpp
struct SceneSnapshot {
    // ... other scene data ...
    std::vector<uint8_t> clipGroups;  // Per-clip group assignment
    std::vector<float> groupGains;    // Per-group gain in dB
};

void SceneManager::captureScene() {
    for (auto& clip : m_clips) {
        snapshot.clipGroups.push_back(
            m_routing->getClipGroup(clip.handle)
        );
    }
    for (uint8_t g = 0; g < 4; g++) {
        snapshot.groupGains.push_back(m_routing->getGroupGain(g));
    }
}
```

### 6.3 State Synchronization

**Location:** `docs/sync-architecture/README.md`

UI polling pattern (no callbacks):

```cpp
// UI thread polls at 75fps
void updateRoutingUI() {
    for (uint8_t g = 0; g < 4; g++) {
        AudioMeter meter = m_routing->getGroupMeter(g);
        m_ui.setMeterLevel(g, meter.peak_db);
        m_ui.setClipping(g, meter.clipping);
    }
}
```

---

## 7. Performance Characteristics

### 7.1 CPU Usage

| Scenario | Typical CPU | Notes |
|----------|-------------|-------|
| 8 channels, 4 groups | ~0.3% | Minimal processing |
| 32 channels, 8 groups | ~0.8% | Mid-size session |
| 64 channels, 16 groups | ~1.5% | Full capacity |
| With metering enabled | +0.2% | Per-sample RMS calculation |
| With clipping protection | +0.1% | tanh() on clipped samples only |

### 7.2 Memory Footprint

| Component | Size | Notes |
|-----------|------|-------|
| ChannelState | ~1 KB | Includes gain/pan smoothers |
| GroupState | ~500 B | Includes gain smoother |
| Group buffers | 8 KB/group | 2048 samples × sizeof(float) |
| Temp buffer | 8 KB | Scratch space |

**Total for default config (32 channels, 8 groups):**
~32 KB channels + ~4 KB groups + ~64 KB buffers = **~100 KB**

### 7.3 Latency

- **Routing latency:** 0 samples (pass-through)
- **Parameter change latency:** 1-480 samples (smoothing ramp)
- **Buffer size independence:** Works with any power-of-2 buffer (32-2048)

---

## 8. Usage Examples

### 8.1 Basic Initialization

```cpp
#include <orpheus/routing_matrix.h>

// Create routing matrix
auto routing = createRoutingMatrix();

// Configure for 16-channel session
RoutingConfig config;
config.num_channels = 16;
config.num_groups = 4;
config.num_outputs = 2;
config.solo_mode = SoloMode::SIP;
config.metering_mode = MeteringMode::Peak;
config.gain_smoothing_ms = 10.0f;
config.enable_metering = true;
config.enable_clipping_protection = true;

routing->initialize(config);
```

### 8.2 Channel Configuration

```cpp
// Configure a drum channel
ChannelConfig drumConfig;
drumConfig.name = "Kick";
drumConfig.group_index = 0;  // Route to "Drums" group
drumConfig.gain_db = -3.0f;
drumConfig.pan = 0.0f;       // Center
drumConfig.mute = false;
drumConfig.solo = false;

routing->configureChannel(0, drumConfig);

// Quick gain adjustment (smoothed)
routing->setChannelGain(0, -6.0f);

// Pan slightly left
routing->setChannelPan(0, -0.3f);
```

### 8.3 Solo Logic

```cpp
// Solo the drum group
routing->setGroupSolo(0, true);  // Drums

// Check effective state
if (routing->isSoloActive()) {
    // Non-soloed groups are effectively muted
    bool musicMuted = routing->isGroupMuted(1);  // true
}

// Clear all solos
for (uint8_t g = 0; g < config.num_groups; g++) {
    routing->setGroupSolo(g, false);
}
```

### 8.4 Metering Display

```cpp
// Update UI meters (call at 60-75fps)
void updateMeters() {
    for (uint8_t g = 0; g < 4; g++) {
        AudioMeter meter = routing->getGroupMeter(g);

        // Update peak meter
        ui.setPeakLevel(g, meter.peak_db);

        // Show clipping indicator
        if (meter.clipping) {
            ui.showClipWarning(g);
        }
    }

    // Master meter
    AudioMeter master = routing->getMasterMeter();
    ui.setMasterLevel(master.peak_db);
}
```

### 8.5 Preset Save/Load

```cpp
// Save current routing as preset
RoutingSnapshot preset = routing->saveSnapshot("Studio Mix");

// Store to disk (JSON serialization not shown)
savePresetToFile(preset, "studio_mix.json");

// Load preset
RoutingSnapshot loaded = loadPresetFromFile("studio_mix.json");
routing->loadSnapshot(loaded);
```

### 8.6 Clip Composer Integration

```cpp
#include <orpheus/clip_routing.h>

// Create simplified routing for OCC
auto clipRouting = createClipRoutingMatrix(sessionGraph, 48000);

// Assign clips to groups
clipRouting->assignClipToGroup(kickClip, 0);   // Group A: Drums
clipRouting->assignClipToGroup(bassClip, 1);   // Group B: Bass
clipRouting->assignClipToGroup(padClip, 2);    // Group C: Pads
clipRouting->assignClipToGroup(sfxClip, 3);    // Group D: SFX

// Set group levels
clipRouting->setGroupGain(0, -3.0f);   // Drums at -3 dB
clipRouting->setGroupGain(1, -6.0f);   // Bass at -6 dB
clipRouting->setGroupGain(2, -9.0f);   // Pads at -9 dB

// Solo drums for editing
clipRouting->setGroupSolo(0, true);
```

---

## 9. Error Handling

### 9.1 Error Codes

```cpp
enum class SessionGraphError {
    OK = 0,              // Success
    InvalidParameter,    // Invalid channel/group/output count
    NotInitialized,      // Called before initialize()
    InvalidHandle,       // Invalid clip handle (ClipRoutingMatrix)
    OutOfRange          // Index out of bounds
};
```

### 9.2 Validation

All setter methods validate parameters before applying:

```cpp
SessionGraphError setChannelGain(uint8_t channel, float db) {
    if (!m_initialized) return SessionGraphError::NotInitialized;
    if (channel >= m_config.num_channels) return SessionGraphError::OutOfRange;
    if (db > 12.0f) return SessionGraphError::InvalidParameter;

    // Apply gain...
    return SessionGraphError::OK;
}
```

---

## 10. Testing

### 10.1 Test Files

- `tests/routing/routing_matrix_test.cpp` - Core routing unit tests
- `tests/routing/clip_routing_test.cpp` - Clip routing tests
- `tests/routing/multi_channel_routing_test.cpp` - Multi-channel output tests

### 10.2 Key Test Cases

1. **Initialization validation** - Config limits, error handling
2. **Signal routing** - Verify audio flows correctly through matrix
3. **Gain attenuation** - Verify dB values apply correctly
4. **Pan law** - Verify constant-power behavior
5. **Mute/solo logic** - Verify SIP behavior, solo exclusion
6. **Clipping detection** - Verify meter clipping flags
7. **Thread safety** - Concurrent UI/audio access

---

## 11. Related Documentation

| Document | Description |
|----------|-------------|
| ORP109 | SDK Feature Roadmap (routing feature specs) |
| ORP114 | Gain Staging Bug Investigation |
| `docs/early_spec/core/bus_architecture.md` | Bus timing and voice allocation |
| `docs/early_spec/core/audio_engine.md` | Audio processing chain |
| `docs/sync-architecture/README.md` | UI/SDK state synchronization |
| `docs/API_SURFACE_INDEX.md` | API reference index |

---

## 12. Implementation Files

| File | Purpose |
|------|---------|
| `include/orpheus/routing_matrix.h` | Public IRoutingMatrix interface |
| `include/orpheus/clip_routing.h` | Public IClipRoutingMatrix interface |
| `src/core/routing/routing_matrix.h` | Internal state structures |
| `src/core/routing/routing_matrix.cpp` | IRoutingMatrix implementation |
| `src/core/routing/clip_routing.cpp` | IClipRoutingMatrix implementation |
| `src/core/routing/gain_smoother.h` | GainSmoother class |
| `src/core/routing/gain_smoother.cpp` | GainSmoother implementation |

---

## Appendix A: Constants Reference

```cpp
// IRoutingMatrix limits
constexpr uint8_t MAX_CHANNELS = 64;
constexpr uint8_t MAX_GROUPS = 16;
constexpr uint8_t MAX_OUTPUTS = 32;
constexpr uint8_t UNASSIGNED_GROUP = 255;
constexpr size_t MAX_BUFFER_SIZE = 2048;

// IClipRoutingMatrix limits
constexpr uint8_t NUM_GROUPS = 4;
constexpr float MIN_GAIN_DB = -60.0f;
constexpr float MAX_GAIN_DB = 12.0f;
constexpr float SMOOTHING_TIME_MS = 10.0f;
constexpr uint8_t MAX_OUTPUT_BUS = 15;
constexpr uint8_t MAX_OUTPUT_CHANNELS = 32;
```

---

## Appendix B: Unit Conversion Reference

```cpp
// dB to linear gain
float dbToLinear(float db) {
    if (db <= -100.0f) return 0.0f;
    return std::pow(10.0f, db / 20.0f);
}

// Linear gain to dB
float linearToDb(float linear) {
    if (linear <= 0.0f) return -100.0f;
    return 20.0f * std::log10(linear);
}

// Common values
// 0 dB   → 1.0
// -3 dB  → 0.707
// -6 dB  → 0.5
// -12 dB → 0.25
// -20 dB → 0.1
// -∞ dB  → 0.0
```
