<!-- SPDX-License-Identifier: MIT -->

# ORP271 — ShmUI Console Icon Import and Attribution Gate

**Document type:** Package import and installed-package qualification record  
**Date:** 2026-10-20  
**Branch:** `feat/shmui-console-icons-import`  
**Status:** Imported on the working branch; not merged, not tagged, not pushed

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
`c7cc4ae2149545d62d0727b98cb1feaf8501f20f` in
`packages/shmui-juce/shmui-juce-import.json`, with content hash
`d5039c891da9e4b8228ebc7adb962c365fd7d761d73fe9435b77c7ff2a0ba202` and the
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
  licence texts, `NOTICE.md` and `provenance.json`.

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

The import lives on the local branch `feat/shmui-console-icons-import`
(`6b2477e2e` import, `193f4685f` consumer gate and README). Neither commit is
published to a remote: remote publication needs authorisation, and the
ShmUI source revision it mirrors is likewise local-only on
`feature/shm-027-console-icons`.
