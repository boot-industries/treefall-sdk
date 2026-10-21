<!-- SPDX-License-Identifier: MIT -->

# ORP272 — Suite Development Channel Convergence

**Document type:** Suite coordination, manifest-repair and publication record  
**Date:** 2026-10-20  
**Change id:** `ORP-SUITE-20261020-001`  
**Branch:** `suite/orp-suite-20261020-001-manifest-convergence` → `main`  
**Status:** Open — the manifest corrections are published here; the publication
results, the Whitebox convergence and the final verification sweep are recorded
in the follow-up commit for the same change id, because the suite merge order
requires this repository to publish first.

## Objective

Bring the five-repository Treefall Suite to the state the development channel
of `suite/orpheus-suite.json` itself defines: every checkout on its declared
default branch, every manifest pin equal to the checkout's actual pin, every
generated artifact recorded with its current content hash and provenance, and
no unresolved CMake source resolution. The end state is the coordinator's own
verdict:

```sh
python3 tools/suite.py status --channel development \
  --workspace-root /Users/chrislyons/dev --json   # "errors": []
```

This is a development-channel convergence, not a release. No candidate or
stable snapshot is created, no `release` or `rollback` runs, no version is
promoted, and `snapshots[]` is untouched.

## Refs

| Record | Role in this change |
| --- | --- |
| `suite/orpheus-suite.json` | The system of record, edited here |
| `suite/schema/orpheus-suite.schema.json` | The manifest's schema; unchanged |
| `suite/README.md` | The operational procedure for the coordinator |
| `tools/suite.py` | The coordinator that reports every error class below |
| `tools/shmui_juce_manifest.py` | The post-import hash refresher/check, not an importer |
| `docs/orp/ORP269 Suite Floating Development Coordinator Repair.md` | The floating development-channel semantics used here |
| `docs/orp/ORP271 ShmUI Console Icon Import and Attribution Gate.md` | The current governed import procedure and its commands |
| `docs/orp/ORP123 Wireframes and Documentation Update.md` | The historical `rsync` mirror instruction |
| `docs/orp/ORP175 ShmUI Operational State Contract Handoff.md` | The mirrored file set (`juce/Source`, `juce/tests`, adapted CMake) |
| `docs/orp/ORP265 Treefall SDK Name Establishment.md` | The import-provenance rule |
| `shmui/scripts/sync-juce.sh` | The upstream mirror that performs the import |
| `shmui/juce/ImportContract.json` | The upstream template the import manifest is generated from |
| `fourtrack/docs/ftr/FTR101 ShmUI Provenance Source Boundary Repair.md` | The FourTrack generated-artifact boundary |
| `fourtrack/scripts/check-shmui-swift.py` | The FourTrack consumer-owned provenance checker |
| `clip-composer/docs/occ/OCC216 PR 73 to PR 74 Carry-Forward Assessment.md` | The anchor that keeps `boot-industries/clip-composer#73` open |

## Baseline

`python3 tools/suite.py status --channel development --workspace-root
/Users/chrislyons/dev --json` was run from `/Users/chrislyons/dev/orpheus-sdk`
on 2026-10-20, after fetching the declared tracking ref of all five
repositories:

```sh
WORKSPACE=/Users/chrislyons/dev
for repository in orpheus-sdk shmui fourtrack freqfinder clip-composer; do
  git -C "$WORKSPACE/$repository" fetch origin main
done
python3 tools/suite.py status --channel development \
  --workspace-root "$WORKSPACE" --json
```

The reported error list, quoted exactly:

```json
"errors": [
  "orpheus-sdk: artifact sdk-shmui-juce-package is hash-mismatch",
  "orpheus-sdk: artifact sdk-shmui-juce-package provenance is source-changed",
  "fourtrack: artifact fourtrack-shmui-manifest provenance is source-changed",
  "freqfinder: orpheus-sdk resolution is unavailable",
  "freqfinder: shmui resolution is unavailable",
  "clip-composer: drifted",
  "clip-composer: orpheus-sdk pin is drifted"
]
```

## Diagnosis

### 1. Clip Composer drift and the stale SDK pin

