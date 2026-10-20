<!-- SPDX-License-Identifier: MIT -->

# ORP271 — ShmUI Console Icon Import and Attribution Gate

**Document type:** Package import and installed-package qualification record  
**Date:** 2026-10-20  
**Branch:** `feat/shmui-console-icons-import` → merged to `main` by PR #272 (merge commit `54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21`)  
**Status:** Merged to `main`; not tagged, and no SDK release was cut for this import

## What was imported

The Treefall SDK package `packages/shmui-juce` now carries the SHM027 console
icon set from its source authority, the ShmUI repository. The import was
produced by the governed mirror, not by hand:

```bash
cd /Users/chrislyons/dev/shmui
SDK_ROOT=/Users/chrislyons/dev/orpheus-sdk scripts/sync-juce.sh
SDK_ROOT=/Users/chrislyons/dev/orpheus-sdk scripts/sync-juce.sh --check
```

The mirror recorded the governed source revision
`d420ec64dcb5fd16baa0b95d5048ee09bb95eee8` in
`packages/shmui-juce/shmui-juce-import.json`, with content hash
`4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7` and the
unchanged design-token contract version `0.6.0`. The token contract is
untouched: sourcing icons does not change tokens.

The imported change set:

* `packages/shmui-juce/Icons/ConsoleIconGeometry.{h,cpp}` — the generated
  geometry for the thirteen console, transport and hotkey-legend roles.
* `packages/shmui-juce/Icons/Icons.{h,cpp}` — the new `IconType` members
  (`StopPeers`, `FadeIn`, `FadeOut`, `Shift`, appended before `NumIcons` so
  every existing enumerator keeps its value), the per-icon render contract
  (`IconRenderMode`, `IconRenderSpec`, `getIconRenderSpec()`,
  `getConsoleStrokeWidth()`) and the removal of the hand-drawn transport
  paths they replace.
* `packages/shmui-juce/Controls/ClipButton.cpp` — the clip button's built-in
  fade indicators now use the shared `IconType::FadeIn` / `IconType::FadeOut`
  roles instead of the volume placeholders.
* `packages/shmui-juce/tests/SHM027ConsoleIconsFixture.cpp` and
  `SHM027ConsoleIconsReference.h` — the native fixture and the upstream ink
  reference it compares against.
* `packages/shmui-juce/Icons/ThirdParty/` — the vendored upstream artwork, the
  licence texts, `NOTICE.md` and `provenance.json`. The provenance record
  carries the operator ratification of 2026-10-20 that the shift glyph follows
  OCC217 (the classic up-caret with a bar, drawn as an outline) rather than
  OCC218's Iconoir arrow-up, which has no bar and cannot satisfy OCC217 D7.

## Installed attribution

The package CMake file — governed by ShmUI and mirrored here — now installs
the attribution material with the package:

```cmake
install(DIRECTORY "${SHMUI_SRC}/Icons/ThirdParty/"
  DESTINATION share/shmui-juce/ThirdParty
  FILES_MATCHING
    PATTERN "*.md"
    PATTERN "*.json"
    PATTERN "*.svg"
    PATTERN "LICENSE")
```

A verified install prefix contains:

```
share/shmui-juce/ThirdParty/NOTICE.md
share/shmui-juce/ThirdParty/provenance.json
share/shmui-juce/ThirdParty/bootstrap-icons/LICENSE
share/shmui-juce/ThirdParty/bootstrap-icons/*.svg
share/shmui-juce/ThirdParty/tabler-icons/LICENSE
share/shmui-juce/ThirdParty/tabler-icons/repeat.svg
share/shmui-juce/ThirdParty/wikimedia-commons/Bhutan stop sign.svg
```

`provenance.json` carries one record per role with the upstream file, the
immutable revision, the licence and its obligations, the local SHA-256, the
geometry, the normalisation transform, the presentation contract and the
selection decision. The upstream SVG files install alongside it so the
recorded digests can be verified offline.

## Installed-package qualification

`tests/cmake/shmui_package_consumer` now exercises the delivered API against
the installed package rather than against a source tree:

* every console role resolves to non-empty geometry through
  `shmui::Icons::getIcon()`;
* the open-geometry marks report `IconRenderMode::Stroked` and the console
  stroke policy (1.10 px at 11 px, 1.25 px at 14 px, 1.50 px at 16 px), while
  the filled transport marks keep `IconRenderMode::Filled`;
* the loop mark renders with an open interior, so the shipped mark is stroked
  rather than filled;
* the installed prefix carries the NOTICE, the provenance record, both MIT
  licence texts and the upstream artwork.

Gate command and result:

