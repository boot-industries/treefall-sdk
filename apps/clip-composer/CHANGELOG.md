# Changelog

All notable changes to Orpheus Clip Composer will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.2.3-alpha] - 2026-05-15 (in progress)

### Added

- **Design-kit indicator glyphs on clip buttons** — All 7 indicators from the design-kit legend now render correctly with paths ported verbatim from the SVG symbol defs: Loop (refresh-cycle), Stop Others (shield + dot), Fade In / Fade Out (filled triangles), FX (sine-wave squiggle), Trim (range brackets), Lock (padlock).
- **Per-clip swatch face tint** — Loaded clip cells now tint their face with the per-clip colour (independent of group). Saturated 55/40 interpolation so a swatch reads at a glance across the grid; falls back to neutral chassis when no swatch is set.
- **Grid-aware ordinal padding** — Clip ordinals pad to the digit width of the largest visible number in the grid (48-cell grid → 2 digits, 100-cell grid → 3 digits).
- **Clip Edit dialog redesign** — Reorganised top-to-bottom in design-kit anatomy order: Title → Waveform block (full-file overview minimap + zoomed display + transport toolbar) → Name → Group/Colour → 2×2 Trim/Fade grid → Flag chips → Advanced → OK/Cancel.
- **`WaveformOverview`** — Full-file 24 px minimap above the main zoomed waveform. Shows a downsampled peak path with translucent green viewport scrubber, magenta IN / cyan OUT trim ticks, and a yellow playhead with glow.
- **`ConsoleChipButton`** — Real focusable `juce::Button` for the dialog's FLAGS row (Loop / Fade In / Fade Out / Stop Others). Amber-tinted when set, inset when off, blue focus halo.
- **`ConsoleActionButton`** — Real focusable `juce::Button` painted via the matte-cap primitive. Replaces painted-but-not-clickable controls in the inspector (Stop All, Cue Buss, M/S).
- **`GroupSelector`** — 4-button A/B/C/D channel selector. Selected button fully lit with the group's signature colour + cream halo; unselected buttons preview at 30% backlit intensity so all four channels read at a glance.
- **Transport strip master meter readout and CUE button** — Mono dB readout next to the master meter on the transport bar (`+/-X.X dB` in amber, "-inf" in muted grey). New CUE ghost button on the far right.
- **Transport strip collision-proofing** — Single `computeLayout()` helper shared by `paint()` and `resized()`. Surfaces collapse in operator-priority order under narrow widths (diagnostics → status → master), so no two surfaces can ever overlap.
- **Inspector content rebuild** — Routing rebuilt as a 5-column table (Group / Output / Gain / Meter / M·S) with real per-group mute/solo buttons. Preferences rebuilt as a key/value list driven by the live audio snapshot (device, sample rate, buffer size, playout route, audition route, status). Playout footer adds Stop All + Cue Buss real action buttons.

### Changed

- **Clip-button face contract** — Face tint comes from the per-clip swatch (visual identifier); the left stripe stays driven by group colour (routing channel). These are independent dimensions.
- **Indicator glyph order on clip cells** — Aligned to the design-kit render order (Loop · Stop Others · Fade In · Fade Out · FX · Trim · Lock).
- **Console palette extended** — Added the design-system named tokens (`kConsoleCoral`, `kConsolePatina`, `kConsoleWalnut`, `kConsoleTan`) so primitives stop reaching for ad-hoc literals.
- **Shared UI primitives in `ConsoleTheme.h`** — `drawEyebrow`, `drawInsetField`, `drawChip`, `drawActionButton`, `drawGroupButton` for consistent paint across surfaces.
- **`HKGroteskLookAndFeel`** sweep — TextEditor / ComboBox / Slider / TextButton overrides so secondary dialogs inherit Console treatment automatically.

### Fixed

- **`clang-format` hook PATH** — Pre-commit hook now invokes `xcrun clang-format` so it works on systems where `clang-format` is only present under the Xcode toolchain.
- **Inspector painted-button regression** — Stop All / Cue Buss / M·S in the inspector were paintings without click handlers. Replaced with real `juce::Button` components wired through `MainComponent::onStopAll` / `onCueBuss` / `onMutePressed` / `onSoloPressed`.
- **Clip-button indicator icons regression** — Indicator strip was anchored at a fixed rect that overlapped the time row and overflowed its width budget; glyphs were clipped or hidden. Now integrated into the bottom flex row with per-flag width reservation.
- **Clip Edit dialog title refresh path** — Title bar paints from `m_metadata.displayName` but the name editor's `onTextChange` didn't trigger a repaint, so the title stayed stale while typing. Now triggers `repaint()` on every keystroke.
- **Fake routing data** — Stripped the `fakeGain[4] = {0, -3, -6, -9}` placeholder from the Routing inspector. Output and Gain columns show "—" until the routing model surfaces real values (`TODO(occ149b-routing)`).

### Known follow-ups (`TODO(occ149c-…)`)

- ColorSwatchPicker still uses the popup-grid dropdown rather than an inline chip row.
- Action triad (Audition / Replace File / Clear) not yet wired.
- Advanced section density — could collapse behind a disclosure.
- Full-file overview minimap is read-only; interactive viewport scrubbing pending.
- Cue markers (HOOK / DROP / OUTRO) and amplitude y-axis labels on the main waveform.

### Documentation

- **OCC147** — Initial UI Refresh Sprint Report (pre-existing).
- **OCC148** — Corrective Pass Report (pre-existing).
- **OCC149** — Design-System Alignment Sprint (this release).
- **OCC149b** — Phase 5b Remediation Plan (this release).

---

## [0.2.2-alpha] - 2026-05-14

### Added

