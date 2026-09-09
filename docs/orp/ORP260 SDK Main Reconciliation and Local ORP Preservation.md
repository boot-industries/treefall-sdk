# ORP260 SDK Main Reconciliation and Local ORP Preservation

**Status:** Completed local reconciliation; no remote push performed.
**Date:** 2026-09-09
**Merge commit:** `7fd025bb`
**Remote:** `git@github.com:chrislyons/treefall-sdk.git`

## Scope

Reconcile the local `main` branch with the renamed SDK repository while preserving both local commits and the local-only ORP documentation records. The checkout directory and active discovery paths remain unchanged.

## Decision

Merge `origin/main` into local `main`; do not hard-reset, force-push, or discard the local documentation tree. The resulting merge commit has local `main` commit `0fd475e8` and fetched remote tip `0238eac1` as parents.

## Preserved local work

- `6809bac9` — records the web contribution harness feasibility plan and renames the record to ORP180.
- `0fd475e8` — removes superseded ORP256 and ORP257 records and adds `treefall-sprint-prompt-corrected.md`.
- All 23 tracked files under `docs/orp/`, retained as local-only records.
- `treefall-sprint-prompt-corrected.md`.

## Conflict resolutions

The remote branch deleted the ORP record tree. The merge produced two unresolved paths:

1. `docs/orp/INDEX.md` — retained the local index.
2. `docs/orp/ORP180 Web Contribution Harness Feasibility and Plan.md` — retained the local renamed and updated record.

The remaining remote ORP deletions were resolved consistently by restoring the complete local `docs/orp/` tree. No non-ORP conflicts remained.

## Verification

- `origin` resolves to `git@github.com:chrislyons/treefall-sdk.git`.
- Both local commits and remote tip `0238eac1` are ancestors of `main`.
- `main` points to merge commit `7fd025bb` and is ahead of `origin/main` by one merge commit.
- The worktree has no unstaged, staged, untracked, or unresolved files.
- No push was performed.
