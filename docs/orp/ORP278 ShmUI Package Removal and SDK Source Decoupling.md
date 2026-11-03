<!-- SPDX-License-Identifier: MIT -->

# ORP278 — ShmUI Package Removal and SDK Source Decoupling

**Date:** 2026-11-02  
**Owner:** Treefall SDK Core Team  
**Status:** Complete  
**Related records:** SHM028, SHM029 Gates 1–6, SHM030, ORP276, ORP277  

## Context and Executive Summary

In accordance with SHM029 Gate 4 and the architectural transition initiated in ORP276, the Treefall SDK (`orpheus-sdk`) has cut over to a clean, public-only SDK with zero dependencies on JUCE, ShmUI, or private credentials.

Downstream consumers (`freqfinder`, `fourtrack`, `clip-composer`) have completed or adopted direct integration with `boot-industries/shmui`:
- **FreqFinder**: Direct source integration via `SHMUI_JUCE_SOURCE_DIR` resolving `shmui/juce`.
- **FourTrack**: Direct consumption of generated Swift design token artifacts and provenance from ShmUI.
- **Clip Composer**: Direct consumption of `boot-industries/shmui` via application submodule at `third_party/shmui` (SHM030).

With no downstream consumers depending on the mirrored `packages/shmui-juce` package in the Treefall SDK, all mirrored source files, CMake options, installation exports, target facades, manifest verification tooling, and obsolete test fixtures have been removed.

## SHM029 Gate Satisfaction

| Gate | Requirement | Status | Evidence |
|---|---|---|---|
| **Gate 1** | Downstream consumer migration verification | **PASS** | FreqFinder and FourTrack consume ShmUI directly; Clip Composer adopts direct `third_party/shmui` submodule integration. No downstream repo requires SDK-mirrored ShmUI. |
| **Gate 2** | SDK root and package decoupling | **PASS** | `packages/shmui-juce` (77 files) removed; `ORPHEUS_ENABLE_SHMUI_JUCE` option deleted from root `CMakeLists.txt`; all installation/export rules decoupled. |
| **Gate 3** | CMake configuration and target facade updates | **PASS** | `cmake/OrpheusSDKConfig.cmake.in` cleaned (CURL dependency and ShmUI target aliases removed); `cmake/TreefallCompatibility.cmake` updated; `cmake/installed-targets.json.in` optional list set to `[]`. |
| **Gate 4** | Test suite and installed manifest decoupling | **PASS** | `tests/cmake/find_package/installed_manifest.cmake` decoupled; JUCE FetchContent removed from test fixture; `tools/shmui_juce_manifest.py` and all `tests/cmake/shmui_*` fixtures removed. |
| **Gate 5** | Suite coordination manifest alignment | **PASS** | `suite/orpheus-suite.json` updated: removed generated artifact `sdk-shmui-juce-package`, removed verification gate `sdk-shmui-manifest`, removed mirror dependency edge, added direct `shmui` -> `clip-composer` dependency edge. `tools/suite.py validate` and `tests/suite_test.py` pass cleanly. |
| **Gate 6** | Documentation and contract boundaries | **PASS** | `AGENTS.md`, `README.md`, `ARCHITECTURE.md`, `PROGRESS.md`, and `ORP278` updated to reflect `occ-app-platform` as the sole in-repo package and establish strict private source boundary policy. |

## Exact Removals

### 1. CMake Options and Export Targets
- **Option removed:** `ORPHEUS_ENABLE_SHMUI_JUCE` ("Build + export the shmui-juce UI component package (requires JUCE)").
- **Native targets removed:** `orpheus_shmui_juce`, `orpheus_shmui_juce_gl`.
- **Target aliases removed:**
  - `Orpheus::shmui_juce`
  - `Orpheus::shmui_juce_gl`
  - `Treefall::shmui_juce`
  - `Treefall::shmui_juce_gl`
- **Installation export rule:** Unconditional `export(EXPORT OrpheusSDKTargets)` restored in root `CMakeLists.txt`.

### 2. File and Directory Deletions
- `packages/shmui-juce/` (77 files removed, including `shmui-juce-import.json`, `ShmUI.h`, `CMakeLists.txt`, `Icons/`, `Audio/`, `Components/`, `Controls/`, `Utils/`, `tests/`, `Shaders/`).
- `tools/shmui_juce_manifest.py`.
- `tests/cmake/run_shmui_package.cmake`.
- `tests/cmake/run_shmui_no_opengl.cmake`.
- `tests/cmake/shmui_package_consumer/` (`CMakeLists.txt`, `main.cpp`).
- `tests/cmake/shmui_package_producer/` (`CMakeLists.txt`).
- `tests/cmake/shmui_no_opengl/` (`CMakeLists.txt`, `main.cpp`).

### 3. In-Tree Applications & CI
- `apps/wave-finder/CMakeLists.txt`: Removed `SHMUI_JUCE_PATH` subdirectory addition and `orpheus_shmui_juce` linkage.
- `.github/workflows/ci-pipeline.yml`: Removed the `Build ShmUI-JUCE consumer without OpenGL` CI step.

## Verification and Invariants

1. **Clean Public-Only Build**:
   The SDK builds cleanly with zero references to JUCE or ShmUI. Only `packages/occ-app-platform` remains as an in-repo application package.
2. **Relocated Package Consumer**:
   `cmake_find_package` installs the SDK to a relocated clean prefix and exercises discovery across all four discovery modes (`legacy`, `treefall`, `legacy-first`, `treefall-first`). The installed manifest declares empty `optional` targets and verifies all required and platform-conditional targets without JUCE.
3. **No Fallback or Compatibility Aliases**:
   In accordance with the clean cutover rule, no silent fallback to the removed mirror, compatibility aliases with missing implementations, or dangling header install rules remain in the SDK.
4. **Suite Coordination**:
   `python3 tools/suite.py validate --json` confirms the suite coordination graph is valid. Downstream impact calculation (`python3 tools/suite.py affected shmui --json`) proves that changes in `shmui` no longer transitively impact `orpheus-sdk`.

## References

- [1] SHM028: ShmUI Private Repository Source Boundary and Direct Consumer Integration.
- [2] SHM029: Treefall SDK Decoupling and Mirror Retirement Specification (Gates 1–6).
- [3] SHM030: Clip Composer Direct ShmUI Submodule Integration.
- [4] ORP276: ShmUI Private Source Compartmentalization Handoff.
- [5] ORP277: Treefall SDK Coherence Audit, Downstream Suite Sync, and Strategic Roadmap.
