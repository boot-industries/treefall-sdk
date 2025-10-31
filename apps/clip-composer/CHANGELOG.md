# Changelog

All notable changes to Orpheus Clip Composer will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
