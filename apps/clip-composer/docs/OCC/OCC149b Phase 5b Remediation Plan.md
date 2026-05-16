# OCC149 Phase 5b — Remediation plan

**Date:** 2026-05-15
**Branch:** `codex/clip-composer-ui-refresh`
**Predecessor commits to remediate:** `99084a62` (P1) · `175055d0` (P2) · `1b6270bb` (P3) · `f806c300` (P4) · `99726a2e` (P5)
**Trigger:** user audit identified shortcuts that hurt app architecture, broken controls, dead UI, mockup misalignment, and a colour-application regression.

---

## Operating constraints (added this round)

1. **Operator narratives are the contract.** Every UI surface must serve a defined operator workflow. The four modes are:
   - **Playout** — live operator triggering clips at the chassis under air. Surfaces: full grid, master meter, Stop All / Panic / Cue. *Do not* break the operator's read-at-a-glance flow.
   - **Edit** — sound-designer / producer trimming clips, setting fades, assigning group, choosing colour. Surfaces: 420 px inspector summary + modal Clip Edit dialog.
   - **Routing** — engineer mapping the four groups onto physical outputs, setting trims and mute/solo. Surfaces: 420 px inspector matrix wired to the real routing model.
   - **Preferences** — operator confirming device / sample rate / buffer / paths. Surfaces: 420 px inspector pulling live snapshot fields.
2. **No painted controls.** Anything that the operator can click or focus must be a real `juce::Button` / `Component` with mouse and keyboard handlers. Paintings are for indicators, not interactive surfaces.
3. **No visual collisions.** Under any supported chassis width (~1100 px live floor up to user max), no two surfaces may share pixels. Paint and `resized()` must agree on geometry.
4. **No placeholder data in shipping UI.** If a model field doesn't exist, the surface must either be omitted, marked clearly as "—", or wired to a deliberate stub with a `TODO(occ149b)` reference. No silent `fakeGain = {0,-3,-6,-9}` shipped as real UI.
5. **The Clip Edit dialog must structurally match the mockup.** Not "approximate." If the existing dialog has controls outside the mockup anatomy (gain dial, pitch dial, nudge buttons, zoom, fade-curve combos), they belong in a clearly-secondary section *below* the primary mockup-anatomy block, not woven through it. The primary block — waveform / name / group / colour / 2×2 trim+fade / flag chips / action row — must be visually intact and unmistakable.

---

## Audit findings, by phase

### Phase 1 — Token + LookAndFeel sweep
- `drawEyebrow`, `drawChip`, `drawInsetField` are **dead code** (0 call sites outside the file). `drawChip` even computes a `cream` variable then discards it with `(void)cream`.
- `drawActionButton` only used by **painted** "buttons" in the inspector — no callback, no focus, no hover. So it's also functionally dead.
- `drawGroupButton` is the only primitive that pulls weight (used by `GroupSelector`).
- **Net:** shipped a "shared primitives library" most of which doesn't ship. The plan said primitives should be shared between dialog and inspector — Phase 4 didn't use them at all.

### Phase 2 — Indicator icon regression
- Dropped the PLAY chip and STOP-OTHERS hexagon glyphs unilaterally. Argued "playing state is already signaled by the border pulse." That's a product-design call I had no authority to make.
- Flattened all flag glyph colours to cream-0.95. The original system used **semantic colour per flag** (yellow loop, cyan fade-in, orange fade-out) that the operator can read across the grid. Mockup spec is *for the mockup*; it doesn't override semantic colour for OCC's installed-base affordance.
- **Net:** I changed UX semantics in the name of "mockup alignment."

### Phase 3 — Inspector content rebuild
- The inspector's **Stop All / Cue Buss footer** in Playout, and the **M·S buttons** in Routing, are paintings. Click → nothing happens. No `mouseDown`, no `juce::Button`, no callback wiring, no keyboard focus, no accessibility. This is the worst sin of the sprint — adding controls that look real but aren't.
- **`fakeGain[4] = {0, -3, -6, -9}`** is hardcoded in the painted Routing table. Shipped as fake gain readouts.
- Routing has no real model wiring for output names, gain, mute/solo. The 5-column table is *aesthetically aligned* and *functionally empty*.

### Phase 4 — Clip Edit dialog
- Title bar swap: pulls `m_metadata.displayName` for the title — but **`paint()` is not triggered on name-editor text changes**, so the title doesn't refresh as the operator types.
- The actual mockup anatomy (110 px waveform / name / group / colour / 2×2 trim+fade / flag chips / matte action row) was **not delivered**. I shipped a Group selector swap and a title-bar paint change and called the phase done.
- **Net:** dialog still doesn't look like the mockup. As the user observed.

