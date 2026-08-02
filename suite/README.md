# Orpheus Suite synchronization and release management

This directory is the versioned suite coordination contract. The Orpheus SDK
repository is the system of record because it already owns the cross-cutting
release tooling and generated ShmUI-JUCE manifest check. The suite is **not** a
new superproject: FourTrack and Clip Composer retain their application-owned
SDK submodules, while FreqFinder retains its package/source resolution modes.

## Decision

Use a hybrid model:

- `orpheus-suite.json` is the human-reviewable, machine-validated manifest.
- `schema/orpheus-suite.schema.json` defines its versioned shape.
- `../tools/suite.py` is a standard-library Python coordinator.
- `.github/workflows/suite-coherence.yml` validates the manifest and safe CLI
  defaults; repository-specific build and native acceptance workflows remain in
  their owning repositories.

A dedicated manifest repository was rejected because the SDK already owns the
release/check ecosystem and a new private repository would add an ownership and
credential boundary without improving the current graph. A submodule-only
superproject was rejected because gitlinks express exact checkout commits but
not compatibility windows, generated provenance, acceptance evidence, release
channels, sequencing, or rollback policy. A monorepo was rejected because the
five repositories have independent ownership, native/macOS build boundaries,
assets, and release cadence. The coordinator is optional automation behind the
manifest, not a second source of truth.

## Evidence-backed graph

The manifest edges are based on repository evidence, not repository names:

| Provider | Consumer | Evidence |
| --- | --- | --- |
| `shmui` | `orpheus-sdk` | `shmui/scripts/sync-juce.sh` identifies `shmui/juce/Source` as the source of truth and mirrors it to `orpheus-sdk/packages/shmui-juce`. |
| `orpheus-sdk` | `fourtrack` | `fourtrack/.gitmodules` and `fourtrack/CMakeLists.txt` consume `third_party/orpheus-sdk`. |
| `orpheus-sdk` | `clip-composer` | `clip-composer/.gitmodules` and `clip-composer/CMakeLists.txt` consume `third_party/orpheus-sdk`. |
| `orpheus-sdk` | `freqfinder` | `freqfinder/CMakeLists.txt` supports an installed package or `ORPHEUS_SDK_SOURCE_DIR`; the observed `build-release/CMakeCache.txt` uses the source override. |
| `shmui` | `fourtrack` | `fourtrack/apps/fourtrack-mac/Generated/Shmui/provenance.json` records the ShmUI revision, SDK revision, token contract, and generated manifest hash. |
| `shmui` | `freqfinder` | `freqfinder/CMakeLists.txt` supports `SHMUI_JUCE_SOURCE_DIR`; the observed cache resolves the sibling `shmui/juce`. |

The edge direction is provider → consumer. `suite.py affected shmui` computes
its transitive downstream closure and returns the declared release order.

## Commands

Run from the SDK checkout. All commands are read-only or dry-run unless both
`--apply` and `--yes` are supplied.

```text
python3 tools/suite.py validate --json
python3 tools/suite.py status --json
python3 tools/suite.py doctor --checks --json
python3 tools/suite.py affected shmui --json
python3 tools/suite.py verify --json                 # quick checks
python3 tools/suite.py verify --full --json          # all declared checks

# Plan an exact snapshot sync; --affected takes provider IDs in --repositories.
python3 tools/suite.py sync --snapshot <id> --json
python3 tools/suite.py sync --snapshot <id> --affected --repositories shmui --json
python3 tools/suite.py sync --snapshot <id> --apply --yes --json

# Plan exact consumer-pin updates. Source/package overrides stay manual.
python3 tools/suite.py update orpheus-sdk --revision <sha> --json

# Plan or publish independent dependent PRs.
python3 tools/suite.py coordinate orpheus-sdk \
  --revision <sha> --change-id ORP-SUITE-YYYYMMDD-NNN --json

# Release commands are guarded by checks and acceptance evidence.
python3 tools/suite.py release candidate --from-snapshot <id> \
  --version X.Y.Z --run-checks --acceptance acceptance.json --json
python3 tools/suite.py release stable --from-snapshot <candidate-id> --json

# Whole-suite rollback is also dry-run by default.
python3 tools/suite.py rollback <snapshot-id> --json
```

`status` separates checkout drift, dirty worktrees, remote mismatch, exact
consumer-pin drift, configured source/package resolution, generated content
hash mismatch, and source provenance. A generated artifact may retain an older
generator commit when its declared current source revision and content hash are
still valid; that historic revision is not silently rewritten.

