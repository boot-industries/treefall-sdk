# OCC147 Clip Composer UI Refresh Sprint Report

**Date:** 2026-05-14
**Branch:** `codex/clip-composer-ui-refresh`
**Build version:** `0.2.2-alpha`

## Scope

This sprint refreshes Clip Composer around the Orpheus Console design direction while keeping the app native JUCE. Freqfinder is out of scope.

## Delivered Milestone

- Preserved the prior `main` work in checkpoint commit `4fe38b26`.
- Created the `codex/clip-composer-ui-refresh` branch.
- Added shared grid constants for 6-to-10 columns, 6-to-10 rows, 100 logical slots per tab, and 800 logical UI slots across 8 tabs.
- Added persisted grid layout preferences for `6x6`, `8x6`, `10x6`, `6x8`, `8x8`, `10x8`, `6x10`, `8x10`, and `10x10`; default is `8x6`.
- Updated session, snapshot, keyboard, page-command, paste-special, MIDI, and hotkey code paths to use shared grid constants.
- Kept session JSON backward compatible: `tabIndex` and `buttonIndex` fields load unchanged.
- Refreshed clip button rendering with neutral chassis color, persistent group stripe, state colors for playing/stopping, and density-aware HUD simplification.
- Added a compact Playout transport strip and hid redundant top-row transport buttons in Playout mode.
- Fixed the generated macOS bundle so the app opens from Finder or `open`.

## Design System Notes

- Superseded by OCC148: the sprint ceiling now includes `12x8` visible density because it fits the current 100-slot page key. `10x12` and 960 visible UI slots remain deferred because the current stable page key is `tab * 100 + button`.
- ShmUI remains a support dependency for visualizers/interpolation. The Clip Composer controls were not refactored onto ShmUI.
- The external Orpheus design-system repo is outside the writable roots for this task, so design-system guidance updates are recorded here instead of committed there.

## Verification

- Built app target: `/opt/homebrew/bin/cmake --build build --target orpheus_clip_composer_app`
- Built test target: `/opt/homebrew/bin/cmake --build build --target clip_composer_tests`
- Ran Clip Composer CTest range: `/opt/homebrew/bin/ctest --test-dir build -I 1,49 --output-on-failure`
- Result: 49/49 passed
- Launched app bundle with `open build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Release/OrpheusClipComposer.app`

## Follow-Up

- Continue the remaining secondary-dialog polish pass for Clip Edit, Audio I/O Settings, HotKey Setup, MIDI Devices, MIDI Monitor, Level Meters, Session History, About, and color swatch callouts.
- Manually smoke-test every density preset and every adaptive shell mode with a populated session.
- Move the 10x10/10x12 decision into the Orpheus design-system repo once that repo is writable in the active workspace.