The development channel is floating (`channels.development` is
`{"mode": "floating", "branch": "main", "snapshot_id": null}`), so
`tools/suite.py` compares each checkout's `HEAD` with the declared tracking
ref `origin/main` (`remote_branch_revision`, `tools/suite.py:465`) and reports
any difference as drift.

```sh
git -C /Users/chrislyons/dev/clip-composer rev-parse --abbrev-ref HEAD
# ui/occ-222-shmui-console-icons
git -C /Users/chrislyons/dev/clip-composer rev-list --count origin/main..HEAD
# 12
```

The checkout was on the unpublished OCC222–OCC226 branch at `58826e7`, twelve
commits ahead of `origin/main` (`61cc4790`), with no upstream — hence
`clip-composer: drifted`.

The same repository's pin was stale in the manifest. `repositories[4]
.dependency_pins[0]` recorded

```json
{ "dependency": "orpheus-sdk", "kind": "git-submodule",
  "path": "third_party/orpheus-sdk",
  "revision": "f37503b23f72d7c0bd78ff1276be3c5d4817cee8" }
```

while the checkout's submodule is at a different, already-merged revision:

```sh
git -C /Users/chrislyons/dev/clip-composer submodule status third_party/orpheus-sdk
# 54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21 third_party/orpheus-sdk
```

That is `clip-composer: orpheus-sdk pin is drifted`. The recorded revision
`f37503b2…` is the SDK's SDK-0.9.1 source-adoption commit; the checkout
carries `54329bf0…`, the SDK merge commit that published the governed
ShmUI-JUCE console-icon import (ORP271). The branch's own gitlink is already
`54329bf0…`, so the merge of that branch into `main` carries the correct pin.

### 2. The SDK ShmUI-JUCE artifact hash and provenance

`repositories[0].generated_artifacts[0]` (`id: sdk-shmui-juce-package`,
`path: packages/shmui-juce`) recorded the pre-ORP271 import:

```json
"expected_sha256": "c258caee7eb1c639e2fe7aa654b926aa216d5232149aad52423f897c91167655",
"source_revision": "1f2c87a089d975eec1396360173da87a926835ed",
"current_source_revision": "1f2c87a089d975eec1396360173da87a926835ed"
```

The coordinator hashes the vendored tree itself (`hash_tree`, `tools/suite.py:539`,
excluding only `shmui-juce-import.json`) and reported

```
actual_sha256: 4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7
```

which is `orpheus-sdk: artifact sdk-shmui-juce-package is hash-mismatch`: the
manifest still described the package that existed before the governed
console-icon import.

The provenance error has the same root. The coordinator compares the recorded
`source_revision` with the live ShmUI `HEAD` over the artifact's `source_paths`
(`juce/Source`, `juce/tests`, `juce/CMakeLists.txt`, `juce/ImportContract.json`):

```sh
git -C /Users/chrislyons/dev/shmui diff --quiet \
  1f2c87a089d975eec1396360173da87a926835ed HEAD -- \
  juce/Source juce/tests juce/CMakeLists.txt juce/ImportContract.json
# exit 1  (24 files changed, 2304 insertions, 91 deletions)
```

The governed ShmUI JUCE inputs did change after `1f2c87a0…`, so the artifact
was reported as `provenance is source-changed`. The package's own
`packages/shmui-juce/shmui-juce-import.json` already named the current
import — `source.revision` `d420ec64…`, `source.content_sha256`
`4d2d4c5b…`, `design_token_contract_version` `0.6.0` — so the manifest, not the
package, was out of date.

### 3. The FourTrack ShmUI manifest provenance

`repositories[2].generated_artifacts[0]` (`id: fourtrack-shmui-manifest`,
`path: apps/fourtrack-mac/Generated/Shmui/manifest.json`) also recorded
`source_revision`/`current_source_revision` `1f2c87a0…`, and its
`source_paths` include `juce/Source`:

```sh
git -C /Users/chrislyons/dev/shmui diff --quiet \
  1f2c87a089d975eec1396360173da87a926835ed HEAD -- \
  juce/Source apps/www/styles/orpheus-tokens.css
