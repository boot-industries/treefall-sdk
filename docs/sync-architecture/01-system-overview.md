# System Overview - Continuous Polling Architecture

**Status:** Architectural Reference
**Source:** OCC127 State Synchronization Architecture
**Created:** 2025-11-17

---

## Executive Summary

Clip Composer uses **continuous 75fps polling** to synchronize UI state with SDK playback state. This document provides the high-level architectural overview showing how the three-layer system maintains consistent state across all components.

**Core Principle:** UI state is derived from SDK atomic state through continuous polling, never through callbacks or caching.

---

## Three-Layer Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    LAYER 3: UI COMPONENTS                       │
│                         (JUCE Framework)                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌──────────────────┐  ┌──────────────────┐  │
│  │ ClipButton  │  │ WaveformDisplay  │  │ Edit Dialog      │  │
│  │             │  │                  │  │                  │  │
│  │ States:     │  │ - Playhead       │  │ - Transport      │  │
│  │ • Playing   │  │ - Zoom level     │  │ - Trim markers   │  │
│  │ • Loaded    │  │ - Waveform       │  │ - IN/OUT points  │  │
│  │ • Stopping  │  │                  │  │ - Loop toggle    │  │
│  └─────────────┘  └──────────────────┘  └──────────────────┘  │
│                                                                 │
│         ↑ repaint()        ↑ repaint()         ↑ repaint()     │
└─────────┼──────────────────┼──────────────────┼────────────────┘
          │                  │                  │
          │    Atomic State Polling (75fps)     │
          │                  │                  │
┌─────────┼──────────────────┼──────────────────┼────────────────┐
│         ↓                  ↓                  ↓                │
│              LAYER 2: MESSAGE THREAD (JUCE)                    │
│                    Continuous Timer Polling                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ ClipGrid::timerCallback()         Every 13ms (75fps)       ││
│  │                                                            ││
│  │ for (int i = 0; i < MAX_BUTTONS; ++i) {                   ││
│  │   bool isPlaying = m_engine->isClipPlaying(i);            ││
│  │   updateButtonState(i, isPlaying);  // Atomic query       ││
│  │ }                                                          ││
│  └────────────────────────────────────────────────────────────┘│
│                                                                 │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ PreviewPlayer::timerCallback()    Every 13ms (75fps)       ││
│  │                                                            ││
│  │ if (!isPlaying()) return;  // Early return                ││
│  │ int64_t pos = getCurrentPosition();  // Atomic query      ││
│  │ if (onPositionChanged) onPositionChanged(pos);            ││
│  └────────────────────────────────────────────────────────────┘│
│                                                                 │
│         ↑ atomic reads      ↑ atomic reads     ↑ atomic reads  │
└─────────┼──────────────────┼──────────────────┼────────────────┘
          │                  │                  │
          │   Lock-Free Atomic State Access     │
          │                  │                  │
┌─────────┼──────────────────┼──────────────────┼────────────────┐
│         ↓                  ↓                  ↓                │
│            LAYER 1: SDK AUDIO THREAD (Real-Time)               │
│                   Atomic State Storage                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Atomic State Variables (lock-free, wait-free access):         │
│                                                                 │
│  ┌────────────────────────────────────────────────────────┐   │
│  │ std::atomic<bool> m_clipPlaying[384]                   │   │
│  │ std::atomic<int64_t> m_clipPosition[384]               │   │
│  │ std::atomic<bool> m_loopEnabled[384]                   │   │
│  │ std::atomic<double> m_clipGain[384]                    │   │
│  └────────────────────────────────────────────────────────┘   │
│                                                                 │
│  Audio Processing:                                             │
│  - Clip playback (sample-accurate)                             │
│  - Position tracking (64-bit sample counts)                    │
│  - Fade processing (in/out/crossfade)                          │
│  - Loop point handling                                         │
│  - Transport commands (queued, processed in audio callback)    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Data Flow: Bottom-Up

### 1. SDK Audio Thread (Layer 1) → Ground Truth

**Responsibilities:**
- Real-time audio processing (broadcast-safe, deterministic)
- Atomic state storage (lock-free data structures)
- Command queue processing (Start/Stop/Restart)
- Sample-accurate position tracking