`doctor --checks` runs the manifest's quick commands. `verify` runs quick checks
by default and all declared checks with `--full`. A check whose executable is
unavailable is reported as `unavailable`, not misreported as a product failure.
Unavailable platform/tooling still blocks release promotion.

`update --apply --yes` can stage an exact git-submodule pin and updates the
manifest's observed consumer pin with a timestamped manifest backup. CMake
source overrides and installed-package selection are reported for an
application owner to reconfigure and review rather than guessed or rewritten.

`coordinate --apply --yes` requires a short-lived GitHub App/fine-grained token
in `ORPHEUS_SUITE_GITHUB_TOKEN` (or `--token-env`). It creates a temporary
worktree, a `suite/<change-id>-<repository>` branch, one pin commit, and one PR
per supported submodule consumer. It uses the manifest's push remote and puts
the change ID in the PR body. Generated PRs carry a loop-prevention marker and
must not trigger a second coordinator run. Source/package consumers remain a
manual plan item.

## Channels and snapshots

The initial `workspace-20260801` record is an immutable **observed development
snapshot**, not a release claim. It records exact commits and dependency pins:

- SDK `0a9e9c34f435b9862d78ab9994ffb48a9f21e149`;
- ShmUI `11ad82bcc500cd187446841b92c5c8ac7c04fef3`;
- FourTrack `87c48b4d53ec0b3bccb1d6c03fcbe4a8ea03ba98`, SDK pin
  `8e8f56629a8aee9062635a7f07b5a03952de4408`;
- FreqFinder `2978d1c38c7ade33ca31022a1cec0f7540e29e71`, using current SDK and
  ShmUI source overrides;
- Clip Composer `3d9d324473b86babd6e5ab82bac349baead47d86`, SDK pin
  `112b49cbee32ef71276c8a451e2d1bd7f6ee7e4c`.

Development follows declared default branches. Candidate creation requires a
clean workspace, reachable exact revisions, generated-artifact freshness and
hashes, all declared checks, and passed human acceptance. Stable promotion
reuses the exact candidate snapshot and changes channel metadata only. No
moving branch is a stable compatibility claim.

`sync` and `rollback` preflight every target revision, refuse dirty worktrees,
create `refs/suite/backups/<timestamp>/<repository>` refs, and apply in
provider/dependency order only after explicit confirmation. Submodule pins are
part of the snapshot plan. A failure stops without reset/clean/discard and
leaves backup refs for a later explicit recovery command.

## Current change boundary

Only `orpheus-sdk` needs implementation changes: the manifest, schema, CLI,
CI safety gate, CTest registration, and generated SDK ShmUI provenance manifest.
`shmui`, `fourtrack`, `freqfinder`, and `clip-composer` are intentional no-ops;
their existing source, consumer, and acceptance contracts are represented in
the manifest without speculative application edits.

The inventory also found that ShmUI's `origin` is fetch-only (`no_push`).
The manifest records that missing push capability explicitly; `status` and
release gates classify it as `push remote is unavailable` instead of claiming
that a ShmUI branch or PR can be published. FourTrack's authorized `publish`
remote is recorded separately.

The pre-write synchronization checks were run first. ShmUI reported only the
stale SDK import-manifest revision; the existing sync script was then invoked
explicitly, and both `shmui/scripts/sync-juce.sh --check` and
`python3 tools/shmui_juce_manifest.py --check` pass afterward.

## Primary references

- [Git submodules](https://git-scm.com/docs/gitsubmodules) and
  [git-submodule](https://git-scm.com/docs/git-submodule)
- [git-repo manifest format](https://gerrit.googlesource.com/git-repo/+/refs/heads/main/docs/manifest-format.md)
- [GitHub workflow events](https://docs.github.com/en/actions/using-workflows/events-that-trigger-workflows)
- [GitHub ruleset rules](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets)
- [GitHub merge queues](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-a-merge-queue)
- [GitHub CODEOWNERS](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/about-code-owners)
- [Dependabot options](https://docs.github.com/en/code-security/reference/supply-chain-security/dependabot-options-reference)
- [Monorepo/polyrepo trade-offs](https://martinfowler.com/articles/microservice-trade-offs.html)

Organization-level decisions remain open: GitHub App installation and
least-privilege repository permissions, CODEOWNERS teams/rulesets, native macOS
runner availability, and the first public suite version/tag. The code does not
invent credentials or claim unavailable native acceptance evidence.
