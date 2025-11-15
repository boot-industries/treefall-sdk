# Transport Control Icons - Evaluation

**Status:** Preview SVGs for evaluation
**Date:** 2025-11-15
**Purpose:** Select best icon library for Clip Composer transport controls

---

## Icon Libraries Evaluated

### 1. Tabler Icons ⭐ **RECOMMENDED**

**Repository:** https://github.com/tabler/tabler-icons
**License:** MIT
**Total Icons:** 5,900+
**Style:** Stroke-based (`stroke="currentColor"`)
**ViewBox:** 24×24

#### Characteristics

- **Simplest path complexity** - Easiest to convert to JUCE `Path` objects
- **Clean, minimal design** - Professional appearance for audio software
- **Stroke-based** - Matches current implementation approach
- **Consistent visual weight** - All icons share same 2px stroke width
- **Large collection** - 5,900+ icons for future UI needs

#### SVG Examples

**Play icon path complexity:** `d="M7 4v16l13 -8z"` (11 characters)

**Files:**

```
transport/tabler/
├── player-skip-back.svg    (390 bytes)
├── player-play.svg         (337 bytes)
├── square.svg              (389 bytes)
├── player-skip-forward.svg (370 bytes)
└── repeat.svg              (427 bytes)
```

#### Why Tabler Wins

1. **Path Simplicity** - Shortest, cleanest SVG paths → easier JUCE conversion
2. **Visual Clarity** - Bold enough to see at small sizes, not overly heavy
3. **Consistency** - All icons share same design language
4. **Performance** - Minimal path complexity = faster rendering
5. **Future-proof** - 5,900+ icons means we can maintain consistent design across entire app

---

### 2. Lucide Icons (Alternative)

**Repository:** https://github.com/lucide-icons/lucide
**License:** ISC
**Total Icons:** 1,624
**Style:** Stroke-based (`stroke="currentColor"`)
**ViewBox:** 24×24

#### Characteristics

- **More complex paths** - Uses arcs and bezier curves for smoother appearance
- **Rounded corners/joins** - Softer visual style
- **Moderate visual weight** - Similar to Tabler but more detailed
- **Smaller collection** - 1,624 icons total

#### SVG Examples

**Play icon path complexity:** `d="M5 5a2 2 0 0 1 3.008-1.728l11.997 6.998a2 2 0 0 1 .003 3.458l-12 7A2 2 0 0 1 5 19z"` (94 characters)

**Files:**

```
transport/lucide/
├── skip-back.svg    (335 bytes)
├── play.svg         (306 bytes)
├── square.svg       (261 bytes)
├── skip-forward.svg (333 bytes)
└── repeat.svg       (347 bytes)
```

#### Pros vs Tabler

- Slightly more polished/refined appearance
- Rounded corners may feel more modern
- Popular in React/web ecosystem

#### Cons vs Tabler

- **More complex paths** - Harder to convert to JUCE, more prone to conversion errors
- Smaller icon collection (1,624 vs 5,900)
- Path complexity increases CPU overhead (minimal but measurable)

---

### 3. Phosphor Icons (Alternative)

**Repository:** https://github.com/phosphor-icons/core
**License:** MIT
**Total Icons:** 9,000+ with 6 weights (thin, light, regular, bold, fill, duotone)
**Style:** Fill-based (`fill="currentColor"`)
**ViewBox:** 256×256 (high-resolution)

#### Characteristics

- **Fill-based approach** - Different rendering paradigm from current stroke implementation
- **Multiple weights** - Can choose visual thickness (thin → bold)
- **High-resolution viewBox** - 256×256 allows fine detail
- **Largest collection** - 9,000+ icons across all weights

#### SVG Examples

**Play icon path complexity:** `d="M232.4,114.49,88.32,26.35a16,16,0,0,0-16.2-.3A15.86,15.86,0,0,0,64,39.87V216.13A15.94,15.94,0,0,0,80,232a16.07,16.07,0,0,0,8.36-2.35L232.4,141.51a15.81,15.81,0,0,0,0-27ZM80,215.94V40l143.83,88Z"` (180+ characters)

**Files:**

```
transport/phosphor/
├── skip-back.svg    (291 bytes)
├── play.svg         (293 bytes)
├── square.svg       (216 bytes)
├── skip-forward.svg (275 bytes)
└── repeat.svg       (473 bytes)
```

#### Pros vs Tabler

- **Weight flexibility** - Can adjust visual thickness (thin/light/regular/bold)
- Largest icon collection (9,000+)
- High-resolution viewBox allows fine detail

#### Cons vs Tabler

- **Fill-based vs stroke-based** - Requires different JUCE implementation approach
- **Complex paths** - Even more complex than Lucide
- Higher overhead for conversion and rendering
- Different visual paradigm may not match rest of UI

---

## Visual Comparison: Path Complexity

Comparing the **play** icon across all three libraries:

| Library    | Path Length | Complexity  | JUCE Conversion   |
| ---------- | ----------- | ----------- | ----------------- |
| **Tabler** | 11 chars    | ⭐ Simplest | ✅ Easiest        |
| Lucide     | 94 chars    | ⚠️ Moderate | ⚠️ Moderate       |
| Phosphor   | 180+ chars  | ❌ Complex  | ❌ Most difficult |

---

## Implementation Notes for JUCE

### Converting SVG to JUCE `Path`

All three libraries use standard SVG path syntax, but complexity varies:

**Tabler (Simplest):**

```cpp
// Tabler player-play: d="M7 4v16l13 -8z"
juce::Path playPath;
playPath.startNewSubPath(7, 4);
playPath.lineTo(7, 20);     // v16
playPath.lineTo(20, 12);    // l13 -8
playPath.closeSubPath();    // z
```

**Lucide (Moderate):**

```cpp
// Lucide play: d="M5 5a2 2 0 0 1 3.008-1.728l11.997..."
// Requires parsing arc commands (a), bezier curves, etc.
// More prone to conversion errors
```

**Phosphor (Complex):**

```cpp
// Phosphor play: Uses fill-based approach with complex compound paths
// Requires different rendering strategy (DrawablePath with fill instead of stroke)
```

### Sizing Constraint: ≤66% Button Area

All icons must be sized to **≤66% of button area** and centered.

For a 40×40px button:

- Icon should fit within 26.4×26.4px square (66% of 40px)
- Centered at 20px, 20px

**Tabler/Lucide approach:**

```cpp
// Scale from 24×24 viewBox to ≤66% button size
float iconSize = buttonSize * 0.66f;
float scale = iconSize / 24.0f;
path.applyTransform(AffineTransform::scale(scale));
```

**Phosphor approach:**

```cpp
// Scale from 256×256 viewBox to ≤66% button size
float iconSize = buttonSize * 0.66f;
float scale = iconSize / 256.0f;
path.applyTransform(AffineTransform::scale(scale));
```

---

## Recommendation Summary

### Use Tabler Icons ✅

**Rationale:**

1. **Simplest implementation** - Minimal path complexity = fewer bugs, faster development
2. **Best performance** - Simple paths render faster than complex bezier curves
3. **Professional aesthetic** - Clean, minimal design perfect for audio software UI
4. **Future-proof** - 5,900+ icons maintain design consistency across entire app
5. **Stroke-based** - Matches current implementation approach (no paradigm shift needed)

### When to Consider Alternatives

**Use Lucide if:**

- You prefer slightly more polished/rounded visual style
- You're already using Lucide elsewhere in UI
- Path complexity is not a concern

**Use Phosphor if:**

- You need multiple weight options (thin → bold)
- You want fill-based icons instead of stroke
- You need the largest possible icon collection

---

## Implementation Plan

### Phase 1: Direct SVG Integration (Recommended)

**Approach:** Use JUCE `Drawable::createFromSVG()` to load SVGs directly

**Pros:**

- No manual path conversion needed
- Preserves exact icon design
- Easy to swap icons if needed

**Cons:**

- Slightly larger memory footprint
- Less control over rendering

**Code example:**

```cpp
auto svg = juce::Drawable::createFromSVG(
    *juce::parseXML(BinaryData::player_play_svg)
);
button->setImages(svg.get());
```

### Phase 2: Manual Path Conversion (Advanced)

**Approach:** Convert SVG paths to JUCE `Path` objects manually

**Pros:**

- Maximum control over rendering
- Smallest memory footprint
- Can optimize paths for performance

**Cons:**

- Time-consuming manual conversion
- Harder to update icons later

**Recommendation:** Start with Phase 1, only move to Phase 2 if performance profiling shows it's necessary.

---

## Icon Attribution & Licensing

### Tabler Icons

- **License:** MIT License
- **Copyright:** Copyright (c) 2020-2025 Paweł Kuna
- **Attribution:** Not required for use, but appreciated
- **Website:** https://tabler.io/icons

### Lucide Icons

- **License:** ISC License
- **Copyright:** Copyright (c) 2020-present, Lucide Contributors
- **Attribution:** Not required for use
- **Website:** https://lucide.dev

### Phosphor Icons

- **License:** MIT License
- **Copyright:** Copyright (c) 2020-present, Phosphor Icons
- **Attribution:** Not required for use, but appreciated
- **Website:** https://phosphoricons.com

All three libraries are **free for commercial use** with no attribution requirement.

---

## Next Steps

1. **Review SVG previews** in `transport/tabler/`, `transport/lucide/`, `transport/phosphor/`
2. **Choose icon library** (recommendation: Tabler)
3. **Embed SVGs** as `BinaryData` in JUCE project
4. **Implement in ClipEditDialog.cpp** replacing current icon drawing code
5. **Test at multiple button sizes** to verify ≤66% sizing constraint
6. **Update CLAUDE.md** with chosen icon library for future reference

---

**Questions?** Reference this document before implementing transport icons.