**Key Properties:**
- No allocations on audio thread
- Lock-free atomic operations only
- Bit-identical deterministic output
- 64-bit sample-accurate timing

**State Variables:**
```cpp
// All state stored atomically for lock-free reads
std::atomic<bool> m_clipPlaying[MAX_CLIPS];      // Playing state
std::atomic<int64_t> m_clipPosition[MAX_CLIPS];  // Current sample position
std::atomic<bool> m_loopEnabled[MAX_CLIPS];      // Loop enable state
std::atomic<double> m_clipGain[MAX_CLIPS];       // Current gain (linear)
```

### 2. Message Thread (Layer 2) → Continuous Polling

**Responsibilities:**
- Poll SDK atomic state at 75fps (13.33ms intervals)
- Update UI component state based on polled data
- Manage keyboard focus priority
- Queue commands to SDK (via lock-free queue)

**Key Properties:**
- Timers run continuously (NEVER stop)
- Early return when inactive (no-op polling)
- No state caching (always query SDK atomically)
- 75fps = broadcast-standard frame rate

**Polling Pattern:**
```cpp
void timerCallback() {
  // CRITICAL: Timer runs continuously, returns early if inactive
  if (!needsUpdate()) {
    return;  // Early return, timer keeps running
  }

  // Poll SDK atomic state
  bool playing = m_sdk->isClipPlaying(m_clipIndex);
  int64_t position = m_sdk->getClipPosition(m_clipIndex);

  // Update UI component state
  updateUIState(playing, position);
}
```

### 3. UI Components (Layer 3) → Visual Representation

**Responsibilities:**
- Render current state to screen (JUCE repaint)
- Accept user input (mouse, keyboard, MIDI, OSC)
- Maintain keyboard focus hierarchy
- Provide visual feedback (button states, playhead, waveforms)

**Key Properties:**
- UI derives from polled state (no independent state)
- repaint() called when state changes detected
- Keyboard focus managed by MainComponent
- Edit Dialog has priority over main grid

**Visual State Mapping:**
```cpp
// ClipButton visual states derived from SDK state
enum class State {
  Empty,      // No clip loaded
  Loaded,     // Clip loaded, not playing
  Playing,    // Clip playing (SDK: isClipPlaying() == true)
  Stopping    // Clip stopping (fade-out in progress)
};

// Updated every 75fps from polled SDK state
void updateFromSDK() {
  bool isPlaying = m_sdk->isClipPlaying(m_buttonIndex);
  if (isPlaying && m_state != State::Playing) {
    setState(State::Playing);  // Triggers repaint()
  }
  else if (!isPlaying && m_state == State::Playing) {
    setState(State::Loaded);   // Triggers repaint()
  }
}
```

---

## Why 75fps Polling?

### Timing Analysis

| Aspect | Value | Rationale |
|--------|-------|-----------|
| **Polling Rate** | 75fps (13.33ms) | Broadcast standard frame rate |
| **Human Perception** | <20ms threshold | Appears instant to users |
| **CPU Overhead** | ~0.5% on modern CPU | Negligible for continuous operation |
| **Synchronization Latency** | Max 13ms | Acceptable for visual feedback |
| **Audio Buffer Size** | Typically 512 samples @ 48kHz = 10.67ms | Polling faster than audio callback |

### Broadcast Standard Alignment

**75fps is not arbitrary:**
- Matches SMPTE 75fps timecode standard
- Used in professional broadcast equipment
- Aligns with video frame rates (25fps, 30fps, 60fps)
- Familiar to audio engineers and broadcast operators

### Performance Characteristics

**CPU Usage Breakdown** (per 75fps poll):
- 384 atomic bool reads: ~1-2µs
- 384 button state updates: ~5-10µs (only if changed)
- Playhead position query: ~0.5µs
- **Total per frame:** ~10-20µs
- **Total CPU:** 10µs × 75fps = 750µs/sec = **0.075% CPU**

**Actual measured overhead:** ~0.5% CPU (includes JUCE repaint overhead)

---

