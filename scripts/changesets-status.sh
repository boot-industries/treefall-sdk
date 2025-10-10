#!/usr/bin/env bash
set -euo pipefail

if ! git remote get-url origin >/dev/null 2>&1; then
  echo "→ Remote 'origin' not configured; skipping changeset status."
  exit 0
fi

git remote set-branches --add origin main || true
git fetch --no-tags --prune origin main:refs/remotes/origin/main
export CHANGESET_BASE=origin/main
echo "→ Running changeset status against ${CHANGESET_BASE}"
pnpm changeset status --since="${CHANGESET_BASE}"
