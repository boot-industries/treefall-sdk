#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "$REPO_ROOT"

echo "=== Phase 0 Validation ==="

echo ""
echo "→ Checking monorepo structure..."
[ -f pnpm-workspace.yaml ]
[ -d packages ]

echo ""
echo "→ Verifying dependency installation..."
pnpm install --frozen-lockfile
pnpm list --depth 0

echo ""
echo "→ Verifying workspace configuration..."
cat pnpm-workspace.yaml

echo ""
echo "→ Verifying C++ build..."
if [ ! -d build ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
fi
cmake --build build

echo ""
echo "→ Running C++ tests..."
ctest --test-dir build --output-on-failure

echo ""
echo "→ Running lint suite..."
pnpm run lint

echo ""
echo "✅ Phase 0 validation complete!"