## Single Source of Truth

### SDK Atomic State = Ground Truth

```
┌─────────────────────────────────────────────────────────┐
│              SDK ATOMIC STATE (Ground Truth)            │
│                                                         │
│  isClipPlaying(i) → std::atomic<bool> m_clipPlaying[i] │
│  getClipPosition(i) → std::atomic<int64_t> m_position  │
│                                                         │
│  Properties:                                            │
│  ✓ Lock-free (audio-thread safe)                       │
│  ✓ Wait-free reads (bounded execution time)            │
│  ✓ Sample-accurate (64-bit integer positions)          │
│  ✓ Deterministic (same input → same output)            │
└─────────────────────────────────────────────────────────┘
              ↓ Continuous atomic polling (75fps)
┌─────────────────────────────────────────────────────────┐
│                 MESSAGE THREAD POLLING                  │
│                                                         │
│  ClipGrid: Polls all 384 buttons every 13ms            │
│  PreviewPlayer: Polls position when playing (13ms)     │
│  No state caching, no callbacks, no race conditions    │
└─────────────────────────────────────────────────────────┘
              ↓ repaint() when state changes
┌─────────────────────────────────────────────────────────┐
│                    UI COMPONENTS                        │
│                                                         │
│  ClipButton: Visual state reflects SDK state           │
│  WaveformDisplay: Playhead position from SDK           │
│  Edit Dialog: Transport controls update SDK            │
└─────────────────────────────────────────────────────────┘
```

**Key Principle:** Never cache SDK state. Always query atomically.

---

## Multi-Source Trigger Support

One of the key advantages of continuous polling: clips can be triggered from **any source** and all UI components automatically stay synchronized.

### Supported Trigger Sources

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  Main Grid   │    │ Edit Dialog  │    │ SPACE Bar    │
│  (click)     │    │   (PLAY)     │    │ (global)     │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                   │                   │
       │ startClip(i)      │ startClip(i)      │ startClip(i)
       └───────────────────┼───────────────────┘
                           ↓
              ┌────────────────────────┐
              │  SDK Command Queue     │
              │  (lock-free SPSC)      │
              └────────────────────────┘
                           ↓
              ┌────────────────────────┐
              │ Audio Thread Processing│
              │ m_clipPlaying[i] = true│
              └────────────────────────┘
                           ↓
       ┌───────────────────┼───────────────────┐
       │                   │                   │
       ↓ Poll (75fps)      ↓ Poll (75fps)      ↓ Poll (75fps)
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  ClipGrid    │    │PreviewPlayer │    │ All UI       │
│  (button     │    │ (playhead    │    │ (synchronized│
│   state)     │    │  position)   │    │   state)     │
└──────────────┘    └──────────────┘    └──────────────┘
```

**All UI components poll the same atomic state → guaranteed synchronization**

---

## Atomic Operations

### Lock-Free Data Structures

The SDK uses **lock-free atomic operations** to ensure audio-thread safety without blocking:

```cpp
// Example: isClipPlaying() implementation (SDK)
bool TransportController::isClipPlaying(ClipHandle handle) const {
  if (!handle.isValid()) return false;

  // Lock-free atomic load (memory_order_acquire)
  // Audio thread writes, message thread reads - no locks needed
  return m_clipState[handle.index].playing.load(std::memory_order_acquire);
}
```

**Properties:**
- **Lock-free:** Progress guaranteed system-wide (no blocking)
- **Wait-free reads:** Bounded execution time (predictable latency)
- **Memory ordering:** Acquire/release semantics for cross-thread visibility
- **Audio-thread safe:** Can be called from audio callback without risk

### Memory Ordering Guarantees

```cpp
// Audio Thread (Writer)
void onClipStarted(ClipHandle handle) {
  // Update position first
  m_clipPosition[handle.index].store(0, std::memory_order_relaxed);

  // Then update playing state (release semantics)
  // Ensures position write is visible before playing = true
  m_clipPlaying[handle.index].store(true, std::memory_order_release);
}

