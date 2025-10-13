#!/usr/bin/env bash
# Integration Test Runner
# Run with: ./tests/integration/run-tests.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$ROOT_DIR"

echo "========================================="
echo "Orpheus SDK Integration Tests"
echo "========================================="
echo ""

# Ensure packages are built
echo "Step 1: Verifying package builds..."
if [ ! -d "packages/contract/dist" ] || \
   [ ! -d "packages/client/dist" ] || \
   [ ! -d "packages/react/dist" ]; then
  echo "⚠ Warning: Some packages not built. Building now..."
  pnpm run build
else
  echo "✓ Package builds verified"
fi

echo ""
echo "Step 2: Running smoke tests..."
node --test tests/integration/smoke.test.mjs

# Check if service driver is available
if [ -d "packages/engine-service/dist" ]; then
  echo ""
  echo "Step 3: Service driver tests..."
  echo "(Service driver tests require orpheusd running - skipping in CI)"
  echo "To test manually: start orpheusd and run service-driver.test.mjs"
else
  echo ""
  echo "Step 3: Service driver not built (skipping)"
fi

# Check if native driver is available
if [ -f "packages/engine-native/build/Release/orpheus_native.node" ]; then
  echo ""
  echo "Step 4: Native driver tests..."
  echo "(Native driver integration tests require C++ SDK built)"
else
  echo ""
  echo "Step 4: Native driver not built (skipping)"
fi

echo ""
echo "========================================="
echo "✓ Integration tests completed"
echo "========================================="
