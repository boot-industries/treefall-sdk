<!-- SPDX-License-Identifier: MIT -->

# ORP269 — Suite Floating Development Coordinator Repair

**Document type:** Coordinator behavior and validation record  
**Date:** 2026-10-14  
**Branch:** `fix/suite-checkpoint-provenance`  
**Status:** Implementation ready for parent-owned verification; no release promotion

## Decision and behavior

The development channel follows each repository's declared `fetch_remote` and
`default_branch` tracking ref. Its `snapshot_id` is null. `status`, `doctor`,
and `snapshot observe` use that ref as the expected revision; they do not fetch
and do not substitute local `HEAD` when the ref is unavailable. A checkout that
differs from the tracking revision is reported as drift. An unavailable ref is
reported as `remote-branch-unavailable` with the ref and unavailable state in
the result. Status therefore reflects locally fetched remote-tracking data, not
a live network query; fetch the declared remote and branch before relying on it
as a current remote comparison.

Floating source-workspace dependency pins (such as CMake source overrides)
follow the dependency repository's declared remote/default-branch ref. Git
submodule pins remain exact, reviewed manifest revisions. Explicit `--snapshot`
selection remains immutable and pinned. Candidate and stable channels remain
pinned; a channel without a configured snapshot is reported as
`snapshot-unconfigured` rather than silently following a branch. Observing a
new snapshot records history but leaves the floating development pointer null.
Existing snapshot objects are not rewritten or removed.

The SDK's governed ShmUI-JUCE artifact expectation is derived from the imported
package and `packages/shmui-juce/shmui-juce-import.json`: source revision
`1f2c87a089d975eec1396360173da87a926835ed`, package content hash
`c258caee7eb1c639e2fe7aa654b926aa216d5232149aad52423f897c91167655`, and
contract `0.6.0`. The package tree hash was calculated from the current imported
files, excluding only the import metadata file as declared. The suite manifest
declares explicit SDK gitlink expectations for FourTrack and Clip Composer at
`56a81fcdff445b43d1ba4d9b6684efbbffd11e32` (SDK 0.9.1). FourTrack's approved
repair has merged; Clip Composer integration remains parent-owned. No
application source or native-runtime acceptance is claimed by this SDK
coordinator repair.

## Behavioral coverage

`tests/suite_test.py` covers a floating checkout following an advanced remote
branch, drift and convergence, immutable candidate/explicit snapshots, missing
tracking-ref and unconfigured pinned-channel errors, and a floating sibling
source override diverging from a preserved exact submodule pin. Snapshot
observation also asserts that the floating pointer stays null and prior
snapshot records remain unchanged.

## Validation recipe

From the SDK repository, fetch the manifest's declared branch for each repository
in the isolated suite workspace, then inspect development status:

```sh
WORKSPACE=/path/to/suite-workspace
for repository in orpheus-sdk shmui fourtrack freqfinder clip-composer; do
  git -C "$WORKSPACE/$repository" fetch origin main
done
python3 tools/suite.py status --channel development \
  --workspace-root "$WORKSPACE" --json
```

The coordinator's behavioral gate and manifest/schema validation are:

```sh
ctest --test-dir build --output-on-failure -R '^suite_tool_unit$'
python3 tools/suite.py validate --json
```

To inspect an immutable snapshot, set `WORKSPACE` as above and pass its ID
explicitly with:
`python3 tools/suite.py status --snapshot <snapshot-id> --workspace-root "$WORKSPACE" --json`.
Do not interpret an absent remote-tracking ref as proof that the remote branch itself was deleted;
fetch first, then distinguish fetch failure or an absent branch from local drift.

The initial worker ran no verification; parent-owned evidence follows below.
This change does not promote a candidate or stable release, qualify hardware,
or claim FourTrack native-runtime behavior.

## Parent-owned verification

Manifest/schema validation passed. The first unit run exposed an incorrect
unconfigured-snapshot branch classifying valid floating repositories as
`snapshot-unconfigured`; restricting that branch to non-floating mode corrected
all three failures. All 13 behavioral cases then passed (CTest gate 8.53 s).
The actual workspace `doctor` smoke reports healthy with no errors. Remaining
live canonical/Swift artifact source declarations were aligned with verified
ShmUI commit `1f2c87a`; hashes stayed unchanged. Fetching FourTrack's nested SDK
tracking ref resolved stale reachability without moving its reviewed gitlink.
Governed 58-file import validation passed. Full configured SDK CTest passed
84/84, zero failures (233.84 s), including package, realtime, docs and coordinator
gates. This is observed local verification, not new hosted or release acceptance.
