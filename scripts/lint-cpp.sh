#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found. Please install clang-format >=14." >&2
  exit 1
fi

mapfile -t MODIFIED < <(git diff --name-only --diff-filter=ACMRTUXB)
mapfile -t STAGED < <(git diff --name-only --cached --diff-filter=ACMRTUXB)
mapfile -t UNTRACKED < <(git ls-files --others --exclude-standard)

mapfile -t CANDIDATES < <(
  printf '%s\n' "${MODIFIED[@]}" "${STAGED[@]}" "${UNTRACKED[@]}" |
    grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|ipp)$' |
    grep -E '^(src|include|apps|adapters|tests)/' |
    sort -u
)

if [ "${#CANDIDATES[@]}" -eq 0 ]; then
  echo "No modified C/C++ files detected; skipping clang-format and clang-tidy."
  exit 0
fi

echo "→ Running clang-format checks…"
clang-format -n --Werror "${CANDIDATES[@]}"

echo "→ Running clang-tidy checks…"
mapfile -t TIDY_FILES < <(
  printf '%s\n' "${CANDIDATES[@]}" | grep -E '\.(c|cc|cpp|cxx)$' || true
)

if [ "${#TIDY_FILES[@]}" -eq 0 ]; then
  echo "No translation units found for clang-tidy; skipping static analysis step."
  exit 0
fi

COMPILE_DATABASE="${REPO_ROOT}/build/compile_commands.json"
if [ ! -f "$COMPILE_DATABASE" ]; then
  echo "⚠️  compile_commands.json not found; skipping clang-tidy (install a build with exported commands to enable it)."
  exit 0
fi

clang-tidy "${TIDY_FILES[@]}" -p "$REPO_ROOT/build" --quiet || {
  echo "clang-tidy reported issues." >&2
  exit 1
}

echo "✓ C/C++ linting passed."
