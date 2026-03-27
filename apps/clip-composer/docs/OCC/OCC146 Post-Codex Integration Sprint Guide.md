# OCC146 Post-Codex Integration Sprint Guide

**Date:** 2026-03-26
**Status:** Sprint Guide (Active)
**Scope:** Documentation remediation and new surface documentation following Codex parallel integration streams

## Summary

Three Codex streams (Shell, UI State, Routing) produced working code but left documentation gaps. The shell stream removed MenuBarModel inheritance and replaced magic-integer menu dispatch. The UI state stream wired ClipGrid to a pre-built snapshot with repaint gating. The routing stream introduced AudioRoutingTypes, AudioRoutingHelpers, and per-device route persistence. All three shipped code without corresponding architecture documentation or index updates.

This guide covers two remediation phases: P1 fixes broken navigation caused by stale index references, and P2 fills documentation gaps for four undocumented code surfaces. It also catalogs cross-project performance patterns discovered in freqfinder during the integration audit.

## P1 — Fix Broken Navigation

### 1. docs/INDEX.md

Remove 8+ phantom references to deleted files: `orp/_process/*`, `MIGRATION_v0_to_v1.md`, `API_SURFACE_INDEX.md`, `SDK_TEAM_HANDOFF.md`, `SDK_SPRINT_SUMMARY.md`, `DRIVER_ARCHITECTURE.md`, `DRIVER_INTEGRATION_GUIDE.md`, `CONTRACT_DEVELOPMENT.md`. Replace with accurate pointers to ORP120-125.

### 2. docs/ARCHITECTURE.md

Recreate as stub pointing to ORP124 (authoritative layer map). The previous version was deleted during the bloat remediation sprint (ORP116) but is still referenced by CLAUDE.md and several ORP docs.

### 3. docs/ROADMAP.md

Recreate as stub pointing to ORP121 (audio backend refactoring plan). Same situation as ARCHITECTURE.md — deleted but still referenced.

### 4. apps/clip-composer/OCC.md

Regenerate to match canonical `docs/occ/OCC.md`. Update "Next Available" to OCC147.

### 5. docs/occ/OCC.md

Update "Next Available" to OCC147. Add OCC144-146 to recent docs list.

## P2 — Document Undocumented Surfaces

### 6. OCC147: Routing Architecture

Largest documentation gap. Should cover:

- **AudioRoutingTypes.h** — Route assignment structs, channel mapping enums, the `RouteAssignment` and `ChannelMapping` types that define how clips map to physical outputs
- **AudioRoutingHelpers.cpp/h** — Utility functions for route validation and application, including channel count negotiation and format conversion
- **AudioEngine routing API (~8 methods)** — `setRouteAssignment`, `getRouteAssignment`, `getAvailableOutputChannels`, `applyRouteToBuffer`, and related methods that form the public routing surface
- **AudioSettingsDialog routing UI** — Device-aware route configuration panel that enumerates available channels and presents assignment controls
- **SessionManager per-device persistence** — Route assignments saved and restored per audio device identifier, ensuring routing survives device changes and session reloads

### 7. OCC148: UI State Polling Architecture

Replaces archived OCC127 (State Synchronization Architecture). Should cover:

- **30Hz timer in MainComponent** — `timerCallback` calls `refreshUiSnapshot`, which builds a `ClipComposerUiSnapshot` from all services; ClipGrid then polls this snapshot on its own paint cycle
- **4Hz timer in AudioSettingsDialog** — Monitors device state changes (sample rate, buffer size, active device) and updates UI without user interaction
- **ClipComposerUiSnapshot struct hierarchy** — `ClipUiSnapshot` (per-clip state: position, gain, fade, loop, playing), `SessionUiSnapshot` (active tab, clip list), `AudioEngineUiSnapshot` (CPU load, buffer stats)
- **Consumer pattern** — ClipGrid polls the pre-built snapshot; no direct service queries from UI components. This eliminates lock contention and ensures consistent state within a single frame.
- **Cross-reference: freqfinder UiFrameState** — `~/dev/freqfinder/src/ui/UiFrameState.h` implements the same pattern for spectrum and partial data
- **Cross-reference: freqfinder repaint gating** — SpectrumView data-change gate and PartialButton `suppressRepaint` flag

### 8. OCC149: IAudioDeviceHost Abstraction

Should cover:

- **Dependency injection layer** over `juce::AudioDeviceManager` — isolates audio device enumeration and lifecycle from business logic
- **Production implementation** — `AudioDeviceHost.cpp/h` wraps real JUCE device manager, delegates enumeration, open/close, and callback registration
- **Test implementation** — `FakeAudioDeviceHost` provides deterministic device responses for unit tests without requiring real audio hardware
- **Benefits** — Testability (all AudioEngine tests run without hardware), device enumeration decoupling (swap CoreAudio for ASIO without touching engine code), mockable error conditions

### 9. OCC150: Build Metadata and Versioning

Should cover:

- **BuildInfo.h.in template** — CMake `configure_file` step that stamps `kBuildVersion`, `kBuildGitHash`, `kBuildDate` into a generated header
- **Version string composition** — `major.minor.patch` from CMakeLists.txt project version, combined with short git hash from `git describe`
- **Window title integration** — MainComponent reads `kBuildVersion` to display "Clip Composer v0.x.y (abcdef)" in the title bar
- **About dialog integration** — Full version string, build date, and commit hash displayed in the info panel

## Cross-Project Performance Patterns

Performance patterns discovered in freqfinder (`~/dev/freqfinder`) that are applicable to OCC.

### Batch Repaint Suppression

- **Source:** freqfinder `PartialButton::setSuppressRepaint()` (`src/ui/PartialButtonList.h:28,54`)
- **Pattern:** During batch sync loops, set `suppressRepaint=true` before updating all buttons, `false` after, then issue a single `repaint()`. Eliminates up to 33 repaint regions per tick.
- **OCC Application:** ClipGrid's 48-button refresh loop. Implemented in this sprint (P3-10) via snapshot diff gating — buttons only repaint when their individual snapshot data changes.

### Atomic/Plain Bool Split for RT Hot Path

- **Source:** freqfinder `FrequencyGlide` (`src/dsp/FrequencyGlide.h:50,64,137,142`)
- **Pattern:** Split into `atomic<bool>` (control thread queries) + plain `bool audioThreadRamping_` (audio callback). The audio thread copies the atomic value once at block start, then reads the plain bool per-sample. Eliminates per-sample atomic loads in the inner loop.
- **OCC Application:** AudioEngine clip state queries (`isPlaying`, `isFading`). Filed as P3-13 for future profiling-guided work — not implemented this sprint because current CPU load does not warrant the complexity.

### Data-Change Gated Repaints

- **Source:** freqfinder SpectrumView — skips `repaint()` when snapshot data is unchanged from previous frame
- **Pattern:** Compare current frame data against previous frame buffer; only call `repaint()` if delta exceeds threshold. For frequency data, a per-bin magnitude threshold works well. For progress bars, quantize to 1% steps.
- **OCC Application:** ClipButton progress updates. Implemented in this sprint (P3-10) via snapshot `!=` comparison with 1% progress quantization — a clip at 45.3% and 45.7% produce the same quantized value, suppressing the repaint.

### SIMD Gain Path

- **Source:** freqfinder `FloatVectorOperations::multiply` for non-ramping master gain
- **Pattern:** Use JUCE's SIMD-optimized `FloatVectorOperations` for constant-gain buffer multiplication instead of per-sample scalar loops. Only applies when gain is not ramping — ramping still requires per-sample interpolation.
- **OCC Application:** AudioEngine group gain application. Future optimization candidate when profiling identifies gain application as a bottleneck.

## Implementation Status

| Item | Status | Commit |
|------|--------|--------|
| P0-2: Remove MenuBarModel inheritance | Done | `fix(occ): remove MenuBarModel inheritance` |
| P3-9: Named method dispatch | Done | `refactor(occ): replace magic-integer menu dispatch` |
| P3-10: Snapshot wire + repaint gate | Done | `fix(occ-ui): wire ClipGrid to pre-built snapshot` |
| P3-11: Snapshot field additions | Done | `feat(occ-ui): add displayName, color, clipGroup fields` |
| P3-12: Zero-trim progress fix | Done | `fix(occ-ui): use durationSamples fallback` |
| P3-13: Atomic/plain bool split | Future | Documented here for profiling-guided work |
| P1: Fix broken navigation | Next Sprint | See sections 1-5 above |
| P2: Document undocumented surfaces | Next Sprint | See sections 6-9 above |

## References

- [1] OCC127 (archived): State Synchronization Architecture — superseded by planned OCC148
- [2] ORP120-125: Current SDK documentation suite
- [3] freqfinder repo: `~/dev/freqfinder` — cross-project performance patterns
- [4] Codex integration branch: `codex-occ-freqfinder-sprint`
