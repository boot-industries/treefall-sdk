# Correct vs Incorrect Patterns - Anti-Pattern Reference

**Status:** Architectural Reference
**Source:** OCC127 State Synchronization Architecture
**Created:** 2025-11-17

---

## Executive Summary

This document provides **side-by-side comparisons** of correct and incorrect state synchronization patterns. Use this as a quick reference when implementing or reviewing code.

**Golden Rule:** Continuous polling with atomic state queries. No callbacks, no caching, no conditional timer lifecycle.

---

## Pattern 1: Timer Lifecycle

### ❌ INCORRECT: Start/Stop Timer Based on State

```cpp
class PreviewPlayer {
public:
  void play() {
    m_audioEngine->startClip(m_clipHandle);
    startTimer(13);  // ❌ BAD: Starting timer conditionally
  }

  void stop() {
    m_audioEngine->stopClip(m_clipHandle);
    stopTimer();  // ❌ BAD: Stopping timer when state changes
  }

  void timerCallback() override {
    // Assumes timer only runs when playing
    int64_t pos = getCurrentPosition();
    updatePlayhead(pos);
  }
};
```

**Why This Fails:**

```
Timeline:
0ms:    Edit Dialog PLAY clicked → startTimer()
50ms:   Main Grid button clicked → clip playing, but timer running from Edit Dialog
100ms:  Edit Dialog STOP clicked → stopTimer()
150ms:  Main Grid triggers clip AGAIN → Timer NOT running!
        → Playhead frozen, desynchronized
```

---

### ✅ CORRECT: Continuous Timer with Early Return

```cpp
class PreviewPlayer {
public:
  PreviewPlayer() {
    startTimer(13);  // ✅ GOOD: Start once in constructor
  }

  ~PreviewPlayer() {
    stopTimer();  // ✅ GOOD: Stop in destructor (automatic)
  }

  void timerCallback() override {
    // ✅ GOOD: Early return when not playing
    if (!isPlaying()) {
      return;  // Timer keeps running
    }

    // Poll SDK state atomically
    int64_t pos = getCurrentPosition();
    updatePlayhead(pos);
  }

private:
  bool isPlaying() const {
    // ✅ GOOD: Query SDK atomic state directly
    return m_audioEngine->isClipPlaying(m_clipHandle);
  }
};
```

**Why This Works:**

```
Timeline:
0ms:    Component created → startTimer(13)
13ms:   timerCallback() → early return (not playing)
26ms:   timerCallback() → early return (not playing)
39ms:   Main Grid triggers clip → SDK state updates
52ms:   timerCallback() → isPlaying() = true → update playhead ✓
65ms:   timerCallback() → update playhead ✓
```

**Synchronized regardless of trigger source!**

---

## Pattern 2: State Queries

### ❌ INCORRECT: Caching SDK State

```cpp
class ClipGrid {
  // ❌ BAD: Caching state locally
  std::array<bool, 384> m_clipPlayingCache;

  void onClipTriggered(int index) {
    m_audioEngine->startClip(index);
    m_clipPlayingCache[index] = true;  // ❌ Manual cache update
  }

  void timerCallback() override {
    for (int i = 0; i < 384; ++i) {
      // ❌ BAD: Using cached state instead of SDK query
      if (m_clipPlayingCache[i]) {
        m_buttons[i]->setState(ClipButton::State::Playing);
      }
    }
  }
};
```

**Why This Fails:**

```
Scenario:
1. Edit Dialog triggers clip 5 → SDK state updates
2. ClipGrid cache NOT updated (triggered from different component)
3. ClipGrid::timerCallback() → reads cache → m_clipPlayingCache[5] = false
4. Button stays in Loaded state despite clip playing
   → Desynchronization!
```

---

### ✅ CORRECT: Direct Atomic Queries

```cpp
class ClipGrid {
  // ✅ GOOD: No state caching

  void timerCallback() override {
    for (int i = 0; i < 384; ++i) {
      // ✅ GOOD: Query SDK atomic state directly
      bool isPlaying = isClipPlaying ? isClipPlaying(i) : false;

      auto button = m_buttons[i].get();
      if (!button) continue;

      // Update button based on CURRENT SDK state
      if (button->getState() == ClipButton::State::Playing && !isPlaying) {
        button->setState(ClipButton::State::Loaded);
      }
      else if (button->getState() != ClipButton::State::Playing && isPlaying) {
        button->setState(ClipButton::State::Playing);
      }
    }
  }
};
```

**Why This Works:**

```
Scenario:
1. Edit Dialog triggers clip 5 → SDK: m_clipPlaying[5] = true
2. ClipGrid::timerCallback() → isClipPlaying(5) → queries SDK atomically
3. SDK returns true (ground truth)
4. Button updates to Playing state ✓
   → Synchronized!
```

