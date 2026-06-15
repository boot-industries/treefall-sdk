# OCC149 — Clip Composer Design-System Alignment Sprint

**Date:** 2026-05-15
**Branch:** `codex/clip-composer-ui-refresh`
**Build version:** `0.2.2-alpha` → `0.2.3-alpha`
**Predecessors:** OCC147 (initial pass), OCC148 (corrective pass — chassis layout, 12×8 density, button rework)
**Design reference:** `/Users/chrislyons/dev/orpheus_design-system_2605/`

---

## Context

OCC148 landed the structural chassis (36 px top / 52 px bottom strips, 420 px Console inspector, 12×8 density, clip-button rework) but explicitly skipped the Clip Edit dialog and left finer-grain alignment gaps. The goal of OCC149 was to close those gaps so the JUCE build visually matches the design-system mockups.

The sprint ran in two waves:

1. **Phases 1–7 (initial pass)** — token sweep, indicator restoration, inspector rebuild, group selector, transport meter, dialog title refresh.
2. **Phase 5b (remediation)** — user audit identified shortcuts that hurt app architecture: painted "buttons" that didn't click, regressed indicator semantics, broken colour application, transport collision risk, dialog stubbed rather than rewritten. The full remediation plan lives in **OCC149b**.

Architectural commitments introduced during 5b:

- **No painted controls.** If something looks pressable, it must be a `juce::Button` with proper input handling and keyboard focus.
- **`paint()` and `resized()` must consume the same arithmetic.** No fixed `removeFromRight(380)` paired with a wandering left consumption — guarantees no visual collisions.
- **No placeholder data without a `TODO(occ149c-…)` source comment.** Every literal that's not from the model gets a searchable anchor.
- **Operator narratives anchor every surface.** Every inspector surface gets a comment describing which operator uses it for what. If the narrative can't be written, the surface doesn't ship.
- **Design kit is canon, not "install-base affordance."** When the kit's symbol defs specify the glyph paths, port them verbatim — no inventing semantic colours or PLAY chips that aren't in the source.

---

## Scope delivered

### Phase 0 — Pre-flight commit unblock (`15c8fd33`)
Husky/lint-staged invoked bare `clang-format`, missing on systems where it only lives under the Xcode Command Line Tools. Updated `.lintstagedrc.json` to invoke `xcrun clang-format`. The OCC148 staged work was committed as the OCC149 baseline.

### Phase 1 — Token + LookAndFeel sweep (`99084a62`)
Extended `DesignTokens.h` with the design-system named tokens (`kConsoleCoral`, `kConsolePatina`, `kConsoleWalnut`, `kConsoleTan`). Added shared UI primitives to `ConsoleTheme.h`:
- `drawEyebrow` — UPPERCASE letter-spaced section header.
- `drawInsetField` — recessed input well.
- `drawChip` — amber-tinted toggle chip.
- `drawActionButton` — matte-cap action button (Default / Primary / Danger / Amber / Ghost variants).
- `drawGroupButton` — A/B/C/D channel selector with the group's signature colour.

### Phase 2 — Clip-button indicator glyphs restored (`175055d0`)
The indicator strip was anchored at a fixed `(getRight() - 54, getBottom() - 22, 48, 14)` rect that overlapped the time row and overflowed its width budget — glyphs were clipped or hidden. Rebuilt the bottom flex row so time consumes the left and indicators consume the right with per-flag width reservation.

### Phase 3 — Inspector content rebuild + swatch/group split (`1b6270bb`)
- Routing rebuilt as the mockup's 5-column table (GROUP / OUTPUT / GAIN / METER / M·S) with inline meter bars driven by the live snapshot.
- Preferences rebuilt as a key/value list sourced from the live audio snapshot.
- Playout footer adds Stop All + Cue Buss buttons.
- Clip-button paint contract clarified: face tint = swatch colour, stripe = group colour, independent dimensions.

### Phase 4 — Clip Edit dialog group selector + title bar (`f806c300`)
- New `GroupSelector` component (4 A/B/C/D buttons). Replaces the prior Group ComboBox; drives only the routing channel, leaving the swatch picker untouched.
- Title bar redrawn to the mockup spec: eyebrow ("EDIT CLIP") + bold clip name + mono `#NNN` index.

