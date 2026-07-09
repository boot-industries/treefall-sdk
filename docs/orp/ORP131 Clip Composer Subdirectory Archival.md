# ORP131 Clip Composer Subdirectory Archival

**Status:** Complete — landed on `main`
**Author:** Session work, 2026-07-09
**Scope:** Repo structure — `apps/clip-composer/` removal, CI, CMake, docs
**Severity:** Medium — build/CI config referenced a directory being removed
**Related:** OCC migration to standalone repo (`chrislyons/clip-composer`), ORP126 (Codex integration audit/checkpoint)

---

## Summary

The Orpheus Clip Composer application, which previously lived in
`orpheus-sdk/apps/clip-composer/`, was migrated to its **own standalone
repository** (`github.com/chrislyons/clip-composer`) that consumes this SDK as a
git submodule. This document records the **archival of the original
subdirectory** from the SDK: nothing was deleted, all content was preserved and
byte-verified, and the SDK's build/CI/docs were updated to remove the now-dead
references.

## Migration verification (before touching anything)

The new repo was confirmed healthy and complete before any change to the SDK:

- **New repo:** `~/dev/clip-composer`, branch `main` at `f4a463c`, fully pushed
  to `origin` (`git@github.com:chrislyons/clip-composer.git`). Local HEAD ==
  `origin/main`.
- **SDK submodule:** `third_party/orpheus-sdk` pinned to `v0.3.0`
  (`ef43bec`).
- **Source parity:** every git-tracked file under the old subdir was present in
  the new repo. The only source differences were **intentional include-path
  rewrites** adapting subdir-relative includes to submodule-relative ones:
  - `AudioEngine.cpp`: `../../../src/core/transport/transport_controller.h`
    → `../../third_party/orpheus-sdk/src/core/transport/transport_controller.h`
  - `ClipButton.h`: `../../../packages/shmui-juce/Utils/Interpolation.h`
    → `../../third_party/orpheus-sdk/packages/shmui-juce/Utils/Interpolation.h`

## Assets preserved (were git-ignored, so not carried by the migration)

The SDK root `.gitignore` ignores `*.png`, `*.jpg`, `*.pdf` (binary blobs kept
out of the lean C++ SDK), so several working-tree-only assets existed **only** in
the old subdir and were **not** git-tracked. These were copied into the new repo
before archival and byte-verified (md5-identical):

- `docs/design/LOOKBOOK/` — 15 design reference images (Winamp / ProTools /
  SpotOn / Sony gadgets), 3.3 MB.
- `docs/occ/peers/SpotOn Button Menus and Audio Setup.pdf` (1.69 MB) and
  `docs/occ/peers/SpotOn Manual - 00 - Main Menus.pdf` (5.5 MB).
- 6 `tmp/*.png` screenshots and `.claude/settings.local.json`.

Per decision, the new repo **inherited the SDK's `.gitignore`** (build
artifacts, `.DS_Store`, binary blobs, `*.png` / `*.jpg` / `*.pdf`), so these
design/reference assets remain present-on-disk-but-untracked in the new repo —
matching their prior status in the SDK.

Additionally, 7 archive OCC docs present only in the old subdir
(`archive/CLAUDE.md`, `OCC139`–`OCC144`) were copied into the new repo and
committed there.

## Archive

- **Destination:** `~/archived-repos/clip-composer-sdk-subdir/` (workspace
  convention; inventory in `~/dev/docs/archived-repos.md`).
- **Contents:** source + docs + assets, **excluding** the ~2.9 GB of regenerable
  build directories (`build/`, `build-check/`, `build-codex/`). Archive size:
  ~18 MB, 347 files.
- **Verification:** file-list parity and an aggregate md5 checksum of all
  non-build files matched byte-for-byte between the old subdir and the archive
  (`a4ebd144…`) before any removal. A final per-file check confirmed **0 files
  remained in the subdir that were not already in the archive**.
- The only file intentionally **not** carried into the new repo was
  `depr.launch.sh` (a deprecated launch script superseded by
  `build-launch.sh`); it is preserved in the archive.

## SDK changes

Pre-archival SDK state: `main` at `b7d9ccdd`, 311 git-tracked files under
`apps/clip-composer/`.

- **Removed** `apps/clip-composer/` — `git rm -r` (311 tracked files) plus
  physical removal of the untracked build dirs and archived assets.
- **`CMakeLists.txt`** — removed the `ORPHEUS_ENABLE_APP_CLIP_COMPOSER` option
  and its `add_subdirectory(apps/clip-composer)` block.
- **`.github/workflows/ci-pipeline.yml`** — removed the `clip-composer-tests`
  job (build + ctest + smoke test + log upload) and dropped it from the
  `ci-status` job's `needs` list and success check. Its CI now lives in the
  Clip Composer repo. YAML re-validated.
- **`.claudeignore`** — removed the dead
  `apps/clip-composer/docs/occ/peers/` ignore rule.
- **`CLAUDE.md`** — updated Quick Commands (removed broken `relaunch-occ.sh`
  call), rewrote the "OCC — Clip Composer" section to point at the standalone
  repo, and updated the multi-instance table
  (`~/dev/orpheus-sdk/apps/clip-composer` → `~/dev/clip-composer`).

## Deferred / follow-up

**✅ Done (2026-07-09):** The orphaned OCC-specific `.claude/` tooling has been
cleaned up. Each artifact was evaluated for whether the app still needs it, then
either **migrated** (path-adapted) to the Clip Composer repo or **deleted** from
the SDK:

- **Migrated** to `~/dev/clip-composer/.claude/`, with all hardcoded build paths
  rewritten for the standalone layout (`build/orpheus_clip_composer_app_artefacts/DEBUG/`,
  SDK now a submodule at `third_party/orpheus-sdk`, launch via
  `build-launch.sh`/`clean-relaunch.sh`):
  - `.claude/skills/occ/` — `build-utils.sh`, `validate.sh`, `README.md`
  - `.claude/agents/occ/` — `build-and-test.md`, `release-builder.md`,
    `session-validator.md`, `waveform-optimization.md`, `README.md`
    (the `backups/pre-frontmatter/` cruft was **not** carried over)
- **Deleted** from the SDK (superseded by the app repo's own launch scripts;
  hardcoded the old subdir paths):
  - `.claude/skills/project/occ-quick-rebuild/`
  - `scripts/relaunch-occ.sh`
  - (plus the migrated `.claude/skills/occ/` and `.claude/agents/occ/` originals)
- **Stale navigational references** redirected to the standalone repo in
  `.claude/README.md`, `.claude/skills.json`, `.codex/AGENTS.md`, and
  `.claude/skills/project/shmui/shmui.md`. `.claude/implementation_progress.md`
  (a historical work-log) got a redirect banner rather than rewriting its
  historical `apps/clip-composer/...` path entries.

**Verification:** the SDK still configures and compiles cleanly
(`cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` + `orpheus_transport` build,
both exit 0), and `.claude/skills.json` remains valid JSON. Nothing in the app
repo was overwritten — it had no prior `.claude/` directory.

## Preservation guarantee

Nothing was deleted that was not either (a) byte-verified present in
`~/archived-repos/clip-composer-sdk-subdir/`, or (b) a regenerable build
artifact. The full migrated app remains at `~/dev/clip-composer` and on GitHub.
