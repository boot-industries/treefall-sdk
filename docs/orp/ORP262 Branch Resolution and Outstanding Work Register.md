<!-- SPDX-License-Identifier: MIT -->
# ORP262 Branch Resolution and Outstanding Work Register

**Date:** 2026-09-10  
**Baseline:** `origin/main` at `b8c8b875`  
**Scope:** local and `origin` branches; every `safety/*` branch exempt from pruning

## Resolution performed

- Refreshed and pruned remote-tracking refs with `git fetch --prune origin`.
- Deleted 23 stale local branches whose tips were reachable from `origin/main` or whose pull requests were merged.
- Removed three clean, linked stale worktrees before deleting their merged branches: `feat/orp255-host-neutral-multichannel-metering`, `fix/shm015-level-meter-gradient-import`, and `fix/streaming-prefetch-realtime-sustain`.
- Deleted 21 merged remote branches from `origin`.
- Preserved `suite/orp-suite-20260805-001-controller-docs`: PR #238 is merged, but its linked worktree contains an unrelated modified `AGENTS.md`; deleting it would destroy uncommitted work.
- Created and pushed exempt snapshot `safety/2026-09-10-treefall-frontend-green` at `b8c8b875`.

## Outstanding local branches

Counts are `origin/main...branch`: behind/ahead commit counts. A branch is unresolved when it is not known merged; no deletion is safe from reachability alone.

| Branch | Updated | Tip | Behind | Ahead | Upstream | PR | Linked worktree |
|---|---:|---|---:|---:|---|---|---|
| `audit/release-hardening-2026-08-02` | 2026-08-03 | `b49f2e27` | 104 | 7 | `origin/audit/release-hardening-2026-08-02` | no PR found | `—` |
| `backup/main-before-canonical-reset` | 2026-07-31 | `ec1b6595` | 118 | 3 | `local only` | no PR found | `—` |
| `backup/main-before-pull-20260731` | 2026-07-30 | `5ec1b18a` | 120 | 2 | `local only` | no PR found | `—` |
| `backup/orp-suite-20260805-001-controller-premerge` | 2026-08-05 | `7f306075` | 94 | 9 | `local only` | no PR found | `—` |
| `backup/orp168-pre-rebase` | 2026-07-31 | `c752aff5` | 120 | 8 | `local only` | no PR found | `—` |
| `chore/boot-industries-urls` | 2026-09-09 | `dbd8de70` | 4 | 1 | `origin/chore/boot-industries-urls` | PR #260 open | `—` |
| `docs/orp154-fourtrack-handoff` | 2026-07-16 | `f085673c` | 182 | 1 | `local only` | no PR found | `—` |
| `docs/orp160-command-sheet-refresh` | 2026-07-25 | `8947e1ee` | 146 | 8 | `origin/docs/orp160-command-sheet-refresh` | no PR found | `—` |
| `docs/orp170-suite-sync-etiquette` | 2026-08-04 | `73332070` | 104 | 1 | `origin/main` | no PR found | `/Users/chrislyons/dev/orpheus-suite-sync-doc.U2yT1y/orpheus-sdk` |
| `feat-coreaudio-route-state` | 2026-08-03 | `b49f2e27` | 104 | 7 | `local only` | no PR found | `—` |
| `feat/orp254-shmui-segmented-meter` | 2026-08-31 | `a5474085` | 60 | 4 | `origin/feat/orp254-shmui-segmented-meter` | no PR found | `/Users/chrislyons/dev/orpheus-sdk-segmented-meter` |
| `feature/orp-output-contract-canonical` | 2026-08-13 | `40f37b03` | 103 | 1 | `local only` | no PR found | `/Users/chrislyons/dev/orpheus-sdk-contract` |
| `fix/coreaudio-directional-buffer-monitor` | 2026-08-13 | `c760e6b5` | 71 | 4 | `origin/fix/coreaudio-directional-buffer-monitor` | PR #247 open | `—` |
| `fix/orp182-shmui-meter-gradient-import-safety` | 2026-08-13 | `4e029f04` | 89 | 3 | `origin/fix/orp182-shmui-meter-gradient-import-safety` | no PR found | `/Users/chrislyons/dev/orpheus-sdk-shm015-meter-gradient-safety` |
| `fix/orp244-suite-handoff` | 2026-08-10 | `81c6a660` | 76 | 2 | `origin/feat/shm024-operational-state-parity` | no PR found | `—` |
| `recovery/stash-20251013-orp-docs` | 2025-10-13 | `017609eb` | 607 | 4 | `local only` | no PR found | `—` |
| `recovery/stash-20251027-phase3` | 2025-10-27 | `9aa45f86` | 607 | 54 | `local only` | no PR found | `—` |
| `recovery/stash-20251112-multivoice-debug` | 2025-11-12 | `be66b9ef` | 532 | 3 | `local only` | no PR found | `—` |
| `recovery/stash-20251114-occ130-sprint-b` | 2025-11-14 | `be1296da` | 526 | 6 | `local only` | no PR found | `—` |
| `recovery/stash-20251118-ux-improvements` | 2025-11-18 | `6d5da437` | 520 | 8 | `local only` | no PR found | `—` |
| `recovery/stash-20251128-main-github-desktop` | 2025-11-28 | `6fa6e996` | 505 | 2 | `local only` | no PR found | `—` |
| `recovery/stash-20251128-pr187-review` | 2025-11-28 | `2b013a93` | 504 | 2 | `local only` | no PR found | `—` |
| `recovery/stash-20260117-occ116-117-backend-menu-dialogs` | 2026-01-17 | `9f1c16cc` | 493 | 5 | `local only` | no PR found | `—` |
| `recovery/stash-20260227-occ-code-simplifier-audit` | 2026-02-27 | `0f67b500` | 433 | 2 | `local only` | no PR found | `—` |
| `recovery/stash-20260327-main-pre-sync` | 2026-03-27 | `bbde7fe3` | 429 | 3 | `local only` | no PR found | `—` |
| `refactor/20260726-private-shmui-cutover` | 2026-07-28 | `b8f30235` | 127 | 7 | `origin/refactor/20260726-private-shmui-cutover` | no PR found | `—` |
| `release/orp176-coreaudio-directional-src` | 2026-08-25 | `930c42d2` | 67 | 36 | `origin/release/orp176-coreaudio-directional-src` | PR #250 open | `—` |
| `suite/orp-suite-20260805-001-controller-docs` | 2026-08-05 | `4282c01e` | 92 | 1 | `origin/suite/orp-suite-20260805-001-controller-docs` | PR #238 merged | `/Users/chrislyons/dev/.orpheus-suite-worktrees/ORP-SUITE-20260805-001/controller` |