- **Adaptive grid density** - Added persisted Clip Grid Layout preferences for `6x6`, `8x6`, `10x6`, `6x8`, `8x8`, `10x8`, `6x10`, `8x10`, and `10x10`. The default remains `8x6`.
- **Live dense grid preset** - Added the mockup-backed `12x8` visible layout for 96 live cells while preserving 100 logical slots per tab. `10x12` remains deferred.
- **100-slot logical tabs** - Promoted per-tab UI capacity from 48 to 100 logical slots while keeping `tabIndex` and `buttonIndex` stable for existing sessions.
- **Playout transport strip** - Added a compact bottom transport strip for Playout mode with Stop All, Panic, latency, performance, and now-playing status.
- **Full inspector shell** - Added a 420px Console inspector for Edit, Routing, and Preferences so authoring modes match the full-chassis mockups.
- **Grid density tests** - Added coverage for 100-slot tab capacity, global index conversion, grid layout persistence keys, supported density dimensions, and legacy 48-visible-slot session loading.

### Changed

- **Clip button rendering** - Refreshed clip buttons with neutral loaded chassis colors, persistent group stripes, high-contrast playing/stopping states, and density-aware HUD simplification.
- **Mockup-faithful live chrome** - Reworked Playout around a 36px top strip and 52px bottom transport strip, with no right inspector and a grid-first canvas.
- **Keyboard and page operations** - Updated keyboard mapping, copy/paste/page commands, MIDI/hotkey scope, paste special, and UI snapshots to use shared grid constants.
- **Console styling** - Restyled the live transport and grid surfaces around the Orpheus Console design tokens.

### Fixed

- **macOS app launch** - Added a post-build bundle alignment step so `CFBundleExecutable` resolves to an executable in the app bundle.

---

## [0.2.0-alpha] - 2025-10-31

### Fixed

- **Stop Others fade-out** - Fixed zigzag distortion in fade-out when using "Stop Others" feature. Fade-out now uses pre-computed gain smoothing for clean, artifact-free transitions identical to manual stop behavior. (`src/core/transport/transport_controller.cpp:309-314`)

- **Real-time button state tracking** - Clip buttons now update in real-time during playback at 75fps (broadcast standard). Visual state accurately reflects audio playback state with no frozen or laggy button updates. (`ClipGrid.cpp:149-176`)

- **Edit Dialog time counter spacing** - Fixed text collision between time counter and waveform display by adding 10px vertical margin. Time counter is now fully readable with clear visual separation. (`ClipEditDialog.cpp:1434-1436`)

- **Keyboard shortcut playback restart** - `[` and `]` keyboard shortcuts now restart playback from new IN point after setting trim points, matching the behavior of `<` and `>` mouse buttons for consistent rapid audition workflows. (`ClipEditDialog.cpp:1763-1766, 1795-1800`)

- **Single command transport** - Click-to-jog now uses single SDK command (`seekClip()`) instead of 4-command workaround (stop, updateMetadata, start, updateMetadata). Results in gap-free, sample-accurate seeking with better UX responsiveness. Added `AudioEngine::seekClip()` API. (`AudioEngine.cpp:378-398`)

- **Trim point edit laws** - Playhead now respects trim boundaries across ALL input methods (Cmd+Click, Cmd+Shift+Click, keyboard shortcuts, time editor, nudge buttons). Two laws enforced: (1) If IN point set after playhead → restart from IN, (2) If OUT point set before/at playhead → jump to IN and restart. Prevents playback from ever escaping trim boundaries. (`ClipEditDialog.cpp:339-355, 686-736`)

### Performance

- 75fps visual sync adds <1% CPU overhead
- No audio dropouts during transport seeking
- UI remains responsive during rapid trim edits

### Known Issues

- Audio device selection requires manual configuration in preferences (UI pending for v0.2.1)
- Latch acceleration sensitivity may require tuning based on user feedback (deferred to v0.2.1)

---

## [0.1.0-alpha] - 2025-10-22

### Added

- **960-clip grid layout** - 10×12 button grid × 8 tabs for organizing large clip libraries
- **Edit Dialog** - Comprehensive clip editor with trim IN/OUT points, fade controls, gain adjustment, and loop mode
- **Session save/load** - JSON-based session format preserving all clip metadata (trim, fade, gain, loop, color, routing)
- **4 Clip Groups with routing** - Flexible routing matrix supporting 4 independent clip groups to master bus
- **Waveform display** - Visual waveform rendering with playhead tracking and click-to-jog navigation
- **Transport controls** - Play/pause, stop, loop mode, and keyboard shortcuts (Space, arrow keys, modifier combos)
- **Multi-tab isolation** - Full transport isolation across all 8 tabs (no cross-tab triggering)
- **Real-time audio engine** - CoreAudio/ASIO support with <5ms latency and <30% CPU usage for 16 simultaneous clips
- **Keyboard navigation** - Comprehensive keyboard shortcuts for rapid workflow (documented in OCC099)

### Performance

- Session load time: <2 seconds for 960 clips
- CPU usage: <30% with 16 simultaneous clips (Intel i5 8th gen)
- Round-trip latency: <16ms (512 samples @ 48kHz)
- Memory: Stable over extended sessions (no leaks)

### Technical

- JUCE 8.0.4 framework
- Orpheus SDK M2 integration
- C++20 codebase
- macOS CoreAudio support (ASIO/WASAPI coming)

---

## References

- [OCC093](docs/occ/OCC093%20v020%20Sprint%20-%20Completion%20Report.md) - v0.2.0 Sprint Completion Report
- [OCC102](docs/occ/OCC102.md) - v0.2.0 Release & v0.2.1 Planning
- [OCC026](docs/occ/OCC026.md) - MVP Definition & 6-Month Roadmap

---

**Project:** Orpheus Clip Composer
**License:** Proprietary (beta software)
**Website:** [Coming Soon]
