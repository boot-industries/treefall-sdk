<!-- SPDX-License-Identifier: MIT -->

# ORP276 — ShmUI Private Source Compartmentalization Handoff

**Date:** 2026-10-23  
**Owner:** ShmUI team, with a coordinated SDK/consumer cutover  
**Status:** Handoff only; no ShmUI or consumer repository changed  
**Related review:** SDK PR #277 / ORP273 remediation

## Decision and current evidence

Keep this optimization review SDK-only. Do not hand-edit the governed import,
remove a package that current consumers require, or change downstream SDK pins.
The ownership migration needs a separate coordinated change; PR #277 neither
implements nor claims to complete it.

The existing SDK boundary is a **build boundary, not a source-confidentiality
boundary**:

- `CMakeLists.txt` defaults `ORPHEUS_ENABLE_SHMUI_JUCE` to `OFF`; core builds do
  not require JUCE. Enabling it adds `packages/shmui-juce` when JUCE is available.
- `packages/shmui-juce/CMakeLists.txt` declares the imported controls, icons,
  audio and component sources. Its header identifies ShmUI as source authority
  and `scripts/sync-juce.sh` as the upstream mirror mechanism.
- The SDK repository contains those source files regardless of the option.
  The option therefore does not keep their contents out of a source checkout.
- When explicitly enabled, SDK installation exports `Orpheus::shmui_juce` and
  installs matching headers beneath `include/orpheus/shmui-juce`.
- `tests/cmake/find_package/installed_manifest.cmake` and the dedicated
  `cmake_shmui_package_consumer` fixture currently govern those optional exports.
- `suite/orpheus-suite.json` records the SDK import as a generated artifact;
  `tools/shmui_juce_manifest.py` verifies the present mirror, not privacy.

The optimization PR changes none of these import/package surfaces. Its meter
refactor remains host-neutral SDK telemetry; it does not move UI presentation,
app workflows or app assets into the SDK.

## Required ShmUI-team work

1. Establish the approved private distribution boundary in `shmui/`: source
   package or private build/install package, version/provenance metadata, license
   inventory and authorized consumer access. Preserve the current public C++
   target/component/token contract where it is still needed by consumers.
2. Change the upstream synchronization workflow so it no longer copies private
   implementation into a public SDK checkout. A manifest containing hashes is
   not a substitute for withholding the private source bytes.
3. Provide a supported consumer integration contract: explicit ShmUI source or
   package location, JUCE ownership, non-OpenGL default, generated Swift/JUCE
   token artifact provenance, and installation/linking instructions. Do not make
   SDK public package discovery require access to a private repository.
4. Deliver the complete list of downstream import paths and generated artifacts
   that must migrate. Clip Composer's SDK-import integration, FreqFinder's
   separate private source integration, and FourTrack's generated Swift
   provenance are distinct contracts; do not assume one global path rewrite.
5. Supply reproducible private-package fixtures that build/link/run outside the
   SDK checkout, including non-OpenGL UI components and generated-token checks.
   Run actual affected UI acceptance in the consumer-owned repositories.

## Coordinated SDK follow-through (separate task)

After the ShmUI integration is available, migrate callers before removing the
SDK copy. Update SDK CMake options/exports, installed-target manifests, optional
ShmUI fixtures, suite artifact/provenance records, demo integration, README and
package documentation together. Keep installed SDK-only core/audio/routing/
transport/diagnostics consumer fixtures runnable without ShmUI, JUCE, adjacent
repositories, private credentials or private package discovery.

Use a clean cutover: do not leave a silent fallback to the old public mirror,
compatibility aliases for removed private implementation, or an advertised
optional export with missing headers. Consumer repository changes and pins
remain owned by their respective teams, not this SDK review branch.

## Public-distribution acceptance

- Inventory the exact proposed public source/package/release contents, not just
  default build outputs. Exclude private implementation, app assets, research,
  credentials and app-specific workflow policy from the intended public payload.
- Review already-published repository history, releases and license notices with
  the repository owner before claiming confidentiality or changing licensing.
  Deleting files in a new commit does not remove copies from existing history.
  This handoff makes no legal conclusion about prior distributions.
- Verify a clean public-only SDK checkout and installed-prefix consumer can
  configure, compile, link and render without private access.
- Verify private consumers against the chosen SDK and ShmUI pair, with recorded
  source revisions/content hashes and explicit rebuild/qualification evidence.
- Do not report PR #277's SDK optimization merge as approval to publish private
  ShmUI source, or as completion of the compartmentalization migration.

## References

[1] `CMakeLists.txt`, SDK ShmUI option, add-subdirectory and install/export guards.  
[2] `packages/shmui-juce/CMakeLists.txt`, governed mirror ownership and targets.  
[3] `tests/cmake/find_package/installed_manifest.cmake` and
`tests/cmake/shmui_package_consumer/`, installed optional-package contract.  
[4] ORP272, governed import and suite-provenance boundary.  
[5] ORP254, open SDK / internal design system / proprietary app ownership strategy.
