# Component Map - Notes

**Last Updated:** 2026-01-18
**Related Diagram:** [component-map.mermaid.md](./component-map.mermaid.md)

## Overview

This document describes the detailed component relationships within the Orpheus SDK, including the new components introduced in ORP121 Audio Backend Refactoring.

## Core Components

### Session Management

#### SessionGraph
The root container for all session data:
- **Tracks:** Collection of audio/MIDI tracks
- **Tempo:** BPM and time signature
- **Metadata:** Session name, author, notes

**Location:** `include/orpheus/session_graph.h`

#### Clip
Individual audio clip with playback metadata:
- **Trim Points:** `trimInSamples`, `trimOutSamples` (64-bit sample counts)
- **Fades:** Duration and curve type for IN/OUT
- **Gain:** Per-clip volume in dB
- **Loop:** Enable/disable looping at trim OUT

### Transport System

#### TransportController
Manages real-time clip playback:
- Maintains `activeClips` map for currently playing clips
- Sample-accurate timing using 64-bit sample counts
- Fade, gain, and loop processing in audio callback

**Key Methods:**
- `startClip()` - Add clip to active playback
- `stopClip()` - Remove clip from active playback
- `processAudio()` - Real-time audio callback (NO allocations)
- `processCallbacks()` - UI thread callback processing

#### SPSCCallbackQueue (ORP121 C-03)

**NEW in ORP121:** Lock-free Single-Producer Single-Consumer ring buffer.

**Problem Solved:** Previous mutex-based queue caused priority inversion risk when audio thread waited for UI thread.

**Implementation:**
```cpp
static constexpr size_t CALLBACK_QUEUE_SIZE = 256;
std::array<std::function<void()>, CALLBACK_QUEUE_SIZE> m_callbackRing;
std::atomic<size_t> m_callbackWriteIndex{0};
std::atomic<size_t> m_callbackReadIndex{0};
```

**Memory Ordering:**
- `relaxed` for same-thread reads
- `acquire` for cross-thread reads
- `release` for writes

**Location:** `src/core/transport/transport_controller.cpp`

#### ActiveClip
Lock-free state container for playing clips:
- All state variables are `std::atomic<>` for thread-safe access
- Audio thread reads, UI thread writes
- No mutex required for gain/position updates

### Routing System (ORP121 Enhanced)

#### RoutingConfig (ORP121 Q-03)

Configuration structure with new fields:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `numInputChannels` | uint8_t | 2 | Input channel count |
| `numOutputChannels` | uint8_t | 2 | Output channel count |
| `numGroups` | uint8_t | 4 | Routing group count |
| `sampleRate` | uint32_t | 48000 | **NEW:** Sample rate in Hz |
| `headroomMode` | HeadroomMode | None | **NEW:** Headroom management |
| `enableTruePeak` | bool | false | **NEW:** True-peak metering |

**Location:** `include/orpheus/routing_matrix.h`

#### HeadroomMode (ORP121 Q-05)

**NEW in ORP121:** Automatic gain reduction modes.

| Mode | Formula | Use Case |
|------|---------|----------|
| `None` | 1.0 (no change) | Manual gain staging |
| `PerGroup` | 10^(-3*(n-1)/20) | -3 dB per channel in group |
| `Global` | 10^(-3*(total-1)/20) | Based on all active channels |
| `Logarithmic` | 1/sqrt(n) | Broadcast standard (-10*log10(n)) |

**Example:** 4 channels with `Logarithmic` mode = -6.02 dB reduction

#### RoutingMatrix

Enhanced N×M routing matrix:

**New Features:**
- Per-channel TruePeakMeter instances
- Per-group TruePeakMeter instances
- Master output TruePeakMeter
- Headroom compensation in `processRouting()`
- Constant-power pan law (cos/sin)

**Processing Order:**
1. Read input samples
2. Apply channel gain (with smoothing)
3. Apply pan law (constant-power)
4. Sum to group buffers
5. Apply headroom compensation
6. Apply group gain
7. Sum to master output
8. Apply soft-knee limiter
9. Update metering

#### TruePeakMeter (ORP121 Q-04)

**NEW in ORP121:** ITU-R BS.1770-4 compliant inter-sample peak detection.

**Technical Specifications:**
- **Oversample Factor:** 4x
- **Filter Taps:** 48 total (12 taps × 4 phases)
- **Filter Type:** Polyphase FIR interpolation
- **Accuracy:** ~0.1 dB for inter-sample peaks

**Why True-Peak?**
Standard peak meters measure actual sample values, but D/A converters reconstruct the continuous waveform, which can peak between samples. True-peak metering detects these inter-sample peaks that cause converter clipping.

**Location:** `src/core/routing/true_peak_meter.h`

#### GainSmoother (ORP121 C-01)

**ENHANCED in ORP121:** Extended range from 0-1 to 0-3.98 linear.

**Previous:** Clipped at 1.0 (0 dB) - no boost possible
**New:** Range up to 3.98 (+12 dB) - professional boost capability

```cpp
static constexpr float MAX_GAIN_DB = 12.0f;
static constexpr float MAX_LINEAR_GAIN = 3.981071705534972f;  // 10^(12/20)
```

#### SoftKneeLimiter (ORP121 C-02)

**NEW in ORP121:** Continuous soft-knee limiter replacing discontinuous tanh.

**Previous Problem:** Discontinuity at 0.9 threshold caused audible clicks.

**Solution:** Smooth tanh saturation with soft knee:
- **Threshold:** -2 dBFS (0.794 linear)
- **Knee Width:** 0.3
- **Ceiling:** 0.9999

**Mathematical Properties:**
- C1 continuous (smooth first derivative)
- No audible clicks at threshold crossing
- Graceful saturation above threshold

## Component Relationships

### Transport → Routing Flow

```
TransportController
    ↓ (per clip)
ActiveClip → AudioFileReader → samples
    ↓
Apply fade IN/OUT
    ↓
Apply clip gain
    ↓
RoutingMatrix.process()
    ↓
Channel processing (gain, pan, metering)
    ↓
Group mixing (headroom compensation)
    ↓
Master output (limiter, metering)
```

### Callback Flow (Lock-Free)

```
Audio Thread                    UI Thread
    │                              │
    │ postCallback()               │
    ↓                              │
SPSCCallbackQueue ──────────────→ processCallbacks()
(ring buffer)                      │
                                   ↓
                              Execute callbacks
                              Update UI
```

## When to Modify Components

### Adding New Clip Metadata
1. Update `Clip` struct in `session_graph.h`
2. Update `ActiveClip` with atomic field
3. Update JSON serialization in `session_json.cpp`
4. Update `processAudio()` to use new field

### Adding New Metering Type
1. Create new meter class (follow TruePeakMeter pattern)
2. Add to ChannelState, GroupState, MasterState
3. Update `processMetering()` in RoutingMatrix
4. Add getter method to IRoutingMatrix interface

### Adding New Headroom Mode
1. Add enum value to HeadroomMode
2. Update `getHeadroomCompensation()` switch statement
3. Add unit test for new mode

## Related Diagrams

- [architecture-overview](./architecture-overview.notes.md) - High-level system design
- [data-flow](./data-flow.notes.md) - Sequence of operations

## References

1. ORP121 Audio Backend Refactoring Master Plan
2. ITU-R BS.1770-4 - True-peak metering specification
3. EBU R128 - Loudness normalization standard