**Single source of truth (SDK) ensures consistency.**

---

## Pattern 3: Multi-Source Synchronization

### ❌ INCORRECT: Callback-Based State Propagation

```cpp
class PreviewPlayer {
  void play() {
    m_audioEngine->startClip(m_clipHandle);

    // ❌ BAD: Manual callback for state update
    if (onPlayStateChanged) {
      onPlayStateChanged(true);
    }
  }
};

class MainComponent {
  void onClipTriggered(int index) {
    m_audioEngine->startClip(index);

    // ❌ BAD: Manually notifying Edit Dialog
    if (m_currentEditDialog && m_currentEditDialog->getClipIndex() == index) {
      m_currentEditDialog->onPlayStateChanged(true);  // Easy to forget!
    }
  }
};
```

**Why This Fails:**

```
Trigger Sources:
- Edit Dialog PLAY → callback fired ✓
- Main Grid button → callback NOT fired ✗ (manual propagation missed)
- SPACE bar → callback NOT fired ✗
- MIDI controller → callback NOT fired ✗

Result: UI desynchronized depending on trigger source
```

---

### ✅ CORRECT: Polling-Based Synchronization

```cpp
class PreviewPlayer {
  PreviewPlayer() {
    startTimer(13);  // ✅ Continuous polling
  }

  void timerCallback() override {
    // ✅ GOOD: Poll SDK state directly
    bool playing = m_audioEngine->isClipPlaying(m_clipHandle);

    if (!playing) {
      return;  // Early return
    }

    int64_t pos = m_audioEngine->getClipPosition(m_clipHandle);
    updatePlayhead(pos);
  }
};

class ClipGrid {
  ClipGrid() {
    startTimer(13);  // ✅ Continuous polling
  }

  void timerCallback() override {
    for (int i = 0; i < 384; ++i) {
      // ✅ GOOD: Poll SDK state directly
      bool playing = isClipPlaying ? isClipPlaying(i) : false;
      updateButtonState(i, playing);
    }
  }
};
```

**Why This Works:**

```
Trigger Sources (ALL work the same):
- Edit Dialog PLAY → SDK state updates → all components poll → synchronized ✓
- Main Grid button → SDK state updates → all components poll → synchronized ✓
- SPACE bar → SDK state updates → all components poll → synchronized ✓
- MIDI controller → SDK state updates → all components poll → synchronized ✓

Result: Guaranteed synchronization regardless of source
```

---

## Pattern 4: Keyboard Focus Management

### ❌ INCORRECT: No Focus Restoration

```cpp
void MainComponent::onClipTriggered(int buttonIndex) {
  m_audioEngine->startClip(globalClipIndex);
  // ❌ Missing: Restore keyboard focus to Edit Dialog
}
```

**Why This Fails:**

```
User Workflow:
1. Open Edit Dialog for clip 5
2. Press 'I' to set IN point → Works ✓
3. Click Main Grid button 10 → Clip 10 starts
4. Press 'I' again → Triggers Main Grid clip 'I' instead of Edit Dialog! ✗
   → Keyboard focus stolen by main grid
```

---

### ✅ CORRECT: Restore Focus to Edit Dialog

```cpp
void MainComponent::onClipTriggered(int buttonIndex) {
  int globalClipIndex = /* ... */;
  m_audioEngine->startClip(globalClipIndex);

  // ✅ GOOD: Restore keyboard focus to Edit Dialog if open
  if (m_currentEditDialog != nullptr && m_currentEditDialog->isVisible()) {
    m_currentEditDialog->grabKeyboardFocus();
  }
}
```

**Why This Works:**

```
User Workflow:
1. Open Edit Dialog for clip 5
2. Press 'I' to set IN point → Works ✓
3. Click Main Grid button 10 → Clip 10 starts
   → Focus immediately restored to Edit Dialog
4. Press 'I' again → Sets IN point in Edit Dialog ✓
   → Keyboard shortcuts work as expected
```

---

## Pattern 5: Timer Callback Early Return

### ❌ INCORRECT: Stopping Timer in Callback

```cpp
void PreviewPlayer::timerCallback() {
  if (!isPlaying()) {
    stopTimer();  // ❌ BAD: Stopping timer when inactive
    return;
  }

  int64_t pos = getCurrentPosition();
  updatePlayhead(pos);
}
```

**Why This Fails:**

```
Timeline:
0ms:    Edit Dialog PLAY → timer running, playhead updates
50ms:   Edit Dialog STOP → timer stopped
100ms:  Main Grid triggers same clip → Timer NOT running!
        → Playhead frozen ✗
```

---

### ✅ CORRECT: Early Return, Timer Keeps Running

```cpp
void PreviewPlayer::timerCallback() {
  if (!isPlaying()) {
    return;  // ✅ GOOD: Early return, timer keeps running
  }

  int64_t pos = getCurrentPosition();
  updatePlayhead(pos);
}
```

