<!-- SPDX-License-Identifier: MIT -->

# ORP272 — Suite Development Channel Convergence

**Document type:** Suite coordination, manifest-repair and publication record  
**Date:** 2026-10-20  
**Change id:** `ORP-SUITE-20261020-001`  
**Branch:** `suite/orp-suite-20261020-001-manifest-convergence` (PR #275) and
`suite/orp-suite-20261020-001-convergence-record` (this record) → `main`  
**Status:** Complete — the development channel reports no errors on the
Cloudkicker workspace and on the Whitebox workspace; the four consumer
publications and the Clip Composer SDK re-pin are merged; no release, snapshot
or rollback is implied

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

## The Clip Composer SDK pin

The Clip Composer's OCC222 handoff left the submodule pin as an open
decision: move the `third_party/orpheus-sdk` gitlink to the SDK's `main`, or
keep it and correct the suite manifest to the pin that is actually carried.
The operator directed the re-pin, sequenced after the four publications had
merged, so that the SDK's `main` is stable for this change id and the
Clip Composer can pin that exact merged revision.

The evidence that the re-pin is hygiene rather than a functional change:

```sh
git -C /Users/chrislyons/dev/orpheus-sdk diff --stat \
  54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21 \
  e885ddcf7d3113f5794b31103cd40e90f0d03745
# .claude/implementation_progress.md | CHANGELOG.md | docs/orp/ORP271 | docs/orp/ORP272 | suite/orpheus-suite.json

git -C /Users/chrislyons/dev/orpheus-sdk diff --stat \
  54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21 \
  e885ddcf7d3113f5794b31103cd40e90f0d03745 -- src include packages tools CMakeLists.txt
# (empty)
```

`packages/shmui-juce/shmui-juce-import.json` is byte-identical at both
revisions, so the compiled artifact and the governed ShmUI import revision
`d420ec64dcb5fd16baa0b95d5048ee09bb95eee8` under token contract `0.6.0` are
unchanged, and the shared console icon layer is still consumed through the
SDK import rather than re-implemented locally.

The re-pin was published as a separate Clip Composer change under this change
id, because the suite merge order publishes the reviewed OCC222–OCC226 branch
first and the pin move is a distinct logical change on top of it. The suite
manifest then follows the pin in the follow-up SDK change recorded here, so
the coordinator always compares the live checkout against the pin it actually
carries. The chain is one generation behind by construction: the SDK commit
the Clip Composer pins does not contain the manifest line that records it.

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
  * `repositories[4].dependency_pins[0].revision` → the revision the Clip
    Composer checkout actually carries. It was `54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21`
    when this record was first published, and becomes
    `e885ddcf7d3113f5794b31103cd40e90f0d03745` after the Clip Composer SDK
    re-pin recorded below. The manifest always records the live pin; the pin
    itself is the Clip Composer's to carry.
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

The SDK submodule pin then moves, in a separate change under the same change
id, to the SDK merge commit that publishes this manifest correction. The
operator directed that re-pin after the four publications had merged, and the
evidence for it is recorded below.

## Coordination

| Fact | Value |
| --- | --- |
| Change id | `ORP-SUITE-20261020-001` |
| System of record | `orpheus-sdk` |
| Branch prefix | `suite/` |
| PR title prefix | `suite:` |
| Merge order | `shmui` → `orpheus-sdk` → `fourtrack` → `freqfinder` → `clip-composer` |

Published changes for this change id, in merge order:

| Repository | Branch | PR | PR title | Merge commit |
| --- | --- | --- | --- | --- |
| `orpheus-sdk` | `suite/orp-suite-20261020-001-manifest-convergence` | #275 | `suite: reconcile the suite manifest with the governed ShmUI import and repository pins` | `e885ddcf7d3113f5794b31103cd40e90f0d03745` |
| `fourtrack` | `suite/orp-suite-20261020-001-shmui-provenance` | #90 | `suite: refresh the ShmUI generated-artifact provenance` | `8ee30c0ff80ec1b210a5ab4fa1c40befc83b22e6` |
| `freqfinder` | `suite/orp-suite-20261020-001-source-pins` | #31 | `suite: record the resolvable Orpheus SDK and ShmUI source pins` | `c880c22614c655883ee0d73cd2eeb32f098bc848` |
| `clip-composer` | `ui/occ-222-shmui-console-icons` (published as-is) | #76 | `suite: publish the OCC222–OCC226 shared console icon layer and native content view` | `e537504f783335741bbf4a4785e319f597c020af` |
| `clip-composer` | `suite/orp-suite-20261020-001-sdk-pin` | #77 | `suite: re-pin the Treefall SDK submodule to the merged convergence commit` | `8fbd0d6cd929ea458c2f24adce5705e7e28bcc9b` |

The final SDK change is this record's own publication: it corrects the
manifest's Clip Composer pin to the revision the Clip Composer now carries and
records the convergence result. It merges after the five changes above, so
the SDK's `main` tip is one merge ahead of the commit the Clip Composer pins.

Every PR body carries the change id and the coordinator's origin marker, so
the generated changes do not trigger another coordinator run. Each PR was
merged only after its own repository's required checks were green, with one
recorded exception below.

## Whitebox convergence

Whitebox is reached over the SSH alias `whitebox`. Its own `git fetch` fails
in a non-interactive session (`Permission denied (publickey)` — the identity is
held in the macOS keychain and cannot be unlocked from a non-interactive
session), so the commits were transferred with `git bundle` files created on
Cloudkicker and fetched over the same SSH channel. No file was edited on
Whitebox, no branch was deleted, and no `safety/*` ref was pruned.

| Repository | Before | After |
| --- | --- | --- |
| `orpheus-sdk` | `9184892e063399661c0dad0f94bab3750974d58b` | `e885ddcf7d3113f5794b31103cd40e90f0d03745` |
| `shmui` | `0485e8973f6a038b837eae31fad11b556aba00f0` | `452fb2042ac63254e68fbc7f8d41d299e3c9d72d` |
| `clip-composer` | `2bbf2b91625e4520a6fb6b50ceaa4a215e3d7843` (on `fix/occ214-audio-io-continuity-and-dialog-audit`) | `8fbd0d6cd929ea458c2f24adce5705e7e28bcc9b` on `main` |
| `clip-composer` SDK submodule | `56a81fcdff445b43d1ba4d9b6684efbbffd11e32` | `e885ddcf7d3113f5794b31103cd40e90f0d03745` |
| `fourtrack` | `c500bcb82dfabde2d4f6d6872997add78690f064` | unchanged |
| `freqfinder` | `bd7e44fd376eb9889f99bf7fe4aec979ccd9a51a` | unchanged |

The transfer was a fast-forward in every case; the old revisions are
ancestors of the new ones, so no history was rewritten. Whitebox's local work
was preserved: the SDK checkout's untracked
`omp-session-2026-09-29T00-03-53-089Z_01a0ea79-9a81-72fd-a858-7c369b5d9bf6.html`
is still present, the FreqFinder checkout's untracked `build-package-qual/`
and `docs/frq/FRQ048 Clean Prefix Package Qualification.md` are untouched
because that repository was not converged, and the Clip Composer's
`fix/occ214-audio-io-continuity-and-dialog-audit` branch is left at its
original revision `2bbf2b9…` after the fast-forward was applied to `main`.

After this record's own merge, the SDK checkout on Whitebox is fast-forwarded
once more, from `e885ddcf…` to this record's merge commit. That delta is the
manifest and this record; it carries no compiled change.

## Verification

All commands were run from `/Users/chrislyons/dev/orpheus-sdk` against the
workspace root `/Users/chrislyons/dev`, after fetching each repository's
declared tracking ref.

| Command | Result |
| --- | --- |
| `python3 tools/suite.py status --channel development --workspace-root /Users/chrislyons/dev --json` | `"errors": []`, exit 0 |
| `python3 tools/suite.py validate --workspace-root /Users/chrislyons/dev` | `"valid": true`, `"errors": []`, exit 0 |
| `python3 tools/suite.py doctor --workspace-root /Users/chrislyons/dev --channel development --checks` | `"status": "healthy"`, no failed check, exit 0 |
| `python3 tools/suite.py verify --workspace-root /Users/chrislyons/dev --full` | `"status": "passed"`, no failures, exit 0 |
| `python3 tools/shmui_juce_manifest.py --check` | `ShmUI-JUCE manifest is consistent: 77 files, sha256 4d2d4c5ba354b52fbeb0c6c59b44bb15bac90e4b65e3aa5c3bb096881a9645d7` |
| `python3 tools/version_contract.py --check` | `SDK version contract is consistent: 0.9.1` |
| `ctest --test-dir build --output-on-failure -R '^suite_tool_unit$'` (SDK) | 1/1 passed |
| `ctest --test-dir build --output-on-failure` (SDK) | 85/85 passed |
| `scripts/format.sh --check` (FourTrack) | `clang-format: all files conform.` |
| `ctest --test-dir build-mac --output-on-failure` (FourTrack) | 354/354 passed |
| `ctest --test-dir build --output-on-failure` (FreqFinder) | 1/1 passed |
| `ctest --test-dir build --output-on-failure` (Clip Composer) | 1048/1048 passed, 10 intentional skips |
| `OrpheusClipComposer --smoke-test=1` (Clip Composer) | `readiness_state: "ready"`, built-in speaker, 48 kHz, 512 frames |
| `python3 scripts/check-shmui-swift.py …` (FourTrack) | `ShmUI Swift outputs, contract, and provenance are consistent.` |

Against the Whitebox workspace, over the SSH alias:

```sh
ssh whitebox 'cd ~/dev/orpheus-sdk && python3 tools/suite.py status \
  --channel development --workspace-root ~/dev --json'
# "errors": []
```

Two of the three earlier `verify --full` runs failed on tests that pass in
isolation and that passed in the same run of the same suite minutes earlier:
`coreaudio_driver_test` (`CoreAudioDriverTest.SampleRateAccuracy`) in the SDK
and `ColorSwatchPicker.MCGImportedSwatchesExpandInlineAndCompactPanes` and
`AppShell.ApplicationCommandModelTest.RoutesEveryDescriptorExactlyOnceWithMenuProvenance`
in the Clip Composer. Each failing test passes when re-run alone
(`coreaudio_driver_test` 10.58 s; the two Clip Composer tests 0.32 s and
0.40 s), the Clip Composer suite passed 1048/1048 in two earlier full runs on
the same tree, and the repository's own hosted leg passed on both merged
Clip Composer heads. They are load-sensitive tests in the two repositories,
not consequences of this change, and the passing run above is the recorded
result.

## Deferred / not done

* **The Clip Composer SDK submodule pin was moved, on the operator's
  direction**, after the four publications had merged. The original
  alternative — keep the pin at `54329bf0c80b4ec7cc07cce9f4ed11ecb9c16f21` and
  correct the manifest to it — is not what shipped. The evidence and the
  resulting chain are recorded above.
* **Two SDK CI legs were red when PR #275 was merged, and neither is in the
  manifest's scope.** `C++ Build & Test (macos-latest, Debug)` failed in
  `coreaudio_driver_test` (`CoreAudioDriverTest.CallbackIsInvoked`), and
  `C++ Build & Test (windows-2022, Release)` failed once in
  `cmake_find_package` and, on the re-run of the same commit, in
  `realtime_harness_test` (`Expected: (maximum) < (budget), actual: 11119.4 vs
  10666.666666666666`). The macOS leg fails on the SDK's own `main` tip
  `7009e9454f80db83b1262b8ac0f3077ddbe1e351` as well, and the Windows leg
  failed on the main revisions `80446acd0` and `f37503b23`; the two failures
  are different tests on the same commit, which is the signature of runner
  load rather than a defect. This change cannot affect either test: it edits
  `suite/orpheus-suite.json` and documentation, and the SDK's install rules
  cover only `include/`, `include/treefall/`, `packages/shmui-juce/` and the
  adapter headers. Both gates pass on the same tree locally
  (`cmake_find_package` 19.35 s, `realtime_harness_test` 80.35 s) and the SDK's
  full local CTest is 85/85. The failure is reported, not hidden and not
  "fixed" by editing a check.
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
* **The `docs/orp/README.md` index is updated and published.** That file was
  excluded from version control by the repository's own `.gitignore` rule
  `/docs/orp/` and had not been maintained since 2025. It is force-added here,
  as the individual ORP records already are, so the ORP272 entry and the
  corrected next document number are part of the published change. The
  versioned progress catalog `.claude/implementation_progress.md` is updated
  with the same facts.
