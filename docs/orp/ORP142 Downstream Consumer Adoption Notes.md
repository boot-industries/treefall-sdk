<!-- SPDX-License-Identifier: MIT -->

# ORP142 — Downstream Consumer Adoption Notes

**Document type:** Non-binding downstream guidance  
**Status:** Proposed; separate from [[ORP141 Reliability and Adoption Sprint Plan]]  
**Scope:** Suggested adoption and validation work for `~/dev/clip-composer`, `~/dev/freqfinder`, `~/dev/fourtrack`, and `~/dev/shmui`. This document creates **no SDK requirement** and does not authorize changes in those repositories.

---

## 1. Boundary

ORP141 is an SDK-only implementation plan. Its release, packaging, integrity,
routing, time, platform, telemetry, and ShmUI-JUCE package work can complete using
SDK-owned fixtures and CI alone.

These notes preserve the downstream observations that informed ORP141 without
making child applications a dependency, release blocker, or scope expansion for the
SDK program. Each app maintainer decides whether and when to schedule the work
below in that repository.

---

## 2. Suggested downstream validation after an SDK release candidate

These are optional confirmation steps, not SDK acceptance gates:

| Consumer | Suggested validation | Why it is useful |
| --- | --- | --- |
| Clip Composer | Resolve the candidate through installed `find_package` rather than its source fallback; exercise transport prepare/panic, routing access, scene recall, and UI telemetry against its existing tests. | Detects package-target or lifecycle mismatch at the flagship clip-playback integration boundary. |
| FreqFinder | Remove the FRQ033 source-forcing workaround only after the candidate resolves `Orpheus::core` and `Orpheus::audio_utils`, includes `audio_analysis.h`, and passes its existing analysis/standalone checks. | Proves the released package—not merely the SDK checkout—contains the analysis facade FreqFinder requires. |
| FourTrack | Run its existing content-level offline render test at 4096 frames, then validate writer/input/source behavior under its existing sanitizer suite. | Preserves the discovered large-block non-silence contract and validates the previously adopted ORP134 I/O primitives. |
| ShmUI | Compare the named source revision, component list, required JUCE modules, optional OpenGL flag, and design-token contract with the SDK import manifest. | Prevents source/package drift without making app UI behavior part of the SDK core. |

---

## 3. Future opt-in opportunities

### Clip Composer

- When ORP141 R1 scene completion lands, adopt the complete scene-assignment and
  stop-playback semantics rather than retaining application workarounds.
- Consume the SDK's fixed-capacity telemetry snapshots on the message thread; keep
  presentation logic and app-specific analyzers out of the SDK.
- For verified media, display the SDK-provided resolution state and retain the
  application’s existing recovery UX/persistence policy.

### FreqFinder

- After an R0 package release, make package-mode resolution the normal integration
  path and reserve a source-tree override for SDK development only.
- Continue owning plugin parameter, editor, and analyzer-view state locally. Adopt
  only the generic SDK telemetry and fingerprint contracts that directly reduce
  duplication.
- Treat a shared `AnalysisSource`/UI snapshot model as optional: contribute it only
  if its requirements are proven identical to another consumer’s model.

### FourTrack

- Keep the current SDK writer, input ring, and streaming-source adoption intact;
  validate candidate packages instead of recreating SDK I/O locally.
- Use public routing block semantics and check failures at the engine boundary; keep
  app-level bounce transaction policy in FourTrack.
- Consider the SDK one-shot voice utility only if ORP141 R5 lands. Musical grid,
  time-signature, click content, and scheduling remain FourTrack responsibilities.

### ShmUI

- Keep `~/dev/shmui` as the design-token and JUCE-source authority. Update the
  SDK's `packages/shmui-juce` import through the manifest rather than hand-copying
  unversioned source.
- Keep OpenGL optional and validate a non-OpenGL consumer path.
- Do not move visual components or application visual state into the Orpheus core.

---

## 4. Handoff rule

Any child-app change derived from these notes requires its own app-repository sprint
plan, owner, test gates, and release decision. A successful SDK release does not
imply that any consumer has adopted it; conversely, lack of adoption does not block
SDK completion under ORP141.
