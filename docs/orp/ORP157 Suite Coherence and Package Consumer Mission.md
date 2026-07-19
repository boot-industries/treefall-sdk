<!-- SPDX-License-Identifier: MIT -->

# ORP157 — Suite Coherence and Package Consumer Mission

**Status:** Proposed handoff
**Date:** 2026-07-19
**Owner:** Orpheus SDK
**Purpose:** Establish a trustworthy, installed-package path for the shared SDK and its optional Shmui-JUCE component without importing child-application policy into the SDK.

## Starting state

- The reviewed SDK mainline was clean at `99dad08a`; its checkout described itself as `v0.6.1-10-g99dad08a`.
- Remote `origin` publishes the immutable `v0.6.2` tag. Clip Composer is already pinned to its `v0.6.2` submodule revision.
- ORP154/ORP155 delivered the trigger-voice, directional CoreAudio endpoint, isolated routing-meter, and capture-telemetry contracts required by FourTrack. ORP156 records 151/151 CTest evidence at merge time.
- The SDK can export `Orpheus::shmui_juce` when configured with JUCE and `ORPHEUS_ENABLE_SHMUI_JUCE`; FreqFinder still has a documented source-tree fallback when that installed optional target is unavailable.

## Mission

Make the package boundary demonstrably coherent for real downstream consumers. This is a packaging and contract-validation mission, not an application-feature sprint.

1. Refresh refs and establish the exact relationship among `main`, `v0.6.2`, and the checkout before changing versions, pins, or claims. Do not infer a tag relationship from a different worktree.
2. Audit the configured release/package path for `Orpheus::shmui_juce`:
   - configure it with the intended JUCE dependency and `ORPHEUS_ENABLE_SHMUI_JUCE=ON`;
   - install to a clean prefix; and
   - prove that an external CMake consumer can resolve the exported target without source-tree fallback.
3. Preserve the existing component boundary: the non-OpenGL target remains the default shared path; `Orpheus::shmui_juce_gl` remains opt-in. Do not enable OpenGL merely to make a consumer configure.
4. Use a focused consumer fixture that represents FreqFinder's actual needs: `Orpheus::core`, `Orpheus::audio_utils`, and non-OpenGL `Orpheus::shmui_juce`. The fixture must compile/link/run through the installed package only.
5. If the optional package cannot be delivered with its JUCE dependency, report the exact supported distribution contract and leave FreqFinder's explicit source override intact. Do not add an implicit sibling-source fallback or hand-copy Shmui sources into an app.
6. Record the exact tag/commit, CMake options, installed targets, consumer command/output, and any platform limits in a follow-up ORP implementation record. Reconcile version/support/package claims only after evidence exists.

## Non-goals

- No app UI, SwiftUI, JUCE view, playlist, analyzer, or FourTrack workflow logic in the SDK.
- No new alias, compatibility shim, or runtime CSS/token parsing.
- No promotion of WASAPI support without hosted Windows package/ABI and hardware evidence.

## Required verification

Run the focused installed-package consumer plus:

```sh
ctest --test-dir <build> --output-on-failure -R '^(cmake_find_package|realtime_static_audit|docs_path_audit)$'
python3 tools/shmui_juce_manifest.py --check
```

For a change to public/package behavior, also run the full configured SDK suite. Verify exported-symbol callers before changing a public header.

## Copy/paste agent prompt

> Resume Orpheus SDK from ORP157. First establish the immutable `main`/`v0.6.2` relationship in this worktree. Then prove or repair the documented installed-package route for non-OpenGL `Orpheus::shmui_juce` using a clean-prefix external consumer representing FreqFinder. Preserve the SDK/app ownership boundary and all realtime invariants. Do not alter downstream pins or add app-specific policy. Run the focused package, realtime, manifest, and documentation gates; record only observed package support and platform evidence in the next ORP document.
