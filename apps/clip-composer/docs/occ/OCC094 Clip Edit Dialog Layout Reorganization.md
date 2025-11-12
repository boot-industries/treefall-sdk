# OCC094 - Clip Edit Dialog Layout Reorganization

Complete redesign of the "Clip Edit" dialog with improved organization, compact layout, and better visual hierarchy.

## Context

The original "Edit Clip" dialog had a vertical layout that consumed excessive vertical space and separated related controls. The new "Clip Edit" layout implements a reorganized design that:

- Moves Clip Name to the top for immediate visibility
- Reorders sections for better workflow
- Adds intelligent spacing and alignment
- Improves waveform zoom behavior

## Changes Implemented

### 1. Section Reorganization

**New section order (top to bottom):**

1. Clip Name field (moved to top, full width)
2. File info panel + zoom controls
3. Waveform display
4. Transport controls + time display
5. Trim controls (In/Out with Duration centered)
6. Fade In/Out controls
7. Color + Group controls (expanded horizontally)
8. OK/Cancel buttons

### 2. Title Update

- Changed dialog title from "Edit Clip" to "Clip Edit"
- Location: `ClipEditDialog.cpp:1338`

### 3. Clip Name Field Relocation

- **Before:** Bottom section, above Color and Group
- **After:** Top of dialog, immediately below header
- **Benefit:** Immediately visible, full width available, better for long names

Implementation:

- Moved to lines 1355-1366 (top of content area)
- Full width field with overflow handling
- Single-line mode with horizontal scrolling
- Left-aligned text (start always visible)
- No scrollbar (clean appearance)

### 4. Zoom Controls Repositioning

- **Before:** Centered below waveform display (separate row)
- **After:** Top-right corner, aligned with file info header
- **Benefit:** Saves vertical space, keeps controls near waveform
- **Added:** Left margin (GRID) for spacing from file info

Implementation:

- Combined file info panel and zoom controls on same row (headerRow)
- File info takes available width, zoom controls fixed at right (GRID \* 13)
- Left margin added for visual separation
- Maintains existing zoom functionality (-, 1x, +)

### 5. Transport Time Display Centering

- **Before:** Fixed spacing below transport buttons
- **After:** Vertically centered between buttons and Duration field
- **Benefit:** Better visual balance, equidistant positioning

Implementation:

- Increased transport section height to GRID \* 10
- Used `withSizeKeepingCentre()` to position time display
- Time display now floats in center rather than fixed offset

### 6. Trim Controls Layout Updates

- **Before:** Two vertical columns (Trim In left, Trim Out right)
- **After:** Horizontal layout with three rows:
  - Row 1: "Trim In" label (left) + "Trim Out" label (right)
  - Row 2: Trim In time/SET + Duration (centered box) + SET/Trim Out time
  - Row 3: < > CLR buttons (left) + CLR < > buttons (right)

- **Benefit:** More compact, better visual symmetry, duration display more prominent

Implementation details:

- Trim In controls: Time field (GRID _ 10) + SET button (GRID _ 5)
- Trim Out controls: SET button (GRID _ 5) + Time field (GRID _ 10) [reversed order]
- Duration box: Centered, GRID \* 18 wide with border/background styling
- Navigation buttons: < (GRID _ 3) + > (GRID _ 3) + CLR (GRID \* 5)

### 4. Duration Display Enhancement

- **Before:** Plain text label with light grey color
- **After:** Bordered box with dark background and white bold text

Styling changes:

```cpp
Font: Inter 14pt bold (was 12pt plain)
Text color: White (was light grey)
Background: #2a2a2a (dark grey)
Border: #555555 (medium grey)
Justification: Centered
```

### 5. Metadata Section Restructure

- **Before:**
  - Row 1: Clip Name label
  - Row 2: Clip Name field
  - Row 3: Color label + dropdown + Group label + dropdown

- **After:**
  - Row 1: Clip Name label
  - Row 2: Clip Name field (60% width) + Color label/dropdown + Group label/dropdown

- **Benefit:** More compact, single row for all metadata fields

Implementation:

- Clip Name field: 60% of available width
- Color: Label (GRID _ 5) + Combo (GRID _ 10)
- Group: Label (GRID \* 6) + Combo (remaining width)

### 6. Fade Section Optimization

- **Before:** Two separate column sections with labels above
- **After:** Labels on same row, controls on row below (left/right split maintained)

- **Benefit:** Saves one grid row (GRID _ 8 → GRID _ 6)

Implementation:

- Fade In/Out labels rendered on same row (left/right positioning)
- Controls maintain left/right split with time + curve dropdowns

## Technical Details

### Files Modified

- `apps/clip-composer/Source/UI/ClipEditDialog.cpp`
  - Lines 1087-1093: Duration label styling
  - Lines 1338: Title text change
  - Lines 1344-1584: Complete `resized()` method rewrite

### Layout Constants

- Grid unit: 10px (unchanged)
- Title bar: 50px (unchanged)
- Content padding: GRID \* 2 (unchanged)
- Header row: GRID \* 3 (file info + zoom)
- Waveform: GRID \* 15 (unchanged)
- Transport section: GRID \* 6 (unchanged)
- Trim section: GRID _ 10 (was GRID _ 12)
- Duration box: GRID _ 18 wide, GRID _ 3 tall
- Metadata row: GRID \* 3 (unchanged)
- Fade section: GRID _ 6 (was GRID _ 8)

### Visual Hierarchy Improvements

1. Duration display now visually prominent (bordered box vs plain text)
2. Zoom controls positioned near waveform (top-right) for better UX
3. Trim controls symmetrical (left/right mirrored layout)
4. Metadata fields consolidated on single row (less vertical scanning)
5. Fade controls more compact (shared label row)

## Testing Considerations

### Visual Verification

- [ ] Zoom controls visible in top-right corner
- [ ] Duration box has visible border and dark background
- [ ] Trim In/Out labels aligned left/right
- [ ] Trim controls symmetrical (time + SET on both sides)
- [ ] Navigation buttons (< > CLR) properly aligned
- [ ] Clip Name field takes ~60% width
- [ ] Color and Group dropdowns fit on same row as Clip Name
- [ ] Fade In/Out labels on same row
- [ ] OK/Cancel buttons remain bottom-right

### Functional Verification

- [ ] All trim controls function correctly (SET, CLR, < >)
- [ ] Zoom controls work (-, 1x, +)
- [ ] Duration display updates when trim points change
- [ ] Keyboard shortcuts still work ([ ] ; ' for navigation)
- [ ] Focus order maintained (Name → Trim IN → Trim OUT)
- [ ] All metadata fields editable

### Regression Testing

- [ ] Waveform display still renders correctly
- [ ] Transport controls function (play, stop, skip, loop)
- [ ] Preview player works (space to play/pause)
- [ ] Fade controls update metadata
- [ ] Dialog resizing maintains layout proportions

## Performance Impact

No performance impact expected. Layout changes are purely visual reorganization using existing JUCE layout methods. No new allocations or processing overhead introduced.

## Accessibility

- Duration display now more readable (larger, bold, better contrast)
- Symmetrical trim layout easier to understand
- Reduced vertical height improves visibility on smaller screens
- No functional changes to keyboard navigation

## Next Steps

1. Manual testing with actual audio clips
2. Visual verification at different dialog sizes
3. User feedback on new layout (if in beta)
4. Document in release notes for v0.2.0+

## References

[1] Original layout design: apps/clip-composer/docs/OCC/OCC010 Wireframes v1.md
[2] Component architecture: apps/clip-composer/docs/OCC/OCC023.md