### Phase 5 — Transport master meter + CUE
- `paint()` reaches `removeFromRight(380)`, `resized()` reserves `320 + 60` on the right and consumes from the left for memory/CPU labels. On a chassis narrower than ~880 px the **master meter overlaps memory/CPU**. Untested. **Visual collision.**
- `onCue` callback declared and wired through the lambda — but **MainComponent never connects it**. Clicking CUE does nothing.
- **Net:** half a feature plus a latent collision.

### Cross-cutting — Clip Button face colour regression (user-reported)
- ColorSwatchPicker writes colour through `setClipColor` correctly. Data path is fine. The regression is in **paint math**:
  - Loaded-state tint interpolates only **28 % top / 20 % bottom** of the swatch over a dark chassis. Result: even a saturated red looks like dark grey-with-a-hint.
  - `m_clipColor` defaults to `juce::Colours::darkgrey` in the constructor, which **passes the `!isTransparent()` test** but tints the face indistinguishably from a no-swatch state.
- Visible symptom: changing the colour swatch produces no visible face colour change. The original (pre-Phase-3) paint pinned the face to `groupColor`, so this is a Phase 3 regression I introduced when I split swatch and stripe.

### Cross-cutting — Operator narrative drift
- I treated the mockup as a *visual* spec only, decoupled from the operator workflows. Result: I painted ornaments where the operator expects affordances.

---

## Phase 5b deliverables (the remediation)

Eight items, executed in order. Each ends with build + 51/51 tests + manual launch checkpoint before the next.

### 5b.1 — Restore Clip Button face colour application
- Increase tint interpolation from 0.28/0.20 to a level where a saturated swatch is unmistakable. Target: ~0.55 top / 0.40 bottom over the chassis, retaining Loaded-state legibility.
- Change the `hasSwatch` test from `isTransparent` to "is meaningfully non-grey": treat `juce::Colours::darkgrey` and `juce::Colours::transparentBlack` both as "no swatch set," fall back to a neutral chassis tint (not the group colour, since group already lives on the stripe).
- Verify the data path end-to-end via `ColorSwatchPicker` → `setClipColor` → next repaint.

### 5b.2 — Inspector buttons become real buttons
- Build a small `ConsoleActionButton` class — `juce::Button` subclass that paints via the existing `drawActionButton` primitive, exposes click callbacks, handles hover/down states, and is keyboard-focusable.
- Replace the painted Stop All / Cue Buss / M·S in `ConsoleInspectorPanel` with `ConsoleActionButton` instances. Wire Stop All and Cue Buss through `MainComponent::onStopAll` / `onCue`. M·S buttons stay no-op until the routing model exposes mute/solo, but they're real focusable buttons.
- `drawActionButton` stays as the paint primitive used internally by `ConsoleActionButton::paintButton`.

### 5b.3 — Remove fake routing data
- Strip the `fakeGain[4]` placeholder. Display "—" in the GAIN column until the routing model exposes gain per group.
- Output names: pull from the routing snapshot if available, else "—" with a `TODO(occ149b-routing)` comment in code. No hardcoded "Out 1-2 / 3-4 / 5-6 / 7-8" pretending to be real.

### 5b.4 — Restore indicator glyph semantics
- Bring back the per-flag accent colour: loop → `kAccentYellow`, fade-in → `kAccentCyan`, fade-out → `kAccentOrange`, stop-others → `kMeterRed` filled hexagon (not a cream diamond outline).
- Bring back the PLAY chip rendered when the cell is in `State::Playing`, in the top-right of the meta row (not the bottom row, to avoid colliding with time and other flag glyphs). It's a small green chip with a white play triangle — distinct from the border pulse.
- Keep the bottom-row geometry I built in Phase 2 (it correctly co-located time + flags without overflow), but restore the colour and glyph semantics.

### 5b.5 — Restore Clip Edit dialog refresh path
- Add `repaint()` on the dialog when `m_nameEditor->onTextChange` fires, so the title bar tracks live edits.
- Title bar shows `m_metadata.displayName` if non-empty, otherwise `"New Clip"` — not "Untitled" or "Clip Edit." The eyebrow "EDIT CLIP" already gives the section identity.

