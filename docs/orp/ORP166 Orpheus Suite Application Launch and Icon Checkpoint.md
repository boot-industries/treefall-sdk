# ORP166 Orpheus Suite Application Launch and Icon Checkpoint

**Status:** Checkpoint complete
**Date:** 2026-07-28
**Scope:** Cloudkicker Orpheus SDK, Shmui, Clip Composer, FreqFinder, and FourTrack checkouts

## Purpose

Record the synchronized, runnable application-suite baseline after local cleanup,
application-icon integration, launcher recovery, and native launch verification.
This is the rollback/reference point for the next application work.

## Repository Baseline

All repositories are on `main`, have no staged, unstaged, or untracked Git
changes, and have zero divergence from `origin/main`.

| Repository | `main` commit | Current state |
|---|---:|---|
| Orpheus SDK | `2819ca0e` | Clean; aligned with `origin/main`; library only. |
| Shmui | `b7d7e12` | Clean; aligned with `origin/main`; library only. |
| Clip Composer | `e0e6689` | Clean; aligned with `origin/main`; icon PR #23 merged. |
| FreqFinder | `ebfcd03` | Clean; aligned with `origin/main`; icon PR #23 merged. |
| FourTrack | `909589c` | Clean; aligned with `origin/main`; icon PR #37 and the launcher commits are merged. |

The initial cleanup deliberately discarded stale local edits, untracked build
artifacts, a deleted FourTrack feature branch, and rejected launcher changes.
Clip Composer and FourTrack submodules were restored to their superproject pins.

## Application Entry Points

Libraries do not have launch scripts:

- Orpheus SDK
- Shmui

Application launch paths are now explicit:

| Application | Existing-bundle launch | Build then launch | Notes |
|---|---|---|---|
| Clip Composer | `./relaunch.sh` | `./build-launch.sh` | `./clean-relaunch.sh` remains the diagnostic path for stale-bundle conditions. |
| FreqFinder | `./relaunch.sh [debug|release]` | Configure/build `FreqFinder_Standalone`, then relaunch | Debug bundle lives below `build/`. |
| FourTrack | `./relaunch.sh [app-args...]` | `./build-launch.sh [app-args...]` | Uses `build-fourtrack-app` and Ninja because the Unix Makefiles generator cannot enable Swift. |

`relaunch.sh` is launch-only: it stops a previous process, starts an already
built app bundle, and verifies process startup. FourTrack's `build-launch.sh`
configures the opt-in macOS shell, disables unrelated tests, builds target
`FourTrack`, then delegates to `relaunch.sh`.

## Icon Integration

Merged application icon pull requests:

- Clip Composer PR #23: source PNG/SVG and JUCE `ICON_BIG` integration. The
  CMake path uses the canonical lowercase `Resources/icons/` directory.
- FreqFinder PR #23: current PNG/SVG artwork through JUCE `ICON_BIG` and
  `ICON_SMALL`; removed duplicate manual `.icns` packaging.
- FourTrack PR #37: PNG/SVG/ICNS resources, `CFBundleIconFile`, and bundle
  resource staging.

The FourTrack PR also removed internal `FTR###` document references from the
operator-visible Settings footnotes. Internal source comments remain unchanged.

## Verification Evidence

- Clip Composer: configured and built
  `orpheus_clip_composer_app`; the macOS bundle build copied `Icon.icns`.
  User confirmed the latest app passes visual review.
- FreqFinder: configured against the local Orpheus SDK and Shmui JUCE source,
  built `FreqFinder_Standalone`, copied `Icon.icns`, and launched the resulting
  Debug standalone bundle.
- FourTrack: `build-launch.sh` configured a fresh Ninja/Swift Debug build,
  copied `FourTrackIcon.icns`, built and ad-hoc-signed `FourTrack.app`, then
  launched it.
- Process verification observed both `Freqfinder` and `FourTrack` running after
  the latest builds launched.

Clip Composer's GitHub CI test job exceeded its repository-owned five-minute
step timeout at test 199. The icon change was locally configured and built
successfully; the user explicitly directed its merge without retrying or
starting additional remote CI jobs.

## Current Operating Rules

1. Keep routine UI inspection to `relaunch.sh`; it must not reconfigure or
   mutate a working bundle.
2. Use FourTrack `build-launch.sh` only when no valid app bundle exists or its
   macOS CMake configuration must be refreshed.
3. Keep the application repositories on `main` and fast-forward from
   `origin/main`; do not recreate local launcher variants.
4. Treat application icons as bundle resources: verify the generated `.app`
   contains its declared ICNS file after icon changes.
5. Do not surface internal ORP/FTR planning identifiers in operator-facing UI.