```bash
cmake -Dbuild_type=Debug \
  -Dsdk_source_dir=/Users/chrislyons/dev/orpheus-sdk \
  -Dproducer_source_dir=$PWD/tests/cmake/shmui_package_producer \
  -Dproducer_binary_dir=/tmp/shm027-sdk/producer_build \
  -Dconsumer_source_dir=$PWD/tests/cmake/shmui_package_consumer \
  -Dconsumer_binary_dir=/tmp/shm027-sdk/consumer_build \
  -Dinstall_prefix=/tmp/shm027-sdk/prefix \
  -Ddependency_dir=/tmp/shm027-sdk/deps \
  -Drequired_version=0.9.1 \
  -P $PWD/tests/cmake/run_shmui_package.cmake
# -> orpheus_shmui_package_consumer: 1/1 passed
```

The consumer derives the install prefix from `OrpheusSDK_DIR`, so the check
follows the installed layout rather than a hard-coded path.

## Boundary and posture

The OpenGL posture is unchanged: `orpheus_shmui_juce_gl` is still optional and
still off by default, and the core target still does not link `juce_opengl`.
The SDK version stays 0.9.1, and no ABI table or public C header changed. The
change is additive for consumers: the new enumerators are appended before
`NumIcons`, and the existing `drawIcon()` signature keeps its meaning for
every icon that was already filled.

Downstream consumers that draw the loop or the stop-peers badge now receive
stroked artwork instead of filled artwork. That is the intended correction -
the sourced marks are open geometry - and it is visible only for those two
roles.

## Revision availability

The import is published. `main` in `boot-industries/treefall-sdk` carries the
import through merge commit `54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21` (PR
#272), which contains `6b2477e2e` (import), `193f4685f` (consumer gate and
README), `85e00e488` (import record), `67ce4654f`, `2133647f9` and
`c0ba8d38` (import refreshes). A point-in-time snapshot of the delivered
branch is kept in `safety/treefall-sdk-20261020`.

The ShmUI source revision it mirrors, `d420ec64dcb5fd16baa0b95d5048ee09bb95eee8`,
is published in `boot-industries/shmui` on `main` through merge commit
`3ebfe5d7135657971c8e8975faa5095b03f3f031` (PR #34); the snapshot of that
branch is `safety/shmui-20261020`. The import can be re-derived from ShmUI
`main` with `scripts/sync-juce.sh`.

## CI note — import run (PR #272)

The `macos-latest` leg of the import run failed in the CoreAudio driver
suite: `CoreAudioDriverTest.CallbackIsNotInvokedAfterStop` on the first
attempt and `CoreAudioDriverTest.PublishesCallbackActiveClipCount` on the
re-run — a different case each time. The same suite fails on `main`'s own
2026-10-19 merge commit (`CoreAudioDriverTest.StereoConfiguration`), so the
failure is a pre-existing macOS runner flake and not a consequence of this
import. Every other leg is green: Ubuntu Debug and Release, Windows Debug
and Release, C++ lint, the Linux TSan ingress evidence and the Linux consumer
syscall evidence. The package consumer gate, which is the leg that covers
this import, passed on macOS in both attempts.

That evidence belongs to the import run on PR #272. It says nothing about the
documentation closeout below, which is a separate pipeline run on its own
head.

## Documentation closeout (PR #273)

The import was already published through PR #272. Its closeout is
documentation only and carries three changes:

* the published-revision corrections in this record;
* a scoped `Unreleased` entry in `CHANGELOG.md` for the published import; and
* the corresponding entry in `.claude/implementation_progress.md`.

The closeout changes no published artifact. The implementation revision stays
`54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21`, the governed manifest
`packages/shmui-juce/shmui-juce-import.json` is not restamped, the attribution
install rule in `packages/shmui-juce/CMakeLists.txt` is untouched, the
`share/shmui-juce/ThirdParty/` destination is unchanged, and the design-token
contract stays `0.6.0`. No build, packaging or rendering qualification is
re-run for it; the repository documentation path audit is the only gate the
closeout adds.

The closeout merged to `main` as merge commit
`dabe8c03a0d58e74418d304dcf9af8a5e615868f` (PR #273), whose second parent is
the reviewed closeout head
`48e43e33e8c19f0906f6b2419d0457ade4efe5db`. All ten pipeline legs on that
head passed, including `C++ Build & Test (windows-2022, Debug)`, so the merged
head is the CI-qualified head. That run is separate from the import run
recorded above, which belongs to PR #272.

The merge itself was performed by the `chrislyons` account at 09:38:26Z, in
the same minute the closeout head was pushed and before the pipeline legs for
that head had completed. `main` carries no branch protection, and the
`Orpheus SDK Rules` ruleset blocks only branch deletion and force pushes, so
no CI requirement gated the merge; the legs that ran afterwards on the merged
head all passed. The closeout carried documentation only, so no package,
manifest or rendering artifact was published without qualification.

## Outstanding operator ratification

One operator decision is still open, and it is not an SDK implementation item.
SHM027 §6 records that the transport family moved from OCC218's Iconoir glyphs
to Bootstrap Icons, and that substitution still awaits explicit operator
ratification. The Shift decision (Bootstrap classic outline) and the mirrored
Fade Out decision in the same section are closed and are not reopened here.
The decision list lives in the ShmUI record
`docs/shm/SHM027 Console Icons and SDK Import Handoff.md` §6.