**Why This Works:**

```
Timeline:
0ms:    Edit Dialog PLAY → timer running, playhead updates ✓
13ms:   timerCallback() → isPlaying() = true → update ✓
26ms:   timerCallback() → isPlaying() = true → update ✓
39ms:   Edit Dialog STOP → timer STILL running
52ms:   timerCallback() → isPlaying() = false → early return (no-op)
65ms:   timerCallback() → isPlaying() = false → early return (no-op)
78ms:   Main Grid triggers same clip → SDK state updates
91ms:   timerCallback() → isPlaying() = true → update ✓
        → Playhead starts updating immediately!
```

---

## Pattern 6: Component Communication

### ❌ INCORRECT: Direct Component-to-Component Communication

```cpp
class MainComponent {
  void onClipTriggered(int index) {
    m_audioEngine->startClip(index);

    // ❌ BAD: Direct component communication
    m_clipGrid->updateButton(index);
    if (m_currentEditDialog) {
      m_currentEditDialog->onClipStarted();
    }
    m_transportControls->updateState();

    // What if we add more components? Have to update this!
  }
};
```

**Problems:**

- Tight coupling between components
- Easy to forget components (leads to desync)
- Not extensible (adding components requires code changes)
- Brittle (breaks when component hierarchy changes)

---

### ✅ CORRECT: Components Poll Shared State

```cpp
class MainComponent {
  void onClipTriggered(int index) {
    // ✅ GOOD: Only write to SDK state
    m_audioEngine->startClip(index);

    // ✅ GOOD: Restore focus (UI concern, not state propagation)
    if (m_currentEditDialog && m_currentEditDialog->isVisible()) {
      m_currentEditDialog->grabKeyboardFocus();
    }

    // That's it! All components automatically synchronized via polling
  }
};

// Components poll independently
class ClipGrid {
  void timerCallback() override {
    // Polls SDK state automatically
    for (int i = 0; i < 384; ++i) {
      bool playing = isClipPlaying(i);
      updateButtonState(i, playing);
    }
  }
};

class PreviewPlayer {
  void timerCallback() override {
    // Polls SDK state automatically
    bool playing = isPlaying();
    if (playing) {
      int64_t pos = getCurrentPosition();
      updatePlayhead(pos);
    }
  }
};
```

**Benefits:**

