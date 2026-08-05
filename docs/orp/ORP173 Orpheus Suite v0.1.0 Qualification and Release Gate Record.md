# ORP173 Orpheus Suite v0.1.0 Qualification and Release Gate Record

**Status:** Published component merges and isolated qualification passed;
candidate capture remains blocked by missing native acceptance evidence
and required candidate gates (2026-08-05)
**Change ID:** `ORP-SUITE-20260805-001`
**Related:** ORP169, ORP170, FTR075

---

## Scope and safety boundary

All work ran under the dedicated suite root:

`/Users/chrislyons/dev/.orpheus-suite-worktrees/ORP-SUITE-20260805-001/`

The active checkouts under `/Users/chrislyons/dev/` were not switched, reset,
cleaned, staged, committed, or overwritten. Recovery branches remain separate:

- `safety/orp-suite-20260805-001-shmui`
- `safety/orp-suite-20260805-001-orpheus-sdk`
- `safety/orp-suite-20260805-001-fourtrack`
- `safety/orp-suite-20260805-001-freqfinder`
- `safety/orp-suite-20260805-001-clip-composer`

The qualification workspace is marked with `.orpheus-suite-workspace`. Its five
repositories are independent worktrees; no shared superproject or shared HEAD
was introduced.

## Selected local revision set

| Repository | Selected revision | Remote-main comparison | Qualification role |
| --- | --- | --- | --- |
| ShmUI | `a7b94b8be49875c7adc0ed5d4617150bb1d18cfa` | equals `origin/main`; PR #21 merged [3] | canonical JUCE source |
| Orpheus SDK | `582ab2e01723ccaa5e776845c5e774efecfc1c9d` | published package merge; reachable from current `origin/main` control merge `ba1db4f599a4d29b1d6938e5f6d288f409080d0a`; PR #236 and control PR #237 merged [1], [2] | package and suite system of record |
| FourTrack | `c77ed582c3fb07a4ec13da6e1cf0a649cb4bd03a` | equals `origin/main`; route-state PR #58 and suite PR #59 merged [4], [5] | native recorder consumer |
| FreqFinder | `20e80aa1d9cb8ce874525cdeb3d249b7d40b95fb` | equals `origin/main` | isolated source-override consumer |
| Clip Composer | `9d820fd05e44e2c2f6ccd8e176a93664e5e5332d` | equals `origin/main`; PR #35 merged [6] | SDK-pinned playback consumer |

The SDK control-record merge `ba1db4f599a4d29b1d6938e5f6d288f409080d0a`
contains this manifest and documentation; it is coordination metadata and is
not substituted for the selected package component revision `582ab2e...` [2].

The selected set is a published component set, not a release claim. Candidate
preflight still requires the declared checks, immutable snapshot creation, and
all six required native macOS acceptance records.

## Generated-artifact boundary

The canonical ShmUI sequence ran in order and left the source worktree clean:

1. `pnpm --filter=www gen:juce-tokens`
2. `pnpm --filter=www check:juce-tokens`
3. `pnpm --filter=www test:swift-tokens`
4. `pnpm --filter=www registry:build`
5. `pnpm --filter=www validate:registries`

The registry closure reported 55 source items and 65 files. Generated Swift
outputs remained byte-identical:

| Artifact | SHA-256 |
| --- | --- |
| `OrpheusDesignTokens.swift` | `13ddd076cc47e17ff807ab9d2c0c8a7e4656e339af63e3ec01d33fe5bee6da2c` |
| `manifest.json` | `a165e5aad550a9b073b22244cade3f1bbf8a329caadebb179c62b0d8732c9d60` |

The SDK ShmUI-JUCE package was regenerated from the selected ShmUI revision.
`sync-juce.sh --check` passed with an explicit `SDK_ROOT`, and
`python3 tools/shmui_juce_manifest.py --check` reported 54 files with package
hash `f34c156d9202261090d6185bbce356b23dd675a96f4c5f39ab90a07c8e4822bc`.

FourTrack's generated provenance checker, mutation tests, and format check
passed. Its provenance records ShmUI `a7b94b8...`, SDK `582ab2e...`, contract
`0.5.0`, and the generated manifest hash.

## Qualification evidence

| Qualification | Result |
| --- | --- |
| SDK configured build and CTest | Passed; 77/77 |
| Coordinator unit contracts | Passed; 10/10 |
| Coordinator manifest/schema validation | Passed |
| FourTrack Debug macOS configure and build | Passed; native app linked and ad-hoc signed |
| FourTrack CTest | Passed; 270/270 |
| FreqFinder Release source-override configure and build | Passed against isolated SDK `582ab2e...` and ShmUI `a7b94b8...` paths |
| FreqFinder CTest | Passed; one test target |
| Clip Composer Debug source build | Passed against isolated SDK `582ab2e...` |
| Clip Composer CTest | Passed; 634 passed and one intentional performance skip |
| FourTrack ShmUI provenance and mutation checks | Passed |

The FreqFinder cache resolves exactly:

- `ORPHEUS_SDK_SOURCE_DIR=/Users/chrislyons/dev/.orpheus-suite-worktrees/ORP-SUITE-20260805-001/qualification/orpheus-sdk`
- `SHMUI_JUCE_SOURCE_DIR=/Users/chrislyons/dev/.orpheus-suite-worktrees/ORP-SUITE-20260805-001/qualification/shmui/juce`