## Outstanding remote branches

| Branch | Updated | Tip | Behind | Ahead | PR |
|---|---:|---|---:|---:|---|
| `audit/release-hardening-2026-08-02` | 2026-08-02 | `e4753d37` | 104 | 2 | no PR found |
| `chore/boot-industries-urls` | 2026-09-09 | `dbd8de70` | 4 | 1 | PR #260 open |
| `claude/audit-code-bloat-01Pwxo6NQRewPJ6uQ2z2G5qD` | 2025-11-18 | `e52da046` | 519 | 1 | PR #182 closed |
| `claude/investigate-ci-macos-windows-011CUxAwYCSDHBxBAqbpGW7o` | 2025-11-09 | `1693af7c` | 565 | 3 | PR #160 closed |
| `claude/occ130-playhead-bug-fix-015XHrCAUFbUYT27S5pXLqpK` | 2025-11-14 | `0fa21b1d` | 526 | 2 | PR #174 closed |
| `claude/refactor-audio-backend-F2faZ` | 2026-01-18 | `b0ae8c40` | 493 | 1 | PR #192 closed |
| `docs/orp160-command-sheet-refresh` | 2026-07-21 | `883c65e8` | 146 | 3 | no PR found |
| `feat/orp162-fourtrack-live-output` | 2026-07-22 | `ad20160f` | 145 | 1 | no PR found |
| `feat/orp254-shmui-segmented-meter` | 2026-08-31 | `a5474085` | 60 | 4 | no PR found |
| `fix/coreaudio-directional-buffer-monitor` | 2026-08-12 | `cc59e3ef` | 71 | 4 | PR #247 open |
| `fix/orp168-live-metadata-priming` | 2026-08-28 | `a7ecd076` | 89 | 5 | no PR found |
| `fix/orp182-shmui-meter-gradient-import-safety` | 2026-08-13 | `eb1eb851` | 89 | 2 | no PR found |
| `fix/orp182-shmui-meter-import-safety` | 2026-08-12 | `7acdda20` | 89 | 1 | no PR found |
| `recovery/2026-07-28-sdk-112b49c` | 2026-07-27 | `112b49cb` | 133 | 1 | no PR found |
| `refactor/20260726-private-shmui-cutover` | 2026-07-26 | `1cfe6436` | 127 | 6 | no PR found |
| `release/orp176-coreaudio-directional-src` | 2026-08-21 | `fc9e4b0d` | 67 | 28 | PR #250 open |

## Active pull requests

- [#260 `chore/boot-industries-urls`](https://github.com/boot-industries/treefall-sdk/pull/260)
- [#250 `release/orp176-coreaudio-directional-src`](https://github.com/boot-industries/treefall-sdk/pull/250)
- [#247 `fix/coreaudio-directional-buffer-monitor`](https://github.com/boot-industries/treefall-sdk/pull/247)

## Exempt safety inventory

- Local: 4 branches.
- Remote: 7 branches.
- No `safety/*` ref was deleted or rewritten.

## Next resolution pass

1. Merge or close the three active pull requests, then prune their head branches.
2. Inspect the unique commits on branches marked `no PR found`; either land the still-relevant delta or record supersession before deletion.
3. Resolve or preserve the modified `AGENTS.md` in the PR #238 controller worktree, then remove that worktree and branch.
4. Retain all `safety/*` branches unless a separate explicit instruction removes the exemption.
