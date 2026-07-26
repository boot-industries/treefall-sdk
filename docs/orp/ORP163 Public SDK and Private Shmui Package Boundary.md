# ORP163 Public SDK and Private Shmui Package Boundary

**Date:** 2026-07-26
**Status:** Implemented
**SDK:** 0.7.0
**Stable C ABI:** 1.0

## Decision

Orpheus SDK is a public, host-neutral audio SDK. Shmui is a separate private UI
system. SDK source, build targets, installed packages, release metadata, tests,
and CI must not carry Shmui implementation.

This is a clean ownership cutover:

- `Orpheus::*` names public SDK contracts only.
- `Shmui::juce` and `Shmui::juce_gl` are owned by the private Shmui repository.
- Applications pin Orpheus SDK and Shmui independently.
- Applications do not compile files from SDK `src/`.

## SDK changes

SDK 0.7.0 removes:

- `packages/shmui-juce` from current public source;
- `ORPHEUS_ENABLE_SHMUI_JUCE`;
- `Orpheus::shmui_juce` and `Orpheus::shmui_juce_gl` aliases;
- Shmui install/export rules and installed-target metadata;
- Shmui-only CMake consumers, manifest tooling, and public CI steps.

Host-neutral targets, public headers, and stable C ABI 1.0 remain unchanged.
`Orpheus::audio_driver_manager` remains a required installed target and package
fixtures exercise its public factory.

## Downstream migration

Clip Composer now:

- pins SDK 0.7.0 as its submodule;
- links `Orpheus::audio_driver_manager` instead of compiling
  `driver_manager.cpp`;
- calls `createAudioDriverManager()` directly instead of resolving a
  compiler-specific mangled symbol; and
- checks in a Shmui vendor artifact and links `Shmui::juce`.

FreqFinder now:

- requires installed SDK 0.7.0 or an equivalent explicit source override;
- checks in its own Shmui artifact rather than probing `../shmui` or the SDK;
- links `Shmui::juce`; and
- runs design-token drift tests against the pinned artifact CSS.

FourTrack already uses public SDK targets and a generated Swift token contract.
Its separate light-mode/presence worktree was not modified.

## Migration for other consumers

Replace:

```cmake
target_link_libraries(app PRIVATE Orpheus::shmui_juce)
```

with an application-owned Shmui artifact and:

```cmake
add_subdirectory(third_party/shmui-juce)
target_link_libraries(app PRIVATE Shmui::juce)
```

Use `Shmui::juce_gl` only for the optional OpenGL surface. The private artifact
is generated and verified from Shmui with `scripts/vendor-juce.py`.

## Evidence

Final evidence observed for the migration:

- SDK 0.7.0 configured and built with tests enabled; all 156 CTest contracts
  passed, including `docs_path_audit`, `cmake_find_package`, package-runtime,
  CoreAudio, and version-rejection coverage;
- a clean Release SDK install exported only the public target manifest, and its
  `include`, `lib`, and `share` payload contained no Shmui references;
- Clip Composer configured and built against public SDK 0.7.0 plus its pinned
  private artifact; 549 contracts passed and the host-dependent CPU gate was
  intentionally skipped;
- FreqFinder configured, built, and passed its design-token contracts both from
  the public SDK source override and from the clean Release install prefix; and
- Shmui passed canonical lint, typecheck, 167-test, and production-build
  commands, plus the native non-OpenGL fixture and both downstream artifact
  drift checks.

The completion report records the full canonical command matrix and final
package/archive inspection.

## Public-history limitation

The public Git history contains earlier Shmui snapshots. This change removes
Shmui from current source and future packages; it does not rewrite history.
Repository-history rewriting is a separate legal/security operation and was not
authorized by this delivery.
