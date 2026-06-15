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

`loadClipToButton()` now calls:

```cpp
m_audioEngine->loadClip(globalClipIndex, finalPath);
```

instead of loading the originally selected `filePath`. This keeps the session JSON, the project audio copy, and the active playback buffer aligned.

### 3. Dialog implementation notes updated

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
- `51/51` Clip Composer tests passed.

---

## Remaining follow-ups

- Manual UI smoke test with a populated session: open Edit Dialog → Replace File → choose Copy to Project vs Link to Original → confirm playback uses the expected media.
- Consider adding a non-modal status toast after replacement so operators get a clear confirmation without reopening the dialog.
- Longer term: split `loadClipToButton()` into a pure load service and UI prompt shell so Replace File can preserve selected metadata fields more selectively.