The final observed snapshot preflight passed with clean worktrees, fresh
artifact hashes, configured remotes, exact dependency pins, and remote-main
reachability. It captured:

- SDK `582ab2e01723ccaa5e776845c5e774efecfc1c9d`;
- ShmUI `a7b94b8be49875c7adc0ed5d4617150bb1d18cfa`;
- FourTrack `c77ed582c3fb07a4ec13da6e1cf0a649cb4bd03a`;
- FreqFinder `20e80aa1d9cb8ce874525cdeb3d249b7d40b95fb`; and
- Clip Composer `9d820fd05e44e2c2f6ccd8e176a93664e5e5332d`.

The immutable observed record is
`development-0-1-0-20260805-001`. The final candidate dry run with
`--run-checks` passed every declared candidate check; only the six required
human acceptance records blocked candidate creation.

## Rollback evidence

The historic `workspace-20260801` snapshot was exercised in a second,
sacrificial suite workspace:

1. `python3 tools/suite.py rollback workspace-20260801 --json` produced a plan
   with five repository revisions and two nested SDK pin operations, with no
   applied mutations.
2. The same command with `--apply --yes` detached all five rollback worktrees,
   applied both consumer SDK pins, and emitted backup refs under
   `refs/suite/backups/20260805T135352Z/`.
3. Post-apply inspection matched every snapshot commit and nested pin exactly;
   all rollback worktrees were clean and detached.

The rollback workspace was disposable and separate from both the active
checkouts and the qualification workspace.

## Acceptance ledgers and package prerequisite

The controller records the required acceptance shapes at:

- `suite/evidence/ORP-SUITE-20260805-001/candidate-acceptance.json`
- `suite/evidence/ORP-SUITE-20260805-001/stable-acceptance.json`

Both ledgers truthfully remain `pending`; no local build, visual check, native
hardware route, or performance run was relabeled as human acceptance. Stable
promotion also requires a versioned SDK package or tag at the selected SDK
revision. `git ls-remote origin` exposed the existing `v0.6.7` tag only; no
published tag or package resolves to the selected import revision.

## Release gate result

The immutable development snapshot
`development-0-1-0-20260805-001` was created. No candidate snapshot or stable
pointer was changed. This is intentional:

1. All five component revisions are published remote-main merges, and all
   dependency pins and generated-artifact provenance resolve to those exact
   SHAs.
2. The final candidate verification gate passed
   `sdk-shmui-manifest`, `sdk-version-contract`, `shmui-juce-freshness`,
   `shmui-token-contract`, `fourtrack-format`, `freqfinder-tests`, and
   `clip-composer-tests`.
3. Candidate acceptance records for native macOS launch and operator visual
   checks remain uncollected. No local build result is relabeled as human
   acceptance evidence.
4. Stable promotion additionally requires a versioned SDK package or tag at
   `582ab2e01723ccaa5e776845c5e774efecfc1c9d` and the six stable hardware,
   real-device route, realtime-performance, analysis-performance, and
   sixteen-clip records.

The immutable historic `workspace-20260801` snapshot remains unchanged. The
candidate and stable ledgers truthfully remain pending.

## Required continuation

Collect and record the six candidate acceptance pairs in
`suite/evidence/ORP-SUITE-20260805-001/candidate-acceptance.json`, then rerun:

`python3 tools/suite.py release candidate --manifest suite/orpheus-suite.json --workspace-root ../qualification --from-snapshot development-0-1-0-20260805-001 --snapshot-id candidate-0-1-0-20260805-001 --version 0.1.0 --acceptance suite/evidence/ORP-SUITE-20260805-001/candidate-acceptance.json --run-checks --apply --yes --json`

Only after a candidate exists may the stable hardware and performance evidence
be collected and `release stable` be attempted. Do not create a release tag or
rewrite either immutable snapshot.

## References

[1] C. Lyons, “suite: refresh ShmUI import (ORP-SUITE-20260805-001),” GitHub
pull request 236, Aug. 5, 2026. [Online]. Available:
https://github.com/chrislyons/orpheus-sdk/pull/236

[2] C. Lyons, “suite: record ORP170 synchronization checkpoint
(ORP-SUITE-20260805-001),” GitHub pull request 237, Aug. 5, 2026. [Online].
Available: https://github.com/chrislyons/orpheus-sdk/pull/237

[3] C. Lyons, “suite: fix isolated sync paths (ORP-SUITE-20260805-001),”
GitHub pull request 21, Aug. 5, 2026. [Online]. Available:
https://github.com/chrislyons/shmui/pull/21

[4] C. Lyons, “feat(ftr): add capability-driven CoreAudio route selection,”
GitHub pull request 58, Aug. 5, 2026. [Online]. Available:
https://github.com/chrislyons/fourtrack/pull/58

[5] C. Lyons, “suite: repin generated provenance to SDK merge
(ORP-SUITE-20260805-001),” GitHub pull request 59, Aug. 5, 2026. [Online].
Available: https://github.com/chrislyons/fourtrack/pull/59

[6] C. Lyons, “suite: repin SDK merge (ORP-SUITE-20260805-001),” GitHub pull
request 35, Aug. 5, 2026. [Online]. Available:
https://github.com/chrislyons/clip-composer/pull/35