(Note: this phase was a stub — the full mockup-anatomy dialog rewrite was deferred to phase 5b.6 after the user audit.)

### Phase 5 — Transport strip master meter + CUE button (`99726a2e`)
- Master meter resized to 160×12 with the four-stop green→yellow→orange→red gradient.
- New mono 11 pt dB readout next to the meter (amber when signal present, muted grey when silent).
- New CUE ghost button on the far right with `onCue` callback hook.

### Phase 5b.1 — Clip-button face colour restored (`dcfa7bea`)
Phase 3 tint math was too weak (28%/20% over dark chassis); `m_clipColor` defaults to `juce::Colours::darkgrey` (opaque) so the `!isTransparent()` test passed and the chassis fell back to a meaningless dark-grey tint. Fixed:
- Treat both transparent and `darkgrey` as "no swatch set."
- When a real swatch is set, interpolate 55/40 toward it so the operator reads swatches at a glance.

### Phase 5b.2 — Inspector painted controls become real buttons (`dcfa7bea`)
- New `ConsoleActionButton` class (`juce::Button` subclass painting via `drawActionButton`).
- Replaced painted Stop All / Cue Buss / M·S in `ConsoleInspectorPanel` with real instances. Stop All dispatches to the existing `MainComponent::onStopAll`; Cue Buss + M·S log stub calls pending real model wiring.

### Phase 5b.3 — Fake routing data stripped (`dcfa7bea`)
Removed `fakeGain[4] = {0, -3, -6, -9}` placeholder. Output and Gain columns show "—" until the routing model surfaces real values.

### Phase 5b.4 — Indicator glyph baseline (`4bd1ecde`)
First port of the design-kit indicator paths (Loop / Fade In / Fade Out / Stop Others) from `components.jsx`.

### Phase 5b.4b — All 7 design-kit indicator glyphs (`77a4602e`)
Read the authoritative SVG symbol defs in `preview/components-clip-buttons.html` and ported all 7 glyphs verbatim: Loop, Stop Others (shield with centre dot), Fade In, Fade Out, FX (sine-wave squiggle), Trim (range brackets), Lock (padlock). Added `m_trimEnabled` / `m_lockEnabled` state fields (kept `m_effectsEnabled` as FX). Progress bar rewritten to match the kit spec (3 px white with 6 px shadow, no track).

### Phase 5b.4c — Ordinal digit width tracks grid (`fb3be319`)
ClipGrid now computes the grid-wide digit width once (`numDigits(columns * rows)`) and pushes it to every button. 6×8 grid → 2 digits, 10×10 grid → 3 digits, etc.

### Phase 5b.5 — Dialog title refresh path (`86d04e8c`)
Name editor's `onTextChange` now triggers `repaint()` so the title bar tracks live edits. Fallback string changed from "Untitled" to "New Clip".

### Phase 5b.7 — Transport collision proofing + CUE wiring (`49181a1c`)
- Single `computeLayout()` helper shared by `paint()` and `resized()`. Surfaces collapse in operator-priority order under narrow widths: diagnostics → status → master.
- Diagnostic labels `setVisible(false)` when collapsed so no stray paint lands behind the master cluster.
- `onCue` wired in MainComponent (logs the press; real cue-buss handler pending).

### Phase 5b.6 — Clip Edit dialog mockup anatomy (`76327325`)
Rewrote `resized()` and `paint()` so the dialog reads top-to-bottom in design-kit order: Title → Waveform → Name → Group/Colour → 2×2 Trim/Fade → Flags → Advanced → OK/Cancel. Legacy `juce::Label` controls hidden in favour of painted eyebrows. Documented deferrals: action triad, chip-style flags, full-file overview.

### Phase 5b.6b — Dialog feel polish (`65b899b0`)
- New `ConsoleChipButton` class. FLAGS row now has four real chip-style toggles (Loop / Fade In / Fade Out / Stop Others); Fade chips bridge to the existing fade-time combos.
- GroupSelector reskinned for channel-strip feel: selected = fully lit matte cap + cream halo; unselected = 30% backlit preview of the group colour.
- Per-cell trim/fade eyebrows (TRIM IN / TRIM OUT / FADE IN / FADE OUT) instead of a single broken `TRIM · FADE` (UTF-8 encoding bug).

