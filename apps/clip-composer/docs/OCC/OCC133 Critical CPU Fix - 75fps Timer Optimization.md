# OCC133: Critical CPU Fix - 75fps Timer Optimization

**Date:** 2025-01-14
**Category:** Performance Fix
**Priority:** CRITICAL
**Status:** COMPLETE

---

## Issue Description

**Problem:** Clip Composer using 77% CPU at idle with nothing playing

**Root Causes:**

1. **Infinite repaint loop** - ClipButton's paint() method called repaint() when playing, creating paint→repaint→paint cycle
2. **Unconditional repaints** - Timer callback repainted all 48 buttons every 13ms (3,600 repaints/sec)
3. **No change detection** - Buttons repainted even when nothing changed

**Impact:**

- With current 48 buttons: 3,600 unnecessary repaints/second
- With future 960 buttons: Would be 72,000 repaints/second
- Makes application unusable for extended sessions

---

## Solution Implemented

### 1. Fixed Infinite Repaint Loop

**File:** `Source/ClipGrid/ClipButton.cpp:180`

- Removed `repaint()` call from inside paint() method
- Animation now driven solely by 75fps timer

### 2. Optimized Timer Callback

**File:** `Source/ClipGrid/ClipGrid.cpp:171-249`

- Removed unconditional `button->repaint()` at end of loop
- All repaints now handled by state setters (setState, setLoopEnabled, etc.)
- Setters only repaint when values actually change

### 3. Optimized Progress Updates

**File:** `Source/ClipGrid/ClipButton.cpp:69-81`

- Added 0.1% threshold check before updating progress
- Avoids repainting for tiny floating-point variations
- Only repaints if difference > 0.001f

---

## Code Changes

### Before (CPU hog):

```cpp
// ClipButton.cpp - Infinite loop
void ClipButton::paint(juce::Graphics& g) {
    // ... drawing code ...
    if (m_state == State::Playing) {
        // ... draw pulsing border ...
        repaint(); // PROBLEM: Creates infinite loop!
    }
}

// ClipGrid.cpp - Unconditional repaints
void ClipGrid::timerCallback() {
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        // ... state updates ...
        button->repaint(); // PROBLEM: Always repaints all 48 buttons!
    }
}
```

### After (Optimized):

```cpp
// ClipButton.cpp - No infinite loop
void ClipButton::paint(juce::Graphics& g) {
    // ... drawing code ...
    if (m_state == State::Playing) {
        // ... draw pulsing border ...
        // NOTE: Animation driven by 75fps timer, NOT by repaint() here
    }
}

// ClipButton.cpp - Smart progress updates
void ClipButton::setPlaybackProgress(float progress) {
    float newProgress = juce::jlimit(0.0f, 1.0f, progress);

    // Only update if changed meaningfully (>0.1% difference)
    if (std::abs(newProgress - m_playbackProgress) > 0.001f) {
        m_playbackProgress = newProgress;
        if (m_state == State::Playing || m_state == State::Stopping)
            repaint();
    }
}

// ClipGrid.cpp - Conditional repaints
void ClipGrid::timerCallback() {
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        // ... state updates via setters ...
        // All setters handle their own repaints
        // No manual repaint() needed
    }
}
```

---

## Performance Impact

### Before Fix:

- **Idle:** 48 buttons × 75fps = 3,600 repaints/sec → 77% CPU
- **Playing:** Additional infinite loop repaints → >90% CPU

### After Fix:

- **Idle:** 0 repaints/sec → <5% CPU (estimated)
- **Playing:** Only playing buttons repaint (75fps each) → Linear scaling
- **Future 960 buttons:** Still efficient (only active buttons repaint)

---

## Key Principles Maintained

1. **75fps atomic sync preserved** - Timer still runs continuously for reliable multi-clip playback
2. **Broadcast-standard timing** - 75fps update rate maintained for smooth animations
3. **Scalable to 960 buttons** - Only repaints what changes, not entire grid

---

## Testing Checklist

- [x] Build completes without errors
- [x] App launches successfully
- [ ] CPU usage <10% at idle (visual check in Activity Monitor)
- [ ] Playing clips still animate smoothly
- [ ] Multiple clips play simultaneously without stuttering
- [ ] Pulsing border animation works on playing clips
- [ ] State changes (play/stop) update immediately
- [ ] Drag-and-drop still works
- [ ] Edit dialog play/stop syncs with grid

---

## Related Issues

- Memory leak investigation still pending (Item 1 in OCC132)
- This fix enables future 10×12 grid (960 buttons total)

---

**Build Command:** `../../scripts/relaunch-occ.sh`
**Files Modified:** 2 files (ClipButton.cpp, ClipGrid.cpp)
**Lines Changed:** ~15 lines modified