- Loose coupling (components don't know about each other)
- Can't forget components (they poll automatically)
- Extensible (add components without modifying triggers)
- Robust (works regardless of component hierarchy)

---

## Pattern 7: Atomic Memory Ordering

### ❌ INCORRECT: Non-Atomic or Relaxed Ordering

```cpp
// ❌ BAD: Non-atomic state variable
class TransportController {
  bool m_clipPlaying[384];  // ❌ Not atomic! Race condition!

  void startClip(int index) {
    m_clipPlaying[index] = true;  // ❌ Non-atomic write
  }

  bool isClipPlaying(int index) const {
    return m_clipPlaying[index];  // ❌ Non-atomic read, UB!
  }
};
```

**Why This Fails:**

- **Data race:** Undefined behavior when audio thread writes and UI thread reads simultaneously
- **No synchronization:** Memory writes may not be visible across threads
- **Compiler optimizations:** May reorder or eliminate reads/writes

---

### ✅ CORRECT: Atomic with Proper Memory Ordering

```cpp
// ✅ GOOD: Atomic state variables
class TransportController {
  std::atomic<bool> m_clipPlaying[384];  // ✅ Atomic type

  void startClip(int index) {
    // ✅ GOOD: Atomic write with release semantics
    // Ensures all prior writes are visible when this write is seen
    m_clipPlaying[index].store(true, std::memory_order_release);
  }

  bool isClipPlaying(int index) const {
    // ✅ GOOD: Atomic read with acquire semantics
    // Ensures we see all writes that happened before the release
    return m_clipPlaying[index].load(std::memory_order_acquire);
  }
};
```

**Why This Works:**

- **No data races:** Atomic operations are well-defined for concurrent access
- **Memory ordering:** Acquire-release ensures writes are visible across threads
- **Lock-free:** No blocking, audio-thread safe

---

## Comparison Summary Table

| Pattern | ❌ INCORRECT | ✅ CORRECT |
|---------|-------------|-----------|
| **Timer Lifecycle** | Start/stop timer based on state | Start in constructor, never stop |
| **Timer Callback** | Stop timer when inactive | Early return, timer keeps running |
| **State Queries** | Cache SDK state locally | Query SDK atomically every poll |
| **State Updates** | Manual callback propagation | Automatic via polling |
| **Component Comm** | Direct component-to-component | All poll shared SDK state |
| **Trigger Sources** | Callbacks only from one source | All sources write to same state |
| **Keyboard Focus** | No focus management | Restore focus to Edit Dialog |
| **Memory Access** | Non-atomic or relaxed ordering | Atomic with acquire-release |

---

## Quick Decision Tree

### Implementing a New Component

```
Q: Does this component need to show SDK state (playing, position, etc.)?
└─> YES
    Q: Should it update in real-time?
    └─> YES
        ✅ CORRECT PATTERN:
        - startTimer(13) in constructor
        - timerCallback() with early return
        - Query SDK atomically (no caching)
        - No callbacks for state updates

    └─> NO (e.g., one-time state query)
        ✅ CORRECT PATTERN:
        - Query SDK atomically when needed
        - No timer required

└─> NO (e.g., static UI element)
    ✅ CORRECT PATTERN:
    - No polling needed
```

---

## Code Review Checklist

### When Reviewing State Synchronization Code

**Red Flags (Anti-Patterns):**

- [ ] ❌ `startTimer()` called in `play()`, `stop()`, or state change methods
- [ ] ❌ `stopTimer()` called in `timerCallback()` based on state
- [ ] ❌ Local caching of SDK state (e.g., `m_cachedPlaying`, `m_lastPosition`)
- [ ] ❌ Callbacks for state updates (e.g., `onPlayStateChanged()`, `onPositionChanged()`)
- [ ] ❌ Manual component notification (e.g., `m_otherComponent->updateState()`)
- [ ] ❌ Conditional timer lifecycle (e.g., `if (!isTimerRunning()) startTimer()`)
- [ ] ❌ Non-atomic state variables (e.g., `bool m_playing` instead of `std::atomic<bool>`)

**Green Flags (Correct Patterns):**

- [ ] ✅ `startTimer(13)` in constructor, `stopTimer()` in destructor only
- [ ] ✅ `timerCallback()` with early return pattern (`if (!active) return;`)
- [ ] ✅ Direct SDK atomic queries (`m_sdk->isClipPlaying(handle)`)
- [ ] ✅ No state caching (always query SDK)
- [ ] ✅ Components poll independently (no inter-component calls)
- [ ] ✅ Focus management in trigger methods (restore focus to Edit Dialog)
- [ ] ✅ Atomic types with proper memory ordering (`std::memory_order_acquire/release`)

---

## Real-World Regression Example

### Historical Bug: OCC127 Investigation (November 2025)

**Symptoms Observed:**

- Edit Dialog playhead stopped updating when clip triggered from main grid
- Keyboard focus stolen by main grid, shortcuts stopped working
- Button states desynchronized

**Root Cause:**

Assistant attempted to "optimize" by adding `stopTimer()` in `PreviewPlayer::timerCallback()`:

```cpp
// ❌ BAD CODE (regression)
void PreviewPlayer::timerCallback() {
  if (!isPlaying()) {
    stopTimer();  // "Optimization" - BROKE SYNCHRONIZATION!
    return;
  }
  // ...
}
```

**Result:**

- Timer stopped when Edit Dialog clip stopped
- Main grid triggered clip → timer not running → playhead frozen
- Added callbacks to "fix" → created race conditions → more desync

**Correct Fix:**

Reverted to continuous polling with early return:

```cpp
// ✅ GOOD CODE (original pattern)
void PreviewPlayer::timerCallback() {
  if (!isPlaying()) {
    return;  // Early return, timer keeps running
  }
  // ...
}
```

**Lesson:** Continuous polling is the architecture, not a bug. Don't "optimize" it away.

---

## Related Documents

- **01-system-overview.md:** High-level architecture
- **02-polling-pattern.md:** Timer lifecycle details
- **03-multi-source-triggers.md:** Multi-source synchronization
- **05-component-interactions.md:** Component implementation details
- **06-focus-management.md:** Keyboard focus priority
- **OCC127:** State Synchronization Architecture (source)

---

## References

### Source Code (Correct Patterns)

- **PreviewPlayer:** `apps/clip-composer/Source/UI/PreviewPlayer.cpp` (commit 7ca6842e)
- **ClipGrid:** `apps/clip-composer/Source/ClipGrid/ClipGrid.cpp` (commit 7ca6842e)
- **MainComponent:** `apps/clip-composer/Source/MainComponent.cpp` (focus management)

### Atomic Operations

- **std::atomic:** https://en.cppreference.com/w/cpp/atomic/atomic
- **Memory ordering:** https://en.cppreference.com/w/cpp/atomic/memory_order
- **Lock-free programming:** Herb Sutter's "atomic<> Weapons" talks

---

**Document Status:** Architectural Reference (Anti-Pattern Guide)
**Maintained By:** Orpheus SDK Development Team
**Last Updated:** 2025-11-17
**Use For:** Code reviews, new component implementation, regression prevention