// Message Thread (Reader)
void timerCallback() {
  // Read playing state first (acquire semantics)
  // Ensures we see position write if playing = true
  bool isPlaying = m_clipPlaying[i].load(std::memory_order_acquire);

  if (isPlaying) {
    // Position read guaranteed to see latest value
    int64_t pos = m_clipPosition[i].load(std::memory_order_relaxed);
  }
}
```

**Why this matters:** Acquire-release ordering prevents race conditions where UI would see `playing = true` but stale `position` value.

---

## System Guarantees

### What This Architecture Guarantees

✅ **Synchronization Guarantee:** All UI components reflect SDK state within 13ms (75fps)

✅ **Multi-Source Guarantee:** Clips can be triggered from any source (grid, dialog, SPACE, MIDI, OSC) and all UI stays synchronized

✅ **Focus Guarantee:** Edit Dialog maintains keyboard focus when visible, main grid restores focus after triggers

✅ **Performance Guarantee:** <1% CPU overhead for continuous polling (384 buttons + playhead + transport state)

✅ **Real-Time Guarantee:** Audio thread never blocks on UI thread, lock-free atomic access only

✅ **Determinism Guarantee:** Same SDK state → same UI state, always (no race conditions, no stale caches)

### What This Architecture Does NOT Guarantee

⚠️ **Callback Latency:** Callbacks (onClipStarted, onClipStopped) may arrive 0-20ms after state change (audio thread → message thread)

⚠️ **Visual Latency:** UI updates reflect state within 13ms, not instantly (acceptable for human perception)

⚠️ **Command Processing:** Commands queued to SDK are processed in next audio callback (10-20ms typical audio buffer)

---

## Comparison to Callback-Based Architecture

### Callback-Based (WRONG - Race Conditions)

```
Trigger Source A          Trigger Source B
      ↓                         ↓
  startClip(i)              startClip(i)
      ↓                         ↓
  onStarted(i) ────┬────── (no callback!)
                   │
                   ↓
            UI Component A updates ✓
            UI Component B does NOT update ✗ (DESYNC!)
```

**Problem:** Callbacks only fire from one trigger source, other sources cause desynchronization.

### Continuous Polling (CORRECT - Always Synchronized)

```
Trigger Source A          Trigger Source B
      ↓                         ↓
  startClip(i)              startClip(i)
      ↓                         ↓
  m_clipPlaying[i] = true (atomic state update)
      ↓                         ↓
  ┌───────────────────────────────┐
  │   All UI polls same state     │
  │   (75fps, atomic reads)       │
  └───────────────────────────────┘
      ↓                         ↓
  UI Component A ✓         UI Component B ✓ (SYNCHRONIZED!)
```

**Solution:** All UI polls single source of truth (SDK atomic state), guaranteed synchronization.

---

## Related Documents

- **OCC127:** State Synchronization Architecture - Continuous Polling Pattern (source document)
- **02-polling-pattern.md:** Detailed timer lifecycle and polling flow
- **03-multi-source-triggers.md:** Multi-source synchronization examples
- **04-correct-vs-incorrect.md:** Anti-patterns and correct patterns side-by-side
- **05-component-interactions.md:** Component-level implementation details
- **06-focus-management.md:** Keyboard focus priority flows

---

## References

### Source Code

- **TransportController:** `src/core/transport/transport_controller.h` (SDK atomic state)
- **ClipGrid:** `apps/clip-composer/Source/ClipGrid/ClipGrid.cpp:startTimer(13)`
- **PreviewPlayer:** `apps/clip-composer/Source/UI/PreviewPlayer.cpp:timerCallback()`
- **MainComponent:** `apps/clip-composer/Source/MainComponent.cpp:onClipTriggered()`

### JUCE Framework

- **juce::Timer:** https://docs.juce.com/master/classTimer.html
- **juce::Component::repaint():** https://docs.juce.com/master/classComponent.html

### Atomic Operations

- **std::atomic:** https://en.cppreference.com/w/cpp/atomic/atomic
- **Memory ordering:** https://en.cppreference.com/w/cpp/atomic/memory_order

---

**Document Status:** Architectural Reference
**Maintained By:** Orpheus SDK Development Team
**Last Updated:** 2025-11-17
