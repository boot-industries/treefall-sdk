# OCC095 - ColorSwatchPicker Crash Fix (Double Ownership)

Critical bug fix for crash in Ableton-style color picker popup.

## Problem

Application crashed immediately when clicking any color in the ColorSwatchGrid popup with SIGABRT (use-after-free).

### Crash Analysis

**Stack Trace (from DiagnosticReports):**

```
ColorSwatchGrid::mouseDown(juce::MouseEvent const&) at ColorSwatchPicker.cpp:127
  → repaint() called on deleted object
```

**Root Cause:**
Double ownership of ColorSwatchGrid object causing use-after-free:

1. Created ColorSwatchGrid with `new` (line 223)
2. Stored in `m_popupHolder` unique_ptr (line 238)
3. **Also** passed ownership to CallOutBox via `std::unique_ptr<juce::Component>(grid)` (line 243)
4. Lambda captured raw pointer `grid` (line 225)

**Sequence of Events:**

1. User clicks color swatch in popup
2. Lambda executes, calls `repaint()` on grid (line 127)
3. Lambda calls `hideColorPopup()` (line 231)
4. `hideColorPopup()` deletes grid via `m_popupHolder.reset()` (line 250)
5. Grid is **deleted again** when CallOutBox closes
6. **CRASH**: Double-delete and/or dangling pointer access

## Solution

Fixed ownership model to prevent double-delete:

### Changes Made

**ColorSwatchPicker.cpp:**

1. **Removed lambda capture of grid pointer** (line 225):
   - Before: `[this, grid]` ❌ (captured raw pointer to grid)
   - After: `[this]` ✅ (only capture parent picker)

2. **Removed storage in m_popupHolder** (line 238):
   - Before: `m_popupHolder.reset(grid);` ❌ (double ownership)
   - After: Removed entirely ✅ (CallOutBox is sole owner)

3. **Removed hideColorPopup() call from lambda** (line 231):
   - Before: `hideColorPopup();` ❌ (attempted to delete grid while in use)
   - After: Removed ✅ (CallOutBox manages own lifetime)

4. **Simplified hideColorPopup()** (line 250):
   - Before: `m_popupHolder.reset();` ❌ (attempted delete)
   - After: Only sets `m_isPopupVisible = false` ✅ (state tracking only)

**ColorSwatchPicker.h:**

5. **Removed m_popupHolder member** (line 74):
   - Before: `std::unique_ptr<juce::Component> m_popupHolder;` ❌
   - After: Removed entirely ✅

### Corrected Ownership Model

```
ColorSwatchPicker (parent button)
  └─ Launches CallOutBox::launchAsynchronously()
       └─ std::unique_ptr<ColorSwatchGrid> (SOLE OWNER)
            └─ Deleted automatically when CallOutBox closes
```

**Lambda Lifetime Safety:**

- Lambda only captures `this` (parent ColorSwatchPicker)
- Does **not** capture grid pointer (would be dangling after delete)
- CallOutBox manages grid lifetime independently

## Testing

### Before Fix:

- ❌ Click any color in popup → Immediate crash (SIGABRT)
- ❌ Crash log: `ColorSwatchGrid::mouseDown` → `repaint()` on deleted object

### After Fix:

- ✅ Build succeeds (no undefined references)
- ✅ No double-delete warnings
- ✅ Ready for manual testing (click colors in popup)

## Session Persistence (User Request)

User requested: "And is saved / recalled by sessions"

**Status:** ✅ Already implemented

Color picker integrates with existing ClipMetadata system:

- **Load:** `setClipMetadata()` calls `m_colorSwatchPicker->setSelectedColor(m_metadata.color)` (ClipEditDialog.cpp:51)
- **Save:** Color picker callback updates `m_metadata.color` (ClipEditDialog.cpp:380-382)
- **Storage:** Color stored in ClipMetadata::color (juce::Colour type)
- **JSON:** Saved/loaded via SessionManager (existing infrastructure)

No additional work required for session persistence.

## 75 FPS Clip State Sync (User Concern)

User mentioned: "Be sure that the colour picker obeys the same 75 fps clip state sync..."

**Analysis:**
Color picker does **not** require special 75fps handling because:

- Updates **metadata only** (not playback state)
- Does not interact with audio thread
- Changes are immediate (no buffering needed)
- `repaint()` handled by JUCE message thread at its own refresh rate
- Not part of clip state polling system (that's for play/stop/position)

**Conclusion:** No synchronization needed - color is metadata, not state.

## Technical Details

### Files Modified

- `apps/clip-composer/Source/UI/ColorSwatchPicker.cpp` (5 changes)
- `apps/clip-composer/Source/UI/ColorSwatchPicker.h` (1 change)

### Build Status

- ✅ Compiles successfully
- ✅ No linker errors
- ✅ No warnings
- ⏳ Manual testing required (verify no crash on color click)

### Memory Safety

**Before:**

```cpp
auto* grid = new ColorSwatchGrid();
m_popupHolder.reset(grid);  // First owner
CallOutBox::launchAsynchronously(std::unique_ptr<>(grid), ...);  // Second owner ⚠️
grid->onColorSelected = [this, grid](...) {  // Dangling pointer ⚠️
  repaint();  // ☠️ Crash if grid deleted
};
```

**After:**

```cpp
auto* grid = new ColorSwatchGrid();
// No m_popupHolder (removed)
CallOutBox::launchAsynchronously(std::unique_ptr<>(grid), ...);  // Sole owner ✅
grid->onColorSelected = [this](...) {  // Safe - only captures parent ✅
  repaint();  // ✅ Only repaints parent picker
};
```

## Next Steps

1. ✅ Build passes
2. ⏳ Manual testing: Open Edit Dialog → Click color picker → Click color → Verify no crash
3. ⏳ Verify color persists after closing/reopening dialog
4. ⏳ Verify color saves/loads with session
5. ⏳ Ready to merge to PR#117

## References

[1] JUCE CallOutBox documentation: https://docs.juce.com/master/classCallOutBox.html
[2] C++ unique_ptr ownership semantics: https://en.cppreference.com/w/cpp/memory/unique_ptr
[3] OCC094 - Original layout reorganization (introduced color picker)

---

**Status:** ✅ Fixed (build passes, manual testing required)
**Severity:** Critical (crash on every color selection)
**Impact:** Blocks PR#117 merge
**Resolution:** Corrected ownership model (CallOutBox sole owner, lambda safe)