# exit 1  (21 files changed)
```

so the coordinator reported `fourtrack: artifact fourtrack-shmui-manifest
provenance is source-changed`.

The FourTrack bytes themselves were never stale: no `hash-mismatch` was
reported for that artifact, and the vendored pair is byte-identical to the
canonical ShmUI output:

```sh
cmp /Users/chrislyons/dev/shmui/dist/swift/manifest.json \
    /Users/chrislyons/dev/fourtrack/apps/fourtrack-mac/Generated/Shmui/manifest.json
# identical
shasum -a 256 /Users/chrislyons/dev/fourtrack/apps/fourtrack-mac/Generated/Shmui/manifest.json
# 86385b412bfcc28d38a8d0fbbbbd2d98d69f6a138af864644dc7d2dcd14ff3a8
```

The defect was that the suite manifest described the artifact's source
revision as `1f2c87a0…` while the governed ShmUI source has since moved to
`d420ec64…`.

### 4. The FreqFinder CMake source resolution

`repositories[3].dependency_pins[]` are `cmake-source-override` pins resolved
against the FreqFinder build cache (`tools/suite.py:735-748`): the
coordinator reads `ORPHEUS_SDK_SOURCE_DIR` and `SHMUI_JUCE_SOURCE_DIR` from
`freqfinder/build/CMakeCache.txt` (or `freqfinder/build-release/CMakeCache.txt`)
and compares them with the manifest's `configured_path`/`workspace_path`. No
FreqFinder build tree existed in the workspace, so no cache entries were found
and both pins were reported `resolution is unavailable`.

The recorded revisions were stale as well: `ca949b2e…` for the SDK override
and `1e8f6de2…` for the ShmUI override, while the workspace's tracking refs
were `7009e9454f80db83b1262b8ac0f3077ddbe1e351` and
`452fb2042ac63254e68fbc7f8d41d299e3c9d72d`.

FreqFinder's own `CMakeLists.txt` consumes exactly the two variables the
manifest names — `ORPHEUS_SDK_SOURCE_DIR` must name a tree containing
`CMakeLists.txt` and `include/orpheus/audio_analysis.h`, and
`SHMUI_JUCE_SOURCE_DIR` must name a ShmUI JUCE source tree containing
`CMakeLists.txt` — so the `workspace_path`, `configured_path` and `config`
fields were already correct and only the recorded revisions and the missing
configured build tree were at fault.

## The governed import, as discovered

The import is performed by the upstream mirror script, not by hand and not by
the SDK's manifest tool. The chain of records that establishes this:

* `docs/orp/ORP123 Wireframes and Documentation Update.md:92` records the
  original mirror as `rsync -av --delete ~/dev/shmui/juce/Source/
  ~/dev/orpheus-sdk/packages/shmui-juce/`. That is the historical instruction,
  superseded by the scripted mirror.
* `docs/orp/ORP175 ShmUI Operational State Contract Handoff.md:17` records the
  v0.6.0 handoff mirroring ShmUI `juce/Source`, `juce/tests` and the adapted
  CMake file into `packages/shmui-juce`.
* `docs/orp/ORP265 Treefall SDK Name Establishment.md:63` records the
  provenance rule: every file in the package is hashed into
  `shmui-juce-import.json`, so editing the package without a new upstream
  ShmUI revision would falsify import provenance.
* `docs/orp/ORP271 ShmUI Console Icon Import and Attribution Gate.md` records
  the current procedure and its exact commands, and states that the import was
  produced by the governed mirror rather than by hand.
* The procedure itself is `shmui/scripts/sync-juce.sh`, owned by the source
  authority. It mirrors `shmui/juce/Source/` (flattened) and `shmui/juce/tests/`
  into the package, generates the package `CMakeLists.txt` from
  `shmui/juce/CMakeLists.txt`, formats the destination copies with the SDK's
  `.clang-format`, and regenerates `shmui-juce-import.json` from
  `shmui/juce/ImportContract.json` with `@SOURCE_REVISION@` and
  `@CONTENT_SHA256@` substituted.
* `tools/shmui_juce_manifest.py` is only the post-import refresher: `--sync`
  rewrites `source.content_sha256` over the already-vendored package and
  `--check` verifies it. It never copies source and never advances
  `source.revision`.

**Revision imported.** The mirror stamps
`git -C <shmui> log -1 --format=%H -- juce/Source juce/tests
juce/CMakeLists.txt juce/ImportContract.json` into the import manifest, so the
imported revision is the last commit on ShmUI's declared default branch that
touches the governed JUCE inputs:

```sh
git -C /Users/chrislyons/dev/shmui log -1 --format=%H -- \
  juce/Source juce/tests juce/CMakeLists.txt juce/ImportContract.json