### 5b.6 — Clip Edit dialog mockup anatomy (the real Phase 4)
This is the largest piece. Plan it as a sub-sprint:
1. **Reorganise `resized()`** so the top of the dialog is the mockup's primary block in exactly the mockup order, in dedicated row regions:
   1. Title bar (50 px) — already done.
   2. Waveform (110 px, the existing `m_waveformDisplay`).
   3. NAME row (eyebrow 14 px + field 28 px).
   4. GROUP row (eyebrow 14 px + 4-button `GroupSelector` 28 px).
   5. COLOUR row (eyebrow 14 px + `ColorSwatchPicker` 28 px) — the missing dimension the design kit omitted.
   6. 2×2 trim/fade grid (eyebrow row + 2 rows of fields, each row 32 px).
   7. FLAGS chip row — Loop / Fade In / Fade Out / Stop Others as real `ConsoleChipButton` instances bound to the existing `m_loopButton`, `m_fadeInEnabled`, `m_fadeOutEnabled`, `m_stopOthersButton` model values.
   8. Action row (36 px) — `AUDITION` (primary blue), `REPLACE FILE…` (matte), `CLEAR` (danger).
2. **Move the existing secondary controls** (gain dial, pitch dial, nudge buttons, zoom, fade-curve combo, transport playback) into a clearly-secondary `Advanced` section below the primary block, OR a tabbed secondary surface. The operator still needs them, but they don't compete for the primary anatomy.
3. **Build a real `ConsoleChipButton` class** (`juce::Button` subclass painting via `drawChip`) so the flag chips are clickable. The four chips bind to the existing model fields and persist with the dialog's commit path.
4. **Verify trim/fade fields render mono-tabular** so their numerics are readable.

### 5b.7 — Transport collision fix + CUE wiring
- Reconcile `TransportControls::paint()` master block geometry with `resized()` left-cluster geometry. Both must consume from the same `bounds` arithmetic — paint() must use the leftover space after resized() has placed the labels, not a fixed `removeFromRight(380)`.
- Add a min-width clamp: if window width < the sum of left-cluster + master cluster + CUE button + gaps, the master meter and dB readout collapse first (their text falls back to "—"), so labels never overlap.
- Wire `onCue` in `MainComponent::createTransportControls` or equivalent — make a stub handler that logs `"Cue requested"` and calls into the existing audition route if available, else no-op with a `TODO(occ149b-cue)`. Either way, the click is acknowledged.

### 5b.8 — Build, test, manual inspection
- After each item, build `orpheus_clip_composer_app` + `clip_composer_tests` and run `ctest`. Expect 51/51.
- After all eight items, launch the app via the existing Release bundle. Verify the four operator-mode surfaces match their narratives:
  - Playout: full grid + transport meter + working Cue button.
  - Edit (grid right-click → dialog): dialog matches mockup anatomy.
  - Routing: matrix shows real outputs (or "—") and the M·S buttons are clickable real components.
  - Preferences: device fields populated from snapshot.
- No visual collisions on any window width down to 1100 px.

---

## Architectural commitments going forward

- **Painted controls** are forbidden — if something looks clickable, it must be a `juce::Button` or `Component` with proper input handling.
- **Operator narrative tests:** every new inspector surface gets a one-paragraph narrative comment in the source describing which operator does what with it. If I can't write the narrative, the surface doesn't ship.
- **No placeholder data without a TODO**: every literal numeric or string that's not from the model gets a `TODO(occ149b-…)` source comment so the gap is searchable.
- **Min-width discipline:** every container's `paint()` must read the same arithmetic as its `resized()`. Both work from the same `bounds.removeFrom*` chain or they share helper functions.
- **The Phase 4 dialog rewrite was deferred — that was wrong.** The plan said "rewrite resized() and paint() to match mockup anatomy" and I shipped a Group selector swap. Phase 5b.6 is the real Phase 4.

---

## Verification matrix

After 5b.8:

| Surface | Operator | Test |
|---|---|---|
| Clip cell face colour | Sound designer | Open Edit, change swatch → face tints visibly across all 12 swatches |
| Clip cell flag glyphs | Live operator | Set loop / fade-in / fade-out / stop-others → glyphs appear in original semantic colours, plus PLAY chip when state=Playing |
| Inspector / Playout footer | Live operator | Click Stop All in inspector → all clips stop. Click Cue Buss → cue toggles |
| Inspector / Routing | Engineer | Outputs/gain show "—" until model populated; M·S buttons click (no-op) |
| Inspector / Preferences | Operator | All six rows populated from live snapshot |
| Clip Edit dialog | Sound designer | Anatomy matches mockup top-to-bottom; type in name → title bar live-updates; click flag chip → flag toggles and persists; click AUDITION → preview plays |
| Transport master meter | Live operator | Window down to 1100 px width: no overlap between left labels and meter; dB readout updates; CUE click logs |

All seven rows must pass before Phase 6 / 7 resume.
