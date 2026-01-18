# ORP121: Audio Backend Refactoring Master Plan

**Status:** Planning Complete
**Priority:** HIGH
**Date:** 2026-01-18
**Author:** Audio Architecture Consultant (Claude)
**Scope:** SDK Core Audio Backend (transport, routing, gain staging, multi-channel)
**Prerequisites:** ORP114 (gain staging bug fixes) should be completed first

---

## Executive Summary

Comprehensive audit of the Orpheus SDK audio backend identified **23 discrete issues** across 6 categories. This document provides a complete implementation plan to close all gaps and achieve production-ready status for broadcast/theater applications.

**Key Findings:**
- **4 Critical bugs** requiring immediate fix (GainSmoother range, tanh discontinuity, callback mutex, pan law)
- **6 Architectural gaps** blocking professional use (stereo routing, multi-channel, output bus wiring)
- **13 Quality improvements** for long-term maintainability (nomenclature, documentation, testing)

**Estimated Total Effort:** 48-64 engineering hours across 4 phases

---

## Table of Contents

1. [Issue Registry](#1-issue-registry)
2. [Phase 1: Critical Bug Fixes](#2-phase-1-critical-bug-fixes)
3. [Phase 2: Stereo Routing Implementation](#3-phase-2-stereo-routing-implementation)
4. [Phase 3: Multi-Channel Architecture](#4-phase-3-multi-channel-architecture)
5. [Phase 4: Quality & Nomenclature](#5-phase-4-quality--nomenclature)
6. [Acceptance Criteria](#6-acceptance-criteria)
7. [Test Plan](#7-test-plan)
8. [Risk Assessment](#8-risk-assessment)
9. [File Index](#9-file-index)

---

## 1. Issue Registry

### 1.1 Critical Issues (P0)

| ID | Issue | Location | Impact | Effort |
|----|-------|----------|--------|--------|
| **C-01** | GainSmoother clips at 0 dB | `gain_smoother.cpp:22` | Cannot apply gain boosts (+1 to +12 dB) | 15 min |
| **C-02** | tanh limiter discontinuity | `routing_matrix.cpp:687-688` | Audible click at 0.9 threshold | 30 min |
| **C-03** | Callback mutex in audio path | `transport_controller.cpp:916-928` | Priority inversion risk | 2 hrs |
| **C-04** | Pan law values discarded | `routing_matrix.cpp:567-571` | No stereo positioning | 1 hr |

### 1.2 Architectural Issues (P1)

| ID | Issue | Location | Impact | Effort |
|----|-------|----------|--------|--------|
| **A-01** | Mono summing before routing | `transport_controller.cpp:460-468` | Stereo width lost | 4 hrs |
| **A-02** | Mono group buffers | `routing_matrix.cpp:580-581` | No true stereo image | 4 hrs |
| **A-03** | Output bus routing ignored | `routing_matrix.cpp:623-626` | Multi-output unusable | 2 hrs |
| **A-04** | No multi-channel format abstraction | N/A (missing) | No surround/Atmos support | 8 hrs |
| **A-05** | No upmix/downmix matrices | N/A (missing) | Cannot convert formats | 4 hrs |
| **A-06** | Double gain application unclear | `transport_controller.cpp` + `routing_matrix.cpp` | Confusing gain staging | 2 hrs |

### 1.3 Quality Issues (P2)

| ID | Issue | Location | Impact | Effort |
|----|-------|----------|--------|--------|
| **Q-01** | Mixed case styles | Throughout headers | Inconsistent API | 4 hrs |
| **Q-02** | Inconsistent terminology | `group` vs `bus`, `handle` vs `id` | Confusing naming | 4 hrs |
| **Q-03** | Sample rate hardcoded | `routing_matrix.cpp:59,708,751` | 48kHz assumption | 1 hr |
| **Q-04** | No true-peak metering | `routing_matrix.h:52` | LUFS mode stub only | 4 hrs |
| **Q-05** | No headroom management | `routing_matrix.cpp:580` | Clip summing can overflow | 2 hrs |
| **Q-06** | Missing gain staging documentation | N/A | Unclear signal flow | 2 hrs |
| **Q-07** | No threading stress tests | `tests/transport/` | Race conditions untested | 4 hrs |
| **Q-08** | No waveform rendering perf tests | `tests/` | UI freeze undetected | 2 hrs |
| **Q-09** | Silent error handling | `AudioEngine.cpp` | Failures unlogged | 2 hrs |
| **Q-10** | Cue buss dynamic allocation | `AudioEngine.cpp:537-579` | Memory fragmentation | 2 hrs |
| **Q-11** | Hard-coded button limits | `AudioEngine.h:308` | 384 max, need 960 | 3 hrs |
| **Q-12** | No CPU/memory profiling API | N/A | Hard to diagnose | 4 hrs |
| **Q-13** | Incomplete Doxygen coverage | Various headers | API underdocumented | 4 hrs |

---

## 2. Phase 1: Critical Bug Fixes

**Objective:** Resolve all P0 issues
**Effort:** 4-6 hours
**Risk:** Low (isolated changes, high test coverage)

### 2.1 Task C-01: Fix GainSmoother Range

**File:** `src/core/routing/gain_smoother.cpp`

**Current Code (Line 22):**
```cpp
target = std::clamp(target, 0.0f, 1.0f);  // BUG: Clips at unity gain
```

**Fixed Code:**
```cpp
// ORP121 C-01: Allow gains up to +12 dB (3.981 linear)
// Rationale: Professional mixing requires boost capability
// 32-bit float provides ~1528 dB headroom, clipping at output stage is sufficient
static constexpr float MAX_LINEAR_GAIN = 3.981071705534972f;  // 10^(12/20) = +12 dB
target = std::clamp(target, 0.0f, MAX_LINEAR_GAIN);
```

**Also Update:**
- `gain_smoother.h` - Add `static constexpr float MAX_GAIN_DB = 12.0f;`
- `routing_matrix.cpp:127` - Verify gain range matches (-60 to +12 dB)

**Acceptance Criteria:**
- [ ] `setChannelGain(ch, 6.0f)` results in 2x linear gain (not 1.0)
- [ ] `setChannelGain(ch, 12.0f)` results in ~4x linear gain
- [ ] Existing unit tests pass
- [ ] No audible artifacts in gain transitions

---

### 2.2 Task C-02: Fix tanh Limiter Discontinuity

**File:** `src/core/routing/routing_matrix.cpp`

**Current Code (Lines 687-688):**
```cpp
if (std::abs(sample) > 0.9f) {
  sample = std::tanh(sample * 0.9f) / 0.9f;  // BUG: Discontinuity at 0.9
}
```

**Problem Analysis:**
- At `sample = 0.9`: Output = 0.9 (unchanged, just above threshold)
- At `sample = 0.9001`: Output = tanh(0.81009) / 0.9 = 0.796 (jump down!)
- This creates a **discontinuity of ~0.104** causing audible clicks

**Fixed Code:**
```cpp
// ORP121 C-02: Continuous soft-knee limiter
// Uses soft-knee compression starting at -2 dBFS (0.794) with smooth transition
// Prevents discontinuity while maintaining headroom protection
static constexpr float THRESHOLD = 0.794f;     // -2 dBFS
static constexpr float KNEE_WIDTH = 0.3f;      // Soft knee range
static constexpr float CEILING = 0.9999f;      // Final hard limit

float abs_sample = std::abs(sample);
if (abs_sample > THRESHOLD) {
  // Soft-knee: tanh saturation with continuous curve
  float excess = abs_sample - THRESHOLD;
  float knee_ratio = excess / KNEE_WIDTH;
  float compressed = THRESHOLD + std::tanh(knee_ratio) * KNEE_WIDTH;
  sample = std::copysign(std::min(compressed, CEILING), sample);
}
```

**Mathematical Verification:**
- At threshold (0.794): excess = 0, tanh(0) = 0, output = 0.794 (continuous)
- At 1.0: excess = 0.206, tanh(0.687) = 0.596, output = 0.794 + 0.179 = 0.973
- At 2.0: excess = 1.206, tanh(4.02) = 0.999, output = 0.794 + 0.300 = 1.094 -> clamped to 0.9999
- **Curve is C1 continuous** (no jumps, smooth derivative)

**Acceptance Criteria:**
- [ ] No audible click when signal crosses 0.9 threshold
- [ ] Output never exceeds +-1.0
- [ ] Sine wave at 1.5x amplitude produces smooth saturation curve
- [ ] THD measurement shows gradual increase (no discontinuity spike)

---

### 2.3 Task C-03: Lock-Free Callback Queue

**File:** `src/core/transport/transport_controller.cpp`

**Current Code (Lines 916-928):**
```cpp
void TransportController::postCallback(std::function<void()> callback) {
  std::lock_guard<std::mutex> lock(m_callbackMutex);  // BUG: Mutex in audio path
  m_callbackQueue.push(std::move(callback));
}

void TransportController::processCallbacks() {
  std::lock_guard<std::mutex> lock(m_callbackMutex);
  while (!m_callbackQueue.empty()) {
    auto callback = std::move(m_callbackQueue.front());
    m_callbackQueue.pop();
    callback();
  }
}
```

**Problem:**
- `postCallback()` called from audio thread (processAudio)
- `processCallbacks()` called from UI thread (timerCallback)
- Mutex contention can cause priority inversion (audio thread waits for UI thread)

**Fixed Code:**
```cpp
// ORP121 C-03: Lock-free SPSC callback queue
// Audio thread writes, UI thread reads - no contention

// In header (transport_controller.h):
static constexpr size_t CALLBACK_QUEUE_SIZE = 256;  // Power of 2 for masking
std::array<std::function<void()>, CALLBACK_QUEUE_SIZE> m_callbackRing;
std::atomic<size_t> m_callbackWriteIndex{0};
std::atomic<size_t> m_callbackReadIndex{0};

// In implementation:
void TransportController::postCallback(std::function<void()> callback) {
  // Audio thread: Write to ring buffer (lock-free)
  size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_relaxed);
  size_t nextIdx = (writeIdx + 1) & (CALLBACK_QUEUE_SIZE - 1);  // Mask for wrap

  // Check if queue full (read index caught up)
  if (nextIdx == m_callbackReadIndex.load(std::memory_order_acquire)) {
    // Queue full - drop callback (better than blocking audio thread)
    // TODO: Increment dropped callback counter for diagnostics
    return;
  }

  m_callbackRing[writeIdx] = std::move(callback);
  m_callbackWriteIndex.store(nextIdx, std::memory_order_release);
}

void TransportController::processCallbacks() {
  // UI thread: Read from ring buffer (lock-free)
  size_t readIdx = m_callbackReadIndex.load(std::memory_order_relaxed);
  size_t writeIdx = m_callbackWriteIndex.load(std::memory_order_acquire);

  while (readIdx != writeIdx) {
    auto& callback = m_callbackRing[readIdx];
    if (callback) {
      callback();
      callback = nullptr;  // Clear slot
    }
    readIdx = (readIdx + 1) & (CALLBACK_QUEUE_SIZE - 1);
  }

  m_callbackReadIndex.store(readIdx, std::memory_order_release);
}
```

**Memory Ordering Rationale:**
- `relaxed` for same-thread reads (writeIdx in postCallback, readIdx in processCallbacks)
- `acquire` for cross-thread reads (writeIdx in processCallbacks, readIdx in postCallback)
- `release` for writes (ensures callback data visible before index update)

**Acceptance Criteria:**
- [ ] No mutex in audio path (verify with ThreadSanitizer)
- [ ] Callbacks still delivered correctly (UI receives clip state changes)
- [ ] Queue overflow handled gracefully (no crash, diagnostic counter)
- [ ] Stress test: 10,000 callbacks/second without drops

---

### 2.4 Task C-04: Wire Pan Law Application

**File:** `src/core/routing/routing_matrix.cpp`

**Current Code (Lines 567-571):**
```cpp
float pan_left = channel.pan_left->process();
float pan_right = channel.pan_right->process();
(void)pan_left;  // Unused for now
(void)pan_right; // Unused for now
```

**Problem:** Pan smoothers compute values but results are discarded. All clips play center.

**Fixed Code:**
```cpp
// ORP121 C-04: Apply constant-power pan law
// Pan position: -1.0 (left) to +1.0 (right), 0.0 = center
// Constant-power: -3 dB at center (sqrt(0.5) ≈ 0.707)
// This ensures perceived loudness is constant across pan positions

float pan_left = channel.pan_left->process();    // Left channel coefficient
float pan_right = channel.pan_right->process();  // Right channel coefficient

// Apply pan to mono source -> stereo output
// For now, source is mono (sample), output is stereo (group_buffer_L/R)
float sample_L = sample * pan_left;
float sample_R = sample * pan_right;

// Sum into stereo group buffers (see Task A-02 for buffer changes)
group_buffer_L[frame] += sample_L;
group_buffer_R[frame] += sample_R;
```

**Pan Coefficient Calculation** (update `setChannelPan()`):
```cpp
SessionGraphError RoutingMatrix::setChannelPan(uint8_t channel_index, float pan) {
  // pan: -1.0 (left) to +1.0 (right)
  pan = std::clamp(pan, -1.0f, 1.0f);

  // Constant-power pan law: L^2 + R^2 = 1
  // At center (pan=0): L = R = sqrt(0.5) ≈ 0.707 (-3 dB each)
  // At hard left (pan=-1): L = 1.0, R = 0.0
  // At hard right (pan=+1): L = 0.0, R = 1.0

  float angle = (pan + 1.0f) * 0.25f * M_PI;  // 0 to pi/2
  float pan_left = std::cos(angle);
  float pan_right = std::sin(angle);

  auto& channel = m_channels[channel_index];
  channel.pan_left->setTarget(pan_left);
  channel.pan_right->setTarget(pan_right);

  return SessionGraphError::OK;
}
```

**Note:** This task requires stereo group buffers (Task A-02). If implementing before A-02, use temporary stereo output.

**Acceptance Criteria:**
- [ ] Pan = -1.0 outputs only to left channel
- [ ] Pan = +1.0 outputs only to right channel
- [ ] Pan = 0.0 outputs equal level to both channels
- [ ] Panning a clip preserves perceived loudness (constant power)
- [ ] Pan changes are smoothed (no zipper noise)

---

## 3. Phase 2: Stereo Routing Implementation

**Objective:** Enable true stereo signal flow through the routing matrix
**Effort:** 12-16 hours
**Risk:** Medium (structural changes, requires thorough testing)
**Dependencies:** Phase 1 complete

### 3.1 Task A-01: Preserve Stereo From Source

**File:** `src/core/transport/transport_controller.cpp`

**Current Code (Lines 460-468):**
```cpp
// Mix all file channels to mono for routing
float monoSample = 0.0f;
for (size_t ch = 0; ch < numFileChannels; ++ch) {
  monoSample += clipReadBuffer[srcIndex];
}
monoSample /= static_cast<float>(numFileChannels);  // Average to mono
```

**Problem:** Multi-channel files collapsed to mono before any processing.

**Fixed Code:**
```cpp
// ORP121 A-01: Preserve source channel separation
// Files output to L/R clip buffers based on source format:
// - Mono: Duplicate to both L/R
// - Stereo: Direct mapping L->L, R->R
// - Multi-channel: Use standard downmix or direct routing

// New member variables (in transport_controller.h):
// std::vector<std::array<std::vector<float>, 2>> m_clipChannelBuffersStereo;

// Process stereo-aware output
float gainedSample_L, gainedSample_R;

if (numFileChannels == 1) {
  // Mono source: Duplicate to both channels (center phantom image)
  float mono = clipReadBuffer[frame] * gain;
  gainedSample_L = mono;
  gainedSample_R = mono;
} else if (numFileChannels == 2) {
  // Stereo source: Preserve L/R separation
  gainedSample_L = clipReadBuffer[frame * 2 + 0] * gain;
  gainedSample_R = clipReadBuffer[frame * 2 + 1] * gain;
} else {
  // Multi-channel (>2): Apply ITU-R BS.775 downmix to stereo
  // L_out = L + 0.707*C + 0.707*Ls
  // R_out = R + 0.707*C + 0.707*Rs
  // (Simplified for 5.1: L, R, C, LFE, Ls, Rs)
  gainedSample_L = applyDownmixLeft(clipReadBuffer, frame, numFileChannels) * gain;
  gainedSample_R = applyDownmixRight(clipReadBuffer, frame, numFileChannels) * gain;
}

// Write to stereo clip buffers
m_clipChannelBuffersStereo[i][0][frame] = gainedSample_L;
m_clipChannelBuffersStereo[i][1][frame] = gainedSample_R;
```

**Downmix Helper Functions:**
```cpp
// ITU-R BS.775-3 standard downmix coefficients
static constexpr float DOWNMIX_CENTER = 0.7071067811865476f;  // sqrt(0.5)
static constexpr float DOWNMIX_SURROUND = 0.7071067811865476f;

float TransportController::applyDownmixLeft(const float* src, size_t frame, size_t numCh) {
  if (numCh < 6) {
    // Quad or less: simple average
    float sum = 0.0f;
    for (size_t ch = 0; ch < numCh; ch += 2) sum += src[frame * numCh + ch];
    return sum / ((numCh + 1) / 2);
  }
  // 5.1/7.1 layout: L=0, R=1, C=2, LFE=3, Ls=4, Rs=5
  float L = src[frame * numCh + 0];
  float C = src[frame * numCh + 2];
  float Ls = src[frame * numCh + 4];
  return L + DOWNMIX_CENTER * C + DOWNMIX_SURROUND * Ls;
}

float TransportController::applyDownmixRight(const float* src, size_t frame, size_t numCh) {
  if (numCh < 6) {
    float sum = 0.0f;
    for (size_t ch = 1; ch < numCh; ch += 2) sum += src[frame * numCh + ch];
    return sum / (numCh / 2);
  }
  float R = src[frame * numCh + 1];
  float C = src[frame * numCh + 2];
  float Rs = src[frame * numCh + 5];
  return R + DOWNMIX_CENTER * C + DOWNMIX_SURROUND * Rs;
}
```

**Acceptance Criteria:**
- [ ] Mono file plays as centered phantom image
- [ ] Stereo file preserves L/R separation
- [ ] 5.1 file downmixes correctly to stereo
- [ ] No change in gain with format conversion

---

### 3.2 Task A-02: Stereo Group Buffers

**File:** `src/core/routing/routing_matrix.cpp`

**Current Code:**
```cpp
// Mono group buffer
std::vector<std::vector<float>> m_group_buffers;  // [group][frame]
```

**Fixed Code:**
```cpp
// ORP121 A-02: Stereo group buffers
// Each group has L/R channel pair for true stereo imaging

// In routing_matrix.h:
struct GroupBuffer {
  std::vector<float> left;   // Left channel samples
  std::vector<float> right;  // Right channel samples

  void resize(size_t frames) {
    left.resize(frames, 0.0f);
    right.resize(frames, 0.0f);
  }

  void clear() {
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
  }
};

std::vector<GroupBuffer> m_group_buffers;

// In processRouting():
// Clear group buffers
for (auto& group : m_group_buffers) {
  group.clear();
}

// Sum channels into groups (stereo-aware)
for (uint8_t ch = 0; ch < m_config.num_channels; ++ch) {
  auto& channel = m_channels[ch];
  if (channel.group_index == UNASSIGNED_GROUP) continue;

  auto& group = m_group_buffers[channel.group_index];

  // Get stereo input (from transport controller)
  const float* input_L = channel_inputs_L[ch];
  const float* input_R = channel_inputs_R[ch];
  if (!input_L || !input_R) continue;

  for (uint32_t frame = 0; frame < num_frames; ++frame) {
    float channel_gain = channel.gain_smoother->process();
    float pan_L = channel.pan_left->process();
    float pan_R = channel.pan_right->process();

    // Apply gain and pan
    float sample_L = input_L[frame] * channel_gain * pan_L;
    float sample_R = input_R[frame] * channel_gain * pan_R;

    // Check mute/solo
    if (!isChannelEffectivelyMuted(ch)) {
      group.left[frame] += sample_L;
      group.right[frame] += sample_R;
    }
  }
}
```

**Acceptance Criteria:**
- [ ] Group outputs are stereo (L/R pair)
- [ ] Panning affects L/R balance correctly
- [ ] Multiple clips sum correctly to group
- [ ] Group gain applies equally to L/R

---

### 3.3 Task A-03: Wire Output Bus Routing

**File:** `src/core/routing/routing_matrix.cpp`

**Current Code (Lines 623-626):**
```cpp
// All groups sum to outputs 0-1 regardless of output_bus setting
for (uint8_t out = 0; out < std::min(config.num_outputs, (uint8_t)2); ++out) {
  master_output[out][frame] += sample;
}
```

**Fixed Code:**
```cpp
// ORP121 A-03: Route groups to configured output buses
// GroupConfig.output_bus determines which stereo pair receives the group

// Sum groups into master outputs based on output_bus assignment
for (uint8_t g = 0; g < m_config.num_groups; ++g) {
  auto& group = m_groups[g];
  auto& buffer = m_group_buffers[g];

  if (group.config.mute || isGroupEffectivelyMuted(g)) continue;

  // Determine output channel pair from output_bus
  // output_bus 0 = channels 0-1 (stereo pair 1)
  // output_bus 1 = channels 2-3 (stereo pair 2)
  // etc.
  uint8_t out_L = group.config.output_bus * 2;
  uint8_t out_R = group.config.output_bus * 2 + 1;

  // Validate output channels exist
  if (out_R >= m_config.num_outputs) continue;

  float group_gain = group.gain_smoother->process();

  for (uint32_t frame = 0; frame < num_frames; ++frame) {
    float sample_L = buffer.left[frame] * group_gain;
    float sample_R = buffer.right[frame] * group_gain;

    master_output[out_L][frame] += sample_L;
    master_output[out_R][frame] += sample_R;
  }
}
```

**Acceptance Criteria:**
- [ ] Group with output_bus=0 routes to channels 0-1
- [ ] Group with output_bus=1 routes to channels 2-3
- [ ] Groups with different buses don't interfere
- [ ] Invalid output_bus (>= num_outputs/2) handled gracefully

---

### 3.4 Task A-06: Document Gain Staging Model

**File:** Create `docs/orp/GAIN_STAGING.md`

**Content:**
```markdown
# Orpheus SDK Gain Staging Model

## Signal Flow

```
Source File (0 dBFS normalized)
       │
       ▼
┌──────────────────────┐
│ Clip Gain            │  User setting: -inf to +12 dB
│ (clip.gainLinear)    │  Applied in: TransportController::processAudio()
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Fade Gain            │  Envelope: 0.0 to 1.0
│ (calculateFadeGain)  │  Types: FadeIn, FadeOut, RestartCrossfade, StopFade
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Channel Gain         │  User setting: -inf to +12 dB
│ (channel.gain)       │  Applied in: RoutingMatrix::processRouting()
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Pan Law              │  Position: -1.0 to +1.0
│ (constant power)     │  L/R coefficients sum to constant energy
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Group Summing        │  Additive (linear sum)
│ (group_buffer +=)    │  No automatic gain compensation
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Group Gain           │  User setting: -inf to +12 dB
│ (group.gain)         │  Applied per-group before master sum
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Master Summing       │  Additive (linear sum)
│ (master_output +=)   │  Groups → Output buses
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Master Gain          │  User setting: -inf to +12 dB
│ (master.gain)        │  Final output level control
└──────────────────────┘
       │
       ▼
┌──────────────────────┐
│ Soft Limiter         │  Threshold: -2 dBFS
│ (tanh soft-knee)     │  Ceiling: -0.001 dBFS
└──────────────────────┘
       │
       ▼
Output (-1.0 to +1.0)
```

## Gain Ranges

| Stage | Min | Max | Unit |
|-------|-----|-----|------|
| Clip Gain | -inf | +12 | dB |
| Channel Gain | -inf | +12 | dB |
| Group Gain | -inf | +12 | dB |
| Master Gain | -inf | +12 | dB |
| Pan | -1.0 | +1.0 | position |

## Headroom Considerations

32-bit float provides ~1528 dB of dynamic range. Internal headroom is effectively unlimited.

**Clipping occurs only at:**
1. Soft limiter (intentional, gradual saturation)
2. DAC output (hardware, must stay within +-1.0)

**Recommended gain structure:**
- Source files: Normalized to -6 dBFS peak
- Clip gains: Unity (0 dB) unless adjustment needed
- Channel gains: Unity (0 dB)
- Group gains: Adjust for submix balance (-6 to 0 dB)
- Master gain: Final trim (-3 to 0 dB)

This structure provides ~15 dB of headroom at each summing stage.
```

**Acceptance Criteria:**
- [ ] Document accurately reflects implementation
- [ ] All gain stages are listed with ranges
- [ ] Headroom guidance is correct for 32-bit float

---

## 4. Phase 3: Multi-Channel Architecture

**Objective:** Enable surround sound and immersive audio formats
**Effort:** 16-20 hours
**Risk:** Medium-High (significant API changes)
**Dependencies:** Phase 2 complete

### 4.1 Task A-04: Channel Format Abstraction

**File:** Create `include/orpheus/channel_format.h`

```cpp
// ORP121 A-04: Multi-channel format abstraction
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace orpheus {

/// Standard channel layouts
enum class ChannelLayout : uint8_t {
  Mono = 1,
  Stereo = 2,
  LCR = 3,           // Left, Center, Right (theater)
  Quad = 4,          // L, R, Ls, Rs (legacy surround)
  Surround_5_0 = 5,  // L, R, C, Ls, Rs
  Surround_5_1 = 6,  // L, R, C, LFE, Ls, Rs
  Surround_7_1 = 8,  // L, R, C, LFE, Ls, Rs, Lb, Rb
  Atmos_5_1_2 = 8,   // 5.1 + 2 height (Ltm, Rtm)
  Atmos_5_1_4 = 10,  // 5.1 + 4 height
  Atmos_7_1_4 = 12,  // 7.1 + 4 height
  Ambisonics_FOA = 4,   // First-order: W, X, Y, Z (ACN/SN3D)
  Ambisonics_HOA2 = 9,  // Second-order
  Ambisonics_HOA3 = 16, // Third-order
  Custom = 255       // User-defined
};

/// Speaker positions for channel mapping
enum class Speaker : uint8_t {
  L = 0,    // Left
  R = 1,    // Right
  C = 2,    // Center
  LFE = 3,  // Low Frequency Effects
  Ls = 4,   // Left Surround
  Rs = 5,   // Right Surround
  Lb = 6,   // Left Back
  Rb = 7,   // Right Back
  Ltf = 8,  // Left Top Front
  Rtf = 9,  // Right Top Front
  Ltb = 10, // Left Top Back
  Rtb = 11, // Right Top Back
  // Ambisonics (ACN order)
  W = 0,    // Omnidirectional
  Y = 1,    // Front-back
  Z = 2,    // Up-down
  X = 3,    // Left-right
  // Higher-order ambisonics continue ACN sequence...
  None = 255
};

/// Channel format descriptor
struct ChannelFormat {
  ChannelLayout layout;
  uint8_t num_channels;
  std::array<Speaker, 32> channel_map;  // Speaker assignment per channel
  std::string name;                     // Human-readable name

  /// Check if format is bed-based (speaker feeds)
  bool isBedFormat() const {
    return layout != ChannelLayout::Ambisonics_FOA &&
           layout != ChannelLayout::Ambisonics_HOA2 &&
           layout != ChannelLayout::Ambisonics_HOA3;
  }

  /// Check if format is scene-based (ambisonics)
  bool isAmbisonics() const {
    return !isBedFormat();
  }

  // Factory methods
  static ChannelFormat Mono();
  static ChannelFormat Stereo();
  static ChannelFormat Surround51();
  static ChannelFormat Surround71();
  static ChannelFormat Atmos714();
  static ChannelFormat Ambisonics(uint8_t order);
  static ChannelFormat Custom(uint8_t numChannels);
};

/// Downmix/upmix coefficient matrix
/// Usage: output[out_ch] = sum(input[in_ch] * coefficients[out_ch][in_ch])
struct MixMatrix {
  uint8_t input_channels;
  uint8_t output_channels;
  std::array<std::array<float, 32>, 32> coefficients;  // [out][in]

  /// Apply mix matrix to audio buffer
  void apply(const float* const* input, float** output, size_t num_frames) const;

  // Standard matrices
  static MixMatrix Downmix_51_to_Stereo();
  static MixMatrix Downmix_71_to_Stereo();
  static MixMatrix Downmix_71_to_51();
  static MixMatrix Upmix_Stereo_to_51();  // Phantom center, no rear
  static MixMatrix Upmix_Mono_to_Stereo();
};

} // namespace orpheus
```

**Implementation File:** `src/core/channel_format.cpp`

```cpp
#include <orpheus/channel_format.h>
#include <cmath>

namespace orpheus {

// ITU-R BS.775-3 coefficients
static constexpr float K_CENTER = 0.7071067811865476f;    // sqrt(0.5), -3 dB
static constexpr float K_SURROUND = 0.7071067811865476f;  // sqrt(0.5), -3 dB
static constexpr float K_LFE = 0.0f;                       // LFE typically omitted in downmix

ChannelFormat ChannelFormat::Mono() {
  ChannelFormat fmt;
  fmt.layout = ChannelLayout::Mono;
  fmt.num_channels = 1;
  fmt.channel_map[0] = Speaker::C;  // Mono is conceptually center
  fmt.name = "Mono";
  return fmt;
}

ChannelFormat ChannelFormat::Stereo() {
  ChannelFormat fmt;
  fmt.layout = ChannelLayout::Stereo;
  fmt.num_channels = 2;
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.name = "Stereo";
  return fmt;
}

ChannelFormat ChannelFormat::Surround51() {
  ChannelFormat fmt;
  fmt.layout = ChannelLayout::Surround_5_1;
  fmt.num_channels = 6;
  fmt.channel_map[0] = Speaker::L;
  fmt.channel_map[1] = Speaker::R;
  fmt.channel_map[2] = Speaker::C;
  fmt.channel_map[3] = Speaker::LFE;
  fmt.channel_map[4] = Speaker::Ls;
  fmt.channel_map[5] = Speaker::Rs;
  fmt.name = "5.1 Surround";
  return fmt;
}

MixMatrix MixMatrix::Downmix_51_to_Stereo() {
  MixMatrix m;
  m.input_channels = 6;
  m.output_channels = 2;
  std::fill(&m.coefficients[0][0], &m.coefficients[0][0] + 32*32, 0.0f);

  // L_out = L + K_CENTER*C + K_SURROUND*Ls
  m.coefficients[0][0] = 1.0f;       // L -> L
  m.coefficients[0][2] = K_CENTER;   // C -> L
  m.coefficients[0][4] = K_SURROUND; // Ls -> L

  // R_out = R + K_CENTER*C + K_SURROUND*Rs
  m.coefficients[1][1] = 1.0f;       // R -> R
  m.coefficients[1][2] = K_CENTER;   // C -> R
  m.coefficients[1][5] = K_SURROUND; // Rs -> R

  return m;
}

void MixMatrix::apply(const float* const* input, float** output, size_t num_frames) const {
  for (size_t frame = 0; frame < num_frames; ++frame) {
    for (uint8_t out_ch = 0; out_ch < output_channels; ++out_ch) {
      float sum = 0.0f;
      for (uint8_t in_ch = 0; in_ch < input_channels; ++in_ch) {
        sum += input[in_ch][frame] * coefficients[out_ch][in_ch];
      }
      output[out_ch][frame] = sum;
    }
  }
}

} // namespace orpheus
```

**Acceptance Criteria:**
- [ ] All standard formats constructible via factory methods
- [ ] MixMatrix correctly applies ITU-R BS.775 downmix
- [ ] Coefficient matrices are symmetric where applicable
- [ ] Unit tests verify all conversions

---

### 4.2 Task A-05: Integrate Mix Matrices into Routing

**File:** `src/core/routing/routing_matrix.cpp`

**Changes:**
1. Add `ChannelFormat` to `ChannelConfig` and `GroupConfig`
2. Store per-channel input format
3. Apply format conversion during channel→group summing
4. Apply format conversion during group→output summing

```cpp
// In routing_matrix.h:
struct ChannelConfig {
  // ... existing fields ...
  ChannelFormat input_format;   // Source file format
  ChannelFormat output_format;  // Format for group summing (usually stereo)
  MixMatrix format_converter;   // Pre-computed conversion matrix
};

// In processRouting():
for (uint8_t ch = 0; ch < m_config.num_channels; ++ch) {
  auto& channel = m_channels[ch];

  // Apply format conversion if needed
  if (channel.input_format.num_channels != channel.output_format.num_channels) {
    channel.format_converter.apply(
      channel_inputs[ch],       // Multi-channel input
      temp_converted_buffer,    // Stereo (or group format) output
      num_frames
    );
    // Use converted buffer for further processing
    input_for_summing = temp_converted_buffer;
  } else {
    input_for_summing = channel_inputs[ch];
  }

  // Continue with gain/pan/sum...
}
```

**Acceptance Criteria:**
- [ ] 5.1 source file correctly downmixes to stereo group
- [ ] Stereo source file passes through unchanged to stereo group
- [ ] Mono source file upmixes to stereo (phantom center)
- [ ] Format conversion is seamless (no glitches)

---

## 5. Phase 4: Quality & Nomenclature

**Objective:** Improve code quality, consistency, and maintainability
**Effort:** 16-20 hours
**Risk:** Low (non-breaking changes, documentation)

### 5.1 Task Q-01: Standardize Case Style

**Standard:** `snake_case` for member variables, `PascalCase` for types

**Files to Update:**
- `include/orpheus/transport_controller.h`
- `include/orpheus/routing_matrix.h`
- `include/orpheus/clip_routing.h`
- `src/core/transport/transport_controller.cpp`
- `src/core/routing/routing_matrix.cpp`

**Before:**
```cpp
struct AudioFileEntry {
  int64_t trimInSamples;    // camelCase
  double fadeInSeconds;     // camelCase
  float gainDb;             // camelCase (inconsistent dB casing)
};
```

**After:**
```cpp
struct AudioFileEntry {
  int64_t trim_in_samples;   // snake_case
  double fade_in_seconds;    // snake_case
  float gain_dB;             // snake_case with proper unit suffix
};
```

**Migration Strategy:**
1. Create type aliases for backward compatibility
2. Mark old names `[[deprecated]]`
3. Update internal code to use new names
4. Update tests and examples
5. Remove aliases in next major version

---

### 5.2 Task Q-02: Normalize Terminology

| Old Term | New Term | Rationale |
|----------|----------|-----------|
| `group_index` | `bus_index` | Industry standard (Pro Tools, Logic, Dante) |
| `ClipHandle` | `ClipId` | "Handle" implies ownership; "Id" is identifier |
| `num_channels` (input) | `num_inputs` | Disambiguate from L/R channels |
| `num_outputs` | `num_output_channels` | Explicit meaning |
| `UNASSIGNED_GROUP` | `BUS_NONE` | Shorter, clearer |
| `stopOthersOnPlay` | `exclusive_mode` | Clearer intent |
| `m_activeClipCount` | `m_active_voice_count` | Reflects multi-voice |

**Migration:** Same as Q-01 (aliases + deprecation)

---

### 5.3 Task Q-03: Remove Hardcoded Sample Rate

**Files:** `src/core/routing/routing_matrix.cpp`

**Current Code (Line 59):**
```cpp
m_gainSmoothingTimeMs = 10.0f;  // Assumes 48kHz for sample count
```

**Fixed Code:**
```cpp
// Pass sample rate to initialize()
SessionGraphError RoutingMatrix::initialize(const RoutingConfig& config, uint32_t sample_rate) {
  m_sampleRate = sample_rate;
  m_gainSmoothingSamples = static_cast<uint32_t>(
    config.gain_smoothing_ms * 0.001f * static_cast<float>(sample_rate)
  );
  // ...
}
```

---

### 5.4 Task Q-04: Implement True-Peak Metering

**File:** `src/core/routing/routing_matrix.cpp`

**Current Code:**
```cpp
enum class MeteringMode : uint8_t {
  Peak = 0,
  RMS = 1,
  TruePeak = 2,  // Stub only
  LUFS = 3       // Stub only
};
```

**Implementation for TruePeak (ITU-R BS.1770-4):**
```cpp
// True-peak requires 4x oversampling
class TruePeakMeter {
  static constexpr int OVERSAMPLE_FACTOR = 4;
  std::array<float, 48> m_filterCoeffs;  // FIR filter for interpolation
  std::array<float, 48> m_history;

public:
  TruePeakMeter() {
    // Initialize ITU-R BS.1770 filter coefficients
    initializeFilter();
  }

  float process(float sample) {
    // Shift history
    std::memmove(&m_history[1], &m_history[0], 47 * sizeof(float));
    m_history[0] = sample;

    // Calculate 4 interpolated samples
    float peak = 0.0f;
    for (int phase = 0; phase < OVERSAMPLE_FACTOR; ++phase) {
      float interpolated = 0.0f;
      for (int tap = 0; tap < 12; ++tap) {
        interpolated += m_history[tap] * m_filterCoeffs[phase * 12 + tap];
      }
      peak = std::max(peak, std::abs(interpolated));
    }

    return peak;
  }
};
```

---

### 5.5 Task Q-05: Add Headroom Management

**File:** `include/orpheus/routing_matrix.h`

```cpp
struct RoutingConfig {
  // ... existing fields ...

  /// Automatic gain compensation mode
  enum class HeadroomMode : uint8_t {
    None = 0,           ///< No compensation (sum can exceed 0 dBFS)
    PerGroup = 1,       ///< Divide by number of channels per group
    Global = 2,         ///< Divide by total active channels
    Logarithmic = 3     ///< -3 dB per doubling of channels
  };

  HeadroomMode headroom_mode = HeadroomMode::None;  ///< Default: no compensation
};
```

**Implementation:**
```cpp
// In processRouting(), calculate compensation factor
float getHeadroomCompensation(uint8_t group_index) const {
  switch (m_config.headroom_mode) {
    case HeadroomMode::None:
      return 1.0f;
    case HeadroomMode::PerGroup: {
      int active = countActiveChannelsInGroup(group_index);
      return active > 0 ? 1.0f / static_cast<float>(active) : 1.0f;
    }
    case HeadroomMode::Global: {
      int active = countTotalActiveChannels();
      return active > 0 ? 1.0f / static_cast<float>(active) : 1.0f;
    }
    case HeadroomMode::Logarithmic: {
      int active = countActiveChannelsInGroup(group_index);
      // -3 dB per doubling: 1/sqrt(n)
      return active > 0 ? 1.0f / std::sqrt(static_cast<float>(active)) : 1.0f;
    }
  }
  return 1.0f;
}
```

---

### 5.6 Tasks Q-06 through Q-13: Documentation and Testing

| Task | Deliverable |
|------|-------------|
| Q-06 | `docs/orp/GAIN_STAGING.md` (see Phase 2) |
| Q-07 | `tests/transport/threading_stress_test.cpp` |
| Q-08 | `tests/performance/waveform_benchmark.cpp` |
| Q-09 | Logging framework integration in AudioEngine |
| Q-10 | Fixed-size Cue buss pool in AudioEngine |
| Q-11 | Dynamic button count support |
| Q-12 | `include/orpheus/performance_monitor.h` API |
| Q-13 | Doxygen comments on all public APIs |

---

## 6. Acceptance Criteria

### Phase 1 Complete When:
- [ ] All unit tests pass
- [ ] GainSmoother allows +12 dB boost
- [ ] No audible artifacts from limiter
- [ ] ThreadSanitizer clean (no mutex in audio path)
- [ ] Pan positions audibly affect L/R balance

### Phase 2 Complete When:
- [ ] Stereo files preserve L/R separation
- [ ] Multi-channel files downmix correctly
- [ ] Groups output stereo pairs
- [ ] Output bus routing works (group → specific output pair)
- [ ] GAIN_STAGING.md document complete

### Phase 3 Complete When:
- [ ] ChannelFormat API implemented
- [ ] MixMatrix applies ITU-R BS.775 coefficients
- [ ] 5.1 → stereo downmix verified
- [ ] Ambisonics to stereo conversion works
- [ ] Format negotiation automatic

### Phase 4 Complete When:
- [ ] snake_case applied to all new code
- [ ] Deprecated aliases for old names
- [ ] Sample rate parameterized
- [ ] True-peak metering functional
- [ ] Headroom modes implemented
- [ ] 90%+ Doxygen coverage on public APIs
- [ ] Threading stress tests pass

---

## 7. Test Plan

### 7.1 Unit Tests (New)

```cpp
// tests/routing/gain_smoother_test.cpp
TEST(GainSmoother, AllowsPositiveGainBoost) {
  GainSmoother smoother(48000, 10.0f);  // 10ms smoothing
  smoother.setTarget(dbToLinear(6.0f));  // +6 dB

  // Process enough samples for smoothing to complete
  for (int i = 0; i < 480; ++i) {
    smoother.process();
  }

  EXPECT_NEAR(smoother.process(), dbToLinear(6.0f), 0.01f);
}

TEST(GainSmoother, AllowsMaxGainBoost) {
  GainSmoother smoother(48000, 10.0f);
  smoother.setTarget(dbToLinear(12.0f));  // +12 dB = 3.981

  for (int i = 0; i < 480; ++i) {
    smoother.process();
  }

  EXPECT_NEAR(smoother.process(), 3.981f, 0.01f);
}

// tests/routing/limiter_test.cpp
TEST(SoftLimiter, ContinuousAtThreshold) {
  // Verify no discontinuity at 0.9 threshold
  float below = applySoftLimit(0.89f);
  float at = applySoftLimit(0.90f);
  float above = applySoftLimit(0.91f);

  // Should be monotonically increasing with no jump
  EXPECT_LT(below, at);
  EXPECT_LT(at, above);
  EXPECT_LT(above - at, 0.02f);  // No large jump
}

// tests/routing/pan_law_test.cpp
TEST(PanLaw, ConstantPower) {
  RoutingMatrix routing;
  routing.initialize({.num_channels = 1, .num_groups = 1, .num_outputs = 2});

  // Pan center: L and R should both be ~0.707 (-3 dB)
  routing.setChannelPan(0, 0.0f);

  float pan_L, pan_R;
  routing.getChannelPanCoefficients(0, pan_L, pan_R);

  EXPECT_NEAR(pan_L, 0.707f, 0.01f);
  EXPECT_NEAR(pan_R, 0.707f, 0.01f);

  // Verify constant power: L^2 + R^2 = 1
  EXPECT_NEAR(pan_L * pan_L + pan_R * pan_R, 1.0f, 0.01f);
}

// tests/routing/stereo_routing_test.cpp
TEST(StereoRouting, PreservesStereoWidth) {
  TransportController transport;
  // Load stereo test file (L=sine, R=silence)
  auto handle = transport.registerClipAudio(1, "test_stereo_lr.wav");

  float output_L[256], output_R[256];
  float* outputs[2] = {output_L, output_R};

  transport.startClip(handle);
  transport.processAudio(outputs, 2, 256);

  // Left channel should have signal
  float rms_L = calculateRMS(output_L, 256);
  // Right channel should be silent
  float rms_R = calculateRMS(output_R, 256);

  EXPECT_GT(rms_L, 0.1f);
  EXPECT_LT(rms_R, 0.001f);  // Essentially silent
}
```

### 7.2 Integration Tests

```cpp
// tests/integration/multi_channel_test.cpp
TEST(MultiChannel, Downmix51ToStereo) {
  TransportController transport;
  RoutingMatrix routing;

  routing.initialize({.num_channels = 1, .num_groups = 1, .num_outputs = 2});

  // Load 5.1 test file
  auto handle = transport.registerClipAudio(1, "test_51_surround.wav");
  transport.startClip(handle);

  // Process through routing
  float* clip_outputs[6];  // 5.1 channels
  float* master_output[2]; // Stereo

  transport.processAudio(clip_outputs, 6, 256);
  routing.processRouting(clip_outputs, master_output, 256);

  // Verify downmix coefficients applied
  // C channel should appear in both L and R at -3 dB
  // Ls should appear in L only
  // Rs should appear in R only
  // (Verify with known test tones)
}
```

### 7.3 Stress Tests

```cpp
// tests/stress/callback_queue_test.cpp
TEST(CallbackQueue, HighThroughput) {
  TransportController transport;
  std::atomic<int> callback_count{0};

  // Simulate 10,000 callbacks/second for 1 second
  auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(1)) {
    transport.postCallback([&callback_count]() {
      callback_count++;
    });
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }

  // Process callbacks on "UI thread"
  transport.processCallbacks();

  // Should have received most callbacks (some may be dropped if queue fills)
  EXPECT_GT(callback_count.load(), 9000);
}
```

---

## 8. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| API breaking changes | Medium | High | Use deprecation aliases, semver |
| Performance regression | Low | Medium | Benchmark before/after each phase |
| Threading bugs | Medium | High | ThreadSanitizer in CI |
| Format conversion artifacts | Low | Medium | A/B test against reference DAW |
| Backward incompatibility | Medium | Medium | Maintain old codepaths behind flags |

---

## 9. File Index

### Core Files Modified

| File | Phase | Tasks |
|------|-------|-------|
| `src/core/routing/gain_smoother.cpp` | 1 | C-01 |
| `src/core/routing/gain_smoother.h` | 1 | C-01 |
| `src/core/routing/routing_matrix.cpp` | 1, 2 | C-02, C-04, A-02, A-03 |
| `src/core/routing/routing_matrix.h` | 2, 4 | A-02, Q-05 |
| `src/core/transport/transport_controller.cpp` | 1, 2 | C-03, A-01 |
| `src/core/transport/transport_controller.h` | 1, 2, 4 | C-03, A-01, Q-01, Q-02 |

### New Files Created

| File | Phase | Task |
|------|-------|------|
| `include/orpheus/channel_format.h` | 3 | A-04 |
| `src/core/channel_format.cpp` | 3 | A-04 |
| `docs/orp/GAIN_STAGING.md` | 2 | A-06 |
| `tests/routing/gain_smoother_test.cpp` | 1 | C-01 |
| `tests/routing/limiter_test.cpp` | 1 | C-02 |
| `tests/routing/pan_law_test.cpp` | 1 | C-04 |
| `tests/routing/stereo_routing_test.cpp` | 2 | A-01, A-02 |
| `tests/integration/multi_channel_test.cpp` | 3 | A-04, A-05 |
| `tests/stress/callback_queue_test.cpp` | 1 | C-03 |

---

## 10. References

- **ORP114:** Gain staging bug investigation (trim race condition, restart crossfade)
- **ORP117:** Routing matrix technical explainer
- **ORP118:** Multi-voice architecture and audio summing topology
- **ITU-R BS.775-3:** Multichannel stereophonic sound system with or without accompanying picture
- **ITU-R BS.1770-4:** Algorithms to measure audio programme loudness and true-peak audio level
- **AES17-2020:** AES standard method for digital audio engineering - Measurement of digital audio equipment

---

## 11. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-18 | Claude (Audio Consultant) | Initial comprehensive plan |

---

**Document Status:** Ready for Implementation
**Handoff Target:** Local Agent
**Estimated Total Effort:** 48-64 hours across 4 phases
