# OCC150 — Action Triad Replace File and Portable Audio Path Fix

**Date:** 2026-06-14
**Branch:** `feat/occ-audio-utility-polish`
**Scope:** Clip Edit dialog utility completion + audio-load path correctness

---

## Context

Clip Composer is moving toward the fast, dependable operator workflows expected from broadcast soundboard tools in the Sigma SpotOn / Merging Ovation class. The OCC149 design-system pass surfaced the Clip Edit dialog's action triad (`AUDITION / REPLACE FILE / CLEAR`), but `REPLACE FILE` still had no concrete file-picker workflow.

A second functional issue was found in the shared clip-load path: when an operator chose to copy an imported file into the project audio folder, `SessionManager` persisted the copied `finalPath`, but `AudioEngine` still loaded the original `filePath`. That split the session's portable media reference from the actual playback source.

---

## Changes delivered

### 1. Edit Dialog `REPLACE FILE` is now wired

`MainComponent::onClipDoubleClicked()` now assigns `dialog->onReplaceFileClicked`:

- Opens an async `juce::FileChooser` for common audio formats (`wav`, `aiff`, `aif`, `flac`, `mp3`).
- Reuses the existing `loadClipToButton()` path so replacement follows the same session/audio-engine route as drag/drop and menu loading.
- Uses `juce::Component::SafePointer<ClipEditDialog>` so a queued async file chooser callback cannot dereference a destroyed dialog.
- Closes the dialog after a successful replacement to avoid editing stale waveform or metadata UI.

### 2. Portable copied-audio path now reaches the audio engine

`loadClipToButton()` now builds an `occ::ClipLoadPlan` and calls:

```cpp
m_sessionManager->loadClip(buttonIndex, loadPlan.sessionPath);
m_audioEngine->loadClip(globalClipIndex, loadPlan.audioEnginePath);
```

For linked imports, both paths are the original source. For copied imports, both paths are the unique project-audio destination. This keeps the session JSON, the project audio copy, and the active playback buffer aligned.

### 3. Path planning is now covered by focused tests

`Source/Core/ClipLoadPlan.h` isolates the copied-vs-linked path decision from modal UI and audio-engine side effects. `tests/test_clip_load_plan.cpp` verifies:

- Linked imports use the original path for both `SessionManager` and `AudioEngine`.
- Copied imports use the copied project path for both `SessionManager` and `AudioEngine`.
- Existing project media is not overwritten; duplicate names receive a unique suffix.

### 4. Dialog implementation notes updated

The Clip Edit dialog layout comment now reflects that `REPLACE FILE` delegates to MainComponent's file chooser + reload path rather than remaining an OCC149c deferral.

---

## Real-time safety notes

- The file chooser and media import flow run on the JUCE message thread, not the audio callback.
- The replacement path uses the existing pre-buffered clip slot loading path; no new allocations, locks, or file I/O were introduced inside the real-time audio callback.
- The async chooser capture intentionally keeps the `FileChooser` alive through callback completion while `SafePointer` protects the dialog lifetime.

---

## Verification

Commands run from `/Users/chrislyons/dev/orpheus-sdk`:

```bash
xcrun clang-format -i apps/clip-composer/Source/MainComponent.cpp apps/clip-composer/Source/UI/ClipEditDialog.cpp
cmake --build build --target orpheus_clip_composer_app clip_composer_tests -j$(sysctl -n hw.ncpu)
ctest --test-dir build/apps/clip-composer/tests --output-on-failure
```

Results:

- Debug app target built successfully: `orpheus_clip_composer_app`.
- Test target built successfully: `clip_composer_tests`.
- `54/54` Clip Composer tests passed after adding the three ClipLoadPlan regression tests.

---

## Remaining follow-ups

- Manual UI smoke test with a populated session: open Edit Dialog → Replace File → choose Copy to Project vs Link to Original → confirm playback uses the expected media.
- Add a non-modal replacement status message so operators get clear confirmation without interrupting show flow.
- Split `loadClipToButton()` further into a pure load service and UI prompt shell so Replace File can preserve selected metadata fields more selectively.