### Phase 5b.6c — Design-kit waveform block + transport rail (`5c9a426e`)
Matched the kit's three-tier waveform pattern from `preview/components-waveform.html`:
1. **`WaveformOverview` (new)** — 24 px full-file minimap above the main waveform with translucent green viewport scrubber, magenta IN / cyan OUT trim ticks, yellow playhead with glow.
2. Main zoomed `WaveformDisplay` retained.
3. **Transport toolbar** restored to prominence directly under the waveform: left transport cluster (skip-back / play / stop / skip-end) · centre position readout · right zoom cluster (− / level / +).
Advanced section trimmed to only the truly secondary controls (gain, deferred pitch, SET/CLR/nudge, fade-curves, trim duration readout) — no more transport/zoom duplication.

---

## Verification

Each phase landed with a clean cmake build and 51/51 ctest. Tests:
- `/opt/homebrew/bin/cmake --build build --target orpheus_clip_composer_app clip_composer_tests`
- `/opt/homebrew/bin/ctest --test-dir build/apps/clip-composer/tests --output-on-failure`

Manual visual inspection done after Phases 5b.5 / 5b.6 / 5b.6b / 5b.6c with the app launched from the existing Release bundle. User-confirmed observations drove the iteration order — three rounds of design-kit screenshot updates landed during the sprint and were folded into the build.

The four operator-mode surfaces (Playout / Edit / Routing / Preferences) are functional. The Clip Edit dialog matches the mockup anatomy with the waveform block now matching the kit's three-tier pattern.

---

## OCC149c completion status

The OCC149c visual-alignment follow-ups have been reconciled on `feat/occ-audio-utility-polish`:

- **Colour chips** — `ColorSwatchPicker` is an inline chip row in the Clip Edit dialog; the compact popup remains only for context-menu use.
- **Chip buttons** — `ConsoleChipButton` is a real focusable `juce::Button` with amber active state and focus halo.
- **Action triad** — `AUDITION`, `REPLACE FILE`, and `CLEAR` are real dialog buttons. Replace File delegates through MainComponent's file chooser/load path and preserves operator metadata intent.
- **Advanced collapse** — the secondary Advanced section collapses behind the disclosure and sets parked controls non-visible so keyboard focus cannot land on hidden gain/trim/fade controls.
- **Minimap scrub** — `WaveformOverview` supports click-to-jump and drag-to-scrub viewport interaction.
- **Waveform overview utility** — cue-marker primitives (`HOOK`, `DROP`, `OUTRO`, custom) and the main waveform scale are present. Cue-marker persistence/editor UX is a future session-metadata feature, not an OCC149 design-alignment blocker.
- **Routing** — inspector rows now read output labels, gain, mute, and solo from the audio-engine/UI snapshot. Current transport topology routes all four groups to `Main L/R`, which is the operator-true value until per-group bus assignment exists.
- **Cue/PFL** — Cue controls are real but correctly feature-gated until the app has both multichannel-output detection and configured cue routing. This avoids shipping a fake PFL path on the main output.

Verification for the completion pass:

```bash
cmake --build build --target orpheus_clip_composer_app clip_composer_tests -j$(sysctl -n hw.ncpu)
ctest --test-dir build/apps/clip-composer/tests --output-on-failure
```

Result: `60/60` tests passed.

---

## Architectural artifacts

New files introduced this sprint:
- `Source/UI/ConsoleActionButton.h`
- `Source/UI/ConsoleChipButton.h`
- `Source/UI/GroupSelector.h`
- `Source/UI/WaveformOverview.h`

Significantly modified:
- `Source/UI/ConsoleTheme.h` — shared paint primitives.
- `Source/UI/DesignTokens.h` — design-system named tokens.
- `Source/UI/ClipEditDialog.{h,cpp}` — full mockup-anatomy rewrite of `resized()` and `paint()`.
- `Source/UI/ConsoleInspectorPanel.{h,cpp}` — real button instances, real data, mode-aware visibility.
- `Source/ClipGrid/ClipButton.{h,cpp}` — design-kit indicator glyphs, swatch face tint, grid-aware ordinal width.
- `Source/Transport/TransportControls.{h,cpp}` — `computeLayout()` collision proofing, master meter dB readout, CUE button.

---

**Status:** Complete for OCC149/OCC149b/OCC149c visual-alignment scope. Branch `feat/occ-audio-utility-polish` pushed to origin.
