# OCC148 Clip Composer UI Refresh Corrective Pass

**Date:** 2026-05-15
**Branch:** `codex/clip-composer-ui-refresh`
**Build version:** `0.2.2-alpha`

## Context

The first UI refresh pass compiled and worked functionally, but it did not meet the supplied UI kit mockups. The main misses were structural: Playout still felt like a raw grid prototype, authoring modes had no full-chassis inspector, and the high-density study's live `12x8` target was not available even though it fits the 100-slot page model.

## Corrective Scope

- Made Playout match the live chassis: 36px top strip, 52px bottom strip, no inspector, grid-first body.
- Added a 420px Console inspector for Edit, Routing, and Preferences to restore the full-chassis layout.
- Added `DisplayPreferences::GridLayout::Columns12Rows8` with persisted key `12x8`; this gives 96 visible cells and keeps `tab * 100 + button` stable.
- Kept `10x12` deferred because it would require 120 visible cells and a broader session/indexing migration.
- Reworked clip button rendering toward the UI kit: recessed empty wells, muted ordinals, neutral loaded caps with persistent group stripe, and progressively simplified metadata.
- Updated Console palette values to match the design-system petrol/cream/amber vocabulary more closely.

## Verification

- Built app and tests:
  `/opt/homebrew/bin/cmake --build build --target orpheus_clip_composer_app clip_composer_tests`
- Ran tests:
  `/opt/homebrew/bin/ctest --test-dir build/apps/clip-composer/tests --output-on-failure`
- Result: 51/51 CTest cases passed, with hardware/audio-dependent cases skipped by the existing test guards.
- Launched app bundle:
  `open -n build/apps/clip-composer/orpheus_clip_composer_app_artefacts/Release/OrpheusClipComposer.app`

## Visual QA Note

Desktop capture was attempted with `screencapture -x /private/tmp/clip-composer-refresh-smoke.png`, but the environment returned `could not create image from display`. The app launch path was exercised, but screenshot evidence still needs to be captured from an interactive desktop session.

## Remaining Follow-Up

- Deep polish pass on the internals of Clip Edit and other secondary dialogs where the shared LookAndFeel does not fully control custom layout.
- Manual visual review of Playout `8x6`, `6x8`, `10x10`, and `12x8` with a populated session.
- Update the external Orpheus design-system repo with the `12x8`/100-slot decision once that repo is writable.
