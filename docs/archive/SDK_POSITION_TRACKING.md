# SDK Position Tracking Guarantees

**Document Version:** 1.0
**Last Updated:** October 27, 2025
**Sprint:** ORP089

## Overview

This document specifies the behavior and guarantees of position tracking, metadata updates, and edit law enforcement in the Orpheus SDK transport layer. It is intended for SDK consumers building applications like Orpheus Clip Composer (OCC).

## Table of Contents

1. [Transient States After Restart](#transient-states-after-restart)
2. [Metadata Update Timing](#metadata-update-timing)
3. [OUT Point Enforcement](#out-point-enforcement)
4. [Position Query Best Practices](#position-query-best-practices)
5. [Code Examples](#code-examples)

---

## Transient States After Restart

### Behavior

After calling `restartClip()`, there is a **0-26ms window** where `getClipPosition()` may return stale position data before the audio thread processes the first buffer.

**Root Cause:**

- `restartClip()` is called from the **message thread** (UI thread)
- Position update is applied **atomically** to the active clip state
- Audio thread processes the update on its **next callback** (typically 10-26ms later)
- `getClipPosition()` queries the **actual render position**, not metadata

**Timeline:**

| Time      | Thread  | Event                                  |
| --------- | ------- | -------------------------------------- |
| t=0ms     | Message | `restartClip()` called                 |
| t=0ms     | Message | Position atomically updated to trim IN |
| t=0-26ms  | Audio   | Next `processAudio()` call             |
| t=10-26ms | Audio   | Position begins advancing from trim IN |

### Recommended Pattern: 2-Tick Grace Period

When tracking position changes after restart, implement a grace period to ignore transient states:

```cpp
class PreviewPlayer {
private:
  int m_ticksSinceRestart = 0;

public:
  void onRestartClip() {
    m_ticksSinceRestart = 0;  // Reset grace period counter
  }

  void onPositionUpdate(int64_t position) {
    // Grace period: Ignore first 2 ticks after restart (≤26ms @ 75 FPS)
    if (m_ticksSinceRestart < 2) {
      m_ticksSinceRestart++;
      return;  // Skip edit law enforcement during startup
    }

    // Normal edit law enforcement
    if (position < m_metadata.trimInSamples || position >= m_metadata.trimOutSamples) {
      handleEditLawViolation(position);
    }
  }
};
```

**Why 2 Ticks?**

- At 75 FPS (broadcast standard), 1 tick = 13.33ms
- 2 ticks = 26.66ms, which covers worst-case audio callback latency
- After 2 ticks, position data is guaranteed accurate

---

## Metadata Update Timing

### Guarantees

Calling `updateClipTrimPoints()`, `updateClipFades()`, or `updateClipMetadata()` from the message thread guarantees visibility in the audio thread **within 1 audio buffer** (~10ms @ 512 samples, 48kHz).

### Memory Ordering Semantics

- **Message Thread:** Uses `std::memory_order_release` when storing atomics
- **Audio Thread:** Uses `std::memory_order_acquire` when loading atomics
- **Result:** C++11 happens-before relationship ensures visibility

### Timing Table

| Event                           | Time     | Thread         |
| ------------------------------- | -------- | -------------- |
| `updateClipMetadata()` called   | t=0ms    | Message thread |
| Mutex acquired, atomics updated | t=0ms    | Message thread |
| `updateClipMetadata()` returns  | t=0ms    | Message thread |
| Next `processAudio()` call      | t=0-10ms | Audio thread   |
| Audio thread sees new metadata  | t=0-10ms | Audio thread   |

**Worst-Case Latency:** 1 audio buffer period (512 samples @ 48kHz = 10.67ms)
**Best-Case Latency:** Immediate (if audio thread loads atomic after release)

### Race Condition Analysis

**Scenario 1: Metadata Update Before `startClip()`**

```cpp
// SAFE: Metadata always applied before playback starts
m_audioEngine->updateClipMetadata(handle, metadata);
m_audioEngine->startClip(handle);  // ← Uses updated metadata
```

✅ **Why Safe:** `startClip()` loads metadata from persistent storage (mutex-protected). Metadata update completes before `startClip()` acquires lock.

**Scenario 2: Metadata Update During Playback**

```cpp
// SAFE: Atomic updates visible within 1 buffer
m_audioEngine->updateClipMetadata(handle, metadata);
// Audio thread sees new trimOut within ~10ms
```

✅ **Why Safe:** Atomic stores use release semantics, atomic loads use acquire semantics. C++11 memory model guarantees visibility.

**Scenario 3: Rapid Metadata Updates**

```cpp
// SAFE: Last update wins
m_audioEngine->updateClipMetadata(handle, metadata1);
m_audioEngine->updateClipMetadata(handle, metadata2);
m_audioEngine->updateClipMetadata(handle, metadata3);
// Audio thread may see metadata1, metadata2, or metadata3
```

✅ **Why Safe:** Last update always wins (atomic stores are linearizable). No torn reads possible.

---

## OUT Point Enforcement

### SDK Responsibility vs UI Responsibility

**SDK Enforces:**

- ✅ Automatic stop/loop when clip reaches OUT point
- ✅ Sample-accurate enforcement (±512 samples max)
- ✅ Fade-out application (if configured)
- ✅ Callback firing (`onClipStopped` or `onClipLooped`)

**UI Responsibility:**

- ✅ Display current position
- ✅ Visualize trim points (waveform markers)
- ✅ Respond to SDK callbacks for visual feedback
- ❌ **NOT** responsible for polling position to enforce OUT point
- ❌ **NOT** responsible for manually stopping playback at OUT

### Loop Mode Behavior

When `setClipLoopMode(handle, true)` is enabled:

1. Audio thread detects `position >= trimOut` in `processAudio()`
2. Position is **automatically reset** to `trimIn` (sample-accurate)
3. Audio file reader is **seeked** to `trimIn` (gap-free)
4. `onClipLooped()` callback is posted to message thread
5. Clip continues playing from `trimIn` (no stop, no gap)

**Sample Accuracy:**

- Best case: ±0 samples (loop detected at exact OUT point)
- Worst case: ±512 samples (buffer size @ 48kHz)
- Typical: ±256 samples (~5.3ms @ 48kHz)

This is **acceptable for broadcast audio**. Sample-perfect loops would require lookahead (not feasible in real-time).

### Non-Loop Mode Behavior

When `setClipLoopMode(handle, false)` is enabled:

1. Audio thread detects `position >= trimOut` in `processAudio()`
2. If `fadeOutSeconds > 0`, fade-out begins from current position
3. If `fadeOutSeconds == 0`, clip stops immediately
4. `onClipStopped()` callback is posted to message thread
5. Clip is removed from active list after fade-out completes

**Sample Accuracy:**

- Stop detection: ±512 samples max (buffer granularity)
- Fade-out start: Sample-accurate (begins immediately upon detection)

---

## Position Query Best Practices

### When to Poll Position

✅ **Good Use Cases:**

- UI waveform scrubbing (mouse drag, click-to-jog)
- Timeline display updates (playhead position)
- Progress bar rendering
- Time code display

### When NOT to Poll

❌ **Bad Use Cases:**

- Edit law enforcement (use OUT point enforcement + callbacks instead)
- Detecting when clip reaches end (use `onClipStopped` callback)
- Loop boundary detection (use `onClipLooped` callback)

### Performance Considerations

**Atomic Reads Are Cheap:**

- `getClipPosition()` performs a single atomic load
- Cost: ~100 CPU cycles (negligible overhead)
- Safe to call at UI frame rate (e.g., 75 FPS)

**Polling Pattern:**

```cpp
void WaveformDisplay::timerCallback() {
  if (m_isPlaying) {
    int64_t position = m_transport->getClipPosition(m_handle);
    if (position >= 0) {
      updatePlayhead(position);
    }
  }
}
```

---

## Code Examples

### Example 1: Grace Period Implementation (OCC PreviewPlayer)

```cpp
class PreviewPlayer {
private:
  int m_ticksSinceRestart = 0;
  ClipMetadata m_metadata;

public:
  void play() {
    // Update metadata BEFORE starting playback
    m_audioEngine->updateClipMetadata(m_handle, m_metadata);

    if (m_isPlaying) {
      m_audioEngine->restartClip(m_handle);
      m_ticksSinceRestart = 0;  // Reset grace period
    } else {
      m_audioEngine->startClip(m_handle);
      m_isPlaying = true;
    }
  }

  void onPositionUpdate(int64_t position) {
    // Grace period: Ignore first 2 ticks after restart
    if (m_ticksSinceRestart < 2) {
      m_ticksSinceRestart++;
      return;
    }

    // Update UI playhead
    updateWaveformPlayhead(position);
  }
};
```

### Example 2: Metadata Update Before Playback

```cpp
void ClipEditDialog::onTrimInChanged(int64_t newTrimIn) {
  m_metadata.trimInSamples = newTrimIn;

  // Update metadata first
  m_audioEngine->updateClipMetadata(m_handle, m_metadata);

  // Then restart playback (uses updated metadata)
  if (m_previewPlayer->isPlaying()) {
    m_audioEngine->restartClip(m_handle);
  }
}
```

### Example 3: Waveform Scrubbing with seekClip()

```cpp
void WaveformDisplay::mouseDown(const MouseEvent& e) {
  int64_t clickPosition = pixelToSample(e.x);

  if (m_previewPlayer->isPlaying()) {
    // Seamless seek (no gap, no stop/start)
    m_audioEngine->seekClip(m_handle, clickPosition);
  } else {
    // Not playing - start from clicked position
    m_metadata.trimInSamples = clickPosition;
    m_audioEngine->updateClipMetadata(m_handle, m_metadata);
    m_audioEngine->startClip(m_handle);

    // Restore original trim IN
    m_metadata.trimInSamples = m_originalTrimIn;
    m_audioEngine->updateClipMetadata(m_handle, m_metadata);
  }
}
```

### Example 4: Loop Indicator UI Pattern

```cpp
class ClipButton {
private:
  void onClipLooped(ClipHandle handle, TransportPosition position) {
    if (handle == m_handle) {
      // Flash button to indicate loop event
      setBackgroundColor(Colors::yellow);

      // Reset color after 100ms
      Timer::callAfter(100, [this]() {
        setBackgroundColor(m_originalColor);
      });
    }
  }

  void timerCallback() {
    // Update loop indicator icon
    bool isLooping = m_transport->isClipLooping(m_handle);
    m_loopIcon->setVisible(isLooping);
  }
};
```

### Example 5: OUT Point Enforcement (SDK Handles This)

```cpp
// ❌ WRONG: Don't manually enforce OUT point in UI
void PreviewPlayer::onPositionUpdate(int64_t position) {
  if (position >= m_metadata.trimOutSamples) {
    m_audioEngine->stopClip(m_handle);  // ← SDK already does this!
  }
}

// ✅ CORRECT: Let SDK enforce, just respond to callback
void PreviewPlayer::onClipStopped(ClipHandle handle, TransportPosition position) {
  if (handle == m_handle) {
    m_isPlaying = false;
    updateUI();  // Update button state, clear playhead, etc.
  }
}
```

---

## Summary

| Topic                     | Key Takeaway                                                |
| ------------------------- | ----------------------------------------------------------- |
| **Transient States**      | Use 2-tick grace period after restart (~26ms @ 75 FPS)      |
| **Metadata Timing**       | Updates visible within 1 audio buffer (~10ms @ 512 samples) |
| **OUT Point Enforcement** | SDK handles automatically; UI responds to callbacks         |
| **Position Polling**      | Safe for UI updates, NOT for edit law enforcement           |
| **Loop Mode**             | SDK restarts automatically; ±512 samples accuracy           |

---

## References

[1] ORP088 - SDK Team Answers: Edit Law Enforcement & Position Tracking
[2] ORP086 - Seamless Clip Restart API
[3] ORP089 - Edit Law Enforcement & Seamless Seek API (this sprint)
[4] `include/orpheus/transport_controller.h` - Public API
[5] `src/core/transport/transport_controller.cpp` - Implementation

---

**Document Prepared By:** SDK Core Team
**Date:** October 27, 2025
**Sprint:** ORP089 - Edit Law Enforcement & Seamless Seek
**Status:** ✅ COMPLETE

---

_Generated with Claude Code — Anthropic's AI-powered development assistant_