# d420ec64dcb5fd16baa0b95d5048ee09bb95eee8
```

`d420ec64dcb5fd16baa0b95d5048ee09bb95eee8` is
`fix(juce): deliver the classic outlined shift mark` (2026-10-20 02:04:34
-0400). ShmUI's `main` tip is `452fb2042ac63254e68fbc7f8d41d299e3c9d72d`,
which does not touch the governed inputs, so the imported revision and the
branch tip are different commits and the tip was not assumed.

**Files vendored.** The mirror produced 77 hashed files in
`packages/shmui-juce`: the flattened `juce/Source` tree (including
`Icons/ThirdParty/`, `Shaders/`, `Utils/`), the `juce/tests` fixtures under
`tests/`, and the generated package `CMakeLists.txt`. The import manifest
itself is excluded from the content hash by the manifest's declared
`exclude_paths` and by the tool.

**SDK-side adaptations.** The package is not a verbatim copy of upstream:

* `packages/shmui-juce/CMakeLists.txt` is generated by the upstream mirror
  (`gen_pkg_cmake`) for the SDK's flattened package layout. The only
  difference from `shmui/juce/CMakeLists.txt` is line 44,
  `set(SHMUI_SRC "${CMAKE_CURRENT_SOURCE_DIR}")` instead of
  `"${CMAKE_CURRENT_SOURCE_DIR}/Source"`.
* The mirrored C/C++ sources are re-formatted to the SDK's `.clang-format`
  (LLVM, 2-space) instead of ShmUI's own 4-space style, so the bytes the SDK
  stores are the bytes the SDK's formatter produces.
* `packages/shmui-juce/Shaders/` is an upstream copy: `diff -rq` against
  `shmui/juce/Source/Shaders` reports no difference. The task brief's
  expectation that `Shaders/` is an SDK-side adaptation is not supported by
  the tree; the only SDK-side adaptation is the generated CMake file and the
  formatter style.

**Commands used.**

```sh
cd /Users/chrislyons/dev/shmui
SDK_ROOT=/Users/chrislyons/dev/orpheus-sdk scripts/sync-juce.sh
SDK_ROOT=/Users/chrislyons/dev/orpheus-sdk scripts/sync-juce.sh --check
cd /Users/chrislyons/dev/orpheus-sdk
python3 tools/shmui_juce_manifest.py --sync
python3 tools/shmui_juce_manifest.py --check
```

The mirror is idempotent here: the package was already the exact output of a
fresh mirror of `d420ec64…` (ORP271 had performed that import), so the
governed import changed no file and the SDK worktree stayed clean. The
refreshed hash is the same value the import manifest already carried:

```
ShmUI-JUCE manifest is consistent: 77 files, sha256 4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7
```

## Changes

### `orpheus-sdk` (system of record)

* `suite/orpheus-suite.json`
  * `repositories[0].generated_artifacts[0].expected_sha256` →
    `4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7`, the
    coordinator-computed hash of the imported package.
  * `repositories[0].generated_artifacts[0].source_revision` and
    `current_source_revision` → `d420ec64dcb5fd16baa0b95d5048ee09bb95eee8`,
    the revision the package's own import manifest names.
  * `repositories[2].generated_artifacts[*].source_revision` and
    `current_source_revision` → `d420ec64dcb5fd16baa0b95d5048ee09bb95eee8`.
    The FourTrack content hashes are unchanged because the vendored bytes are
    byte-identical to ShmUI's canonical `dist/swift` output.
  * `repositories[3].dependency_pins[].revision` → the SDK tracking revision
    `7009e9454f80db83b1262b8ac0f3077ddbe1e351` and the ShmUI tracking revision
    `452fb2042ac63254e68fbc7f8d41d299e3c9d72d`. `workspace_path`,
    `configured_path` and `config` are unchanged: they already match what
    FreqFinder's `CMakeLists.txt` consumes.
  * `repositories[4].dependency_pins[0].revision` →
    `54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21`, the revision the Clip
    Composer checkout and its branch gitlink actually carry. The pin is not
    moved; the manifest is corrected to reality.
  * `manifest_revision` → `2026-10-20`.
  * `snapshots[]` is untouched.
* `docs/orp/ORP272 Suite Development Channel Convergence.md` — this record.

### `shmui`

No change. The checkout was already on `origin/main` at `452fb204…`, and the
revision the import names (`d420ec64…`) is present in it. ShmUI is
fetch-only in the manifest (`push_url: null`), so it publishes nothing here.

### `fourtrack`

The generated ShmUI pair is re-vendored from ShmUI's canonical output at the
imported revision and its provenance record is checked with the consumer-owned
checker. The bytes are unchanged, because the canonical output at
`d420ec64…` is byte-identical to the vendored pair.

### `freqfinder`

The two `cmake-source-override` pins are made resolvable by configuring the
FreqFinder build tree with the two variables the manifest names, and the
manifest records the revisions the workspace actually resolves. The untracked
`docs/frq/FRQ048 Clean Prefix Package Qualification.md` is left exactly as
found.

### `clip-composer`

No source change and no new commit on the feature branch. The reviewed
`ui/occ-222-shmui-console-icons` branch (12 commits, head `58826e7`) is
published to `main` and the local checkout is moved to the merged `main`.

## Coordination

| Fact | Value |
| --- | --- |
| Change id | `ORP-SUITE-20261020-001` |
| System of record | `orpheus-sdk` |
| Branch prefix | `suite/` |
| PR title prefix | `suite:` |
| Merge order | `shmui` → `orpheus-sdk` → `fourtrack` → `freqfinder` → `clip-composer` |

Planned branches and PR titles for this change id:

| Repository | Branch | PR title |
| --- | --- | --- |
| `orpheus-sdk` | `suite/orp-suite-20261020-001-manifest-convergence` | `suite: reconcile the suite manifest with the governed ShmUI import and repository pins` |
| `fourtrack` | `suite/orp-suite-20261020-001-shmui-provenance` | `suite: refresh the ShmUI generated-artifact provenance` |
| `freqfinder` | `suite/orp-suite-20261020-001-source-pins` | `suite: record the resolvable Orpheus SDK and ShmUI source pins` |
| `clip-composer` | `ui/occ-222-shmui-console-icons` (published as-is) | `suite: publish the OCC222–OCC226 shared console icon layer and native content view` |

Every PR body carries the change id and the coordinator's origin marker, so
the generated changes do not trigger another coordinator run.

## Deferred / not done

* **The Clip Composer SDK submodule pin is not moved.** The pin stays at
  `54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21`. The SDK's `main` tip differs
  from it only by a documentation delta and `shmui-juce-import.json` is
  byte-identical at both, so re-pinning is not required for convergence and
  would change the built artifact. The manifest is corrected to the actual pin
  instead.
* **The OCC225 Clip Edit pointer-offset investigation stays open.** It is
  recorded in `clip-composer/docs/occ/OCC225 …` and is not touched here.
* **The PR #73 transparent-child-meter SDK handoff is not addressed.**
  `boot-industries/clip-composer#73` (branch
  `ui/occ212-morphic-mcg-and-typography`) stays open and untouched, as
  `clip-composer/docs/occ/OCC216 PR 73 to PR 74 Carry-Forward Assessment.md`
  designates. Its morphic face, typography and F-key compaction are not
  implemented.
* **No release.** No candidate or stable snapshot, no `release`, no
  `rollback`, no version promotion, no change to `snapshots[]`.
* **The `docs/orp/README.md` catalog is not updated.** That file is excluded
  from version control by the repository's own `.gitignore` rule `/docs/orp/`
  and has not been maintained since 2025; the versioned catalog for ORP
  records is `.claude/implementation_progress.md`, which this change updates.
